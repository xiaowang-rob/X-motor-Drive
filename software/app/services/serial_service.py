"""串口传输层：端口扫描、连接、协议组帧/拆帧。

帧格式与固件 uart_com 一致：
  [head][id][len][payload...][crc8(id,len,payload)][tail]

当前只负责传输与命令 ID；具体 payload 编解码在 DeviceService 中标记 TODO。
"""

import threading
import time
from queue import Full, Queue

from PyQt5.QtCore import QObject, QTimer, pyqtSignal

try:
    import serial
    import serial.tools.list_ports
except ImportError:  # pragma: no cover - 运行环境缺少 pyserial
    serial = None

from protocol import CmdId, UartFrame


DEVICE_CONFIG = {
    "vendor_id": "0483",
    "devices": {
        "5741": {"name": "XDr-P", "full_name": "X motor Drive Power"},
        "5742": {"name": "XDr-S", "full_name": "X motor Drive Standard"},
        "5740": {"name": "XDr-bl", "full_name": "XDr Bootloader"},
    },
}
KNOWN_DEVICE_NAMES = {info["name"] for info in DEVICE_CONFIG["devices"].values()}
MAX_PAYLOAD = 128
AUTO_CONNECT = False  # 需要自动连接时改为 True
HEARTBEAT_INTERVAL_MS = 200
HEARTBEAT_TIMEOUT_S = 1.0


class SerialService(QObject):
    ports_changed = pyqtSignal(list)
    connected = pyqtSignal(object)
    disconnected = pyqtSignal(str)
    frame_received = pyqtSignal(int, bytes)
    log = pyqtSignal(str)
    error = pyqtSignal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.serial_port = None
        self.is_connected = False
        self.port_entries = []
        self._attempted = set()
        self._auto_connect = AUTO_CONNECT
        self._send_queue = Queue(maxsize=100)
        self._stop = threading.Event()
        self._lock = threading.Lock()
        self._reader_thread = None
        self._writer_thread = None
        self._buffer = bytearray()
        self._last_rx = time.monotonic()
        self.head = UartFrame.UART_PACKET_HEAD
        self.tail = UartFrame.UART_PACKET_TAIL

        self.scan_timer = QTimer(self)
        self.scan_timer.timeout.connect(self.scan_ports)
        self.scan_timer.start(2500)
        self.heartbeat_timer = QTimer(self)
        self.heartbeat_timer.timeout.connect(self._on_heartbeat_timer)
        self.heartbeat_timer.setInterval(HEARTBEAT_INTERVAL_MS)
        self.monitor_timer = QTimer(self)
        self.monitor_timer.timeout.connect(self._check_timeout)
        self.monitor_timer.start(500)

        if serial is None:
            self.error.emit("未安装 pyserial，串口功能不可用")
        else:
            QTimer.singleShot(0, self.scan_ports)

    # ---------- 端口扫描 ----------
    def scan_ports(self):
        if serial is None or self.is_connected:
            return
        try:
            ports = list(serial.tools.list_ports.comports())
        except Exception as error:
            self.error.emit(f"扫描串口失败：{error}")
            return

        entries = []
        for port in ports:
            kind = self._identify_device(port)
            if kind == "unknown":
                continue
            entries.append({
                "device": port.device,
                "type": kind,
                "display": f"{port.device} ({kind})",
            })
        self.port_entries = entries
        self.ports_changed.emit(entries)

        if self._auto_connect and not self.is_connected:
            for entry in entries:
                if entry["type"] not in KNOWN_DEVICE_NAMES:
                    continue
                if entry["device"] in self._attempted:
                    continue
                self._attempted.add(entry["device"])
                if self.connect_to(entry):
                    break

    @staticmethod
    def _identify_device(port):
        hwid = (port.hwid or "").upper()
        desc = (port.description or "").upper()
        vid = DEVICE_CONFIG["vendor_id"].upper()
        for pid, info in DEVICE_CONFIG["devices"].items():
            pid = pid.upper()
            patterns = (
                f"={vid}:{pid}",
                f"VID:{vid}:PID:{pid}",
                f"VID_{vid}&PID_{pid}",
                f"{vid}:{pid}",
            )
            if any(pattern in hwid for pattern in patterns):
                return info["name"]
        if any(key in hwid or key in desc for key in ("BTH", "BLUETOOTH", "蓝牙", "RFCOMM")):
            return "bluetooth"
        return "unknown"

    # ---------- 连接 / 断开 ----------
    def connect_to(self, entry):
        if serial is None or not entry or self.is_connected:
            return False
        try:
            self.serial_port = serial.Serial(
                port=entry["device"],
                baudrate=115200,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=0.1,
                write_timeout=0.5,
                xonxoff=False,
                rtscts=False,
                dsrdtr=False,
            )
            self.is_connected = True
            self._stop.clear()
            self._buffer.clear()
            self._reader_thread = threading.Thread(
                target=self._reader_loop, daemon=True, name="SerialReader")
            self._writer_thread = threading.Thread(
                target=self._writer_loop, daemon=True, name="SerialWriter")
            self._reader_thread.start()
            self._writer_thread.start()
            self._auto_connect = False
            self.heartbeat_timer.start()
            self._last_rx = time.monotonic()
            self.log.emit(f"串口已打开：{entry['display']}")
            self.send(CmdId.CMD_CONNECT, b"\x01")
            self.connected.emit(entry)
            return True
        except Exception as error:
            self.is_connected = False
            self.serial_port = None
            self.error.emit(f"连接失败：{error}")
            return False

    def disconnect(self, reason="手动断开"):
        if not self.is_connected and self.serial_port is None:
            return
        if self.is_connected:
            self._write_packet(CmdId.CMD_CONNECT, b"\x00")
        self.is_connected = False
        self.heartbeat_timer.stop()
        self._stop.set()
        port = self.serial_port
        self.serial_port = None
        if port is not None:
            try:
                port.close()
            except Exception:
                pass
        current = threading.current_thread()
        for thread in (self._reader_thread, self._writer_thread):
            if thread and thread is not current and thread.is_alive():
                thread.join(timeout=0.5)
        self.disconnected.emit(reason)

    # ---------- 发送 ----------
    def send(self, cmd_id, payload=b""):
        if not self.is_connected:
            self.error.emit("尚未连接设备")
            return False
        payload = bytes(payload or b"")
        if len(payload) > MAX_PAYLOAD:
            self.error.emit("发送载荷超过 128 字节")
            return False
        try:
            self._send_queue.put_nowait((int(cmd_id), payload))
            return True
        except Full:
            self.error.emit("发送队列已满，数据包被丢弃")
            return False

    def _write_packet(self, cmd_id, payload):
        if not self.serial_port or not self.serial_port.is_open:
            return False
        packet = self._build_packet(cmd_id, payload)
        with self._lock:
            try:
                self.serial_port.write(packet)
                self.serial_port.flush()
                return True
            except Exception:
                return False

    def _writer_loop(self):
        while not self._stop.is_set():
            try:
                cmd_id, payload = self._send_queue.get(timeout=0.1)
            except Exception:
                continue
            if not self._write_packet(cmd_id, payload):
                self._lost("发送失败")

    def _send_heartbeat(self):
        if self.is_connected:
            self.send(CmdId.CMD_HEARTBEAT, b"")

    def _on_heartbeat_timer(self):
        self._send_heartbeat()

    def _check_timeout(self):
        if not self.is_connected:
            return
        if time.monotonic() - self._last_rx > HEARTBEAT_TIMEOUT_S:
            self._lost("心跳超时")

    # ---------- 接收 / 拆帧 ----------
    def _reader_loop(self):
        while not self._stop.is_set() and self.is_connected:
            try:
                data = self.serial_port.read(1)
                if not data:
                    continue
                self._buffer.extend(data)
                self._parse_buffer()
            except Exception:
                self._lost("接收失败")
                break

    def _parse_buffer(self):
        while len(self._buffer) >= 5:
            index = self._buffer.find(self.head)
            if index < 0:
                self._buffer.clear()
                return
            if index:
                del self._buffer[:index]
            if len(self._buffer) < 3:
                return
            length = self._buffer[2]
            total = 5 + length
            if len(self._buffer) < total:
                return
            if self._buffer[total - 1] != self.tail:
                del self._buffer[0]
                continue
            payload = bytes(self._buffer[3:3 + length])
            check = self._buffer[3 + length]
            if check == self._crc8(bytes([self._buffer[1], length]) + payload):
                self._last_rx = time.monotonic()
                self.frame_received.emit(self._buffer[1], payload)
            del self._buffer[:total]

    def _lost(self, reason):
        if not self.is_connected:
            return
        self.is_connected = False
        self.heartbeat_timer.stop()
        self._stop.set()
        if self.serial_port:
            try:
                self.serial_port.close()
            except Exception:
                pass
        self.serial_port = None
        self._auto_connect = AUTO_CONNECT
        self._attempted.clear()
        self.disconnected.emit(reason)

    # ---------- 组帧 / CRC ----------
    def _build_packet(self, cmd_id, payload):
        payload = bytes(payload or b"")
        length = len(payload)
        packet = bytearray([self.head, int(cmd_id), length])
        packet.extend(payload)
        packet.append(self._crc8(bytes([int(cmd_id), length]) + payload))
        packet.append(self.tail)
        return bytes(packet)

    @staticmethod
    def _crc8(data):
        crc = 0
        for byte in data:
            crc ^= byte
            for _ in range(8):
                crc = ((crc << 1) ^ 0x07) if (crc & 0x80) else (crc << 1)
            crc &= 0xFF
        return crc

    def reset_auto_connect(self):
        self._auto_connect = AUTO_CONNECT
        self._attempted.clear()

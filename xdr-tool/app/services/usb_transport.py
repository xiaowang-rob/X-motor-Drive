"""XDr Vendor Bulk 传输与帧编解码；不依赖 Qt，不嵌套 UART 帧。"""
import binascii
import time

from app.services.usb_discovery import USB_DEVICES

try:
    import usb.core as usb_core
    import usb.util as usb_util
except ImportError:
    usb_core = usb_util = None


USB_HEAD = b"\xa5\x5a"
USB_MAX_PAYLOAD = 4095
USB_PACKET_SIZE = 64
USB_RX_TIMEOUT_S = 1.0


def build_usb_packet(cmd_id, payload=b""):
    payload = bytes(payload)
    cmd_id = int(cmd_id)
    if not 0 <= cmd_id <= 0xFF:
        raise ValueError("USB 命令 ID 必须为 0..255")
    if len(payload) > USB_MAX_PAYLOAD:
        raise ValueError(f"USB 数据不能超过 {USB_MAX_PAYLOAD} 字节")
    body = USB_HEAD + bytes([cmd_id]) + len(payload).to_bytes(2, "little") + payload
    return body + binascii.crc_hqx(body, 0xFFFF).to_bytes(2, "little")


class UsbFrameParser:
    """按长度重组帧，CRC/长度错误逐字节重新同步，空闲一秒丢弃半帧。"""
    def __init__(self):
        self._buffer = bytearray()
        self._last_data = None

    def feed(self, data, now=None):
        now = time.monotonic() if now is None else now
        if self._last_data is not None and now - self._last_data >= USB_RX_TIMEOUT_S:
            self._buffer.clear()
        if data:
            self._last_data = now
            self._buffer.extend(data)
        frames = []
        while self._buffer:
            index = self._buffer.find(USB_HEAD)
            if index < 0:
                # 保留可能跨传输边界的第一个同步字节。
                self._buffer[:] = b"\xa5" if self._buffer[-1] == 0xA5 else b""
                break
            if index:
                del self._buffer[:index]
            if len(self._buffer) < 5:
                break
            length = int.from_bytes(self._buffer[3:5], "little")
            if length > USB_MAX_PAYLOAD:
                del self._buffer[0]
                continue
            total = length + 7
            if len(self._buffer) < total:
                break
            expected = int.from_bytes(self._buffer[total - 2:total], "little")
            if binascii.crc_hqx(self._buffer[:total - 2], 0xFFFF) != expected:
                del self._buffer[0]
                continue
            frames.append((self._buffer[2], bytes(self._buffer[5:total - 2])))
            del self._buffer[:total]
        return frames


class UsbTransport:
    """接口 0 / alt 0；打开精确选中的设备，避免连接到同型号的另一台设备。"""
    def __init__(self):
        self._device = None
        self._claimed = False
        self._detached = False

    def open(self, entry):
        if usb_core is None or usb_util is None:
            raise RuntimeError("未安装 PyUSB，请安装 software/requirements.txt 中的依赖")
        if (entry.get("vid"), entry.get("pid")) not in USB_DEVICES:
            raise ValueError("不是支持的 XDr USB 设备")
        if self._device is not None:
            raise RuntimeError("USB 设备已打开")

        def matches(device):
            identity = (f"usb:{device.idVendor:04x}:{device.idProduct:04x}:"
                        f"{device.bus}:{device.address}")
            return identity == entry["device"]

        device = usb_core.find(idVendor=entry["vid"], idProduct=entry["pid"],
                               custom_match=matches)
        if device is None:
            raise RuntimeError("USB 设备已离线，请重新扫描并选择设备")
        self._device = device
        try:
            try:
                config = device.get_active_configuration()
            except usb_core.USBError:
                device.set_configuration(1)
                config = device.get_active_configuration()
            if config.bConfigurationValue != 1:
                device.set_configuration(1)
                config = device.get_active_configuration()
            interface = config[(0, 0)]
            endpoints = {ep.bEndpointAddress: ep for ep in interface}
            if (interface.bInterfaceClass != 0xFF or
                    any(address not in endpoints or
                        (endpoints[address].bmAttributes & 3) != 2 or
                        endpoints[address].wMaxPacketSize != USB_PACKET_SIZE
                        for address in (0x01, 0x81))):
                raise RuntimeError("USB 接口不是预期的 XDr Vendor Bulk 接口")
            try:
                if device.is_kernel_driver_active(0):
                    device.detach_kernel_driver(0)
                    self._detached = True
            except (NotImplementedError, AttributeError):
                pass  # Windows 等后端不支持查询内核驱动。
            usb_util.claim_interface(device, 0)
            self._claimed = True
            device.set_interface_altsetting(interface=0, alternate_setting=0)
        except Exception:
            self.close()
            raise

    def read(self):
        device = self._device
        if device is None:
            raise RuntimeError("USB 设备未打开")
        try:
            # 大小为 packet 的倍数；允许最大帧、短包、ZLP 和多次传输重组。
            return bytes(device.read(0x81, 65 * USB_PACKET_SIZE, timeout=100))
        except usb_core.USBTimeoutError:
            return b""  # 无数据不是断线；业务层负责心跳超时。

    def write(self, packet):
        device = self._device
        if device is None:
            raise RuntimeError("USB 设备未打开")
        if device.write(0x01, packet, timeout=500) != len(packet):
            raise IOError("USB 发送不完整")
        if len(packet) % USB_PACKET_SIZE == 0:
            if device.write(0x01, b"", timeout=500) != 0:
                raise IOError("USB ZLP 发送失败")

    def close(self):
        device, self._device = self._device, None
        if device is None:
            return
        try:
            if self._claimed:
                usb_util.release_interface(device, 0)
        finally:
            self._claimed = False
            try:
                if self._detached:
                    device.attach_kernel_driver(0)
            except Exception:
                pass
            finally:
                self._detached = False
                usb_util.dispose_resources(device)

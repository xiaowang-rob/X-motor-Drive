"""IAP 固件分块上传工作线程。

协议：
  ERASE  [size:u32] -> [status:u8]
  WRITE  [offset:u32][data<=120B] -> [offset:u32][status:u8]
  EXIT   无 payload，设备复位
"""

import time
import zlib
from pathlib import Path
from queue import Empty, Queue

from PyQt5.QtCore import QThread, pyqtSignal

from protocol import CmdId, FbId
from app.services import payload_codec as codec


BLOCK_SIZE = 120
RESPONSE_TIMEOUT = 2.0
RETRY_COUNT = 3


class IapWorker(QThread):
    progress = pyqtSignal(int, str)
    result = pyqtSignal(bool, str)
    log = pyqtSignal(str)

    def __init__(self, serial, file_path, parent=None):
        super().__init__(parent)
        self.serial = serial
        self.file_path = Path(file_path)
        self._responses = Queue()
        self._stop = False

    def push_frame(self, cmd_id, payload):
        self._responses.put((int(cmd_id), bytes(payload)))

    def stop(self):
        self._stop = True

    def _wait_response(self, cmd_id, timeout=RESPONSE_TIMEOUT):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self._stop:
                return None
            try:
                response_id, payload = self._responses.get(timeout=0.05)
            except Empty:
                continue
            if response_id == cmd_id:
                return payload
        return None

    def _send_wait(self, cmd_id, payload):
        for _ in range(RETRY_COUNT):
            if self._stop:
                return None
            while not self._responses.empty():
                try:
                    self._responses.get_nowait()
                except Empty:
                    break
            if not self.serial.send(cmd_id, payload):
                return None
            response = self._wait_response(cmd_id)
            if response is not None:
                return response
        return None

    def run(self):
        try:
            data = self.file_path.read_bytes()
        except OSError as error:
            self.result.emit(False, f"读取固件失败：{error}")
            return
        if not data:
            self.result.emit(False, "固件文件为空")
            return

        size = len(data)
        crc32 = zlib.crc32(data) & 0xFFFFFFFF
        self.log.emit(f"IAP 文件 {self.file_path.name} {size} 字节 CRC32={crc32:08X}")

        self.progress.emit(1, "擦除目标分区...")
        response = self._send_wait(CmdId.CMD_IAP_ERASE_FLASH, codec.pack_iap_erase(size))
        if not response or response[0] != FbId.FB_EXECUTE:
            self.result.emit(False, "擦除失败")
            return

        total = max(1, (size + BLOCK_SIZE - 1) // BLOCK_SIZE)
        for index, offset in enumerate(range(0, size, BLOCK_SIZE), 1):
            if self._stop:
                self.result.emit(False, "已取消")
                return
            chunk = data[offset:offset + BLOCK_SIZE]
            response = self._send_wait(
                CmdId.CMD_IAP_WRITE_FLASH, codec.pack_iap_write(offset, chunk))
            if response is None:
                self.result.emit(False, f"写入超时 offset={offset}")
                return
            try:
                ack_offset, status = codec.unpack_iap_write_ack(response)
            except ValueError as error:
                self.result.emit(False, f"写入应答错误：{error}")
                return
            if status != FbId.FB_EXECUTE:
                self.result.emit(False, f"写入失败 offset={ack_offset}")
                return
            self.progress.emit(
                int(index * 90 / total), f"写入 {offset + len(chunk)}/{size}")

        # TODO(payload): iap_verify_crc 按整个分区校验，需要先知道分区大小并补 0xFF；
        #                当前只记录主机侧 CRC，等设备提供分区大小后再发 CLEAR/VERIFY。
        self.log.emit(f"校验暂缓：设备整区 CRC 需要分区大小，主机 CRC32={crc32:08X}")
        self.progress.emit(95, "写入完成，复位设备...")
        self.serial.send(CmdId.CMD_IAP_EXIT, b"")
        self.result.emit(True, f"固件写入完成：{size} 字节（校验 TODO）")

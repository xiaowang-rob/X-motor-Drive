"""运行真实连接/收发方法；仅替换 Qt 信号、定时器和硬件。"""
import ast
from pathlib import Path
import threading
import time
from queue import Queue, Full
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

from protocol import CmdId, UartFrame
from app.services.usb_transport import UsbFrameParser, USB_MAX_PAYLOAD, build_usb_packet


def load_service():
    path = Path(__file__).parent / 'app/services/serial_service.py'
    tree = ast.parse(path.read_text(encoding='utf-8'))
    cls = next(node for node in tree.body if isinstance(node, ast.ClassDef))
    cls.bases = []
    cls.body = [node for node in cls.body
                if isinstance(node, ast.FunctionDef) and node.name != '__init__']
    namespace = dict(threading=threading, time=time, Queue=Queue, Full=Full,
                     CmdId=CmdId, UsbFrameParser=UsbFrameParser,
                     USB_MAX_PAYLOAD=USB_MAX_PAYLOAD, build_usb_packet=build_usb_packet,
                     UsbTransport=Mock(), serial=None, MAX_PAYLOAD=128,
                     AUTO_CONNECT=False, HEARTBEAT_TIMEOUT_S=1.0)
    exec(compile(ast.fix_missing_locations(ast.Module(body=[cls], type_ignores=[])),
                 str(path), 'exec'), namespace)
    service = namespace['SerialService']()
    for name in ('connected', 'disconnected', 'frame_received', 'error', 'log', '_transport_failed', 'heartbeat_timer'):
        setattr(service, name, Mock())
    service.serial_port = service._usb = None
    service._reader_thread = service._writer_thread = None
    service.is_connected = False
    service._stop = threading.Event()
    service._lock = threading.Lock()
    service._send_queue = Queue(maxsize=100)
    service._buffer = bytearray()
    service._usb_parser = UsbFrameParser()
    service._last_rx = time.monotonic()
    service.head = UartFrame.UART_PACKET_HEAD
    service.tail = UartFrame.UART_PACKET_TAIL
    service._attempted = set()
    return service, namespace


class UsbConnectionTests(unittest.TestCase):
    def setUp(self):
        self.service, self.namespace = load_service()
        self.transport = self.namespace['UsbTransport'].return_value
        self.entry = dict(transport='usb', display='(USB)XDr-S', device='usb:0483:5760:1:2')

    def connect(self):
        # 不启动后台循环，分别以确定性输入测试真实循环方法。
        with patch.object(threading, 'Thread'):
            self.assertTrue(self.service.connect_to(self.entry))

    def test_usb_connect_without_pyserial_sends_connect_and_cleans_on_disconnect(self):
        self.service._send_queue.put((99, b'old'))
        self.connect()
        self.transport.open.assert_called_once_with(self.entry)
        self.service.connected.emit.assert_called_once_with(self.entry)
        self.assertEqual(self.service._send_queue.get_nowait(), (int(CmdId.CMD_CONNECT), b'\x01'))
        self.assertTrue(self.service._send_queue.empty())
        self.service.disconnect()
        self.transport.write.assert_called_once_with(build_usb_packet(CmdId.CMD_CONNECT, b'\x00'))
        self.transport.close.assert_called_once()
        self.assertIsNone(self.service._usb)
        self.assertFalse(self.service.is_connected)
        self.service.heartbeat_timer.stop.assert_called_once()

    def test_open_failure_cleans_up(self):
        self.transport.open.side_effect = RuntimeError('permission')
        self.assertFalse(self.service.connect_to(self.entry))
        self.transport.close.assert_called_once()
        self.assertIsNone(self.service._usb)
        self.service.connected.emit.assert_not_called()
        self.assertIn('permission', self.service.error.emit.call_args.args[0])

    def test_usb_reader_emits_frames_not_uart(self):
        self.service._usb = self.transport
        self.service.is_connected = True
        wire = build_usb_packet(12, b'data') + build_usb_packet(13)
        def read():
            self.service._stop.set()
            return wire
        self.transport.read.side_effect = read
        self.service._reader_loop()
        self.assertEqual([call.args for call in self.service.frame_received.emit.call_args_list],
                         [(12, b'data'), (13, b'')])
        self.service._transport_failed.emit.assert_not_called()

    def test_io_error_signals_main_thread_and_lost_releases(self):
        self.service._usb = self.transport
        self.service.is_connected = True
        self.transport.read.side_effect = IOError('unplugged')
        self.service._reader_loop()
        self.assertIn('unplugged', self.service._transport_failed.emit.call_args.args[0])
        self.service._lost('unplugged')
        self.transport.close.assert_called_once()
        self.service.disconnected.emit.assert_called_once_with('unplugged')

    def test_writer_builds_usb_frame_and_payload_limit_is_transport_specific(self):
        self.service._usb = self.transport
        self.service.is_connected = True
        self.assertTrue(self.service.send(3, bytes(4095)))
        self.assertFalse(self.service.send(3, bytes(4096)))
        def write(packet):
            self.assertEqual(packet, build_usb_packet(3, bytes(4095)))
            self.service._stop.set()
        self.transport.write.side_effect = write
        self.service._writer_loop()
        self.service._usb = None
        self.assertFalse(self.service.send(3, bytes(129)))

    def test_serial_connection_and_wire_format_remain_unchanged(self):
        serial = SimpleNamespace(Serial=Mock(), EIGHTBITS=8, PARITY_NONE='N', STOPBITS_ONE=1)
        self.namespace['serial'] = serial
        with patch.object(threading, 'Thread'):
            self.assertTrue(self.service.connect_to(dict(device='COM3', display='COM3 (bluetooth)')))
        self.assertIsNone(self.service._usb)
        self.assertTrue(self.service._write_packet(2, b'abc'))
        packet = serial.Serial.return_value.write.call_args.args[0]
        self.assertEqual(packet[:6], bytes([self.service.head, 2, 3]) + b'abc')
        self.assertEqual(packet[-1], self.service.tail)
        self.assertEqual(packet[-2], self.service._crc8(b'\x02\x03abc'))
        self.service._buffer.extend(packet)
        self.service._parse_buffer()
        self.service.frame_received.emit.assert_called_once_with(2, b'abc')
        self.service.disconnect()
        serial.Serial.return_value.close.assert_called_once()


if __name__ == '__main__':
    unittest.main()

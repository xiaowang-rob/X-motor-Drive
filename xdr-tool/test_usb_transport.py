"""USB 线协议与 PyUSB 资源管理回归，无需 Qt 或实际 USB 设备。"""
import unittest
from types import SimpleNamespace
from unittest.mock import Mock, patch

from app.services import usb_transport as usb
from app.services.usb_discovery import USB_DEVICES


class UsbFrameTests(unittest.TestCase):
    def test_crc_vector_and_wire_layout(self):
        import binascii
        self.assertEqual(binascii.crc_hqx(b'123456789', 0xFFFF), 0x29B1)
        packet = usb.build_usb_packet(0x21, b'abc')
        self.assertEqual(packet[:8], b'\xa5\x5a\x21\x03\x00abc')
        crc = 0xFFFF
        for byte in packet[:-2]:
            crc ^= byte << 8
            for _ in range(8):
                crc = ((crc << 1) ^ (0x1021 if crc & 0x8000 else 0)) & 0xFFFF
        self.assertEqual(packet[-2:], crc.to_bytes(2, 'little'))

    def test_fragmentation_zero_and_maximum(self):
        frames = [(1, b''), (2, bytes(range(256)) * 15 + bytes(255))]
        wire = b''.join(usb.build_usb_packet(*frame) for frame in frames)
        for size in (1, 7, 64, 512, len(wire)):
            parser = usb.UsbFrameParser()
            actual = []
            for offset in range(0, len(wire), size):
                actual.extend(parser.feed(wire[offset:offset + size]))
            self.assertEqual(actual, frames)

    def test_invalid_length_crc_and_split_header_recover(self):
        parser = usb.UsbFrameParser()
        packet = usb.build_usb_packet(7, b'ok')
        broken = bytearray(usb.build_usb_packet(8, b'bad'))
        broken[-1] ^= 1
        self.assertEqual(parser.feed(b'noise\xa5'), [])
        self.assertEqual(parser.feed(b'\x5a\x01\xff\xff' + broken + packet), [(7, b'ok')])

    def test_timeout_discards_partial_frame(self):
        parser = usb.UsbFrameParser()
        packet = usb.build_usb_packet(7, b'hello')
        self.assertEqual(parser.feed(packet[:6], now=0), [])
        self.assertEqual(parser.feed(b'', now=1.1), [])
        self.assertEqual(parser.feed(packet, now=1.2), [(7, b'hello')])

    def test_invalid_send_values(self):
        for cmd, payload in ((-1, b''), (256, b''), (1, bytes(4096))):
            with self.assertRaises(ValueError):
                usb.build_usb_packet(cmd, payload)


class UsbTransportTests(unittest.TestCase):
    def setUp(self):
        class UsbError(Exception):
            pass
        class UsbTimeoutError(UsbError):
            pass
        self.core = SimpleNamespace(USBError=UsbError, USBTimeoutError=UsbTimeoutError, find=Mock())
        self.util = Mock()
        self.patches = [patch.object(usb, 'usb_core', self.core), patch.object(usb, 'usb_util', self.util)]
        for item in self.patches:
            item.start()
            self.addCleanup(item.stop)
        vid, pid = next(iter(USB_DEVICES))
        self.device = Mock(idVendor=vid, idProduct=pid, bus=2, address=3)
        interface = [SimpleNamespace(bEndpointAddress=address, bmAttributes=2, wMaxPacketSize=64)
                     for address in (0x01, 0x81)]
        class Interface(list):
            bInterfaceClass = 0xFF
        # 使用真实 dict 子类，保持 PyUSB Configuration 的索引接口。
        class Configuration(dict):
            bConfigurationValue = 1
        self.config = Configuration({(0, 0): Interface(interface)})
        self.device.get_active_configuration.return_value = self.config
        self.device.is_kernel_driver_active.return_value = True
        self.core.find.return_value = self.device
        self.entry = {'vid': vid, 'pid': pid, 'device': f'usb:{vid:04x}:{pid:04x}:2:3'}
        self.transport = usb.UsbTransport()
        self.addCleanup(self.transport.close)

    def test_open_selects_location_claims_and_closes(self):
        self.transport.open(self.entry)
        matcher = self.core.find.call_args.kwargs['custom_match']
        self.assertTrue(matcher(self.device))
        other = SimpleNamespace(idVendor=self.entry['vid'], idProduct=self.entry['pid'], bus=2, address=4)
        self.assertFalse(matcher(other))
        self.device.set_configuration.assert_not_called()
        self.util.claim_interface.assert_called_once_with(self.device, 0)
        self.transport.close()
        self.transport.close()
        self.util.release_interface.assert_called_once_with(self.device, 0)
        self.device.attach_kernel_driver.assert_called_once_with(0)
        self.util.dispose_resources.assert_called_once_with(self.device)

    def test_read_timeout_zlp_and_real_error(self):
        self.transport.open(self.entry)
        self.device.read.side_effect = [self.core.USBTimeoutError(), [], [1, 2], self.core.USBError('unplug')]
        self.assertEqual(self.transport.read(), b'')
        self.assertEqual(self.transport.read(), b'')
        self.assertEqual(self.transport.read(), b'\x01\x02')
        with self.assertRaises(self.core.USBError):
            self.transport.read()
        self.device.read.assert_called_with(0x81, 4160, timeout=100)

    def test_write_adds_zlp_only_for_multiple_of_packet_size(self):
        self.transport.open(self.entry)
        self.device.write.side_effect = lambda address, data, timeout: len(data)
        self.transport.write(usb.build_usb_packet(1, bytes(57)))
        calls = self.device.write.call_args_list
        self.assertEqual([len(call.args[1]) for call in calls], [64, 0])
        self.device.write.reset_mock()
        self.transport.write(usb.build_usb_packet(1, b''))
        self.assertEqual(self.device.write.call_count, 1)
        self.device.write.side_effect = None
        self.device.write.return_value = 1
        with self.assertRaisesRegex(IOError, '不完整'):
            self.transport.write(usb.build_usb_packet(1))

    def test_claim_failure_cleans_up_and_reattaches(self):
        self.util.claim_interface.side_effect = self.core.USBError('permission')
        with self.assertRaisesRegex(self.core.USBError, 'permission'):
            self.transport.open(self.entry)
        self.assertIsNone(self.transport._device)
        self.util.release_interface.assert_not_called()
        self.device.attach_kernel_driver.assert_called_once_with(0)
        self.util.dispose_resources.assert_called_once_with(self.device)

    def test_missing_device_or_dependency(self):
        self.core.find.return_value = None
        with self.assertRaisesRegex(RuntimeError, '离线'):
            self.transport.open(self.entry)
        with patch.object(usb, 'usb_core', None):
            with self.assertRaisesRegex(RuntimeError, 'PyUSB'):
                self.transport.open(self.entry)

    def test_configures_unconfigured_device_and_validates_endpoints(self):
        self.device.get_active_configuration.side_effect = [self.core.USBError(), self.config]
        self.transport.open(self.entry)
        self.device.set_configuration.assert_called_once_with(1)
        self.transport.close()
        self.device.get_active_configuration.side_effect = None
        self.config[(0, 0)][0].bmAttributes = 3
        with self.assertRaisesRegex(RuntimeError, 'Vendor Bulk'):
            self.transport.open(self.entry)
        self.assertIsNone(self.transport._device)


if __name__ == '__main__':
    unittest.main()

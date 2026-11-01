"""无需 Qt 或 USB 硬件的设备枚举测试。"""
import json
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from app.services import usb_discovery as discovery


class UsbDiscoveryTests(unittest.TestCase):
    def test_ids_match_json(self):
        config = json.loads((Path(__file__).resolve().parents[1] /
                             'protocol/devices.json').read_text())
        expected = {(int(item['vid'], 16), int(item['pid'], 16))
                    for board in config['devices'].values()
                    for item in board.values()}
        self.assertEqual(set(discovery.USB_DEVICES), expected)

    def test_enumerates_known_devices_and_distinguishes_locations(self):
        attached = [SimpleNamespace(idVendor=vid, idProduct=pid, bus=1, address=i)
                    for i, (vid, pid) in enumerate(discovery.USB_DEVICES, 1)]
        attached += [SimpleNamespace(idVendor=0x0483, idProduct=0x5760, bus=2, address=1),
                     SimpleNamespace(idVendor=0x0483, idProduct=0x5742, bus=1, address=9)]
        def find(*, find_all, custom_match):
            self.assertTrue(find_all)
            return [dev for dev in attached if custom_match(dev)]
        with patch.object(discovery, 'usb_core', SimpleNamespace(find=find)):
            entries = discovery.scan_usb_devices()
        self.assertEqual(len(entries), 5)
        self.assertEqual(len({entry['device'] for entry in entries}), 5)
        self.assertEqual({entry['type'] for entry in entries},
                         {'XDr-S APP', 'XDr-S BL', 'XDr-P APP', 'XDr-P BL'})
        self.assertTrue(all(entry['transport'] == 'usb' for entry in entries))
        self.assertEqual({entry['display'] for entry in entries},
                         {'(USB)XDr-S', '(USB)XDr-S BL', '(USB)XDr-P', '(USB)XDr-P BL'})
        self.assertTrue(all(entry['display'] == '(USB)' + entry['type'].removesuffix(' APP')
                            for entry in entries))

    def test_no_devices(self):
        with patch.object(discovery, 'usb_core', SimpleNamespace(find=lambda **kw: [])):
            self.assertEqual(discovery.scan_usb_devices(), [])

    def test_missing_pyusb(self):
        with patch.object(discovery, 'usb_core', None):
            with self.assertRaisesRegex(RuntimeError, 'PyUSB'):
                discovery.scan_usb_devices()

    def test_backend_error_is_reported(self):
        def find(**kwargs):
            raise RuntimeError('No backend available')
        with patch.object(discovery, 'usb_core', SimpleNamespace(find=find)):
            with self.assertRaisesRegex(RuntimeError, 'No backend'):
                discovery.scan_usb_devices()

"""直接测试扫描方法，不依赖 Qt、pyserial 或 USB 硬件。"""
import ast
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import Mock

from app.services.usb_discovery import USB_DEVICES


def load_scan_service():
    source = Path(__file__).parent / 'app/services/serial_service.py'
    tree = ast.parse(source.read_text(encoding='utf-8'))
    service = next(node for node in tree.body
                   if isinstance(node, ast.ClassDef) and node.name == 'SerialService')
    methods = [node for node in service.body
               if isinstance(node, ast.FunctionDef)
               and node.name in ('scan_ports', '_identify_device')]
    namespace = {
        'USB_DEVICES': USB_DEVICES,
        'scan_usb_devices': lambda: [],
        'serial': SimpleNamespace(tools=SimpleNamespace(list_ports=SimpleNamespace(comports=lambda: []))),
    }
    module = ast.Module(body=[ast.ClassDef(name='ScanService', bases=[], keywords=[], body=methods,
                                         decorator_list=[])], type_ignores=[])
    exec(compile(ast.fix_missing_locations(module), str(source), 'exec'), namespace)
    return namespace['ScanService'], namespace


class SerialDiscoveryTests(unittest.TestCase):
    def test_bluetooth_survives_unknown_device_filter(self):
        cls, namespace = load_scan_service()
        ports = [SimpleNamespace(device=f'COM{i}', hwid=hwid, description=desc)
                 for i, (hwid, desc) in enumerate([
                     ('BTHENUM\\DEV', ''), ('', 'Bluetooth Serial Port'),
                     ('', '蓝牙串口'), ('', 'RFCOMM'), ('', 'Generic Serial'),
                 ], 1)]
        namespace['serial'].tools.list_ports.comports = lambda: ports
        service = cls()
        service.is_connected = False
        service._auto_connect = False
        service.error = Mock()
        service.ports_changed = Mock()
        service.scan_ports()
        self.assertEqual([entry['device'] for entry in service.port_entries],
                         ['COM1', 'COM2', 'COM3', 'COM4'])
        self.assertTrue(all(entry['type'] == 'bluetooth' for entry in service.port_entries))
        service.ports_changed.emit.assert_called_once_with(service.port_entries)
        service.error.emit.assert_not_called()


if __name__ == '__main__':
    unittest.main()

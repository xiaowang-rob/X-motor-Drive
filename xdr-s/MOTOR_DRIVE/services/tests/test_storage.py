"""在主机上编译真实 storage/parameters 实现，模拟 NOR 和掉电。"""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]


class StorageTests(unittest.TestCase):
    def test_nor_recovery_and_parameters(self):
        with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR')) as tmp:
            binary = Path(tmp) / 'test_storage'
            subprocess.run([
                'gcc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                '-I' + str(ROOT / 'firmware/services'),
                '-I' + str(ROOT / 'firmware/services/abs'),
                '-I' + str(ROOT / 'firmware/services/utils'),
                '-I' + str(ROOT / 'firmware/protocol'),
                str(Path(__file__).with_name('test_storage.c')),
                str(ROOT / 'firmware/services/abs/storage.c'),
                str(ROOT / 'firmware/services/parameters.c'),
                str(ROOT / 'firmware/services/utils/crc.c'),
                '-o', str(binary),
            ], check=True, capture_output=True, text=True)
            result = subprocess.run([str(binary)], check=True, capture_output=True, text=True)
            self.assertIn('storage tests passed', result.stdout)


if __name__ == '__main__':
    unittest.main()

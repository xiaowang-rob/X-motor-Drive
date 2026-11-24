"""在主机编译真实 usb_com，验证命令与参数解包及队列行为。"""
import os
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[3]


class UsbComTests(unittest.TestCase):
    def test_frame_unpacking(self):
        output = Path(os.environ['TMPDIR']) / 'usb-com-tests'
        output.mkdir(parents=True, exist_ok=True)
        binary = output / 'test_usb_com'
        subprocess.run([
            'gcc', '-std=c11', '-Wall', '-Wextra', '-Werror',
            '-fsanitize=address,undefined', '-g',
            '-I' + str(ROOT / 'firmware/services/abs'),
            '-I' + str(ROOT / 'firmware/protocol'),
            str(Path(__file__).with_suffix('.c')),
            str(ROOT / 'firmware/services/abs/usb_com.c'),
            '-o', str(binary),
        ], check=True)
        result = subprocess.run([str(binary)], check=True, capture_output=True, text=True)
        self.assertIn('usb_com tests passed', result.stdout)


if __name__ == '__main__':
    unittest.main()

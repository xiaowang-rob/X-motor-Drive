"""构建脚本回归测试；不连接调试器、不修改固件源码。"""
import contextlib
import io
from pathlib import Path
import sys
import unittest
from unittest.mock import Mock, patch

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
import build
import check_flash


class BuildTests(unittest.TestCase):
    def test_config_loaded_from_firmware(self):
        with patch.object(Path, 'exists', return_value=True), patch.object(Path, 'read_text', autospec=True, return_value='{"fw_type": "bl", "build_type": "Release"}') as read:
            config = build._load_project_config()
            read.assert_called_once_with(build.PROJECT_ROOT / 'project.json')
        self.assertEqual(config['fw_type'], 'bl')
        with patch.object(build, '_project_cfg', config), patch.object(sys, 'argv', ['build.py', 'build']):
            self.assertEqual(build._resolve_cli(), ('xdr_s', 'bl', None))
            self.assertEqual(build._elf_path('xdr_s', 'bl'),
                             build.PROJECT_ROOT / 'build/xdr_s-bl/Release/board/XDr-S-BL.elf')

    def test_debug_files_follow_target(self):
        elf = build.PROJECT_ROOT / 'build/xdr_s-bl/Debug/board/XDr-S-BL.elf'
        with patch.object(build, '_load_board_config', return_value={'debugger_type': 'stlink', 'debugger': {'stlink': {'interface': 'interface/stlink.cfg', 'target': 'target/stm32g4x.cfg'}}}), patch.object(Path, 'mkdir'), patch.object(Path, 'exists', return_value=True), patch.object(build.shutil, 'copy2') as copy, patch.object(Path, 'write_text', autospec=True) as write:
            build._update_debug_files('xdr_s', 'bl', elf, 'stlink')
            copy.assert_called_once_with(elf, build.BUILD_ROOT / 'current.elf')
            self.assertEqual(write.call_args.args[0], build.BUILD_ROOT / 'debug_openocd.cfg')
            text = write.call_args.args[1]
            self.assertIn('interface/stlink.cfg', text)
            self.assertIn('target/stm32g4x.cfg', text)
            self.assertIn(str(build.PROJECT_ROOT / 'openocd_reset.cfg'), text)

    def test_check_flash_reads_firmware_config(self):
        with patch.object(Path, 'exists', return_value=True), patch.object(Path, 'read_text', autospec=True, return_value='{"board":"xdr_s","fw_type":"bl"}') as read:
            self.assertEqual(check_flash._default_target(), ('xdr_s', 'bl'))
            read.assert_called_once_with(build.PROJECT_ROOT / 'project.json')

    def test_paths_match_presets(self):
        for mode in ('Debug', 'Release'):
            with patch.object(build, '_project_cfg', {'build_type': mode}):
                directory = build.PROJECT_ROOT / 'build/xdr_s-app' / mode
                self.assertEqual(build._build_dir('xdr_s', 'app'), directory)
                self.assertEqual(build._elf_path('xdr_s', 'app'), directory / 'board/XDr-S.elf')
        self.assertEqual(build.FIRMWARE_OUT, build.PROJECT_ROOT / 'releases')

    def test_invalid_build_type(self):
        with patch.object(build, '_project_cfg', {'build_type': '../../firmware'}):
            with self.assertRaises(SystemExit):
                build._build_dir('xdr_s', 'app')

    def test_cli_overrides(self):
        with patch.object(sys, 'argv', ['build.py', 'build', '--board=xdr_s', '--type=app', '--debugger=jlink']):
            self.assertEqual(build._resolve_cli(), ('xdr_s', 'app', 'jlink'))

    def test_configure_uses_new_root(self):
        with patch.object(build.subprocess, 'run') as run, contextlib.redirect_stdout(io.StringIO()):
            run.return_value.stdout = 'expected configure failure'
            run.return_value.returncode = 7
            with self.assertRaises(SystemExit) as failure:
                build.cmd_build('xdr_s', 'app')
            self.assertEqual(failure.exception.code, 7)
            args = run.call_args.args[0]
            self.assertIn(f'-S{build.PROJECT_ROOT}', args)
            self.assertIn('-DBOARD=xdr_s', args)
            self.assertIn('-DFW_TYPE=app', args)

    def test_build_failure_does_not_flash(self):
        with patch.object(sys, 'argv', ['build.py', 'bf']), patch.object(build, 'cmd_build', side_effect=SystemExit(2)), patch.object(build.flash_core, 'cmd_flash') as flash:
            with self.assertRaises(SystemExit):
                build.main()
            flash.assert_not_called()

    def test_flash_uses_new_elf(self):
        with patch.object(sys, 'argv', ['build.py', 'flash', '--board=xdr_s', '--type=app']), patch.object(build.flash_core, 'cmd_flash') as flash:
            build.main()
            self.assertEqual(flash.call_args.args[1], build._elf_path('xdr_s', 'app'))

    def test_success_exports_and_debug_link(self):
        def run_result(args, **kwargs):
            if args[0] == 'arm-none-eabi-size':
                return Mock(returncode=0, stdout='text data bss dec hex filename\n67352 360 33128 100840 189e8 test.elf\n')
            return Mock(returncode=0, stdout='', stderr=b'')

        proc = Mock(stdout=iter([]), returncode=0)
        with patch.object(build.subprocess, 'run', side_effect=run_result) as run, patch.object(build, '_update_debug_files'), patch.object(build.subprocess, 'Popen', return_value=proc), patch.object(Path, 'exists', return_value=True), patch.object(Path, 'stat', return_value=Mock(st_size=1024)), patch.object(Path, 'mkdir'), patch.object(Path, 'unlink'), patch.object(Path, 'symlink_to') as symlink, contextlib.redirect_stdout(io.StringIO()) as output:
            build.cmd_build('xdr_s', 'app')
            self.assertIn('66.1KB / 80KB', output.getvalue())
            self.assertNotIn('98.5KB / 80KB', output.getvalue())
            symlink.assert_called_once_with(Path('board/XDr-S.elf'))
            exports = [call.args[0] for call in run.call_args_list if call.args[0][0] == 'arm-none-eabi-objcopy']
            self.assertEqual(len(exports), 2)
            for command in exports:
                self.assertEqual(Path(command[-2]), build._elf_path('xdr_s', 'app'))
                self.assertEqual(Path(command[-1]).parent, build.FIRMWARE_OUT)

    def test_missing_board_stops_before_cmake(self):
        with patch.object(build.subprocess, 'run') as run, contextlib.redirect_stdout(io.StringIO()):
            with self.assertRaises(SystemExit):
                build.cmd_build('xdr_p', 'app')
            run.assert_not_called()

    def test_check_flash_skip(self):
        with patch.object(sys, 'argv', ['check_flash.py', 'skip']), patch.object(check_flash.subprocess, 'run') as run, contextlib.redirect_stdout(io.StringIO()):
            check_flash.main()
            run.assert_not_called()


if __name__ == '__main__':
    unittest.main()

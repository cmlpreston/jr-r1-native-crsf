"""Exercise upload refusal paths without contacting or writing any hardware."""
import contextlib
import io
import os
from pathlib import Path
import runpy
import subprocess
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]


class Environment:
    def __init__(self, target, board):
        self.values = {'$PROJECT_DIR': str(ROOT), '$PIOENV': target, '$BOARD': board}

    def subst(self, key):
        return self.values[key]

    def Append(self, **kwargs):
        pass

    def Replace(self, **kwargs):
        self.upload_command=kwargs.get("UPLOADCMD")


class UploadPreflightTest(unittest.TestCase):
    def check_guard(self, target, board='seeed_xiao_esp32s3', stale=False, fetch_failed=False):
        calls = []
        def git_run(command, **kwargs):
            args = command[1:]
            calls.append(args)
            result = 0
            output = ''
            if args[0] == 'branch': output = 'main'
            if args[0] == 'rev-parse': output = '12345678'
            if args[0] == 'merge-base' and stale: result = 1
            if args[0] == 'fetch' and fetch_failed: result = 1
            return subprocess.CompletedProcess(command, result, output, 'test failure' if result else '')
        def exit_guard(code):
            raise SystemExit(code)
        output = io.StringIO()
        with patch.dict(os.environ, {'JR_R1_ALLOW_UNSAFE_UPLOAD': '0', 'JR_R1_OTA_URL': 'http://127.0.0.1:8785'}), patch('subprocess.run', git_run), contextlib.redirect_stdout(output):
            try:
                runpy.run_path(str(ROOT / 'tools/upload_preflight.py'), init_globals={
                    'Import': lambda name: None, 'env': Environment(target, board),
                    'COMMAND_LINE_TARGETS': ['upload'], 'Exit': exit_guard})
                passed = True
            except SystemExit:
                passed = False
        return passed, calls

    def test_active_targets_keep_git_checks(self):
        for target in ('seeed_xiao_esp32s3-pcb',):
            passed, calls = self.check_guard(target)
            self.assertTrue(passed)
            self.assertIn(['fetch', '--quiet', 'origin', 'main'], calls)
            self.assertIn(['merge-base', '--is-ancestor', 'origin/main', 'HEAD'], calls)

    def test_wifi_transport_still_refuses_stale_branch(self):
        self.assertFalse(self.check_guard('seeed_xiao_esp32s3-pcb', stale=True)[0])

    def test_legacy_hardware_refused(self):
        for target in ('esp32-s3-devkitc-1', 'esp32-s3-devkitc-1-portrait', 'esp32-s3-devkitc-1-btt', 'esp32-s3-devkitc-1-display-test'):
            self.assertFalse(self.check_guard(target, 'esp32-s3-devkitc-1-n16r8')[0])

    def test_portrait_uploads_refused(self):
        for target in ('seeed_xiao_esp32s3-pcb-portrait', 'seeed_xiao_esp32s3-pcb-lcd-test-portrait'):
            self.assertFalse(self.check_guard(target)[0])

    def test_wrong_board_refused(self):
        self.assertFalse(self.check_guard('seeed_xiao_esp32s3-pcb', 'esp32-s3-devkitc-1-n16r8')[0])

    def test_stale_branch_and_failed_fetch_refused(self):
        self.assertFalse(self.check_guard('seeed_xiao_esp32s3-pcb', stale=True)[0])
        self.assertFalse(self.check_guard('seeed_xiao_esp32s3-pcb', fetch_failed=True)[0])


if __name__ == '__main__':
    unittest.main()

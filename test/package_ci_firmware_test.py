#!/usr/bin/env python3
"""Exercise the hosted artifact format against the existing upload verifier."""
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import artifact_preflight
import package_ci_firmware as package


class CiPackageTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='r1-ci-package-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for name in ('platformio.ini', 'include/display_profile.h',
                     'tools/upload_preflight.py', 'tools/artifact_preflight.py',
                     'tools/flash_via_pi.py'):
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, target)
        self.git('init', '-b', 'codex/ci-test')
        self.git('add', '.')
        self.git('-c', 'user.name=CI test', '-c', 'user.email=ci@example.invalid',
                 'commit', '-m', 'Fixture')
        self.commit = self.git('rev-parse', 'HEAD').decode().strip()
        self.branch = self.git('branch', '--show-current').decode().strip()
        self.environment = 'seeed_xiao_esp32s3-pcb'
        build = self.root / '.pio/build' / self.environment
        build.mkdir(parents=True)
        for name in ('firmware.bin', 'bootloader.bin', 'partitions.bin'):
            (build / name).write_bytes(name.encode())
        boot = self.root / '.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin'
        boot.parent.mkdir(parents=True)
        boot.write_bytes(b'boot fixture')

    def git(self, *args):
        return subprocess.check_output(['git', *args], cwd=self.root, stderr=subprocess.DEVNULL)

    def test_package_verifies_and_detects_tampering(self):
        with patch.object(package, 'ROOT', self.root), patch.object(Path, 'home', return_value=self.root):
            package.main(self.commit, self.branch)
            output = self.root / 'ci-output'
            with tarfile.open(output / 'r1-upload.tar.gz') as archive:
                names = set(archive.getnames())
                self.assertIn('r1-upload-manifest.json', names)
                self.assertNotIn('source.tar.gz', names)
                self.assertFalse(any('nvs' in name or 'littlefs' in name for name in names))
            # Verify the standalone manifest alongside the original build inputs.
            shutil.copyfile(output / 'r1-upload-manifest.json', self.root / 'r1-upload-manifest.json')
            manifest = artifact_preflight.verify(self.root, self.environment,
                'seeed_xiao_esp32s3', ['nobuild', 'upload'], 'r1-upload-manifest.json')
            self.assertEqual(manifest['commit'], self.commit)
            self.assertFalse(manifest['ui_audit_passed'])
            with tarfile.open(output / 'source.tar.gz') as snapshot:
                self.assertNotIn('.pio', snapshot.getnames())
                self.assertNotIn('ci-output', snapshot.getnames())
            (self.root / '.pio/build' / self.environment / 'firmware.bin').write_bytes(b'tampered')
            with self.assertRaisesRegex(ValueError, 'checksum mismatch'):
                artifact_preflight.verify(self.root, self.environment,
                    'seeed_xiao_esp32s3', ['nobuild', 'upload'], 'r1-upload-manifest.json')

    def test_rejects_uncommitted_source(self):
        with (self.root / 'platformio.ini').open('a') as source:
            source.write('\n; changed source\n')
        with patch.object(package, 'ROOT', self.root):
            with self.assertRaisesRegex(ValueError, 'Commit source'):
                package.main(self.commit, self.branch)

    def test_rejects_branch_moved_after_event(self):
        self.git('-c', 'user.name=CI test', '-c', 'user.email=ci@example.invalid',
                 'commit', '--allow-empty', '-m', 'Later push')
        with patch.object(package, 'ROOT', self.root):
            with self.assertRaisesRegex(ValueError, 'differs from event commit'):
                package.main(self.commit, self.branch)
        self.assertFalse((self.root / 'ci-output').exists())

    def test_rejects_wrong_branch_identity(self):
        with patch.object(package, 'ROOT', self.root):
            with self.assertRaisesRegex(ValueError, 'differs from event branch'):
                package.main(self.commit, 'codex/other-branch')
        self.assertFalse((self.root / 'ci-output').exists())


if __name__ == '__main__':
    unittest.main()

"""Checks for the copy/upload workflow; never opens SSH or writes firmware."""
import argparse
import importlib.util
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('flash_via_pi',ROOT/'tools/flash_via_pi.py')
flash=importlib.util.module_from_spec(spec);spec.loader.exec_module(flash)

class FlashWorkflowTests(unittest.TestCase):
    def setUp(self):
        self.directory=tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root=Path(self.directory.name)
        (self.root/'include').mkdir()
        for name in ('platformio.ini','include/display_profile.h'):
            shutil.copyfile(ROOT/name,self.root/name)

    def test_installed_profiles_only(self):
        for environment in flash.ENVIRONMENTS:
            flash.validate_hardware(self.root,environment)
        with self.assertRaises(ValueError):
            flash.validate_hardware(self.root,'esp32-s3-devkitc-1')

    def test_portrait_uploads_refused(self):
        for target in ('seeed_xiao_esp32s3-pcb-portrait','seeed_xiao_esp32s3-pcb-lcd-test-portrait'):
            with self.assertRaises(ValueError):flash.validate_hardware(self.root,target)

    def test_wrong_board_and_pin_rejected(self):
        p=self.root/'platformio.ini';original=p.read_text()
        p.write_text(original.replace('board = seeed_xiao_esp32s3','board = esp32dev'))
        with self.assertRaises(ValueError):flash.validate_hardware(self.root,flash.ENVIRONMENTS[0])
        p.write_text(original)
        p=self.root/'include/display_profile.h'
        p.write_text(p.read_text().replace('TFT_MOSI_PIN = 6','TFT_MOSI_PIN = 11'))
        with self.assertRaises(ValueError):flash.validate_hardware(self.root,flash.ENVIRONMENTS[0])

    def test_selected_diagnostic_override_rejected(self):
        p=self.root/'platformio.ini';s=p.read_text()
        s=s.replace('-D JR_R1_XIAO_PCB=1','-D JR_R1_XIAO_PCB=1\n    -D JR_R1_INPUT_BENCH=1')
        p.write_text(s)
        with self.assertRaises(ValueError):flash.validate_hardware(self.root,flash.ENVIRONMENTS[0])

    def test_manifest_detects_bad_transfer(self):
        p=self.root/'firmware.bin';p.write_bytes(b'tested')
        manifest={'sha256':{'firmware.bin':flash.digest(p)}}
        flash.verify_manifest(self.root,manifest)
        p.write_bytes(b'changed')
        with self.assertRaises(ValueError):flash.verify_manifest(self.root,manifest)

    def test_remote_script_preserves_checks_and_quotes_paths(self):
        args=argparse.Namespace(remote_dir='/home/pi/my firmware',remote_pio='/home/pi/venv/bin/pio',environment=flash.ENVIRONMENTS[0],usb_port=flash.USB_PORT)
        script=flash.remote_script(args)
        self.assertIn('git merge-base --is-ancestor origin/main HEAD',script)
        self.assertIn('unset JR_R1_ALLOW_UNSAFE_UPLOAD',script)
        self.assertIn('-t nobuild -t upload --upload-port',script)
        self.assertIn('if fuser ',script)
        self.assertIn('-t envdump > /dev/null',script)
        self.assertLess(script.index('-t envdump'),script.rindex('tar -xzf'))
        self.assertLess(script.rindex('tar -xzf'),script.index("python3 - <<'VERIFY'"))
        self.assertNotIn('kill ',script)
        self.assertIn("cd '/home/pi/my firmware'",script)
        subprocess.run(['bash','-n'],input=script,text=True,check=True)

if __name__=='__main__':unittest.main()

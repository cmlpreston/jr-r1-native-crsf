"""Checks for the landscape artifact upload guard."""
import importlib.util
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import patch
import json

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

    def test_nobuild_includes_boot_selection(self):
        spec=importlib.util.spec_from_file_location('artifact_preflight',ROOT/'tools/artifact_preflight.py')
        artifact=importlib.util.module_from_spec(spec);spec.loader.exec_module(artifact)
        with patch.object(artifact.Path,'home',return_value=self.root):
            images=artifact.upload_images(self.root,flash.ENVIRONMENTS[0])
        self.assertEqual([offset for offset,_ in images],['0x0000','0x8000','0xe000'])
        self.assertTrue(images[0][1].endswith('/bootloader.bin'))
        self.assertTrue(images[1][1].endswith('/partitions.bin'))
        self.assertTrue(images[2][1].endswith('/boot_app0.bin'))
        self.assertNotIn('0x9000',[offset for offset,_ in images]) # preserve NVS
        with self.assertRaises(ValueError):artifact.upload_images(self.root,'wrong')
        self.assertIn('env.Replace(FLASH_EXTRA_IMAGES=upload_images', (ROOT/'tools/upload_preflight.py').read_text())

    def test_source_free_artifact_guard(self):
        spec=importlib.util.spec_from_file_location('artifact_preflight',ROOT/'tools/artifact_preflight.py')
        artifact=importlib.util.module_from_spec(spec);spec.loader.exec_module(artifact)
        (self.root/'tools').mkdir()
        for name in ('upload_preflight.py','artifact_preflight.py','flash_via_pi.py'):
            shutil.copyfile(ROOT/'tools'/name,self.root/'tools'/name)
        environment=flash.ENVIRONMENTS[0]
        names=['platformio.ini','include/display_profile.h','tools/upload_preflight.py','tools/artifact_preflight.py','tools/flash_via_pi.py']
        for name in ('firmware.bin','bootloader.bin','partitions.bin'):
            relative=f'.pio/build/{environment}/{name}'
            p=self.root/relative;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b'tested');names.append(relative)
        boot=self.root/'.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin'
        boot.parent.mkdir(parents=True);boot.write_bytes(b'boot')
        m={'environment':environment,'branch':'codex/test','commit':'a'*40,'source_checks_passed':True,
           'source_snapshot_sha256':'b'*64,'boot_app0_sha256':flash.digest(boot),
           'sha256':{name:flash.digest(self.root/name) for name in names}}
        manifest=self.root/'manifest.json';manifest.write_text(json.dumps(m))
        with patch.object(artifact.Path,'home',return_value=self.root):
            artifact.verify(self.root,environment,'seeed_xiao_esp32s3',{'nobuild','upload'},'manifest.json')
            with self.assertRaises(ValueError):artifact.verify(self.root,environment,'seeed_xiao_esp32s3',{'upload'},'manifest.json')
            with self.assertRaises(ValueError):artifact.verify(self.root,environment,'wrong',{'nobuild','upload'},'manifest.json')
            m['source_checks_passed']=False;manifest.write_text(json.dumps(m))
            with self.assertRaises(ValueError):artifact.verify(self.root,environment,'seeed_xiao_esp32s3',{'nobuild','upload'},'manifest.json')
            m['source_checks_passed']=True;manifest.write_text(json.dumps(m))
            (self.root/names[-1]).write_bytes(b'corrupted')
            with self.assertRaises(ValueError):artifact.verify(self.root,environment,'seeed_xiao_esp32s3',{'nobuild','upload'},'manifest.json')

if __name__=='__main__':unittest.main()

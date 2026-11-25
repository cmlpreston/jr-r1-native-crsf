import json,runpy,pathlib,hashlib,shutil,unittest
from unittest.mock import patch
from er6g_upload_preflight_test import Checks as BaseChecks
ROOT=pathlib.Path(__file__).resolve().parents[1]
MODE=runpy.run_path(str(ROOT/'tools/er6g_mode_upload_preflight.py'))
class ModeChecks(BaseChecks):
 def prepare_mode(self):
  self.prepare_read()
  for n in MODE['MODE_SOURCES']:
   p=self.root/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b'code');self.m['sources'][n]=MODE['sha'](b'code')
  stage=self.m['startup_image'];stage.update(version=MODE['STAGE'],peer_version=MODE['VERSION'])
  small=(self.art/'er6g-startup-stage.bin').read_bytes().replace(b'STARTUP-r3',('STARTUP-'+MODE['STAGE'].rsplit('-',1)[1]).encode())
  small=small[:-32]+hashlib.sha256(small[:-32]).digest()
  stage.update(sha256=MODE['sha'](small),app_sha256=MODE['BASE']['app_digest'](small))
  (self.art/'er6g-startup-stage.bin').write_bytes(small)
  image=b'\xe9\xbe\xef\xca\xfeUNIFIED_ESP32_2400_RX\0RadioMaster ER6-G "pwm_outputs": [14, 1, 3, 15, 2, 4] '+MODE['VERSION'].encode()+bytes.fromhex(stage['app_sha256'])
  (self.art/'er6g-esc-drive-mode.bin').write_bytes(image)
  self.m.update(mode='esc-drive-mode',version=MODE['VERSION'],backup=MODE['BACKUP'],sha256=MODE['sha'](image))
  shutil.copytree(self.backup,self.root/MODE['BACKUP']);self.save_mode()
 def save_mode(self):(self.art/'manifest.json').write_text(json.dumps(self.m))
 def mode_validate(self,live=False,startup=False,art=None):return MODE['validate'](self.root,self.project,'esp32dev',art or self.art,live=live,startup=startup)
 def test_mode_exact_sources_capabilities_and_recovery(self):
  self.prepare_mode();self.mode_validate()
  for key,value in [('writes',True),('version','older'),('mode','esc-read-only'),('backup','wrong')]:
   old=self.m[key];self.m[key]=value;self.save_mode()
   with self.assertRaises(ValueError):self.mode_validate()
   self.m[key]=old;self.save_mode()
  f=self.art/'er6g-esc-drive-mode.bin';good=f.read_bytes();wrong=good.replace(b'UNIFIED_ESP32_2400_RX',b'R1_ER6G_ESC_MODE_BUILD')
  f.write_bytes(wrong);self.m['sha256']=MODE['sha'](wrong);self.save_mode()
  with self.assertRaises(ValueError):self.mode_validate()
  f.write_bytes(good);self.m['sha256']=MODE['sha'](good);self.save_mode()
  (self.root/MODE['MODE_SOURCES'][0]).write_bytes(b'changed')
  with self.assertRaises(ValueError):self.mode_validate()
 def test_mode_live_migration_rejects_wrong_receiver_or_slot(self):
  self.prepare_mode();self.config['settings']['version']=MODE['PREVIOUS_VERSION']
  self.config['esc-listener']='mode=drive starterPeerReady=1 starterArmed=0 starterRunningSlot=2031616 starterOtherSlot=65536 starterOtherSize=1966080'
  def save():self.responses['/config']=json.dumps(self.config).encode()
  save()
  with patch.dict(MODE['validate'].__globals__,{'read_url':lambda p:self.responses[p]}):
   self.mode_validate(True)
   self.config['esc-listener']=self.config['esc-listener'].replace('starterArmed=0','starterArmed=1');save()
   with self.assertRaises(ValueError):self.mode_validate(True)
   self.config['settings']['version']='wrong';save()
   with self.assertRaises(ValueError):self.mode_validate(True)
 def test_stage_needs_exact_verified_full_drive_bank(self):
  self.prepare_mode();art=self.root/'small';art.mkdir()
  (art/'er6g-startup-stage.bin').write_bytes((self.art/'er6g-startup-stage.bin').read_bytes())
  meta=dict(self.m['startup_image'],full_artifacts='artifact',backup=MODE['FULL_BACKUP'])
  (art/'manifest.json').write_text(json.dumps(meta))
  current=json.loads(json.dumps(self.config));current['settings']['version']=MODE['VERSION']
  current['esc-listener']='mode=drive txStarted=0 starterArmed=0 starterPeerReady=0 starterRunningSlot=65536 starterOtherSlot=2031616 starterOtherSize=1966080'
  data={'config.json':json.dumps(current).encode(),'hardware.json':self.responses['/hardware.json'],'options.json':b'{}','original-firmware.bin':(self.art/'er6g-esc-drive-mode.bin').read_bytes()}
  b=self.root/MODE['FULL_BACKUP'];b.mkdir(parents=True)
  for n,d in data.items():(b/n).write_bytes(d)
  (b/'backup-manifest.json').write_text(json.dumps({'backup_sha256':{n:MODE['sha'](d) for n,d in data.items()}}))
  responses={'/config':data['config.json'],'/hardware.json':data['hardware.json'],'/firmware.bin':data['original-firmware.bin']}
  with patch.dict(MODE['validate'].__globals__,{'read_url':lambda p:responses[p]}):
   self.mode_validate(True,True,art)
   responses['/firmware.bin']=b'wrong'
   with self.assertRaises(ValueError):self.mode_validate(True,True,art)
  (art/'er6g-startup-stage.bin').write_bytes(b'wrong')
  with self.assertRaises(ValueError):self.mode_validate(False,True,art)
if __name__=='__main__':unittest.main()

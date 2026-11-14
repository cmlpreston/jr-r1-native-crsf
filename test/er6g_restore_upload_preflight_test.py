import pathlib,runpy,json,shutil,unittest
from unittest.mock import patch
from er6g_mode_upload_preflight_test import ModeChecks
ROOT=pathlib.Path(__file__).resolve().parents[1]
R=runpy.run_path(str(ROOT/'tools/er6g_restore_upload_preflight.py'))
class RestoreChecks(ModeChecks):
 def prepare_restore(self):
  self.prepare_mode();history=self.root/R['HISTORY'];shutil.copytree(self.art,history)
  self.pins={'FULL_SHA':R['sha']((history/'er6g-esc-drive-mode.bin').read_bytes()),'STAGE_SHA':R['sha']((history/'er6g-startup-stage.bin').read_bytes()),'MANIFEST_SHA':R['sha']((history/'manifest.json').read_bytes())}
  self.meta={'mode':'exact-r12-r7-restore','recovery':R['BACKUP'],'full_recovery':R['FULL_BACKUP'],'approved_manifest_sha256':self.pins['MANIFEST_SHA']}
  (self.art/'manifest.json').write_text(json.dumps(self.meta));shutil.copytree(self.backup,self.root/R['BACKUP'])
 def restore_validate(self,live=False,startup=False):return R['validate'](self.root,self.project,'esp32dev',self.art,live=live,startup=startup)
 def test_restore_exact_historical_bytes_and_sources(self):
  self.prepare_restore()
  with patch.dict(R['validate'].__globals__,self.pins):
   self.restore_validate()
   p=self.art/'er6g-esc-drive-mode.bin';data=p.read_bytes();p.write_bytes(data+b'changed')
   with self.assertRaises(ValueError):self.restore_validate()
   p.write_bytes(data);(self.root/'receiver/er6g/reader/Er6gEscRead.cpp').write_bytes(b'changed')
   with self.assertRaises(ValueError):self.restore_validate()
 def test_restore_live_r13_drive_only(self):
  self.prepare_restore();self.config['settings']['version']='4.1.0-er6g-esc-single-r13'
  good='mode=drive txStarted=0 starterPeerReady=0 starterArmed=0 startupStrategy=single-app singlePending=0 singleEntryRequested=0'
  def setdiag(d):self.config['esc-listener']=d;self.responses['/config']=json.dumps(self.config).encode()
  setdiag(good)
  with patch.dict(R['validate'].__globals__,dict(self.pins,read_url=lambda p:self.responses[p])):
   self.restore_validate(True)
   for d in (good.replace('mode=drive','mode=programming'),good.replace('singlePending=0','singlePending=1'),good.replace('starterArmed=0','starterArmed=1')):
    setdiag(d)
    with self.assertRaises(ValueError):self.restore_validate(True)
   setdiag(good);self.responses['/firmware.bin']=b'wrong'
   with self.assertRaises(ValueError):self.restore_validate(True)
 def test_restore_stage_needs_exact_full_readback(self):
  self.prepare_restore();self.config['settings']['version']='4.1.0-er6g-esc-mode-r12'
  self.config['esc-listener']='mode=drive txStarted=0 starterPeerReady=0 starterArmed=0 starterRunningSlot=2031616 starterOtherSlot=65536 starterOtherSize=1966080'
  files={'config.json':json.dumps(self.config).encode(),'hardware.json':self.responses['/hardware.json'],'options.json':b'{}','original-firmware.bin':(self.art/'er6g-esc-drive-mode.bin').read_bytes()}
  b=self.root/R['FULL_BACKUP'];b.mkdir(parents=True)
  for n,d in files.items():(b/n).write_bytes(d)
  (b/'backup-manifest.json').write_text(json.dumps({'backup_sha256':{n:R['sha'](d) for n,d in files.items()}}))
  responses={'/config':files['config.json'],'/hardware.json':files['hardware.json'],'/firmware.bin':files['original-firmware.bin']}
  with patch.dict(R['validate'].__globals__,dict(self.pins,read_url=lambda p:responses[p])):
   self.restore_validate(True,True)
   responses['/firmware.bin']=b'wrong'
   with self.assertRaises(ValueError):self.restore_validate(True,True)
if __name__=='__main__':unittest.main()

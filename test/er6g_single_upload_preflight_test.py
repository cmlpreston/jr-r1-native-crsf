import json,runpy,pathlib,shutil,unittest
from unittest.mock import patch
from er6g_mode_upload_preflight_test import ModeChecks
ROOT=pathlib.Path(__file__).resolve().parents[1]
S=runpy.run_path(str(ROOT/'tools/er6g_single_upload_preflight.py'))
class SingleChecks(ModeChecks):
 def prepare_single(self):
  self.prepare_mode()
  n='receiver/er6g/reader/Er6gEscSingleStartup.cpp';(self.root/n).write_bytes(b'single-code')
  self.m['sources']={n:S['sha']((self.root/n).read_bytes()) for n in S['SOURCES']}
  self.m.pop('startup_image');self.m.update(version=S['VERSION'],startup_strategy='single-app',backup=S['BACKUP'])
  image=b'\xe9\xbe\xef\xca\xfeUNIFIED_ESP32_2400_RX\0RadioMaster ER6-G "pwm_outputs": [14, 1, 3, 15, 2, 4] '+S['VERSION'].encode()+b' startupStrategy=single-app'
  (self.art/'er6g-esc-drive-mode.bin').write_bytes(image);self.m['sha256']=S['sha'](image)
  shutil.copytree(self.backup,self.root/S['BACKUP']);self.save_mode()
 def single_validate(self,live=False):return S['validate'](self.root,self.project,'esp32dev',self.art,live=live)
 def test_single_inventory_strategy_and_no_writes(self):
  self.prepare_single();self.single_validate()
  for k,v in [('startup_strategy','paired'),('writes',True),('version','wrong'),('backup','wrong'),('startup_image',{})]:
   old=dict(self.m);self.m[k]=v;self.save_mode()
   with self.assertRaises(ValueError):self.single_validate()
   self.m=old;self.save_mode()
  (self.root/'receiver/er6g/reader/Er6gEscSingleStartup.cpp').write_bytes(b'changed')
  with self.assertRaises(ValueError):self.single_validate()
 def test_single_live_recovery_and_no_pending(self):
  self.prepare_single();self.config['settings']['version']='4.1.0-er6g-esc-mode-r12'
  good='mode=drive txStarted=0 starterPeerReady=1 starterArmed=0 starterRunningSlot=2031616 starterOtherSlot=65536 starterOtherSize=1966080'
  def setdiag(d):self.config['esc-listener']=d;self.responses['/config']=json.dumps(self.config).encode()
  setdiag(good)
  with patch.dict(S['validate'].__globals__,{'read_url':lambda p:self.responses[p]}):
   self.single_validate(True)
   for d in (good.replace('mode=drive','mode=programming'),good.replace('starterArmed=0','starterArmed=1'),good.replace('starterPeerReady=1','starterPeerReady=0')):
    setdiag(d)
    with self.assertRaises(ValueError):self.single_validate(True)
   setdiag(good);self.responses['/firmware.bin']=b'wrong'
   with self.assertRaises(ValueError):self.single_validate(True)
if __name__=='__main__':unittest.main()

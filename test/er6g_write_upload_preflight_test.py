import json,pathlib,runpy,unittest
from unittest.mock import patch
from er6g_upload_preflight_test import Checks
from er6g_mode_upload_preflight_test import ModeChecks
ROOT=pathlib.Path(__file__).resolve().parents[1]
W=runpy.run_path(str(ROOT/'tools/er6g_write_upload_preflight.py'))
class WriteChecks(Checks):
 def prepare_write(self):
  with patch.dict(ModeChecks.prepare_mode.__globals__,{'MODE':W}):ModeChecks.prepare_mode(self)
  self.m.update(writes=True,write_policy='one-rf-command-ack-readback-no-retry');self.save_mode()
 def save_mode(self):(self.art/'manifest.json').write_text(json.dumps(self.m))
 def write_validate(self,live=False,startup=False,art=None):return W['validate'](self.root,self.project,'esp32dev',art or self.art,live=live,startup=startup)
 def test_write_pair_capability_policy_and_sources(self):
  self.prepare_write();self.write_validate()
  for k,v in [('writes',False),('write_policy','retry'),('version','wrong')]:
   old=self.m[k];self.m[k]=v;self.save_mode()
   with self.assertRaises(ValueError):self.write_validate()
   self.m[k]=old;self.save_mode()
  (self.root/'receiver/er6g/reader/Er6gEscRead.cpp').write_bytes(b'changed')
  with self.assertRaises(ValueError):self.write_validate()
 def test_write_live_requires_proven_idle_r12(self):
  self.prepare_write();self.config['settings']['version']=W['PREVIOUS_VERSION']
  self.config['esc-listener']='mode=drive starterPeerReady=1 starterArmed=0 starterRunningSlot=2031616 starterOtherSlot=65536 starterOtherSize=1966080'
  self.responses['/config']=json.dumps(self.config).encode()
  with patch.dict(W['validate'].__globals__,{'read_url':lambda p:self.responses[p]}):
   self.write_validate(True)
   self.config['esc-listener']=self.config['esc-listener'].replace('starterArmed=0','starterArmed=1');self.responses['/config']=json.dumps(self.config).encode()
   with self.assertRaises(ValueError):self.write_validate(True)
 def test_write_stage_requires_exact_full_pair(self):
  # Reuse the stage test with the write-capable parent fixture/guard.
  with patch.dict(ModeChecks.test_stage_needs_exact_verified_full_drive_bank.__globals__,{'MODE':W}):
   ModeChecks.test_stage_needs_exact_verified_full_drive_bank(self)
 def prepare_mode(self):self.prepare_write()
 def mode_validate(self,live=False,startup=False,art=None):return self.write_validate(live,startup,art)
if __name__=='__main__':unittest.main()

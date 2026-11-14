import gzip, hashlib, json, pathlib, runpy, tempfile, unittest
from unittest.mock import patch
TOOLS=pathlib.Path(__file__).resolve().parents[1]/'tools'
CHECK=runpy.run_path(str(TOOLS/'er4_upload_preflight.py'))
class Er4UploadChecks(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.root=pathlib.Path(self.temp.name);self.project=self.root/'receiver/er4/uploader';self.project.mkdir(parents=True)
  self.art=self.root/'artifact';self.art.mkdir();sources={}
  for name in CHECK['SOURCES']:
   p=self.root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b'code');sources[name]=CHECK['sha'](p.read_bytes())
  (self.art/'elrs.flash.1m.ld').write_bytes(b'layout')
  self.image=b'\xe9RadioMaster ER4 "pwm_outputs": [0, 1, 3, 9]';gz=gzip.compress(self.image)
  (self.art/'er4-esc-rf-only-bench.bin').write_bytes(self.image);(self.art/'er4-esc-rf-only-bench.bin.gz').write_bytes(gz)
  self.manifest={'ldscript_sha256':CHECK['sha'](b'layout'),'sources':sources,'hardware':'radiomaster.rx_2400.er4','upstream':'a9d4a9cb','selector_channel':12,'interface':False,'writes':False,'sha256':CHECK['sha'](self.image),'gzip_sha256':CHECK['sha'](gz)};self.save()
  self.old=b'\xe9old-image';self.config={'config':{'uid':[1,2,3,4,5,6],'modelid':255},'settings':{'target':CHECK['EXPECTED_TARGET'],'product_name':'RadioMaster ER4'}}
  p=self.root/CHECK['BACKUP_DIR'];p.mkdir(parents=True);(p/'original-firmware.bin').write_bytes(self.old);(p/'config.json').write_text(json.dumps(self.config));(p/'backup-manifest.json').write_text(json.dumps({'backup_sha256':{'original-firmware.bin':CHECK['sha'](self.old)}}))
  recovery=self.root/CHECK['RECOVERY_DIR'];recovery.mkdir(parents=True)
  (recovery/'original-firmware.bin').write_bytes(b'original-recovery')
  (recovery/'backup-manifest.json').write_text(json.dumps({'backup_sha256':{'original-firmware.bin':CHECK['sha'](b'original-recovery')}}))
 def tearDown(self):self.temp.cleanup()
 def save(self):(self.art/'manifest.json').write_text(json.dumps(self.manifest))
 def validate(self,**kw):return CHECK['validate'](self.root,self.project,'esp8285',self.art,**kw)
 def test_accepts_rf_only(self):self.validate(live=False)
 def test_refuses_write_enabled(self):
  self.manifest['writes']=True;self.save()
  with self.assertRaises(ValueError):self.validate(live=False)
 def test_direct_read_requires_its_profile_and_reviewed_version(self):
  self.manifest['interface']=True;self.save()
  with self.assertRaises(ValueError):self.validate(live=False)
  with self.assertRaises(ValueError):self.validate(live=False,direct_read=True)
  self.image+=b'4.1.0-er4-esc-direct-read-r6'
  self.manifest.update(diagnostics=True,version='4.1.0-er4-esc-direct-read-r6')
  gz=gzip.compress(self.image)
  (self.art/'er4-esc-rf-only-bench.bin').write_bytes(self.image)
  (self.art/'er4-esc-rf-only-bench.bin.gz').write_bytes(gz)
  self.manifest.update(sha256=CHECK['sha'](self.image),gzip_sha256=CHECK['sha'](gz));self.save()
  self.validate(live=False,direct_read=True)
  self.manifest['writes']=True;self.save()
  with self.assertRaises(ValueError):self.validate(live=False,direct_read=True)
 def test_rf_only_image_rejected_by_direct_profile(self):
  with self.assertRaises(ValueError):self.validate(live=False,direct_read=True)
 def test_refuses_other_project_or_board(self):
  for project,board in ((self.root,'esp8285'),(self.project,'seeed_xiao_esp32s3')):
   with self.assertRaises(ValueError):CHECK['validate'](self.root,project,board,self.art,False)
 def test_refuses_source_changes(self):
  (self.root/CHECK['SOURCES'][0]).write_bytes(b'changed')
  with self.assertRaises(ValueError):self.validate(live=False)
 def test_refuses_corrupt_image(self):
  (self.art/'er4-esc-rf-only-bench.bin').write_bytes(b'bad')
  with self.assertRaises(ValueError):self.validate(live=False)
 def test_live_identity_and_recovery_image(self):
  responses={'/config':json.dumps(self.config).encode(),'/hardware.json':b'{"pwm_outputs":[0,1,3,9]}','/firmware.bin':self.old}
  with patch.dict(CHECK['validate'].__globals__,{'read_url':lambda path:responses[path]}):
   self.validate()
   responses['/firmware.bin']=b'changed'
   with self.assertRaises(ValueError):self.validate()
 def test_keeps_original_recovery_guard(self):
  (self.root/CHECK['RECOVERY_DIR']/'original-firmware.bin').write_bytes(b'changed')
  with self.assertRaises(ValueError):self.validate(live=False)
 def test_refuses_er6g(self):
  self.config['settings']['target']='ER6G'
  with patch.dict(CHECK['validate'].__globals__,{'read_url':lambda path:json.dumps(self.config).encode() if path=='/config' else b'{"pwm_outputs":[0,1,3,9]}'}):
   with self.assertRaises(ValueError):self.validate()
if __name__=='__main__':unittest.main()

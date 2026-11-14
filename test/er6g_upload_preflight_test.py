import hashlib,json,pathlib,runpy,tempfile,unittest
from unittest.mock import patch
ROOT=pathlib.Path(__file__).resolve().parents[1]
C=runpy.run_path(str(ROOT/'tools/er6g_upload_preflight.py'))
START=runpy.run_path(str(ROOT/'tools/er6g_startup_upload_preflight.py'))
class Checks(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.root=pathlib.Path(self.tmp.name);self.project=self.root/'receiver/er6g/uploader';self.project.mkdir(parents=True);self.art=self.root/'artifact';self.art.mkdir();sources={}
  for n in C['SOURCES']+C['LISTEN_SOURCES']+C['READ_SOURCES']:
   p=self.root/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes((ROOT/n).read_bytes() if n.endswith('.patch') else b'code');sources[n]=C['sha'](p.read_bytes())
  self.image=b'\xe9RadioMaster ER6-G "pwm_outputs": [14, 1, 3, 15, 2, 4]';(self.art/'er6g-stock-teamrace-slot-fix.bin').write_bytes(self.image)
  self.m={'hardware':'radiomaster.rx_2400.er6g','upstream':'a9d4a9cb5b5687c4c9d7e9e7fbdf44ad93651da6','base_rx_main_sha256':C['BASE_SHA'],'patch_sha256':C['PATCH_SHA'],'sources':sources,'sha256':C['sha'](self.image),'backup':'tmp/er6g-private-backup-2026-11-06'}
  (self.art/'manifest.json').write_text(json.dumps(self.m));self.backup=self.root/self.m['backup'];self.backup.mkdir(parents=True)
  self.config={'config':{'uid':[1,2,3,4,5,6],'modelid':255,'pwm':[1024]},'settings':{'target':C['EXPECTED_TARGET'],'version':'4.1.0','git-commit':'a9d4a9','product_name':'RadioMaster ER6-G','radio-type':'SX128X','reg_domain_high':'ISM2G4'}}
  data={'config.json':json.dumps(self.config).encode(),'hardware.json':b'{"pwm_outputs":[14,1,3,15,2,4]}','options.json':b'{}','original-firmware.bin':b'\xe9stock'}
  for n,b in data.items():(self.backup/n).write_bytes(b)
  (self.backup/'backup-manifest.json').write_text(json.dumps({'backup_sha256':{n:C['sha'](b) for n,b in data.items()}}))
  self.responses={'/config':data['config.json'],'/hardware.json':data['hardware.json'],'/firmware.bin':data['original-firmware.bin']}
 def tearDown(self):self.tmp.cleanup()
 def validate(self,live=False,listen_only=False,read_only=False):return C['validate'](self.root,self.project,'esp32dev',self.art,live,listen_only,read_only)
 def test_prepared_candidate(self):self.validate()
 def test_listen_only_cannot_use_stock_uploader(self):
  self.m['mode']='esc-listen-only';(self.art/'manifest.json').write_text(json.dumps(self.m))
  with self.assertRaises(ValueError):self.validate()
 def test_listen_profile_identity_capability_and_recovery(self):
  import shutil
  self.m.update(mode='esc-listen-only',version='4.1.0-er6g-esc-listen-r1',programming_tx=False,writes=False)
  self.m['backup']=C['LISTEN_BACKUP'];shutil.copytree(self.backup,self.root/C['LISTEN_BACKUP'])
  self.image+=b'4.1.0-er6g-esc-listen-r1';(self.art/'er6g-esc-listen-only.bin').write_bytes(self.image)
  self.m['sha256']=C['sha'](self.image)
  def save():(self.art/'manifest.json').write_text(json.dumps(self.m))
  save();self.validate(listen_only=True)
  self.config['settings']['version']='4.1.0-teamrace-slot-fix';self.responses['/config']=json.dumps(self.config).encode()
  with patch.dict(C['validate'].__globals__,{'read_url':lambda p:self.responses[p]}):self.validate(True,True)
  for key in ('writes','programming_tx'):
   self.m[key]=True;save()
   with self.assertRaises(ValueError):self.validate(listen_only=True)
   self.m[key]=False
  self.m['version']='other';save()
  with self.assertRaises(ValueError):self.validate(listen_only=True)
 def test_stock_image_rejected_by_listen_profile(self):
  with self.assertRaises(ValueError):self.validate(listen_only=True)
 def prepare_read(self):
  import shutil
  self.m.update(mode='esc-read-only',version='4.1.0-er6g-esc-read-r8c',programming_tx=True,writes=False,backup=C['READ_BACKUP'])
  shutil.copytree(self.backup,self.root/C['READ_BACKUP'])
  self.image+=b'4.1.0-er6g-esc-read-r8c';(self.art/'er6g-esc-read-only.bin').write_bytes(self.image)
  import struct
  payload=b'\xbe\xef\xca\xfeUNIFIED_ESP32_2400_RX\0R1-ER6G-STARTUP-r3 RadioMaster ER6-G';header=bytearray(24);header[0]=0xe9;header[1]=1;header[23]=1
  stage=bytes(header)+struct.pack('<II',0x3f400020,len(payload))+payload
  stage+=b'\0'*(15-len(stage)%16)+b'\0';stage+=hashlib.sha256(stage).digest()
  app_sha=C['app_digest'](stage)
  self.m['startup_image']={'mode':'esc-startup-stage','version':'er6g-startup-r3','hardware':'radiomaster.rx_2400.er6g','writes':False,'rf':False,'gpio':2,'uart':2,'baud':800,'peer_version':'4.1.0-er6g-esc-read-r8c','sha256':C['sha'](stage),'app_sha256':app_sha,'sources':{n:self.m['sources'][n] for n in C['STARTUP_SOURCES']}}
  (self.art/'er6g-startup-stage.bin').write_bytes(stage)
  self.image+=bytes.fromhex(app_sha);(self.art/'er6g-esc-read-only.bin').write_bytes(self.image)
  self.m['sha256']=C['sha'](self.image)
  def save():(self.art/'manifest.json').write_text(json.dumps(self.m))
  save()
 def test_read_only_profile_keeps_setting_writes_disabled(self):
  self.prepare_read()
  def save():(self.art/'manifest.json').write_text(json.dumps(self.m))
  self.validate(read_only=True)
  with self.assertRaises(ValueError):self.validate(listen_only=True)
  with self.assertRaises(ValueError):self.validate()
  self.config['esc-listener']='starterPeerReady=1 starterArmed=1 starterRunningSlot=65536 starterOtherSlot=2031616 starterOtherSize=1966080'
  self.config['settings']['version']='4.1.0-er6g-esc-read-r8b';self.responses['/config']=json.dumps(self.config).encode()
  with patch.dict(C['validate'].__globals__,{'read_url':lambda p:self.responses[p]}):self.validate(True,read_only=True)
  self.m['writes']=True;save()
  with self.assertRaises(ValueError):self.validate(read_only=True)
  self.m['writes']=False;self.m['version']='other';save()
  with self.assertRaises(ValueError):self.validate(read_only=True)
 def test_small_stage_requires_exact_installed_peer_and_recovery(self):
  self.prepare_read();stage=self.root/'startup';stage.mkdir()
  full=self.art
  (stage/'er6g-startup-stage.bin').write_bytes((full/'er6g-startup-stage.bin').read_bytes())
  m=dict(self.m['startup_image'],full_artifacts='artifact',backup=START['BACKUP'])
  (stage/'manifest.json').write_text(json.dumps(m))
  backup=self.root/START['BACKUP'];backup.mkdir(parents=True)
  current=json.loads(json.dumps(self.config));current['settings']['version']='4.1.0-er6g-esc-read-r8c'
  current['esc-listener']='starterArmed=0 starterPeerReady=0 starterRunningSlot=65536 starterOtherSlot=2031616 starterOtherSize=1966080'
  responses={'/config':json.dumps(current).encode(),'/hardware.json':self.responses['/hardware.json'],'/firmware.bin':(full/'er6g-esc-read-only.bin').read_bytes()}
  for name,data in [('config.json',responses['/config']),('hardware.json',responses['/hardware.json']),('options.json',b'{}'),('original-firmware.bin',responses['/firmware.bin'])]:(backup/name).write_bytes(data)
  (backup/'backup-manifest.json').write_text(json.dumps({'backup_sha256':{name:C['sha']((backup/name).read_bytes()) for name in ('config.json','hardware.json','options.json','original-firmware.bin')}}))
  def check():return START['validate'](self.root,self.project,'esp32dev',stage)
  with patch.dict(START['validate'].__globals__['BASE'],{'read_url':lambda p:responses[p]}):
   check()
   for bad in ('starterArmed=1 starterPeerReady=0 starterRunningSlot=65536 starterOtherSlot=2031616 starterOtherSize=1966080','starterArmed=0 starterPeerReady=0 starterRunningSlot=65536 starterOtherSlot=1234 starterOtherSize=1966080','starterArmed=0'):
    changed=dict(current);changed['esc-listener']=bad;responses['/config']=json.dumps(changed).encode()
    with self.assertRaises(ValueError):check()
   responses['/config']=json.dumps(current).encode();responses['/firmware.bin']=b'wrong'
   with self.assertRaises(ValueError):check()
   responses['/firmware.bin']=(full/'er6g-esc-read-only.bin').read_bytes()
   image=stage/'er6g-startup-stage.bin';image.write_bytes(b'corrupt')
   with self.assertRaises(ValueError):check()
 def test_wrong_board_or_project(self):
  for project,board in ((self.root,'esp32dev'),(self.project,'seeed_xiao_esp32s3')):
   with self.assertRaises(ValueError):C['validate'](self.root,project,board,self.art,False)
 def test_changed_sources_or_patch(self):
  for n in C['SOURCES']:
   p=self.root/n;b=p.read_bytes();p.write_bytes(b'changed')
   with self.assertRaises(ValueError):self.validate()
   p.write_bytes(b)
 def test_corrupt_image_or_recovery(self):
  for p in (self.art/'er6g-stock-teamrace-slot-fix.bin',self.backup/'original-firmware.bin'):
   b=p.read_bytes();p.write_bytes(b'changed')
   with self.assertRaises(ValueError):self.validate()
   p.write_bytes(b)
 def test_live_identity_mapping_and_image(self):
  with patch.dict(C['validate'].__globals__,{'read_url':lambda p:self.responses[p]}):
   self.validate(True)
   for section,key,value in (('settings','target','ER4'),('settings','reg_domain_high','EU_CE'),('settings','version','other'),('config','uid',[0]*6),('config','pwm',[0])):
    d=json.loads(json.dumps(self.config));d[section][key]=value;self.responses['/config']=json.dumps(d).encode()
    with self.assertRaises(ValueError):self.validate(True)
   self.responses['/config']=json.dumps(self.config).encode();self.responses['/firmware.bin']=b'changed'
   with self.assertRaises(ValueError):self.validate(True)
if __name__=='__main__':unittest.main()

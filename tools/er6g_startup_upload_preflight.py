"""Guard the small-stage OTA only after its exact full r8 peer is installed."""
import json,pathlib,runpy,re
BASE=runpy.run_path(str(pathlib.Path(__file__).with_name('er6g_upload_preflight.py')))
BACKUP='tmp/er6g-private-backup-2026-11-07-read-r8c'
HOST=BASE['HOST']
def image_name(*_):return 'er6g-startup-stage.bin'
def field(diag,key):
 found=re.search(r'(?:^| )'+re.escape(key)+r'=(\d+)(?: |$)',diag)
 if not found:raise ValueError('Missing live OTA-bank evidence: '+key)
 return int(found.group(1))
def validate(root,project,board,artifacts,live=True):
 root=pathlib.Path(root).resolve();project=pathlib.Path(project).resolve();artifacts=pathlib.Path(artifacts).resolve()
 if project!=root/'receiver/er6g/uploader' or board!='esp32dev':raise ValueError('Startup upload project/board mismatch')
 m=json.loads((artifacts/'manifest.json').read_text())
 if m.get('mode')!='esc-startup-stage' or m.get('version')!='er6g-startup-r3' or m.get('writes') is not False or m.get('rf') is not False:raise ValueError('Only reviewed read-only/no-RF small stage is allowed')
 full=(root/m['full_artifacts']).resolve()
 parent=BASE['validate'](root,project,board,full,live=False,read_only=True)
 if parent['startup_image']!={k:v for k,v in m.items() if k not in ('full_artifacts','backup')}:raise ValueError('Startup metadata differs from installed full peer')
 image=(artifacts/image_name()).read_bytes()
 if image!=(full/image_name()).read_bytes() or BASE['sha'](image)!=m['sha256'] or BASE['app_digest'](image)!=m['app_sha256']:raise ValueError('Startup image differs from exact full-peer digest')
 if b'\xbe\xef\xca\xfeUNIFIED_ESP32_2400_RX\x00R1-ER6G-STARTUP-r3' not in image or b'RadioMaster ER6-G' not in image or len(image)>400000:raise ValueError('Startup image identity/size mismatch')
 backup=(root/m['backup']).resolve()
 if backup!=root/BACKUP:raise ValueError('Require fresh installed-r8 recovery snapshot')
 saved=json.loads((backup/'backup-manifest.json').read_text())
 for name in ('config.json','hardware.json','options.json','original-firmware.bin'):
  if BASE['sha']((backup/name).read_bytes())!=saved['backup_sha256'].get(name):raise ValueError('Startup recovery snapshot changed')
 if live:
  current=json.loads(BASE['read_url']('/config'));old=json.loads((backup/'config.json').read_text());settings=current['settings']
  if settings.get('version')!='4.1.0-er6g-esc-read-r8c' or settings.get('target')!=BASE['EXPECTED_TARGET'] or settings.get('radio-type')!='SX128X' or settings.get('reg_domain_high')!='ISM2G4' or 'ER6-G' not in settings.get('product_name',''):raise ValueError('Full r8 ER6G peer not running')
  if current['config']!=old['config'] or json.loads(BASE['read_url']('/hardware.json')).get('pwm_outputs')!=[14,1,3,15,2,4]:raise ValueError('ER6G settings/pin map changed')
  diag=current.get('esc-listener','')
  if field(diag,'starterArmed')!=0 or field(diag,'starterPeerReady')!=0:raise ValueError('Existing startup stage requires a separate reviewed update')
  if {field(diag,'starterRunningSlot'),field(diag,'starterOtherSlot')}!={0x10000,0x1f0000} or field(diag,'starterOtherSize')!=0x1e0000:raise ValueError('Unexpected live OTA partition layout')
  data=BASE['read_url']('/firmware.bin');expected=(full/'er6g-esc-read-only.bin').read_bytes()
  if data[:len(expected)]!=expected or BASE['sha'](data)!=saved['backup_sha256']['original-firmware.bin']:raise ValueError('Live full-peer image changed since verified snapshot')
 return m

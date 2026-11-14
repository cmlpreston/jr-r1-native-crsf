"""Narrow stock-plus-PR3819 ER6G candidate checks. Upload still needs user approval."""
import hashlib,json,pathlib,urllib.request,runpy,re
app_digest=runpy.run_path(str(pathlib.Path(__file__).with_name("esp_image.py")))["app_digest"]
ENVIRONMENT='er6g-stock-teamrace-wifi'
HOST='192.168.1.218'
EXPECTED_TARGET='UNIFIED_ESP32_2400_RX'
PATCH_SHA='c133e44f9014c8c8616973d54e2d02e38c050146e3b097260046a06acd8942fc'
BASE_SHA='e984fba47da4f2c75dfcb24e281ab8cebc64244f651432e0ff3458287e48bab5'
SOURCES=('receiver/er6g/build.py','receiver/er6g/deploy.py','receiver/elrs/teamrace_filter.py','receiver/elrs/teamrace-telemetry-slot.patch')
LISTEN_SOURCES=('receiver/er6g/listen_prepare.py','receiver/er6g/listener/Er6gEscListen.cpp','receiver/er6g/listener/Er6gEscListen.h')
LISTEN_BACKUP='tmp/er6g-private-backup-2026-11-07-slotfix'
STARTUP_SOURCES=('receiver/er6g/startup/platformio.ini','receiver/er6g/startup/build_only.py','receiver/er6g/startup/build.py','receiver/er6g/startup/src/main.cpp','include/er6g_esc_startup.h','include/surpass_protocol.h','tools/esp_image.py')
READ_SOURCES=('receiver/er6g/reader/Er6gEscRead.cpp','receiver/er6g/reader/Er6gEscRead.h','receiver/er6g/reader/Er6gEscStartupPeer.cpp','receiver/er6g/reader/Er6gEscStartupPeer.h','include/er6g_esc_startup.h','include/surpass_protocol.h')+STARTUP_SOURCES
READ_BACKUP='tmp/er6g-private-backup-2026-11-07-read-r8b-paired'
def image_name(listen_only=False,read_only=False):
 return 'er6g-esc-read-only.bin' if read_only else 'er6g-esc-listen-only.bin' if listen_only else 'er6g-stock-teamrace-slot-fix.bin'
def sha(b):return hashlib.sha256(b).hexdigest()
def read_url(path):
 with urllib.request.urlopen('http://'+HOST+path,timeout=15) as r:return r.read()
def validate(root,project,board,artifacts,live=True,listen_only=False,read_only=False):
 root=pathlib.Path(root).resolve();project=pathlib.Path(project).resolve();artifacts=pathlib.Path(artifacts).resolve()
 if project!=root/'receiver/er6g/uploader' or board!='esp32dev':raise ValueError('ER6G project/board mismatch')
 if sha((root/'receiver/elrs/teamrace-telemetry-slot.patch').read_bytes())!=PATCH_SHA:raise ValueError('Canonical upstream patch changed')
 m=json.loads((artifacts/'manifest.json').read_text())
 if listen_only and read_only:raise ValueError('Choose one ER6G bench profile')
 if m.get('mode','stock-teamrace')!=('esc-read-only' if read_only else 'esc-listen-only' if listen_only else 'stock-teamrace'):raise ValueError('ER6G image does not match selected upload profile')
 if listen_only and (m.get('version')!='4.1.0-er6g-esc-listen-r1' or m.get('programming_tx') is not False or m.get('writes') is not False):raise ValueError('Only reviewed receive-only/no-write ER6G candidate is approved')
 if read_only and (m.get('version')!='4.1.0-er6g-esc-read-r8c' or m.get('programming_tx') is not True or m.get('writes') is not False):raise ValueError('Only reviewed single-pin/read-only ER6G candidate is approved')
 if m.get('hardware')!='radiomaster.rx_2400.er6g' or m.get('upstream')!='a9d4a9cb5b5687c4c9d7e9e7fbdf44ad93651da6' or m.get('base_rx_main_sha256')!=BASE_SHA or m.get('patch_sha256')!=PATCH_SHA:raise ValueError('Unreviewed ER6G source or patch')
 for n in SOURCES+(LISTEN_SOURCES if listen_only or read_only else ())+(READ_SOURCES if read_only else ()):
  if m.get('sources',{}).get(n)!=sha((root/n).read_bytes()):raise ValueError('ER6G source changed after packaging: '+n)
 image=(artifacts/image_name(listen_only,read_only)).read_bytes()
 if listen_only and b'4.1.0-er6g-esc-listen-r1' not in image:raise ValueError('Listen-only version identity missing')
 if read_only and b'4.1.0-er6g-esc-read-r8c' not in image:raise ValueError('Read-only version identity missing')
 if read_only:
  stage=m.get('startup_image',{});stage_data=(artifacts/'er6g-startup-stage.bin').read_bytes()
  if stage.get('mode')!='esc-startup-stage' or stage.get('version')!='er6g-startup-r3' or stage.get('hardware')!='radiomaster.rx_2400.er6g' or stage.get('writes') is not False or stage.get('rf') is not False or stage.get('peer_version')!='4.1.0-er6g-esc-read-r8c' or stage.get('gpio')!=2 or stage.get('uart')!=2 or stage.get('baud')!=800:raise ValueError('Unreviewed startup-stage capability/identity')
  if set(stage.get('sources',{}))!=set(STARTUP_SOURCES):raise ValueError('Startup source inventory changed')
  for name,digest in stage['sources'].items():
   if sha((root/name).read_bytes())!=digest:raise ValueError('Startup source changed: '+name)
  if len(stage_data)>400000 or sha(stage_data)!=stage.get('sha256') or app_digest(stage_data)!=stage.get('app_sha256') or bytes.fromhex(stage['app_sha256']) not in image:raise ValueError('Embedded startup image digest mismatch')
 if sha(image)!=m.get('sha256') or image[:1]!=b'\xe9' or b'RadioMaster ER6-G' not in image or b'"pwm_outputs": [14, 1, 3, 15, 2, 4]' not in image:raise ValueError('ER6G image hash/pin identity mismatch')
 backup=(root/m['backup']).resolve()
 if backup!=root/(READ_BACKUP if read_only else LISTEN_BACKUP if listen_only else 'tmp/er6g-private-backup-2026-11-06'):raise ValueError('Use reviewed fresh ER6G backup')
 saved=json.loads((backup/'backup-manifest.json').read_text())
 for n in ('config.json','hardware.json','options.json','original-firmware.bin'):
  if sha((backup/n).read_bytes())!=saved['backup_sha256'].get(n):raise ValueError('ER6G recovery snapshot changed')
 old=json.loads((backup/'config.json').read_text())
 if live:
  current=json.loads(read_url('/config'));hardware=json.loads(read_url('/hardware.json'));s=current.get('settings',{})
  if s.get('target')!=EXPECTED_TARGET or s.get('version')!=('4.1.0-er6g-esc-read-r8b' if read_only else '4.1.0-teamrace-slot-fix' if listen_only else '4.1.0') or s.get('git-commit')!='a9d4a9' or 'ER6-G' not in s.get('product_name','') or s.get('radio-type')!='SX128X' or s.get('reg_domain_high')!='ISM2G4':raise ValueError('Live receiver is not the reviewed stock ER6G/ISM2G4 target')
  if hardware.get('pwm_outputs')!=[14,1,3,15,2,4]:raise ValueError('ER6G live GPIO map changed')
  if read_only:
   diag=current.get('esc-listener','')
   def field(key):
    found=re.search(r'(?:^| )'+key+r'=(\d+)(?: |$)',diag)
    if not found:raise ValueError('Missing paired-slot migration evidence: '+key)
    return int(found.group(1))
   if field('starterPeerReady')!=1 or field('starterArmed')!=1 or {field('starterRunningSlot'),field('starterOtherSlot')}!={0x10000,0x1f0000} or field('starterOtherSize')!=0x1e0000:raise ValueError('r8c requires the reviewed installed r8b/startup pair')
  for key in ('uid','modelid','pwm','serial-protocol','serial1-protocol','sbus-failsafe','force-tlm','vbind'):
   if current['config'].get(key)!=old['config'].get(key):raise ValueError('ER6G binding/mapping/settings changed since backup')
  if sha(read_url('/firmware.bin'))!=saved['backup_sha256']['original-firmware.bin']:raise ValueError('ER6G live firmware changed since backup')
 return m

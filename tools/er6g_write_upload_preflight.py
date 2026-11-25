"""Exact proven r12/r7 -> single-write/readback r14/r8 app-only OTA migration. No force or retries."""
import json,pathlib,runpy,re,urllib.request
BASE=runpy.run_path(str(pathlib.Path(__file__).with_name('er6g_upload_preflight.py')))
HOST=BASE['HOST'];VERSION='4.1.0-er6g-esc-mode-r14';STAGE='er6g-startup-r8'
PREVIOUS_VERSION='4.1.0-er6g-esc-mode-r12'
BACKUP='tmp/er6g-private-backup-2026-11-11-mode-r12-before-write'
FULL_BACKUP='tmp/er6g-private-backup-2026-11-11-mode-r14'
MODE_SOURCES=('receiver/er6g/drive_pwm_prepare.py','include/esc_programmer_wire.h','receiver/er6g/reader/Er6gEscMode.h','receiver/er6g/reader/Er6gEscMode.cpp')
sha=BASE['sha']
def read_url(path):
 with urllib.request.urlopen('http://'+HOST+path,timeout=30) as r:return r.read()
def image_name(*_,startup=False):return 'er6g-startup-stage.bin' if startup else 'er6g-esc-drive-mode.bin'
def field(diag,key):
 m=re.search(r'(?:^| )'+re.escape(key)+r'=(\d+)(?: |$)',diag)
 if not m:raise ValueError('Missing OTA evidence: '+key)
 return int(m.group(1))
def snapshot(root,path,expected):
 b=(root/path).resolve()
 if b!=root/expected:raise ValueError('Require reviewed migration recovery snapshot')
 m=json.loads((b/'backup-manifest.json').read_text())
 for n in ('config.json','hardware.json','options.json','original-firmware.bin'):
  if sha((b/n).read_bytes())!=m['backup_sha256'].get(n):raise ValueError('Recovery snapshot changed: '+n)
 return b,json.loads((b/'config.json').read_text()),m

def validate(root,project,board,artifacts,live=True,startup=False):
 root=pathlib.Path(root).resolve();project=pathlib.Path(project).resolve();a=pathlib.Path(artifacts).resolve()
 if project!=root/'receiver/er6g/uploader' or board!='esp32dev':raise ValueError('ER6G project/board mismatch')
 m=json.loads((a/'manifest.json').read_text())
 if startup:
  full=(root/m['full_artifacts']).resolve()
  parent=validate(root,project,board,full,live=False)
  if parent['startup_image']!={k:v for k,v in m.items() if k not in ('full_artifacts','backup')}:raise ValueError('Small stage metadata differs from full peer')
  data=(a/image_name(startup=True)).read_bytes()
  if data!=(full/image_name(startup=True)).read_bytes():raise ValueError('Wrong small stage bytes')
  b,old,saved=snapshot(root,m['backup'],FULL_BACKUP)
 else:
  if m.get('mode')!='esc-drive-mode' or m.get('version')!=VERSION or m.get('programming_tx') is not True or m.get('writes') is not True or m.get('write_policy')!='one-rf-command-ack-readback-no-retry':raise ValueError('Only reviewed r14 single-write/readback app is allowed')
  if m.get('hardware')!='radiomaster.rx_2400.er6g' or m.get('upstream')!='a9d4a9cb5b5687c4c9d7e9e7fbdf44ad93651da6' or m.get('base_rx_main_sha256')!=BASE['BASE_SHA'] or m.get('patch_sha256')!=BASE['PATCH_SHA'] or sha((root/'receiver/elrs/teamrace-telemetry-slot.patch').read_bytes())!=BASE['PATCH_SHA']:raise ValueError('Unreviewed source/base/patch')
  for n in BASE['SOURCES']+BASE['LISTEN_SOURCES']+BASE['READ_SOURCES']+MODE_SOURCES:
   if m.get('sources',{}).get(n)!=sha((root/n).read_bytes()):raise ValueError('Source changed after build: '+n)
  data=(a/image_name()).read_bytes()
  if sha(data)!=m.get('sha256') or data[:1]!=b'\xe9' or b'\xbe\xef\xca\xfeUNIFIED_ESP32_2400_RX\0' not in data or VERSION.encode() not in data or b'RadioMaster ER6-G' not in data or b'"pwm_outputs": [14, 1, 3, 15, 2, 4]' not in data:raise ValueError('Full image hash/identity/pin map mismatch')
  stage=m.get('startup_image',{});small=(a/image_name(startup=True)).read_bytes()
  if stage.get('mode')!='esc-startup-stage' or stage.get('version')!=STAGE or stage.get('hardware')!='radiomaster.rx_2400.er6g' or stage.get('peer_version')!=VERSION or stage.get('writes') is not False or stage.get('rf') is not False or (stage.get('gpio'),stage.get('uart'),stage.get('baud'))!=(2,2,800):raise ValueError('Unreviewed small-stage capabilities')
  if set(stage.get('sources',{}))!=set(BASE['STARTUP_SOURCES']):raise ValueError('Small stage source inventory changed')
  for n,h in stage['sources'].items():
   if sha((root/n).read_bytes())!=h:raise ValueError('Small stage source changed: '+n)
  if len(small)>400000 or sha(small)!=stage.get('sha256') or BASE['app_digest'](small)!=stage.get('app_sha256') or bytes.fromhex(stage['app_sha256']) not in data or b'\xbe\xef\xca\xfeUNIFIED_ESP32_2400_RX\0R1-ER6G-STARTUP-r8' not in small:raise ValueError('Small stage identity or embedded hash mismatch')
  b,old,saved=snapshot(root,m['backup'],BACKUP)
 if live:
  current=json.loads(read_url('/config'));hardware=json.loads(read_url('/hardware.json'));settings=current['settings']
  expected=VERSION if startup else PREVIOUS_VERSION
  if settings.get('version')!=expected or settings.get('target')!=BASE['EXPECTED_TARGET'] or settings.get('git-commit')!='a9d4a9' or 'ER6-G' not in settings.get('product_name','') or settings.get('radio-type')!='SX128X' or settings.get('reg_domain_high')!='ISM2G4':raise ValueError('Wrong live ER6G target/version/domain')
  if hardware.get('pwm_outputs')!=[14,1,3,15,2,4] or current['config']!=old['config']:raise ValueError('Live settings/binding/pin map changed')
  diag=current.get('esc-listener','');expected_peer=0 if startup else 1
  if 'mode=drive ' not in diag:raise ValueError('Receiver must be in Drive with no pending programming boot')
  if field(diag,'starterPeerReady')!=expected_peer or field(diag,'starterArmed')!=0 or {field(diag,'starterRunningSlot'),field(diag,'starterOtherSlot')}!={0x10000,0x1f0000} or field(diag,'starterOtherSize')!=0x1e0000:raise ValueError('Unreviewed live paired-bank state')
  live_data=read_url('/firmware.bin')
  if sha(live_data)!=saved['backup_sha256']['original-firmware.bin']:raise ValueError('Live firmware changed since backup')
  if startup:
   expected_data=(full/image_name()).read_bytes()
   if live_data[:len(expected_data)]!=expected_data or 'mode=drive ' not in diag or 'txStarted=0' not in diag:raise ValueError('Exact full r14 Drive recovery peer not verified')
 return m

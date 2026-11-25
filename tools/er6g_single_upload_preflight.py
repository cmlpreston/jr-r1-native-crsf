"""Exact proven r12/r7 pair -> single-app r13 trial, normal app OTA only."""
import json,pathlib,runpy
ROOT=pathlib.Path(__file__).resolve().parents[1]
MODE=runpy.run_path(str(ROOT/'tools/er6g_mode_upload_preflight.py'))
BASE=MODE['BASE'];HOST=MODE['HOST'];sha=MODE['sha']
VERSION='4.1.0-er6g-esc-single-r13'
BACKUP='tmp/er6g-private-backup-2026-11-10-mode-r12-paired'
COMMON_READ_SOURCES=('receiver/er6g/reader/Er6gEscRead.cpp','receiver/er6g/reader/Er6gEscRead.h','receiver/er6g/reader/Er6gEscStartupPeer.cpp','receiver/er6g/reader/Er6gEscStartupPeer.h','include/er6g_esc_startup.h','include/surpass_protocol.h')
SOURCES=BASE['SOURCES']+BASE['LISTEN_SOURCES']+COMMON_READ_SOURCES+MODE['MODE_SOURCES']+('receiver/er6g/reader/Er6gEscSingleStartup.cpp',)
read_url=MODE['read_url']
def image_name(*args,**kwargs):return 'er6g-esc-drive-mode.bin'
def validate(root,project,board,artifacts,live=True,**kwargs):
 root=pathlib.Path(root).resolve();a=pathlib.Path(artifacts).resolve()
 if pathlib.Path(project).resolve()!=root/'receiver/er6g/uploader' or board!='esp32dev':raise ValueError('Single-app project/board mismatch')
 m=json.loads((a/'manifest.json').read_text())
 if m.get('mode')!='esc-drive-mode' or m.get('version')!=VERSION or m.get('startup_strategy')!='single-app' or 'startup_image' in m or m.get('writes') is not False or m.get('programming_tx') is not True:raise ValueError('Only reviewed single-app r13/no-write trial allowed')
 if m.get('hardware')!='radiomaster.rx_2400.er6g' or m.get('upstream')!='a9d4a9cb5b5687c4c9d7e9e7fbdf44ad93651da6' or m.get('base_rx_main_sha256')!=BASE['BASE_SHA'] or m.get('patch_sha256')!=BASE['PATCH_SHA'] or sha((root/'receiver/elrs/teamrace-telemetry-slot.patch').read_bytes())!=BASE['PATCH_SHA']:raise ValueError('Unreviewed base/hardware/patch')
 if set(m.get('sources',{}))!=set(SOURCES):raise ValueError('Single-app source inventory changed')
 for n in SOURCES:
  if sha((root/n).read_bytes())!=m['sources'][n]:raise ValueError('Source changed after build: '+n)
 data=(a/image_name()).read_bytes()
 if sha(data)!=m.get('sha256') or data[:1]!=b'\xe9' or VERSION.encode() not in data or b'\xbe\xef\xca\xfeUNIFIED_ESP32_2400_RX\0' not in data or b'RadioMaster ER6-G' not in data or b'"pwm_outputs": [14, 1, 3, 15, 2, 4]' not in data or b'startupStrategy=single-app' not in data:raise ValueError('Single-app image hash/identity/pin map mismatch')
 b,old,saved=MODE['snapshot'](root,m['backup'],BACKUP)
 if live:
  c=json.loads(read_url('/config'));h=json.loads(read_url('/hardware.json'));s=c['settings'];d=c.get('esc-listener','')
  if s.get('version')!='4.1.0-er6g-esc-mode-r12' or s.get('target')!=BASE['EXPECTED_TARGET'] or s.get('git-commit')!='a9d4a9' or 'ER6-G' not in s.get('product_name','') or s.get('radio-type')!='SX128X' or s.get('reg_domain_high')!='ISM2G4':raise ValueError('Wrong live ER6G version/radio/domain')
  if h.get('pwm_outputs')!=[14,1,3,15,2,4] or c['config']!=old['config']:raise ValueError('Live binding/config/pins changed')
  if 'mode=drive ' not in d or 'txStarted=0' not in d or MODE['field'](d,'starterPeerReady')!=1 or MODE['field'](d,'starterArmed')!=0 or {MODE['field'](d,'starterRunningSlot'),MODE['field'](d,'starterOtherSlot')}!={0x10000,0x1f0000} or MODE['field'](d,'starterOtherSize')!=0x1e0000:raise ValueError('Require proven r12/r7 Drive pair with no pending ticket')
  if sha(read_url('/firmware.bin'))!=saved['backup_sha256']['original-firmware.bin']:raise ValueError('Live firmware changed since recovery snapshot')
 return m

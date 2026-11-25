"""User-authorized exact r13 -> proven r12/r7 restoration, 11 November 2026."""
import pathlib,json,runpy
ROOT=pathlib.Path(__file__).resolve().parents[1]
MODE=runpy.run_path(str(ROOT/'tools/er6g_mode_upload_preflight.py'))
HOST=MODE['HOST'];sha=MODE['sha'];read_url=MODE['read_url']
HISTORY='tmp/er6g-private-deploy-2026-11-10-mode-r12'
ARCHIVE='tmp/er6g-r12-source-baseline-2026-11-10'
FULL_SHA='3917ed7b854765135c8f54e4d5676a67deaeae7f42e88cd4d30442136c979484'
STAGE_SHA='1d9a79830fe318e83edc75729414d38b23c7524f289beec7aa315684f3008430'
MANIFEST_SHA='d38396cb558a3e37e0de206c6e4f7294c5d328ac3c0f1c60da0643f50ea5c161'
BACKUP='tmp/er6g-private-backup-2026-11-11-single-r13'
FULL_BACKUP='tmp/er6g-private-backup-2026-11-11-restored-r12'
def image_name(*args,startup=False,**kwargs):return MODE['image_name'](startup=startup)
def validate(root,project,board,artifacts,live=True,startup=False):
 root=pathlib.Path(root).resolve();a=pathlib.Path(artifacts).resolve()
 if pathlib.Path(project).resolve()!=root/'receiver/er6g/uploader' or board!='esp32dev':raise ValueError('Restore project/board mismatch')
 m=json.loads((a/'manifest.json').read_text())
 if m!={'mode':'exact-r12-r7-restore','recovery':BACKUP,'full_recovery':FULL_BACKUP,'approved_manifest_sha256':MANIFEST_SHA}:raise ValueError('Unreviewed restore metadata')
 historic=root/HISTORY
 if sha((historic/'manifest.json').read_bytes())!=MANIFEST_SHA:raise ValueError('Approved historical manifest changed')
 original=json.loads((historic/'manifest.json').read_text())
 for n,h in original['sources'].items():
  f=root/ARCHIVE/n
  if not f.exists():f=root/n
  if sha(f.read_bytes())!=h:raise ValueError('Historical source proof changed: '+n)
 full=(a/image_name()).read_bytes();small=(a/image_name(startup=True)).read_bytes()
 if sha(full)!=FULL_SHA or sha(small)!=STAGE_SHA or sha((historic/image_name()).read_bytes())!=FULL_SHA or sha((historic/image_name(startup=True)).read_bytes())!=STAGE_SHA:raise ValueError('Restore must use exact proven r12/r7 bytes')
 if original.get('version')!='4.1.0-er6g-esc-mode-r12' or original.get('writes') is not False or original['startup_image']['version']!='er6g-startup-r7' or bytes.fromhex(original['startup_image']['app_sha256']) not in full:raise ValueError('Wrong proven pair identity')
 b,old,saved=MODE['snapshot'](root,FULL_BACKUP if startup else BACKUP,FULL_BACKUP if startup else BACKUP)
 if live:
  c=json.loads(read_url('/config'));h=json.loads(read_url('/hardware.json'));s=c['settings'];d=c.get('esc-listener','')
  expected='4.1.0-er6g-esc-mode-r12' if startup else '4.1.0-er6g-esc-single-r13'
  if s.get('version')!=expected or s.get('target')!=MODE['BASE']['EXPECTED_TARGET'] or s.get('git-commit')!='a9d4a9' or 'ER6-G' not in s.get('product_name','') or s.get('radio-type')!='SX128X' or s.get('reg_domain_high')!='ISM2G4':raise ValueError('Wrong live ER6G target/version/domain')
  if c['config']!=old['config'] or h!=json.loads((b/'hardware.json').read_bytes()) or h.get('pwm_outputs')!=[14,1,3,15,2,4]:raise ValueError('Live binding/config/pins changed')
  if 'mode=drive ' not in d or 'txStarted=0' not in d or MODE['field'](d,'starterArmed')!=0 or MODE['field'](d,'starterPeerReady')!=0:raise ValueError('Require idle Drive with no armed stage')
  if startup:
   if {MODE['field'](d,'starterRunningSlot'),MODE['field'](d,'starterOtherSlot')}!={0x10000,0x1f0000} or MODE['field'](d,'starterOtherSize')!=0x1e0000:raise ValueError('Wrong OTA bank layout')
  elif 'startupStrategy=single-app' not in d or MODE['field'](d,'singlePending') or MODE['field'](d,'singleEntryRequested'):raise ValueError('Single-app request must be clear')
  data=read_url('/firmware.bin')
  if sha(data)!=saved['backup_sha256']['original-firmware.bin']:raise ValueError('Live image changed after snapshot')
  if startup and data[:len(full)]!=full:raise ValueError('Exact restored full app not verified')
 return m

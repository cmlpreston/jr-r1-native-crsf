#!/usr/bin/env python3
"""Prepare a private ER6G candidate preserving UID/Wi-Fi. Never uploads."""
import argparse,pathlib,sys,os,json,secrets,shutil,hashlib
root=pathlib.Path(__file__).resolve().parents[2]
if pathlib.Path(sys.prefix).absolute()!=(root/'.venv').absolute():os.execv(str(root/'.venv/bin/python'),[str(root/'.venv/bin/python'),str(pathlib.Path(__file__).resolve())]+sys.argv[1:])
p=argparse.ArgumentParser();p.add_argument('--build',type=pathlib.Path,required=True);p.add_argument('--backup',type=pathlib.Path,required=True);p.add_argument('--destination',type=pathlib.Path,required=True);a=p.parse_args();a.build=a.build.resolve();a.backup=a.backup.resolve();a.destination=a.destination.resolve()
if a.destination.exists():raise SystemExit('Choose a new private destination')
m=json.loads((a.build/'artifacts/manifest.json').read_text());mode=m.get('mode');listen=mode in ('esc-listen-only','esc-read-only','esc-drive-mode');read=mode in ('esc-read-only','esc-drive-mode')
c=json.loads((a.backup/'config.json').read_text());options=json.loads((a.backup/'options.json').read_text());uid=c['config']['uid']
if len(uid)!=6 or any(type(v)!=int or not 0<=v<=255 for v in uid):raise SystemExit('Invalid saved UID')
options.update({'uid':uid,'flash-discriminator':secrets.randbits(32),'is-airport':False})
a.destination.mkdir(mode=0o700);image=a.destination/('er6g-esc-drive-mode.bin' if mode=='esc-drive-mode' else 'er6g-esc-read-only.bin' if read else 'er6g-esc-listen-only.bin' if listen else 'er6g-stock-teamrace-slot-fix.bin');shutil.copy2(a.build/'artifacts'/image.name,image);image.chmod(0o600)
sys.path.insert(0,str(a.build/'src/python'));import UnifiedConfiguration
os.chdir(a.build/'src')
with image.open('r+b') as f:UnifiedConfiguration.doConfiguration(f,json.dumps(options),'radiomaster.rx_2400.er6g','Unified_ESP32_2400_RX','RM ER6-G',None)
names=('receiver/er6g/build.py','receiver/er6g/deploy.py','receiver/elrs/teamrace_filter.py','receiver/elrs/teamrace-telemetry-slot.patch')+ (('receiver/er6g/listen_prepare.py','receiver/er6g/listener/Er6gEscListen.cpp','receiver/er6g/listener/Er6gEscListen.h') if listen else ())
if read:names+=('receiver/er6g/reader/Er6gEscRead.cpp','receiver/er6g/reader/Er6gEscRead.h','receiver/er6g/reader/Er6gEscStartupPeer.cpp','receiver/er6g/reader/Er6gEscStartupPeer.h','include/er6g_esc_startup.h','include/surpass_protocol.h')+tuple(m.get('startup_image',{}).get('sources',{}))
if m.get('startup_strategy')=='single-app':names+=('receiver/er6g/reader/Er6gEscSingleStartup.cpp',)
if mode=='esc-drive-mode':names+=('receiver/er6g/drive_pwm_prepare.py','include/esc_programmer_wire.h','receiver/er6g/reader/Er6gEscMode.h','receiver/er6g/reader/Er6gEscMode.cpp')
for n in names:
 if n!='receiver/er6g/deploy.py' and m.get('sources',{}).get(n)!=hashlib.sha256((root/n).read_bytes()).hexdigest():raise SystemExit('Firmware source changed since build: '+n)
m=json.loads((a.build/'artifacts/manifest.json').read_text());m.update({'sources':{n:hashlib.sha256((root/n).read_bytes()).hexdigest() for n in names},'sha256':hashlib.sha256(image.read_bytes()).hexdigest(),'binding':'Preserved from fresh private ER6G snapshot','backup':str(a.backup.relative_to(root))})
if read and m.get('startup_image'):
 shutil.copy2(a.build/'artifacts/er6g-startup-stage.bin',a.destination/'er6g-startup-stage.bin')
 (a.destination/'er6g-startup-stage.bin').chmod(0o600)
f=a.destination/'manifest.json';f.write_text(json.dumps(m,indent=2)+'\n');f.chmod(0o600)
print('Private ER6G candidate prepared; UID/Wi-Fi preserved. NO UPLOAD.')

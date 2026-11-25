#!/usr/bin/env python3
"""Build pinned ER6G slot fix or a separate listen-only bench candidate. Never uploads."""
import argparse,pathlib,subprocess,shutil,runpy,hashlib,json,sys,os,secrets
root=pathlib.Path(__file__).resolve().parents[2]
if pathlib.Path(sys.prefix).absolute()!=(root/'.venv').absolute():os.execv(str(root/'.venv/bin/python'),[str(root/'.venv/bin/python'),str(pathlib.Path(__file__).resolve())]+sys.argv[1:])
p=argparse.ArgumentParser();p.add_argument('--upstream',type=pathlib.Path,required=True);p.add_argument('--targets',type=pathlib.Path,required=True);p.add_argument('--destination',type=pathlib.Path,required=True);p.add_argument('--startup-artifacts',type=pathlib.Path);p.add_argument('--single-stage',action='store_true');p.add_argument('--esc-writes',action='store_true');g=p.add_mutually_exclusive_group();g.add_argument('--esc-listen-only',action='store_true');g.add_argument('--esc-read-only',action='store_true');g.add_argument('--esc-drive-mode',action='store_true');a=p.parse_args()
if a.esc_read_only:raise SystemExit('Automatic-entry bench build retired; use --esc-drive-mode')
if a.single_stage and (not a.esc_drive_mode or a.startup_artifacts):raise SystemExit('Single-stage requires --esc-drive-mode without startup artifacts')
if a.esc_writes and (not a.esc_drive_mode or a.single_stage):raise SystemExit('Writes require paired --esc-drive-mode')
a.esc_read_only=a.esc_drive_mode
a.upstream=a.upstream.resolve();a.targets=a.targets.resolve();a.destination=a.destination.resolve()
commit=subprocess.check_output(['git','-C',str(a.upstream),'rev-parse','HEAD'],text=True).strip()
if not commit.startswith('a9d4a9cb') or subprocess.check_output(['git','-C',str(a.upstream),'status','--porcelain'],text=True).strip():raise SystemExit('Require clean pinned official4.1 source')
if a.destination.exists():raise SystemExit('Choose a new disposable directory')
shutil.copytree(a.upstream,a.destination,ignore=shutil.ignore_patterns('.git','.pio','node_modules'))
src=a.destination/'src';base=(src/'src/rx_main.cpp').read_bytes()
runpy.run_path(str(root/'receiver/elrs/teamrace_filter.py'))['apply'](src/'src/rx_main.cpp')
mode='esc-drive-mode' if a.esc_read_only else 'esc-listen-only' if a.esc_listen_only else 'stock-teamrace'
version='4.1.0-er6g-esc-single-r13' if a.single_stage else '4.1.0-er6g-esc-mode-r14' if a.esc_writes else '4.1.0-er6g-esc-mode-r12' if a.esc_read_only else '4.1.0-er6g-esc-listen-r1' if a.esc_listen_only else '4.1.0-teamrace-slot-fix'
startup=None
if a.esc_read_only and not a.single_stage:
 if not a.startup_artifacts:raise SystemExit('Selectable mode requires matching reviewed startup artifacts')
 a.startup_artifacts=a.startup_artifacts.resolve();startup=json.loads((a.startup_artifacts/'manifest.json').read_text())
 image=(a.startup_artifacts/'er6g-startup-stage.bin').read_bytes()
 if startup.get('mode')!='esc-startup-stage' or startup.get('version')!=('er6g-startup-r8' if a.esc_writes else 'er6g-startup-r7') or startup.get('peer_version')!=version or hashlib.sha256(image).hexdigest()!=startup['sha256']:raise SystemExit('Wrong startup artifact')
 for n,digest in startup['sources'].items():
  if hashlib.sha256((root/n).read_bytes()).hexdigest()!=digest:raise SystemExit('Startup source changed after build: '+n)
 if runpy.run_path(str(root/'tools/esp_image.py'))['app_digest'](image)!=startup['app_sha256']:raise SystemExit('Startup app digest changed')
 runpy.run_path(str(root/'receiver/er6g/listen_prepare.py'))['apply_mode'](src,write_enabled=a.esc_writes)
 header=src/'lib/Er6gEscListen/Er6gEscStartupImage.h'
 header.write_text('#pragma once\nconstexpr unsigned char Er6gStartupImageSha[32]={'+','.join('0x'+startup['app_sha256'][i:i+2] for i in range(0,64,2))+'};\n')
elif a.single_stage:
 runpy.run_path(str(root/'receiver/er6g/listen_prepare.py'))['apply_mode'](src,single_stage=True)
elif a.esc_listen_only:runpy.run_path(str(root/'receiver/er6g/listen_prepare.py'))['apply'](src)
(src/'VERSION').write_text(version+' '+commit+'\n')
f=src/'python/elrs_helpers.py';s=f.read_text();anchor='            sha = data.split()[1].strip()';assert s.count(anchor)==1;f.write_text(s.replace(anchor,anchor+'\n            ver = data.split()[0].strip()'))
subprocess.run([sys.executable,str(root/'receiver/elrs/test_telemetry_slot.py'),str(src/'src/rx_main.cpp')],check=True)
if a.esc_read_only:subprocess.run([sys.executable,str(root/'receiver/er6g/test_listen_reservation.py'),str(src/'lib/ServoOutput/devServoOutput.cpp')],check=True)
if a.esc_read_only:subprocess.run([sys.executable,str(root/'receiver/er6g/test_pwm_allocator.py'),str(src/'lib/PWM/PWM_ESP32.cpp')],check=True)
env='Unified_ESP32_2400_RX_via_R1_ESC_MODE' if a.esc_read_only else 'Unified_ESP32_2400_RX_via_UART';subprocess.run([str(root/'.venv/bin/pio'),'run','-d',str(src),'-e',env],check=True)
shutil.copytree(a.targets,src/'hardware',dirs_exist_ok=True,ignore=shutil.ignore_patterns('.git'))
sys.path.insert(0,str(src/'python'));import UnifiedConfiguration
out=a.destination/'artifacts';out.mkdir();firmware=out/('er6g-esc-drive-mode.bin' if a.esc_read_only else 'er6g-esc-listen-only.bin' if a.esc_listen_only else 'er6g-stock-teamrace-slot-fix.bin');shutil.copy2(src/'.pio/build'/env/'firmware.bin',firmware)
os.chdir(src)
with firmware.open('r+b') as f:UnifiedConfiguration.doConfiguration(f,json.dumps({'rcvr-uart-baud':420000,'wifi-on-interval':60,'flash-discriminator':secrets.randbits(32),'is-airport':False}),'radiomaster.rx_2400.er6g','Unified_ESP32_2400_RX','RM ER6-G',None)
names=['receiver/er6g/build.py','receiver/er6g/deploy.py','receiver/elrs/teamrace_filter.py','receiver/elrs/teamrace-telemetry-slot.patch']
if a.esc_listen_only or a.esc_read_only:names+=['receiver/er6g/listen_prepare.py','receiver/er6g/listener/Er6gEscListen.cpp','receiver/er6g/listener/Er6gEscListen.h']
if a.esc_read_only:names+=['receiver/er6g/drive_pwm_prepare.py','receiver/er6g/reader/Er6gEscRead.cpp','receiver/er6g/reader/Er6gEscRead.h','receiver/er6g/reader/Er6gEscStartupPeer.cpp','receiver/er6g/reader/Er6gEscStartupPeer.h','include/er6g_esc_startup.h','include/surpass_protocol.h','include/esc_programmer_wire.h','receiver/er6g/reader/Er6gEscMode.h','receiver/er6g/reader/Er6gEscMode.cpp']+(list(startup['sources']) if startup else ['receiver/er6g/reader/Er6gEscSingleStartup.cpp'])
if startup:shutil.copy2(a.startup_artifacts/'er6g-startup-stage.bin',out/'er6g-startup-stage.bin')
manifest={'mode':mode,'version':version,'programming_tx':a.esc_read_only,'writes':a.esc_writes,'sources':{n:hashlib.sha256((root/n).read_bytes()).hexdigest() for n in names},'upstream':commit,'hardware':'radiomaster.rx_2400.er6g','change':('Selectable Drive PWM / read-only programming UART output5; single receiver, Team Race disabled, plus PR3819' if a.esc_read_only else 'Receive-only output5 GPIO2 bench reservation plus PR3819' if a.esc_listen_only else 'Exact RXdoneISR guard from upstreamPR3819; version metadata only otherwise'),'base_rx_main_sha256':hashlib.sha256(base).hexdigest(),'patch_sha256':hashlib.sha256((root/'receiver/elrs/teamrace-telemetry-slot.patch').read_bytes()).hexdigest(),'sha256':hashlib.sha256(firmware.read_bytes()).hexdigest(),'binding':'Generic build; prepare private preserved-UID image before any authorized upload'}
if startup:manifest['startup_image']=startup
if a.esc_writes:manifest['write_policy']='one-rf-command-ack-readback-no-retry'
if a.single_stage:manifest['startup_strategy']='single-app'
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
subprocess.run(['diff','-u',str(a.upstream/'src/src/rx_main.cpp'),str(src/'src/rx_main.cpp')],stdout=(out/'rx-slot.diff').open('w'),check=False)
print('ER6G '+mode+' artifact prepared; NO UPLOAD:',firmware)

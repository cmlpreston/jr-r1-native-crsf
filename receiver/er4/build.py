#!/usr/bin/env python3
"""Build and package an ER4 RF-only or direct read-only bench image. Never uploads."""
import argparse, hashlib, json, os, pathlib, secrets, shutil, subprocess, sys
root=pathlib.Path(__file__).resolve().parents[2]
# ELRS's configurator imports pyserial even for packaging-only operations.
# Use the same existing interpreter as PlatformIO; never install globally.
if pathlib.Path(sys.prefix).absolute() != (root/'.venv').absolute():
    os.execv(str(root/'.venv/bin/python'),[str(root/'.venv/bin/python'),str(pathlib.Path(__file__).resolve())]+sys.argv[1:])
p=argparse.ArgumentParser();p.add_argument('--upstream',type=pathlib.Path,required=True);p.add_argument('--targets',type=pathlib.Path,required=True);p.add_argument('--destination',type=pathlib.Path,required=True);p.add_argument('--direct-read-only',action='store_true');a=p.parse_args()
a.upstream=a.upstream.resolve();a.targets=a.targets.resolve();a.destination=a.destination.resolve()
root=pathlib.Path(__file__).resolve().parents[2]
source_names=('receiver/elrs/teamrace_filter.py','receiver/elrs/teamrace-telemetry-slot.patch','receiver/er4/Er4EscBridge.cpp','receiver/er4/Er4EscBridge.h','receiver/er4/prepare.py','receiver/er4/build.py','include/esc_programmer_server.h','include/esc_programmer_wire.h','include/surpass_protocol.h')
source_hashes={n:hashlib.sha256((root/n).read_bytes()).hexdigest() for n in source_names}
targets=json.loads((a.targets/'targets.json').read_text())
er4=targets['radiomaster']['rx_2400']['er4']
layout=json.loads((a.targets/'RX'/er4['layout_file']).read_text());layout.update(er4.get('overlay',{}))
if er4['platform']!='esp8285' or layout['pwm_outputs']!=[0,1,3,9]:raise SystemExit('ER4 hardware metadata changed; review the adapter assumptions')
subprocess.run([sys.executable,str(root/'receiver/er4/prepare.py'),'--upstream',str(a.upstream.resolve()),'--destination',str(a.destination.resolve())]+(['--direct-read-only'] if a.direct_read_only else []),check=True)
src=a.destination.resolve()/'src';env='r1_er4_esc_ESP8285_RX_bench'
subprocess.run([str(root/'.venv/bin/pio'),'run','-d',str(src),'-e',env],check=True)
# Use the official hardware metadata and configurator; no invented pin layout.
shutil.copytree(a.targets.resolve(),src/'hardware',dirs_exist_ok=True,ignore=shutil.ignore_patterns('.git'))
sys.path.insert(0,str(src/'python'));import UnifiedConfiguration
out=a.destination.resolve()/'artifacts';out.mkdir();firmware=out/'er4-esc-rf-only-bench.bin'
shutil.copy2(src/'.pio/build'/env/'firmware.bin',firmware)
os.chdir(src)
options={'rcvr-uart-baud':420000,'wifi-on-interval':60,'flash-discriminator':secrets.randbits(32),'is-airport':False}
with firmware.open('r+b') as f:UnifiedConfiguration.doConfiguration(f,json.dumps(options),'radiomaster.rx_2400.er4','Unified_ESP8285_2400_RX','RM ER4 ESC',None)
assert source_hashes=={n:hashlib.sha256((root/n).read_bytes()).hexdigest() for n in source_names}, 'Receiver sources changed during build'
manifest={'sources':source_hashes,'upstream':'a9d4a9cb','hardware':'radiomaster.rx_2400.er4','selector_channel':12,'interface':a.direct_read_only,'writes':False,'diagnostics':True,'version':(src/'VERSION').read_text().split()[0],'binding':'Configure separately; no binding phrase copied','targets_sha256':hashlib.sha256((a.targets.resolve()/'targets.json').read_bytes()).hexdigest(),'sha256':hashlib.sha256(firmware.read_bytes()).hexdigest()}
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('ER4 bench artifact:',firmware)
print('No upload performed. Direct read-only interface:',a.direct_read_only,'; ESC writes disabled.')

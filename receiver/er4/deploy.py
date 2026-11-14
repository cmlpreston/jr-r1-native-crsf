#!/usr/bin/env python3
"""Prepare a private RF-only image preserving this ER4's binding/Wi-Fi. No upload."""
import argparse, gzip, hashlib, json, os, pathlib, secrets, shutil, sys
root=pathlib.Path(__file__).resolve().parents[2]
if pathlib.Path(sys.prefix).absolute()!=(root/'.venv').absolute():os.execv(str(root/'.venv/bin/python'),[str(root/'.venv/bin/python'),str(pathlib.Path(__file__).resolve())]+sys.argv[1:])
p=argparse.ArgumentParser();p.add_argument('--backup',type=pathlib.Path,default=root/'tmp/er4-private-backup-2026-11-05-r2');p.add_argument('--build',type=pathlib.Path,required=True);p.add_argument('--destination',type=pathlib.Path,required=True);a=p.parse_args();a.build=a.build.resolve();a.destination=a.destination.resolve()
if a.destination.exists():raise SystemExit('Choose a new private deployment directory')
backup=a.backup.resolve();config=json.loads((backup/'config.json').read_text());options=json.loads((backup/'options.json').read_text())
uid=config['config']['uid']
if not isinstance(uid,list) or len(uid)!=6 or any(not isinstance(v,int) or not 0<=v<=255 for v in uid):raise SystemExit('Invalid saved ER4 binding UID')
options.update({'uid':uid,'flash-discriminator':secrets.randbits(32),'is-airport':False,'rcvr-uart-baud':420000})
a.destination.mkdir(mode=0o700);firmware=a.destination/'er4-esc-rf-only-bench.bin';shutil.copy2(a.build/'artifacts/er4-esc-rf-only-bench.bin',firmware);firmware.chmod(0o600)
sys.path.insert(0,str(a.build/'src/python'));import UnifiedConfiguration
os.chdir(a.build/'src')
with firmware.open('r+b') as f:UnifiedConfiguration.doConfiguration(f,json.dumps(options),'radiomaster.rx_2400.er4','Unified_ESP8285_2400_RX','RM ER4 ESC',None)
compressed=firmware.with_suffix('.bin.gz');compressed.write_bytes(gzip.compress(firmware.read_bytes(),mtime=0));compressed.chmod(0o600)
shutil.copy2(a.build/'src/elrs.flash.1m.ld',a.destination/'elrs.flash.1m.ld');(a.destination/'elrs.flash.1m.ld').chmod(0o600)
m=json.loads((a.build/'artifacts/manifest.json').read_text());m.update({'ldscript_sha256':hashlib.sha256((a.destination/'elrs.flash.1m.ld').read_bytes()).hexdigest(),'binding':'Preserved from private ER4 backup','sha256':hashlib.sha256(firmware.read_bytes()).hexdigest(),'gzip_sha256':hashlib.sha256(compressed.read_bytes()).hexdigest()})
(a.destination/'manifest.json').write_text(json.dumps(m,indent=2)+'\n');(a.destination/'manifest.json').chmod(0o600)
print('Private ER4 image prepared; binding and Wi-Fi retained; interface='+str(m['interface'])+'; writes OFF.')

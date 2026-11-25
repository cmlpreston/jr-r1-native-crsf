#!/usr/bin/env python3
"""Build/package a small stage. No upload and no bootloader/partition-table change."""
import argparse,pathlib,subprocess,hashlib,json,shutil,sys
ROOT=pathlib.Path(__file__).resolve().parents[3]
NAMES=('receiver/er6g/startup/platformio.ini','receiver/er6g/startup/build_only.py','receiver/er6g/startup/build.py','receiver/er6g/startup/src/main.cpp','include/er6g_esc_startup.h','include/surpass_protocol.h','tools/esp_image.py')
import runpy
app_digest=runpy.run_path(str(ROOT/'tools/esp_image.py'))['app_digest']
def main():
 p=argparse.ArgumentParser();p.add_argument('--destination',type=pathlib.Path,required=True);a=p.parse_args()
 if a.destination.exists():raise SystemExit('Choose a fresh destination')
 subprocess.run([str(ROOT/'.venv/bin/pio'),'run','-d',str(ROOT/'receiver/er6g/startup'),'-e','er6g-startup-build'],check=True)
 source=pathlib.Path('/tmp/r1-er6g-startup-stage-build-2026-11-07/er6g-startup-build/firmware.bin');data=source.read_bytes()
 if len(data)>400000 or b'\xbe\xef\xca\xfeUNIFIED_ESP32_2400_RX\x00R1-ER6G-STARTUP-r8' not in data:raise SystemExit('Unexpected startup image identity/size')
 a.destination.mkdir();shutil.copy2(source,a.destination/'er6g-startup-stage.bin')
 manifest={'mode':'esc-startup-stage','version':'er6g-startup-r8','hardware':'radiomaster.rx_2400.er6g','gpio':2,'uart':2,'baud':800,'writes':False,'rf':False,'peer_version':'4.1.0-er6g-esc-mode-r14','size':len(data),'sha256':hashlib.sha256(data).hexdigest(),'app_sha256':app_digest(data),'sources':{n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest() for n in NAMES}}
 (a.destination/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
 print('Small startup stage packaged; NO UPLOAD:',len(data),'bytes')
if __name__=='__main__':main()

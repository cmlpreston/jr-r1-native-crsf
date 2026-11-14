"""Narrow RF-only and direct read-only ER4 OTA checks. Independent of the XIAO transmitter profile."""
import gzip, hashlib, ipaddress, json, pathlib, subprocess, urllib.request
ENVIRONMENT='er4-esc-rf-bench-wifi'
HOST='192.168.1.130'
EXPECTED_TARGET='R1_ER4_ESC_ESP8285_RX_BENCH'
BACKUP_DIR='tmp/er4-private-backup-2026-11-06-r5'
RECOVERY_DIR='tmp/er4-private-backup-2026-11-05'
SOURCES=('receiver/elrs/teamrace_filter.py','receiver/elrs/teamrace-telemetry-slot.patch','receiver/er4/Er4EscBridge.cpp','receiver/er4/Er4EscBridge.h','receiver/er4/prepare.py','receiver/er4/build.py','include/esc_programmer_server.h','include/esc_programmer_wire.h','include/surpass_protocol.h')
def sha(data):return hashlib.sha256(data).hexdigest()
def read_url(path):
    with urllib.request.urlopen('http://'+HOST+path,timeout=12) as r:return r.read()
def validate(root,project,board,artifact_dir,live=True,direct_read=False):
    root=pathlib.Path(root).resolve();project=pathlib.Path(project).resolve();artifact_dir=pathlib.Path(artifact_dir).resolve()
    if project!=root/'receiver/er4/uploader' or board!='esp8285':raise ValueError('ER4 uploader project/board mismatch')
    if not ipaddress.ip_address(HOST).is_private:raise ValueError('Expected private ER4 address')
    m=json.loads((artifact_dir/'manifest.json').read_text())
    if m.get('hardware')!='radiomaster.rx_2400.er4' or m.get('selector_channel')!=12 or m.get('interface') is not direct_read or m.get('writes') is not False:raise ValueError('ER4 image must match selected RF-only/read-only profile; writes must be OFF')
    if m.get('upstream')!='a9d4a9cb':raise ValueError('Unreviewed receiver source baseline')
    for n in SOURCES:
        if m.get('sources',{}).get(n)!=sha((root/n).read_bytes()):raise ValueError('Receiver source changed since build: '+n)
    image=(artifact_dir/'er4-esc-rf-only-bench.bin').read_bytes()
    compressed=(artifact_dir/'er4-esc-rf-only-bench.bin.gz').read_bytes()
    if sha(image)!=m['sha256'] or sha(compressed)!=m['gzip_sha256'] or gzip.decompress(compressed)!=image:raise ValueError('ER4 image checksum/compression mismatch')
    if direct_read and (m.get('diagnostics') is not True or m.get('version')!='4.1.0-er4-esc-direct-read-r6' or b'4.1.0-er4-esc-direct-read-r6' not in image):raise ValueError('Expected reviewed direct read-only r6 diagnostics image')
    if image[:1]!=b'\xe9' or b'RadioMaster ER4' not in image or b'"pwm_outputs": [0, 1, 3, 9]' not in image:raise ValueError('ER4 hardware identity missing from image')
    if sha((artifact_dir/'elrs.flash.1m.ld').read_bytes())!=m.get('ldscript_sha256'):raise ValueError('ER4 linker/flash-layout provenance mismatch')
    recovery=root/RECOVERY_DIR
    recovery_manifest=json.loads((recovery/'backup-manifest.json').read_text())
    if sha((recovery/'original-firmware.bin').read_bytes())!=recovery_manifest['backup_sha256']['original-firmware.bin']:raise ValueError('Original ER4 recovery image changed')
    backup=root/BACKUP_DIR
    saved=json.loads((backup/'backup-manifest.json').read_text())
    if sha((backup/'original-firmware.bin').read_bytes())!=saved['backup_sha256']['original-firmware.bin']:raise ValueError('Original ER4 recovery image changed')
    old=json.loads((backup/'config.json').read_text())
    if live:
        current=json.loads(read_url('/config'));hardware=json.loads(read_url('/hardware.json'))
        if current.get('settings',{}).get('target')!=EXPECTED_TARGET or 'ER4' not in current.get('settings',{}).get('product_name',''):raise ValueError('Live device is not the expected installed RF-only ER4 receiver')
        if hardware.get('pwm_outputs')!=[0,1,3,9]:raise ValueError('Live ER4 pin layout mismatch')
        for key in ('uid','modelid'):
            if current['config'].get(key)!=old['config'].get(key):raise ValueError('ER4 binding/model identity changed since backup')
        if sha(read_url('/firmware.bin'))!=saved['backup_sha256']['original-firmware.bin']:raise ValueError('Live ER4 firmware changed since backup; review before upload')
    return m

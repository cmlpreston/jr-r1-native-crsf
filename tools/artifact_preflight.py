"""Verify Mac-tested upload artifacts on a source-free Pi USB flash host."""
import hashlib
import json
from pathlib import Path
import re


def verify(root, environment, board, targets, manifest_name):
    root = Path(root)
    if set(targets) != {'nobuild', 'upload'}:
        raise ValueError('Artifact uploads must use nobuild + upload; building on the Pi is forbidden')
    if environment != 'seeed_xiao_esp32s3-pcb' or board != 'seeed_xiao_esp32s3':
        raise ValueError('Artifacts are only approved for the installed XIAO landscape profile')
    manifest = json.loads((root / manifest_name).read_text())
    if manifest.get('environment') != environment or manifest.get('source_checks_passed') is not True:
        raise ValueError('Missing matching Mac source/build/test attestation')
    if not manifest.get('branch') or not re.fullmatch('[0-9a-f]{40}', manifest.get('commit', '')):
        raise ValueError('Missing Mac source identity')
    for key in ('source_snapshot_sha256', 'boot_app0_sha256'):
        if not re.fullmatch('[0-9a-f]{64}', manifest.get(key, '')):
            raise ValueError('Missing source snapshot or bootloader checksum')
    required = {f'.pio/build/{environment}/{name}' for name in ('firmware.bin','bootloader.bin','partitions.bin')}
    required.update({'platformio.ini','include/display_profile.h','tools/upload_preflight.py','tools/artifact_preflight.py','tools/flash_via_pi.py'})
    if set(manifest.get('sha256', {})) != required:
        raise ValueError('Unexpected or missing artifact files')
    for name, expected in manifest['sha256'].items():
        if hashlib.sha256((root / name).read_bytes()).hexdigest() != expected:
            raise ValueError('Artifact checksum mismatch: ' + name)
    # Reuse the same board/profile/pin/config validation as on the Mac.
    import runpy
    runpy.run_path(str(root / 'tools/flash_via_pi.py'))['validate_hardware'](root, environment)
    boot = Path.home()/'.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin'
    if hashlib.sha256(boot.read_bytes()).hexdigest() != manifest['boot_app0_sha256']:
        raise ValueError('Pi framework boot_app0 differs from the tested Mac version')
    return manifest

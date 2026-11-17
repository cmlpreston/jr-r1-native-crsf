#!/usr/bin/env python3
"""Package CI-tested landscape binaries; never connect to or flash hardware."""
import json
from pathlib import Path
import subprocess
import tarfile

from flash_via_pi import ROOT, digest, validate_hardware, verify_manifest


def main():
    environment = 'seeed_xiao_esp32s3-pcb'
    validate_hardware(ROOT, environment)
    # A Git archive binds this package to committed source, excluding local
    # preferences, credentials, private migration files and generated caches.
    if subprocess.check_output(['git', 'status', '--porcelain', '--untracked-files=no'], cwd=ROOT):
        raise ValueError('Commit source changes before packaging CI firmware')
    output = ROOT / 'ci-output'
    output.mkdir(exist_ok=True)
    snapshot = output / 'source.tar.gz'
    subprocess.run(['git', 'archive', '--format=tar.gz', '-o', str(snapshot), 'HEAD'],
                   cwd=ROOT, check=True)
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    branch = subprocess.check_output(['git', 'branch', '--show-current'], cwd=ROOT, text=True).strip()
    if not branch:
        raise ValueError('CI must check out a named branch')
    files = [f'.pio/build/{environment}/{name}' for name in
             ('firmware.bin', 'bootloader.bin', 'partitions.bin')]
    files += ['platformio.ini', 'include/display_profile.h', 'tools/upload_preflight.py',
              'tools/artifact_preflight.py', 'tools/flash_via_pi.py']
    boot = Path.home() / '.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin'
    manifest = {
        'branch': branch, 'commit': commit, 'environment': environment,
        'source_checks_passed': True,
        'ui_audit_run': False,
        'source_snapshot_sha256': digest(snapshot), 'boot_app0_sha256': digest(boot),
        'checks': ['landscape firmware build', 'complete host test suite'],
        'ui_audit_passed': False,
        'sha256': {name: digest(ROOT / name) for name in files},
    }
    manifest_path = output / 'r1-upload-manifest.json'
    manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
    verify_manifest(ROOT, manifest)
    with tarfile.open(output / 'r1-upload.tar.gz', 'w:gz') as archive:
        for name in files:
            archive.add(ROOT / name, arcname=name, recursive=False)
        archive.add(manifest_path, arcname=manifest_path.name)
    print(f'Packaged {branch} at {commit}; hardware upload requires a separate safety review.')


if __name__ == '__main__':
    main()

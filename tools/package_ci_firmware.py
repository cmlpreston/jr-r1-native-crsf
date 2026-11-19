#!/usr/bin/env python3
"""Package CI-tested landscape binaries; never connect to or flash hardware."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tarfile

from artifact_preflight import upload_files
from flash_via_pi import ROOT, digest, validate_hardware, verify_manifest

UI_CHECKS = (
    ('JR_R1_SHARED_UI_CHECK', 'shared UI and layout',
     'full simulator UI audit: navigation, layout and runtime rotation'),
    ('JR_R1_RUN_RECORDS_UI_CHECK', 'run records',
     'run records focused UI audit: persistence, reset, model isolation and both orientations'),
    ('JR_R1_SENSOR_HISTORY_UI_CHECK', 'sensor history',
     'sensor history focused UI audit: all sizes, landscape and portrait'),
    ('JR_R1_ERS_DISCOVERY_UI_CHECK', 'ERS discovery',
     'ERS discovery focused UI audit: queued four-sensor burst, settings and CUT, landscape and portrait'),
    ('JR_R1_COLOUR_SCHEMES_UI_CHECK', 'light colour schemes',
     'light colour schemes UI audit: persistence, Night toggle, picker bounds and both orientations'),
)


def main(expected_commit, expected_branch):
    environment = 'seeed_xiao_esp32s3-pcb'
    validate_hardware(ROOT, environment)
    # A Git archive binds this package to committed source, excluding local
    # preferences, credentials, private migration files and generated caches.
    if subprocess.check_output(['git', 'status', '--porcelain', '--untracked-files=no'], cwd=ROOT):
        raise ValueError('Commit source changes before packaging CI firmware')
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    if commit != expected_commit:
        raise ValueError(f'CI source commit {commit} differs from event commit {expected_commit}')
    branch = subprocess.check_output(['git', 'branch', '--show-current'], cwd=ROOT, text=True).strip()
    if not branch or branch != expected_branch:
        raise ValueError(f'CI source branch {branch!r} differs from event branch {expected_branch!r}')

    output = ROOT / 'ci-output'
    output.mkdir(exist_ok=True)
    snapshot = output / 'source.tar.gz'
    subprocess.run(['git', 'archive', '--format=tar.gz', '-o', str(snapshot), commit],
                   cwd=ROOT, check=True)
    files = upload_files(environment)
    boot = Path.home() / '.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin'
    ui_checks = [(name, detail) for key, name, detail in UI_CHECKS
                 if os.environ.get(key) == 'success']
    manifest = {
        'branch': branch, 'commit': commit, 'environment': environment,
        'source_checks_passed': True,
        'ui_audit_run': bool(ui_checks),
        'source_snapshot_sha256': digest(snapshot), 'boot_app0_sha256': digest(boot),
        'checks': ['landscape firmware build', 'complete host test suite',
                   *(detail for _, detail in ui_checks)],
        'ui_audit_passed': bool(ui_checks),
        'sha256': {name: digest(ROOT / name) for name in files},
    }
    if ui_checks:
        manifest['ui_audit_scope'] = '; '.join(name for name, _ in ui_checks)
    manifest_path = output / 'r1-upload-manifest.json'
    manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
    verify_manifest(ROOT, manifest)
    with tarfile.open(output / 'r1-upload.tar.gz', 'w:gz') as archive:
        for name in files:
            archive.add(ROOT / name, arcname=name, recursive=False)
        archive.add(manifest_path, arcname=manifest_path.name)
    print(f'Packaged {branch} at {commit}; hardware upload requires a separate safety review.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--commit', required=True, help='GitHub event commit SHA')
    parser.add_argument('--branch', required=True, help='GitHub event branch name')
    args = parser.parse_args()
    main(args.commit, args.branch)

#!/usr/bin/env python3
"""Verify a GitHub landscape build and optionally upload it to the installed R1."""
import argparse
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tarfile
import tempfile
import time

from artifact_preflight import SOURCE_FILES, upload_files, verify
from flash_via_pi import ROOT, USB_PORT, digest

REPOSITORY = 'cmlpreston/jr-r1-native-crsf'
WORKFLOW = 'R1 firmware and tests'
ENVIRONMENT = 'seeed_xiao_esp32s3-pcb'
BOARD = 'seeed_xiao_esp32s3'
MANIFEST = 'r1-upload-manifest.json'
FIRMWARE = f'.pio/build/{ENVIRONMENT}/firmware.bin'


def command(*args, capture=False, env=None):
    return subprocess.run(args, cwd=ROOT, env=env, check=True,
                          stdout=subprocess.PIPE if capture else None).stdout


def source_identity():
    branch = command('git', 'branch', '--show-current', capture=True).decode().strip()
    if not branch or command('git', 'status', '--porcelain', '--untracked-files=no', capture=True):
        raise ValueError('Use a named branch with all tracked changes committed')
    commit = command('git', 'rev-parse', 'HEAD', capture=True).decode().strip()
    return branch, commit


def verify_run(run, branch, commit):
    if run.get('status') != 'completed' or run.get('conclusion') != 'success':
        raise ValueError('GitHub build must have completed successfully')
    if (run.get('headSha'), run.get('headBranch'), run.get('workflowName')) != (commit, branch, WORKFLOW):
        raise ValueError('GitHub run does not match the current branch, commit and firmware workflow')


def unpack_verified(artifact, extracted, branch, commit):
    manifest_bytes = (artifact / MANIFEST).read_bytes()
    manifest = json.loads(manifest_bytes)
    if (manifest.get('branch'), manifest.get('commit')) != (branch, commit):
        raise ValueError('Artifact source identity differs from the GitHub run')
    expected = {*upload_files(ENVIRONMENT), MANIFEST}
    with tarfile.open(artifact / 'r1-upload.tar.gz', 'r:gz') as archive:
        entries = archive.getmembers()
        if (len(entries) != len(expected) or {entry.name for entry in entries} != expected
                or any(not entry.isfile() or entry.size > 8 * 1024 * 1024 for entry in entries)):
            raise ValueError('Unexpected archive members or file sizes')
        for entry in entries:
            dest = extracted / entry.name
            dest.parent.mkdir(parents=True, exist_ok=True)
            with archive.extractfile(entry) as source, dest.open('xb') as output:
                shutil.copyfileobj(source, output)
    if (extracted / MANIFEST).read_bytes() != manifest_bytes:
        raise ValueError('Standalone and packaged manifests differ')
    for name in SOURCE_FILES:
        if (extracted / name).read_bytes() != command('git', 'show', f'{commit}:{name}', capture=True):
            raise ValueError(f'Packaged {name} differs from source at {commit}')
    verify(extracted, ENVIRONMENT, BOARD, ['nobuild', 'upload'], MANIFEST)
    image = (extracted / FIRMWARE).read_bytes()
    if commit[:8].encode() not in image or branch.encode() not in image:
        raise ValueError('Firmware image does not contain the expected build identity')
    return manifest


def check_usb():
    port = Path(USB_PORT)
    if not port.exists() or not stat.S_ISCHR(port.stat().st_mode):
        raise ValueError('Connect the installed R1 to the Pi by USB')
    identity = command('udevadm', 'info', '--query=property', f'--name={port}', capture=True).decode()
    if not {'ID_VENDOR_ID=303a', 'ID_MODEL_ID=1001', 'ID_SERIAL_SHORT=90:70:69:11:DE:70'} <= set(identity.splitlines()):
        raise ValueError('USB identity does not match the installed R1')
    busy = subprocess.run(['fuser', '-s', USB_PORT]).returncode
    if busy == 0:
        raise ValueError('Close the Pi serial monitor before uploading')
    if busy != 1:
        raise ValueError('Could not verify that the Pi serial port is idle')


def upload(extracted, branch, commit):
    command('git', 'fetch', 'origin', 'main')
    command('git', 'status', '--short', '--branch')
    if source_identity() != (branch, commit):
        raise ValueError('Source changed while downloading the firmware')
    if subprocess.run(['git', 'merge-base', '--is-ancestor', 'origin/main', commit], cwd=ROOT).returncode:
        raise ValueError('The tested commit must contain freshly fetched origin/main')
    upload_root = ROOT / '.pio/cloud-upload'
    pio = ROOT / '.venv/bin/pio'
    if not upload_root.is_dir() or not pio.is_file():
        raise ValueError('The persistent Pi upload workspace and PlatformIO must already be installed')
    check_usb()
    for name in (*upload_files(ENVIRONMENT), MANIFEST):
        dest = upload_root / name
        dest.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(dir=dest.parent, delete=False) as pending:
            pending_path = Path(pending.name)
            try:
                with (extracted / name).open('rb') as source:
                    shutil.copyfileobj(source, pending)
                pending.close()
                pending_path.replace(dest)
            finally:
                pending_path.unlink(missing_ok=True)
    verify(upload_root, ENVIRONMENT, BOARD, ['nobuild', 'upload'], MANIFEST)
    check_usb()
    print(f'Uploading {branch} / {commit[:8]} with {ENVIRONMENT}', flush=True)
    print('XIAO ESP32-S3, Waveshare LCD (F), 480×320 landscape, schematic (5)', flush=True)
    env = {key: value for key, value in os.environ.items()
           if not key.startswith('PLATFORMIO_') and key != 'JR_R1_ALLOW_UNSAFE_UPLOAD'}
    env['JR_R1_ARTIFACT_MANIFEST'] = MANIFEST
    started = time.monotonic()
    command(str(pio), 'run', '--project-dir', str(upload_root), '--disable-auto-clean',
            '-e', ENVIRONMENT, '-j', '1', '-t', 'nobuild', '-t', 'upload',
            '--upload-port', USB_PORT, env=env)
    print(f'Upload verified in {time.monotonic() - started:.2f}s', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_id', type=int, help='Completed R1 firmware and tests run ID for the current commit')
    parser.add_argument('--flash', action='store_true', help='Upload the verified image to the connected R1')
    args = parser.parse_args()
    if args.run_id < 1:
        parser.error('run_id must be positive')
    branch, commit = source_identity()
    run_id = str(args.run_id)
    run = json.loads(command('gh', 'run', 'view', run_id, '--repo', REPOSITORY, '--json',
                             'status,conclusion,headSha,headBranch,workflowName', capture=True))
    verify_run(run, branch, commit)
    with tempfile.TemporaryDirectory(prefix='r1-cloud-') as work:
        artifact = Path(work) / 'download'
        extracted = Path(work) / 'verified'
        started = time.monotonic()
        command('gh', 'run', 'download', run_id, '--repo', REPOSITORY,
                '--name', f'r1-landscape-{commit}', '--dir', str(artifact))
        print(f'Downloaded firmware artifact in {time.monotonic() - started:.2f}s', flush=True)
        manifest = unpack_verified(artifact, extracted, branch, commit)
        print(f'Verified {branch} / {commit}, firmware SHA256 {digest(extracted / FIRMWARE)}', flush=True)
        print(f"UI audits: {manifest.get('ui_audit_scope', 'none selected')}", flush=True)
        if args.flash:
            upload(extracted, branch, commit)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, tarfile.TarError, subprocess.CalledProcessError) as error:
        sys.exit(f'R1 operation stopped: {error}')

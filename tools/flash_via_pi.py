#!/usr/bin/env python3
"""Build/test locally, copy verified binaries to Pi3, upload using PlatformIO."""
import argparse
import configparser
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ENVIRONMENTS = ('seeed_xiao_esp32s3-pcb', 'seeed_xiao_esp32s3-pcb-portrait')
USB_PORT = '/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_90:70:69:11:DE:70-if00'


def run(command, *, cwd=ROOT, capture=False, env=None):
    print('+', shlex.join(map(str, command)), flush=True)
    return subprocess.run(command, cwd=cwd, check=True, text=True,
                          stdout=subprocess.PIPE if capture else None, env=env).stdout


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def validate_hardware(root, environment):
    if environment not in ENVIRONMENTS:
        raise ValueError('Only the installed XIAO/Waveshare PCB environments are allowed')
    config = configparser.ConfigParser(interpolation=None)
    config.read(root / 'platformio.ini')
    section = config['env:' + environment]
    board = section.get('board')
    if board is None:
        board = config[section['extends']]['board']
    if board != 'seeed_xiao_esp32s3':
        raise ValueError('Unexpected board: ' + board)
    profile = (root / 'include/display_profile.h').read_text()
    if not re.search(r'^#define DISPLAY_PROFILE DISPLAY_PROFILE_WAVESHARE_RPI_35F$', profile, re.M):
        raise ValueError('Default display must be Waveshare LCD (F)')
    pcb = profile.split('#ifdef JR_R1_XIAO_PCB', 1)[1].split('#else', 1)[0]
    pins = {'TFT_DC_PIN': 4, 'TFT_SCK_PIN': 5, 'TFT_MOSI_PIN': 6,
            'TFT_MISO_PIN': -1, 'TFT_RESET_PIN': -1, 'TFT_BACKLIGHT_PIN': 3,
            'TOUCH_SDA_PIN': 1, 'TOUCH_SCL_PIN': 2, 'TOUCH_IRQ_PIN': -1,
            'TOUCH_RESET_PIN': -1}
    for name, value in pins.items():
        if not re.search(r'constexpr int ' + name + r'\s*=\s*' + str(value) + r'\s*;', pcb):
            raise ValueError('Schematic (5) pin allocation changed: ' + name)
    if not re.search(r'constexpr int TFT_CS_PIN\s*=\s*-1\s*;', profile):
        raise ValueError('Expected grounded LCD CS')
    def setting(section_name, key):
        node=config[section_name]
        value=node.get(key)
        if value is None:
            return setting(node['extends'], key) if 'extends' in node else ''
        def expand(match):
            parent,field=match.group(1).rsplit('.',1)
            return setting(parent,field)
        return re.sub(r'\$\{([^}]+)\}',expand,value)
    flags = setting('env:' + environment, 'build_flags')
    if re.search(r'(?:DISPLAY_PROFILE|JR_R1_INPUT_BENCH|JR_R1_DISPLAY_DIAGNOSTIC)\s*=\s*[1-9]', flags):
        raise ValueError('Diagnostic/display override found; review hardware configuration first')
    if '-D JR_R1_XIAO_PCB=1' not in config['env:seeed_xiao_esp32s3-pcb'].get('build_flags', ''):
        raise ValueError('XIAO PCB profile is missing')
    if 'pre:tools/upload_preflight.py' not in config['env:esp32-s3-devkitc-1'].get('extra_scripts', ''):
        raise ValueError('Upload preflight is missing')


def verify_manifest(root, manifest):
    for filename, expected in manifest['sha256'].items():
        if digest(root / filename) != expected:
            raise ValueError('Checksum mismatch: ' + filename)


def remote_script(args):
    q = shlex.quote
    # The manifest and boot_app0 are independently verified on the Pi before
    # invoking the uploader. Nothing builds on the Pi and no serial process is killed.
    return '\n'.join([
        'set -eu',
        'unset JR_R1_ALLOW_UNSAFE_UPLOAD',
        'cd ' + q(args.remote_dir),
        'tar -xzf ' + q(args.remote_dir + '/r1-upload.tar.gz'),
        'git fetch origin main',
        'git status --short --branch',
        'git merge-base --is-ancestor origin/main HEAD',
        "python3 - <<'VERIFY'",
        'import hashlib,json,pathlib,subprocess',
        'm=json.load(open("r1-upload-manifest.json"))',
        'assert subprocess.check_output(["git","rev-parse","HEAD"]).decode().strip()==m["commit"], "Pi Git commit differs from tested source"',
        'for p,h in m["sha256"].items():',
        '    assert hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()==h, "Checksum mismatch: "+p',
        'boot=pathlib.Path.home()/".platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"',
        'assert hashlib.sha256(boot.read_bytes()).hexdigest()==m["boot_app0_sha256"], "boot_app0 differs between machines"',
        'print("Pi artifacts, boot_app0 and Git identity verified")',
        'VERIFY',
        'test -e ' + q(args.usb_port),
        'command -v fuser >/dev/null',
        'if fuser ' + q(args.usb_port) + ' >/dev/null 2>&1; then echo "Close the serial monitor before flashing." >&2; exit 1; fi',
        q(args.remote_pio) + ' run -e ' + q(args.environment) + ' -t nobuild -t upload --upload-port ' + q(args.usb_port),
    ])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--environment', choices=ENVIRONMENTS, default=ENVIRONMENTS[0], help='Default: current landscape layout')
    parser.add_argument('--host', default='pi@gateway.purplenet.online')
    parser.add_argument('--ssh-port', type=int, default=10022)
    parser.add_argument('--remote-dir', default='/home/pi/r1-dev/releases/ui-drive-20261001', help='Reuse this existing Pi checkout')
    parser.add_argument('--remote-pio', default='/home/pi/r1-dev/venv/bin/pio')
    parser.add_argument('--usb-port', default=USB_PORT)
    parser.add_argument('--check', action='store_true', help='Check local hardware configuration and tools only; no build, copy or flash')
    args = parser.parse_args()
    if os.environ.get('JR_R1_ALLOW_UNSAFE_UPLOAD') == '1':
        raise ValueError('Remove JR_R1_ALLOW_UNSAFE_UPLOAD; this script never bypasses preflight')
    validate_hardware(ROOT, args.environment)
    local_pio = ROOT / '.venv/bin/pio'
    if not local_pio.is_file():
        raise ValueError('Local PlatformIO missing: ' + str(local_pio))
    for tool in ('git', 'ssh', 'scp', 'make', 'node', 'sdl2-config'):
        if not shutil.which(tool):
            raise ValueError('Required local tool missing: ' + tool)
    orientation = 'portrait' if args.environment.endswith('-portrait') else 'landscape'
    print(f'XIAO ESP32-S3 / Waveshare LCD (F) / schematic (5) / {orientation}', flush=True)
    if args.check:
        print('Local checks passed. No firmware uploaded.')
        return
    run(['git', 'fetch', 'origin', 'main'])
    run(['git', 'status', '--short', '--branch'])
    run(['git', 'merge-base', '--is-ancestor', 'origin/main', 'HEAD'])
    branch = run(['git', 'branch', '--show-current'], capture=True).strip()
    if not branch:
        raise ValueError('Detached HEAD; refusing upload')
    commit = run(['git', 'rev-parse', 'HEAD'], capture=True).strip()
    work = Path(tempfile.mkdtemp(prefix='r1-flash-'))
    print('Local artifacts / source snapshot:', work, flush=True)
    def source_hashes():
        names=run(['git','ls-files','-z','--cached','--others','--exclude-standard'],capture=True).split('\0')
        return {name:digest(ROOT/name) for name in names if name and (ROOT/name).is_file()}
    tested_sources=source_hashes()
    run([str(local_pio), 'run', '-e', args.environment])
    run(['make', '-C', 'test', 'run'])
    for test in sorted((ROOT / 'test').glob('*test.cjs')):
        run(['node', str(test)])
    run(['make', '-C', 'simulator', '-j4'] + (['portrait'] if orientation == 'portrait' else []))
    simulator = 'simulator/build-portrait/jr-r1-sim-portrait' if orientation == 'portrait' else 'simulator/build/jr-r1-sim'
    audit_env = dict(os.environ, SDL_VIDEODRIVER='dummy', JR_R1_SIM_STATE=str(work / 'audit-state'))
    run([str(ROOT / simulator), '--ui-audit'], env=audit_env)
    if source_hashes()!=tested_sources:
        raise ValueError('Source files changed during build/testing; rerun with edits paused')
    # Keep the complete tested source locally; copy only binaries and the
    # configuration/preflight needed to upload them. The Pi checkout is not a build source.
    sources = run(['git', 'ls-files', '-z', '--cached', '--others', '--exclude-standard'], capture=True).split('\0')
    snapshot = work / 'source.tar.gz'
    with tarfile.open(snapshot, 'w:gz') as archive:
        for name in sources:
            if name and (ROOT / name).is_file():
                archive.add(ROOT / name, arcname=name, recursive=False)
    files = [f'.pio/build/{args.environment}/{name}' for name in ('firmware.bin', 'bootloader.bin', 'partitions.bin')]
    files += ['platformio.ini', 'include/display_profile.h', 'tools/upload_preflight.py']
    boot = Path.home() / '.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin'
    manifest = {'branch': branch, 'commit': commit, 'environment': args.environment,
                'source_snapshot_sha256': digest(snapshot), 'boot_app0_sha256': digest(boot),
                'sha256': {name: digest(ROOT / name) for name in files}}
    manifest_path = work / 'r1-upload-manifest.json'
    manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
    verify_manifest(ROOT, manifest)
    package = work / 'r1-upload.tar.gz'
    with tarfile.open(package, 'w:gz') as archive:
        for name in files:
            archive.add(ROOT / name, arcname=name, recursive=False)
        archive.add(manifest_path, arcname=manifest_path.name)
    print(f'Uploading branch {branch} ({commit[:8]}), {args.environment}, Waveshare LCD (F).', flush=True)
    print('Outputs will restart in CUT. Close any Pi serial monitor before upload.', flush=True)
    ssh = ['ssh', '-p', str(args.ssh_port), '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=12', args.host]
    run(ssh + ['test -d ' + shlex.quote(args.remote_dir)])
    destination = args.host + ':' + shlex.quote(args.remote_dir + '/r1-upload.tar.gz')
    run(['scp', '-P', str(args.ssh_port), '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=12', str(package), destination])
    run(ssh + [remote_script(args)])
    print('Flash completed and verified by PlatformIO. Local record:', work, flush=True)


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print('Flash stopped:', error, file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        print('Interrupted. If writing had started, rerun to finish the flash.', file=sys.stderr)
        sys.exit(130)

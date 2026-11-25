#!/usr/bin/env python3
"""Hardware and checksum helpers for the landscape upload guard."""
import configparser
import hashlib
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
# User instruction 2026-10-26: landscape only for hardware uploads.
ENVIRONMENTS = ('seeed_xiao_esp32s3-pcb',)
USB_PORT = '/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_90:70:69:11:DE:70-if00'


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def validate_hardware(root, environment):
    if environment not in ENVIRONMENTS:
        raise ValueError('Only the installed XIAO/Waveshare PCB environments are allowed')
    config = configparser.ConfigParser(interpolation=None)
    config.read(root / 'platformio.ini')
    section = config['env:' + environment]
    board = section.get('board')
    while board is None and 'extends' in section:
        section = config[section['extends']]
        board = section.get('board')
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


if __name__ == '__main__':
    raise SystemExit('The laptop upload workflow has retired. Use tools/flash_cloud.py on the Pi.')

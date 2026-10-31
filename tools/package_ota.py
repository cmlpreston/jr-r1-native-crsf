#!/usr/bin/env python3
"""Package the approved XIAO landscape application for the handset web updater."""
import argparse
import hashlib
from pathlib import Path
import struct

TARGET = b'xiao-s3-waveshare-f-landscape'
ROOT = Path(__file__).resolve().parents[1]

def package(image):
    if len(image) < 256 or image[0] != 0xe9:
        raise ValueError('Expected an ESP application firmware.bin, not a merged flash image')
    # ESP32-S3 image header chip ID (offset 12); avoid an ESP8266/other ESP image.
    if struct.unpack_from('<H', image, 12)[0] != 9:
        raise ValueError('Expected an ESP32-S3 application')
    return struct.pack('<8s32sI32s', b'R1OTA001', TARGET, len(image),
                       hashlib.md5(image).hexdigest().encode()) + image

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    # Fixed build directory prevents accidentally packaging a portrait/legacy target.
    source = ROOT / '.pio/build/seeed_xiao_esp32s3-pcb/firmware.bin'
    output = args.output or source.with_suffix('.r1ota')
    output.write_bytes(package(source.read_bytes()))
    print(f'OTA package: {output} ({output.stat().st_size} bytes)')

if __name__ == '__main__':
    main()

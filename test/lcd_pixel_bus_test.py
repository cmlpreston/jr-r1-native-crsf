#!/usr/bin/env python3
"""Compile the real pinned GFX/HAL pixel functions with fake hardware registers.

No library files are patched or copied into the repository. Fresh host-only
checkouts can run the standalone helper test; this additional integration proof
runs after the approved PlatformIO environment has installed its dependencies.
"""
from pathlib import Path
import hashlib
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
GFX = ROOT / '.pio/libdeps/seeed_xiao_esp32s3-pcb/GFX Library for Arduino/src'
HAL = Path.home() / '.platformio/packages/framework-arduinoespressif32/cores/esp32/esp32-hal-spi.c'
SPI_CPP = HAL.parents[2] / 'libraries/SPI/src/SPI.cpp'

def function(source, name):
    match = re.search(r'void\s+(?:ARDUINO_ISR_ATTR\s+)?' + name + r'\s*\(', source)
    if not match:
        raise AssertionError('Pinned HAL function missing: ' + name)
    start = source.index('{', match.start())
    depth = 1
    end = start + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end]

def main():
    gfx_source = GFX / 'databus/Arduino_HWSPI.cpp'
    if not gfx_source.exists() or not HAL.exists():
        print('Pinned LCD bus/HAL integration: SKIP (build approved PlatformIO target to install pinned sources)')
        return
    source = HAL.read_text()
    # Verify the real SPIClass entry point forwards the unchanged BYTE count in
    # both transaction branches, matching the register adapter used below.
    wrapper = function(SPI_CPP.read_text(), 'SPIClass::writePixels')
    assert len(re.findall(r'spiWritePixelsNL\(\s*_spi\s*,\s*data\s*,\s*size\s*\)', wrapper)) == 2
    macros = '\n'.join(re.findall(r'^#define MSB_(?:16|PIX)_SET[^\n]+', source, re.M))
    assert macros.count('#define') == 2
    generated = '#include <SPI.h>\n#define ARDUINO_ISR_ATTR\n'
    for name in ('spiWritePixelsNL', 'spiWriteNL', 'spiWriteShortNL'):
        generated += function(source, name) + '\n'
    with tempfile.TemporaryDirectory(prefix='r1-pinned-lcd-') as work:
        work = Path(work)
        (work / 'macros.h').write_text(macros + '\n')
        (work / 'hal.cpp').write_text(generated)
        command = [os.environ.get('CXX', 'c++'), '-std=c++17', '-O1', '-g', '-fsanitize=address',
                   '-fno-omit-frame-pointer', '-DESP32=1', '-DCONFIG_IDF_TARGET_ESP32S3=1',
                   '-include', str(work / 'macros.h'),
                   '-I' + str(ROOT / 'test/lcd_spi_stubs'), '-I' + str(ROOT / 'include'),
                   '-I' + str(GFX), str(ROOT / 'test/lcd_pixel_bus_harness.cpp'),
                   str(gfx_source), str(work / 'hal.cpp'), '-o', str(work / 'test')]
        subprocess.run(command, check=True)
        subprocess.run([str(work / 'test')], check=True)
    print('Pinned GFX SHA256:', hashlib.sha256(gfx_source.read_bytes()).hexdigest())
    print('Pinned ESP32 HAL SHA256:', hashlib.sha256(HAL.read_bytes()).hexdigest())

if __name__ == '__main__':
    main()

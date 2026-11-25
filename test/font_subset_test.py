#!/usr/bin/env python3
"""Validate reproducible assets and compare actual LVGL font resolution."""
from pathlib import Path
import json
import os
import re
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import generate_symbol_fonts as fonts


def lcd_unicode():
    # Tokenize strings before comments, so URLs containing // remain strings.
    token=re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',re.S)
    required=set()
    for path in [*ROOT.glob('src/*.cpp'),*ROOT.glob('src/*.inc'),*ROOT.glob('include/*.h')]:
        if path.name=='setup_portal_page.h':continue
        for match in token.finditer(path.read_text()):
            text=match.group()
            if not text.startswith('"'):continue
            # The LCD currently uses ASCII literals plus LV_SYMBOL macros. Also
            # catch newly introduced literal or \u/\U Unicode before it ships.
            required.update(ord(char) for char in text if ord(char)>127)
            required.update(int(value,16) for pair in re.findall(r'\\u([\da-fA-F]{4})|\\U([\da-fA-F]{8})',text)
                            for value in pair if value)
            escaped=re.findall(r'(?:\\x[\da-fA-F]{2}){2,}',text)
            for group in escaped:
                try:
                    required.update(ord(char) for char in bytes(int(v,16) for v in re.findall(r'\\x([\da-fA-F]{2})',group)).decode('utf8') if ord(char)>127)
                except UnicodeDecodeError:
                    pass  # Binary CRSF constants are not LCD text.
    return required


def main():
    repertoire=json.loads((ROOT/'assets/fonts/symbol-repertoire.json').read_text())
    source_points=lcd_unicode()
    # Keep the pure host suite usable before fetching graphical dependencies.
    # The app-side coverage guard still runs; full byte/metric equivalence runs
    # whenever the pinned simulator or approved PlatformIO LVGL is installed.
    app_names=set()
    token=re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',re.S)
    for path in [*ROOT.glob('src/*.cpp'),*ROOT.glob('src/*.inc'),*ROOT.glob('include/*.h')]:
        if path.name=='setup_portal_page.h':continue
        text=token.sub(lambda match: '' if match.group().startswith(('//','/*')) else match.group(),path.read_text())
        app_names.update(re.findall(r'\bLV_SYMBOL_[A-Z0-9_]+\b',text))
    assert not (app_names-set(repertoire)-{'LV_SYMBOL_DUMMY','LV_SYMBOL_BULLET'}), 'Regenerate symbol fonts for a new LCD icon'
    for family,size in [('dm',12),('dm',14),('dm',16),('manrope',22),('manrope',32),('manrope',48)]:
        text=(ROOT/f'src/fonts/r1_{family}_{size}.inc').read_text()
        assert f'.fallback = &r1_symbols_{size}' in text
        available={int(value,16) for value in re.findall(r'/\* U\+([0-9A-Fa-f]+)',text)}|set(repertoire.values())
        assert source_points<=available, f'LCD Unicode needs font coverage review: {source_points-available}'
    if not (fonts.LVGL/'src/font/lv_font.h').is_file():
        print('Font source coverage PASS; LVGL glyph integration SKIP (build approved target or bootstrap simulator first)')
        return
    subprocess.run([sys.executable,str(ROOT/'tools/generate_symbol_fonts.py'),'--check'],check=True)
    with tempfile.TemporaryDirectory(prefix='r1-font-subsets-') as work:
        work=Path(work)
        lvgl=fonts.LVGL
        common=['-DJR_R1_SIMULATOR','-DLV_CONF_INCLUDE_SIMPLE','-DLV_USE_FONT_COMPRESSED=0',
                '-I'+str(ROOT/'include'),'-I'+str(lvgl),'-I'+str(ROOT)]
        objects=[]
        for name in ('src/font/lv_font.c','src/font/lv_font_fmt_txt.c','src/misc/lv_utils.c'):
            output=work/(Path(name).stem+'.o')
            subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1',*common,'-c',str(lvgl/name),'-o',str(output)],check=True)
            objects.append(str(output))
        cpp=[ROOT/'test/font_subset_compare.cpp',ROOT/'src/r1_symbol_fonts.cpp']
        cpp.extend(ROOT/f'src/r1_{family}_{size}.cpp' for family,size in
                   [('dm',12),('dm',14),('dm',16),('manrope',22),('manrope',32),('manrope',48)])
        output=work/'font_subset_compare'
        baseline=[f'-DR1_BASELINE_FONT_{size}="{lvgl}/src/font/lv_font_montserrat_{size}.c"' for size in fonts.SIZES]
        subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-O1',*common,*baseline,*map(str,cpp),*objects,'-o',str(output)],check=True)
        subprocess.run([str(output),*(hex(point) for point in sorted(source_points))],check=True)
        assert repertoire and len(set(repertoire.values()))==len(repertoire)


if __name__=='__main__':main()

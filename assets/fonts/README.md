# V2 handset fonts

DM Sans: 12 px medium captions; 14/16 px semibold supporting/menu text.
Manrope: 22/32/48 px extra-bold headings/readings. Both families are the same
ones used by the approved browser trial. The handset uses generated 4-bit
antialiased bitmaps, with no runtime TTF loading or network dependency.

Unmodified variable TTFs and SIL OFL 1.1 licences are bundled here from
https://github.com/google/fonts . Exact upstream commit, paths and SHA-256
checksums are in `sources.json`. Generated bitmap source lives in `src/fonts/`;
six small C++ wrappers compile it for firmware, SDL and WebAssembly.

Regeneration: `tools/generate_ui_fonts.py`, using lv_font_conv 1.5.3 and
FontTools 4.68.0. It checks source hashes, creates fixed-weight instances, then
subsets ASCII and the UI's common punctuation/degree characters. The generated
files are checked in, so ordinary builds do not need conversion tools installed.

## Symbol fallbacks

`tools/generate_symbol_fonts.py` copies required glyphs directly from the pinned
LVGL 8.4.0 bitmap fonts. It does not rerasterize or change their metrics. The six
`r1_symbols_*` fallbacks contain the symbols referenced by the native handset and
LVGL widget/theme sources, including keyboard/menu/calendar defaults. The current
11-symbol repertoire is recorded in `symbol-repertoire.json`. Unused FontAwesome
glyphs are intentionally omitted; a new symbol requires regeneration.

The default LVGL face remains the original 14px Montserrat: `r1_default_14`
retains ASCII, degree and bullet glyphs plus their original kerning, using the
symbol-only fallback for icons. This prevents changing default control metrics
or retaining six complete fonts. The preferred DM Sans/Manrope text bitmaps,
kerning, line heights and baselines are unchanged.

The generator verifies original source SHA-256 values from `symbol-sources.json`;
it uses the simulator's pinned LVGL checkout or the approved PlatformIO target's
copy. Run `python3 tools/generate_symbol_fonts.py`, followed by
`python3 test/font_subset_test.py`. The latter checks regeneration, source icon/
Unicode coverage and uses actual LVGL lookup code to compare glyph bytes,
advances, baseline metrics, fallback resolution and default ASCII kerning with
the original fonts. It also runs as part of `make -C test run` (the glyph
integration comparison requires the graphical dependencies).

`LVGL-MIT.txt` preserves the source licence; the copied source fonts include
Montserrat and FontAwesome glyphs distributed by LVGL. Original text-font OFL
licences remain above. No network is used during symbol subsetting.

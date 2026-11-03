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
subsets ASCII and the UI's common punctuation/degree characters. Montserrat
remains the fallback for LVGL icons and uncovered glyphs. The generated files
are checked in, so ordinary builds do not need conversion tools installed.

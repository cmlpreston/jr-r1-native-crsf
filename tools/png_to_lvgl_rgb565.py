#!/usr/bin/env python3
"""Crop, resize and convert an RGBA PNG to an LVGL v8 RGB565 image source."""

from pathlib import Path
import sys

from PIL import Image


def main() -> None:
    if len(sys.argv) != 4:
        raise SystemExit("usage: png_to_lvgl_rgb565.py INPUT OUTPUT_CPP OUTPUT_PREVIEW")

    source = Image.open(sys.argv[1]).convert("RGBA")
    alpha = source.getchannel("A")
    bounds = alpha.getbbox()
    if bounds is None:
        raise SystemExit("input contains no visible pixels")

    source = source.crop(bounds)
    source.thumbnail((78, 54), Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", (82, 58), (11, 18, 28, 255))
    canvas.alpha_composite(source, ((82 - source.width) // 2, (58 - source.height) // 2))
    rgb = canvas.convert("RGB")
    rgb.save(sys.argv[3])

    raw = bytearray()
    for red, green, blue in rgb.getdata():
        pixel = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
        raw.extend((pixel & 0xFF, pixel >> 8))

    rows = []
    for offset in range(0, len(raw), 16):
        rows.append("    " + ", ".join(f"0x{byte:02x}" for byte in raw[offset:offset + 16]))

    output = f'''#include <lvgl.h>\n\nconst LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t hyper7_icon_map[] = {{\n{",\n".join(rows)}\n}};\n\nextern const lv_img_dsc_t hyper7_icon = {{\n    .header = {{\n        .cf = LV_IMG_CF_TRUE_COLOR,\n        .always_zero = 0,\n        .reserved = 0,\n        .w = 82,\n        .h = 58,\n    }},\n    .data_size = sizeof(hyper7_icon_map),\n    .data = hyper7_icon_map,\n}};\n'''
    Path(sys.argv[2]).write_text(output)


if __name__ == "__main__":
    main()

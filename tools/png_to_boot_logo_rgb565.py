#!/usr/bin/env python3
"""Convert the JR PROPO reference PNG into the 480x320 boot-logo asset."""

from pathlib import Path
import sys

from PIL import Image


CONTENT_WIDTH = 420
HORIZONTAL_PADDING = 10
VERTICAL_PADDING = 10
JR_RED = (237, 28, 36)


def main() -> None:
    if len(sys.argv) != 4:
        raise SystemExit(
            "usage: png_to_boot_logo_rgb565.py INPUT OUTPUT_CPP OUTPUT_PREVIEW"
        )

    source = Image.open(sys.argv[1]).convert("RGB")
    pixels = source.load()
    visible = [
        (x, y)
        for y in range(source.height)
        for x in range(source.width)
        if min(pixels[x, y]) < 250
    ]
    if not visible:
        raise SystemExit("input contains no visible logo pixels")

    left = min(point[0] for point in visible)
    top = min(point[1] for point in visible)
    right = max(point[0] for point in visible) + 1
    bottom = max(point[1] for point in visible) + 1
    source = source.crop((left, top, right, bottom))

    # Recover opacity directly from the white-backed artwork. Do not dilate or
    # close this mask: the narrow separators between JR and each PROPO letter,
    # plus the registered marks, are deliberate parts of the original logo.
    logo_mask = Image.new("L", source.size)
    red_mask = Image.new("L", source.size)
    logo_opacity = []
    red_opacity = []
    for red, green, blue in source.getdata():
        is_red = red > green + 35 and red > blue + 35
        if is_red:
            alpha = max((255 - green) / 226.0, (255 - blue) / 218.0)
        else:
            alpha = (255 - (red + green + blue) / 3.0) / 150.0
        alpha = max(0.0, min(1.0, alpha))
        alpha_byte = round(alpha * 255) if alpha >= 0.28 else 0
        logo_opacity.append(alpha_byte)
        red_opacity.append(alpha_byte if is_red else 0)
    logo_mask.putdata(logo_opacity)
    red_mask.putdata(red_opacity)
    target_height = round(source.height * CONTENT_WIDTH / source.width)
    target_size = (CONTENT_WIDTH, target_height)
    logo_mask = logo_mask.resize(target_size, Image.Resampling.LANCZOS)
    red_mask = red_mask.resize(target_size, Image.Resampling.LANCZOS)
    canvas = Image.new(
        "RGB",
        (CONTENT_WIDTH + HORIZONTAL_PADDING * 2,
         target_height + VERTICAL_PADDING * 2),
        (0, 0, 0),
    )
    offset = (HORIZONTAL_PADDING, VERTICAL_PADDING)
    canvas.paste((255, 255, 255), box=offset, mask=logo_mask)
    canvas.paste(JR_RED, box=offset, mask=red_mask)
    canvas.save(sys.argv[3])

    raw = bytearray()
    for red, green, blue in canvas.getdata():
        pixel = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
        raw.extend((pixel & 0xFF, pixel >> 8))

    rows = []
    for offset in range(0, len(raw), 16):
        rows.append(
            "    " + ", ".join(
                f"0x{byte:02x}" for byte in raw[offset:offset + 16]
            )
        )

    output = f'''#include <lvgl.h>

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t jr_propo_boot_logo_map[] = {{
{",\n".join(rows)}
}};

extern const lv_img_dsc_t jr_propo_boot_logo = {{
    .header = {{
        .cf = LV_IMG_CF_TRUE_COLOR,
        .always_zero = 0,
        .reserved = 0,
        .w = {canvas.width},
        .h = {canvas.height},
    }},
    .data_size = sizeof(jr_propo_boot_logo_map),
    .data = jr_propo_boot_logo_map,
}};
'''
    Path(sys.argv[2]).write_text(output)


if __name__ == "__main__":
    main()

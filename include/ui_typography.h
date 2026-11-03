#pragma once
#include <lvgl.h>
LV_FONT_DECLARE(r1_dm_12);
LV_FONT_DECLARE(r1_dm_14);
LV_FONT_DECLARE(r1_dm_16);
LV_FONT_DECLARE(r1_manrope_22);
LV_FONT_DECLARE(r1_manrope_32);
LV_FONT_DECLARE(r1_manrope_48);

// Semantic roles, not arbitrary pixel requests. Keep this scale shared by
// menus, editors, widgets and custom drawing. Bitmap/vector logos are separate.
namespace UiText {
enum Style : uint8_t {
    Caption,   // 12 px: help, chart axes and compact telemetry
    Secondary, // 14 px: supporting text and compact controls
    Menu,      // 16 px: primary labels, values, buttons and tabs
    Heading,   // 22 px: page headings and medium readings
    Reading,   // 32 px: primary widget/editor readings
    Hero       // 48 px: spacious dashboard readings
};
inline const lv_font_t *font(Style style)
{
    switch(style) {
        case Caption: return &r1_dm_12;
        case Secondary: return &r1_dm_14;
        case Menu: return &r1_dm_16;
        case Heading: return &r1_manrope_22;
        case Reading: return &r1_manrope_32;
        case Hero: return &r1_manrope_48;
    }
    return &r1_dm_14;
}
}

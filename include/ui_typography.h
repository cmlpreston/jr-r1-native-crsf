#pragma once
#include <lvgl.h>

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
        case Caption: return &lv_font_montserrat_12;
        case Secondary: return &lv_font_montserrat_14;
        case Menu: return &lv_font_montserrat_16;
        case Heading: return &lv_font_montserrat_22;
        case Reading: return &lv_font_montserrat_32;
        case Hero: return &lv_font_montserrat_48;
    }
    return &lv_font_montserrat_14;
}
}

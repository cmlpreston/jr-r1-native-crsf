#pragma once
#include <lvgl.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>

// LVGL 8 label/style setters invalidate even unchanged content. Use these only
// for live values on existing objects; page construction still sets local styles.
inline void uiSetHidden(lv_obj_t *obj, bool hidden)
{
    if(lv_obj_has_flag(obj,LV_OBJ_FLAG_HIDDEN)==hidden)return;
    if(hidden)lv_obj_add_flag(obj,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(obj,LV_OBJ_FLAG_HIDDEN);
}
inline void uiSetOpacity(lv_obj_t *obj, lv_opa_t opacity)
{
    if(lv_obj_get_style_opa(obj,0)!=opacity)lv_obj_set_style_opa(obj,opacity,0);
}
inline void uiSetText(lv_obj_t *obj, const char *text)
{
    if (std::strcmp(lv_label_get_text(obj), text) != 0) lv_label_set_text(obj, text);
}

inline void uiSetTextFmt(lv_obj_t *obj, const char *format, ...)
{
    char local[128];
    va_list args;
    va_start(args, format);
    va_list copy;
    va_copy(copy, args);
    const int size = std::vsnprintf(local, sizeof(local), format, args);
    va_end(args);
    if (size >= 0 && static_cast<size_t>(size) < sizeof(local)) {
        uiSetText(obj, local);
    } else if (size >= 0) {
        // Preserve long text without truncation; normal live readings fit above.
        auto *text = static_cast<char *>(lv_mem_alloc(static_cast<size_t>(size) + 1));
        if (text) {
            std::vsnprintf(text, static_cast<size_t>(size) + 1, format, copy);
            uiSetText(obj, text);
            lv_mem_free(text);
        }
    }
    va_end(copy);
}

inline void uiSetTextColor(lv_obj_t *obj, lv_color_t value, lv_part_t part)
{
    if (lv_obj_get_style_text_color(obj, part).full != value.full)
        lv_obj_set_style_text_color(obj, value, part);
}
inline void uiSetBgColor(lv_obj_t *obj, lv_color_t value, lv_part_t part)
{
    if (lv_obj_get_style_bg_color(obj, part).full != value.full)
        lv_obj_set_style_bg_color(obj, value, part);
}
inline void uiSetArcColor(lv_obj_t *obj, lv_color_t value, lv_part_t part)
{
    if (lv_obj_get_style_arc_color(obj, part).full != value.full)
        lv_obj_set_style_arc_color(obj, value, part);
}
inline void uiSetTextFont(lv_obj_t *obj, const lv_font_t *value, lv_part_t part)
{
    if (lv_obj_get_style_text_font(obj, part) != value)
        lv_obj_set_style_text_font(obj, value, part);
}

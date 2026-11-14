#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0
#define LV_MEM_SIZE (96U * 1024U)

#ifdef JR_R1_SIMULATOR
#define LV_TICK_CUSTOM 0
#else
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "esp_timer.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR ((uint32_t)(esp_timer_get_time() / 1000ULL))
#endif

#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 0
#define LV_FONT_MONTSERRAT_14 0
#define LV_FONT_MONTSERRAT_16 0
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 0
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 0
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_48 0

// Preserve the original LVGL default face/metrics without six complete fallbacks.
#define LV_FONT_CUSTOM_DECLARE LV_FONT_DECLARE(r1_default_14)
#define LV_FONT_DEFAULT &r1_default_14

#define LV_USE_LOG 0
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0

#endif

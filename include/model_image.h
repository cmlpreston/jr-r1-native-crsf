#pragma once
#include <lvgl.h>
#include "model_appearance.h"
const char *modelImageName(uint8_t image);
const lv_img_dsc_t *modelImage(uint8_t image, uint32_t colour, uint32_t accent=kModelOriginalColour, uint32_t base=kModelOriginalColour);

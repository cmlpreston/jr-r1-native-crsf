#include "model_image.h"
#include <stdlib.h>
#ifndef JR_R1_SIMULATOR
#include <esp_heap_caps.h>
#endif
LV_IMG_DECLARE(hyper7_widget_icon);
LV_IMG_DECLARE(model_buggy);
LV_IMG_DECLARE(model_truggy);
LV_IMG_DECLARE(model_short_course);
LV_IMG_DECLARE(model_monster_truck);
LV_IMG_DECLARE(model_crawler);
LV_IMG_DECLARE(model_touring);
LV_IMG_DECLARE(model_formula);
static const lv_img_dsc_t *const images[] = {&hyper7_widget_icon,&model_buggy,&model_truggy,&model_short_course,&model_monster_truck,&model_crawler,&model_touring,&model_formula};
const char *modelImageName(uint8_t image) {
    static const char *const names[]={"Hyper 7","Buggy","Truggy","Short course","Monster truck","Crawler","Touring","Formula / F1","None"};
    return names[image<kModelImageCount?image:kModelImageNone];
}
const lv_img_dsc_t *modelImage(uint8_t image, uint32_t colour,uint32_t accent,uint32_t base) {
    if(image>=kModelImageNone)return nullptr;
    const auto *src=images[image];
    if(colour==kModelOriginalColour&&accent==kModelOriginalColour&&base==kModelOriginalColour)return src;
    static uint8_t *pixels=nullptr;
    static lv_img_dsc_t result={};
    static uint8_t previous=255;
    static uint32_t previousColour=kModelOriginalColour, previousAccent=kModelOriginalColour,previousBase=kModelOriginalColour;
    if(!pixels){
#ifdef JR_R1_SIMULATOR
        pixels=static_cast<uint8_t*>(malloc(240*160*3));
#else
        pixels=static_cast<uint8_t*>(heap_caps_malloc(240*160*3,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
#endif
    }
    if(!pixels)return src;
    if(previous==image && previousColour==colour&&previousAccent==accent&&previousBase==base)return &result;
    lv_img_cache_invalidate_src(&result);
    for(unsigned i=0;i<src->data_size;i+=3){
        const uint16_t p=src->data[i]|(src->data[i+1]<<8);
        uint8_t r=((p>>11)&31)*255/31,g=((p>>5)&63)*255/63,b=(p&31)*255/31;
        const unsigned pixel=i/3;
        const auto role=modelPaintRole(image,pixel%src->header.w,pixel/src->header.w,src->header.w,src->header.h,r,g,b);
        const uint32_t selected=role==1?colour:role==2?accent:role==3?base:kModelOriginalColour;
        recolourModelMaterial(r,g,b,selected,role);
        const uint16_t tinted=((r&248)<<8)|((g&252)<<3)|(b>>3);
        pixels[i]=tinted&255;pixels[i+1]=tinted>>8;pixels[i+2]=src->data[i+2];
    }
    result=*src;result.data=pixels;previous=image;previousColour=colour;previousAccent=accent;previousBase=base;
    return &result;
}

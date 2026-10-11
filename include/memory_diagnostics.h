#pragma once
#ifndef JR_R1_SIMULATOR
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
namespace MemoryDiagnostics {
// Separate capability pools; DMA is a subset of internal RAM, not extra RAM.
struct Pool { uint32_t total=0,free=0,minimum=0,largest=0; };
struct Snapshot {
    uint32_t sampledMs=0;
    Pool internal,psram,dma;
    bool lvglReady=false;
    uint32_t lvglTotal=0,lvglUsed=0,lvglPeak=0,lvglLargest=0,lvglFragmentation=0;
    int32_t loopStack=-1,controlStack=-1,crsfStack=-1;
};
// All functions run on the Arduino/UI task, the LVGL owner.
void configure(TaskHandle_t crsf,TaskHandle_t control,bool lvglReady);
void sample(bool force=false);
const Snapshot &snapshot();
void print(Print &output);
}
#endif

#pragma once

#include <Arduino.h>
#include <lvgl.h>

struct SimulatorControls {
    int16_t throttlePermille = 0;
    bool linkValid = true;
    int8_t uplinkRssiDbm = -48;
    uint8_t linkQualityPercent = 100;
    uint16_t receiverVoltageDecivolts = 66;
    uint16_t transmitterVoltageDecivolts = 99;
};

bool simulatorDisplayBegin(int16_t width, int16_t height);
void simulatorShutdown();
void simulatorPollEvents();
bool simulatorQuitRequested();
const SimulatorControls &simulatorControls();
void simulatorFlush(const lv_area_t *area, const lv_color_t *pixels);
bool simulatorReadTouch(int16_t &x, int16_t &y, uint16_t &rawX,
                        uint16_t &rawY, uint16_t &rawZ);
bool simulatorTouchDown();
void simulatorSetDisplayEnabled(bool enabled);
void simulatorSetBrightness(uint8_t percent);
void simulatorBeep(uint16_t pitchHz, uint16_t durationMs);
void simulatorStopBeep();
void simulatorWriteScreenshot(Stream &output);
bool simulatorSaveScreenshot(const char *path);


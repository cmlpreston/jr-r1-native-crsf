#pragma once

#include "mcp23017_input.h"

// One owner per chip: display startup and input polling share these instances.
// Called only from setup/Arduino loop, never the independent control task.
// Port indices: A0..A7 = 0..7, B0..B7 = 8..15. Final schematic (5).
constexpr uint8_t PCB_TOUCH_RESET_PORT = 8;   // K2 pin 1, GPB0
constexpr uint8_t PCB_LCD_RESET_PORT = 11;    // K2 pin 4, GPB3
constexpr uint8_t PCB_TOUCH_IRQ_PORT = 12;    // K2 pin 5, GPB4
extern Mcp23017Input pcbKeypad;
extern Mcp23017Input pcbControls;
bool pcbIoBegin();
bool pcbResetTouch();
bool pcbResetDisplay();
bool pcbTouchIrqActive();

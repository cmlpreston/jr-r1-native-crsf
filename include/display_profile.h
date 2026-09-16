#pragma once

// Select the installed transmitter display here. DISPLAY_PROFILE may also be
// supplied as a PlatformIO build flag, which is how the optional BTT build
// environment verifies the fallback panel without editing this file.
#define DISPLAY_PROFILE_BTT_TFT35_SPI 1
#define DISPLAY_PROFILE_WAVESHARE_RPI_35F 2

#ifndef DISPLAY_PROFILE
#define DISPLAY_PROFILE DISPLAY_PROFILE_WAVESHARE_RPI_35F
#endif

#if DISPLAY_PROFILE == DISPLAY_PROFILE_BTT_TFT35_SPI

constexpr const char *DISPLAY_PROFILE_NAME = "BTT TFT35-SPI V2.1";
constexpr bool DISPLAY_USES_GT911 = false;
constexpr bool DISPLAY_HAS_PWM_BACKLIGHT = false;
constexpr bool DISPLAY_HAS_SPI_READBACK = true;

constexpr int TFT_CS_PIN = 10;
constexpr int TFT_DC_PIN = 9;
constexpr int TFT_SCK_PIN = 12;
constexpr int TFT_MOSI_PIN = 11;
constexpr int TFT_MISO_PIN = 13;
constexpr int TFT_RESET_PIN = -1;
constexpr int TFT_BACKLIGHT_PIN = -1;

constexpr int TOUCH_SDA_PIN = 8;
constexpr int TOUCH_SCL_PIN = 18;
constexpr int TOUCH_IRQ_PIN = 16;
constexpr int TOUCH_RESET_PIN = -1;

constexpr uint8_t NS2009_I2C_ADDRESS = 0x48;
constexpr int TOUCH_RAW_X_MIN = 200;
constexpr int TOUCH_RAW_X_MAX = 3850;
constexpr int TOUCH_RAW_Y_MIN = 400;
constexpr int TOUCH_RAW_Y_MAX = 3920;

#elif DISPLAY_PROFILE == DISPLAY_PROFILE_WAVESHARE_RPI_35F

constexpr const char *DISPLAY_PROFILE_NAME = "Waveshare 3.5inch RPi LCD (F)";
constexpr bool DISPLAY_USES_GT911 = true;
constexpr bool DISPLAY_HAS_PWM_BACKLIGHT = true;
constexpr bool DISPLAY_HAS_SPI_READBACK = false;

// The production PHD cable holds LCD_CS low and omits LCD_MISO. Arduino_GFX
// treats -1 as an unconnected pin, leaving GPIO10 and GPIO13 unallocated.
constexpr int TFT_CS_PIN = -1;
#ifdef JR_R1_XIAO_PCB
constexpr int TFT_DC_PIN = 4;       // D3
constexpr int TFT_SCK_PIN = 5;      // D4
constexpr int TFT_MOSI_PIN = 6;     // D5
constexpr int TFT_MISO_PIN = -1;
constexpr int TFT_RESET_PIN = -1;   // K2 GPB3
constexpr int TFT_BACKLIGHT_PIN = 3; // D2
constexpr int TOUCH_SDA_PIN = 1;    // D0
constexpr int TOUCH_SCL_PIN = 2;    // D1
constexpr int TOUCH_IRQ_PIN = -1;   // K2 GPB4
constexpr int TOUCH_RESET_PIN = -1; // K2 GPB0
#else
constexpr int TFT_DC_PIN = 9;
constexpr int TFT_SCK_PIN = 12;
constexpr int TFT_MOSI_PIN = 11;
constexpr int TFT_MISO_PIN = -1;
constexpr int TFT_RESET_PIN = 40;
constexpr int TFT_BACKLIGHT_PIN = 41;

constexpr int TOUCH_SDA_PIN = 8;
constexpr int TOUCH_SCL_PIN = 18;
constexpr int TOUCH_IRQ_PIN = 16;
constexpr int TOUCH_RESET_PIN = 42;

#endif

// INT is held low as reset is released, selecting the GT911's 7-bit 0x5d
// address. The alternative address selected with INT high is 0x14.
constexpr uint8_t GT911_I2C_ADDRESS = 0x5d;
constexpr uint16_t GT911_POINTS_INFO_REGISTER = 0x814e;
constexpr uint16_t GT911_FIRST_POINT_REGISTER = 0x814f;

#else
#error "Unsupported DISPLAY_PROFILE value"
#endif

#ifdef JR_R1_XIAO_PCB
// Conservative shared MCP23017/GT911 bus rate for the current bench wiring.
constexpr uint32_t CONTROL_I2C_FREQUENCY = 100000;
#else
constexpr uint32_t CONTROL_I2C_FREQUENCY = 400000;
#endif

constexpr int16_t DISPLAY_LANDSCAPE_WIDTH = 480;
constexpr int16_t DISPLAY_LANDSCAPE_HEIGHT = 320;
#ifdef JR_R1_DISPLAY_DIAGNOSTIC
// Slow enough for a 24 Msps hobby logic analyser to resolve cleanly while
// diagnosing the panel cable and connectors.
constexpr uint32_t TFT_SPI_FREQUENCY = 1000000;
#elif defined(JR_R1_XIAO_PCB)
// Confirmed working breadboard setting. The 8 MHz trial showed a blank LCD;
// 20 MHz previously produced corruption/blanking. Retain the XIAO pin map.
constexpr uint32_t TFT_SPI_FREQUENCY = 4000000;
#else
// Legacy prototype's trial speed; the active XIAO profile is set above.
constexpr uint32_t TFT_SPI_FREQUENCY = 30000000;
#endif
constexpr uint8_t TFT_BACKLIGHT_LEDC_CHANNEL = 6;
constexpr uint32_t TFT_BACKLIGHT_PWM_FREQUENCY = 5000;
constexpr uint8_t TFT_BACKLIGHT_PWM_BITS = 8;

#if defined(JR_R1_XIAO_PCB) && DISPLAY_PROFILE != DISPLAY_PROFILE_WAVESHARE_RPI_35F
#error "XIAO PCB requires the Waveshare RPi LCD (F) profile"
#endif

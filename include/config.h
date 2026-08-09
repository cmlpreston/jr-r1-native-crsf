#pragma once

#include <Arduino.h>

// ESP32-S3 DevKitC-1 pin assignments.
// GPIO19 and GPIO20 are deliberately not used: they are native USB D-/D+.
constexpr gpio_num_t THROTTLE_ADC_PIN = GPIO_NUM_1;
// Local transmitter-pack measurement. Connect only through the documented
// 47 kOhm / 10 kOhm divider; never connect a battery directly to this pin.
constexpr gpio_num_t TX_BATTERY_ADC_PIN = GPIO_NUM_5;
constexpr uint32_t TX_BATTERY_DIVIDER_TOP_OHMS = 47000;
constexpr uint32_t TX_BATTERY_DIVIDER_BOTTOM_OHMS = 10000;
constexpr gpio_num_t CRSF_SIGNAL_PIN = GPIO_NUM_17;
constexpr gpio_num_t BUZZER_PIN = GPIO_NUM_4;
constexpr uint8_t BUZZER_LEDC_CHANNEL = 7;

// BIGTREETECH TFT35 SPI V2.1. The LCD is powered from 5 V but all interface
// signals are 3.3 V logic. GPIO19/20 remain reserved for native USB.
constexpr int TFT_CS_PIN = 10;       // TFT35 NSS
constexpr int TFT_DC_PIN = 9;        // TFT35 RS / data-command
constexpr int TFT_SCK_PIN = 12;
constexpr int TFT_MOSI_PIN = 11;
constexpr int TFT_MISO_PIN = 13;
constexpr int TOUCH_SDA_PIN = 8;     // NS2009 SDA
constexpr int TOUCH_SCL_PIN = 18;    // NS2009 SCL
constexpr int TOUCH_IRQ_PIN = 16;    // TFT35 IDRQ, active low

constexpr uint32_t TFT_SPI_FREQUENCY = 25000000;
constexpr uint8_t NS2009_I2C_ADDRESS = 0x48;

// Initial NS2009 calibration. Raw coordinates are printed over USB whenever
// the panel is touched so these can be refined for this particular screen.
constexpr int TOUCH_RAW_X_MIN = 200;
constexpr int TOUCH_RAW_X_MAX = 3850;
constexpr int TOUCH_RAW_Y_MIN = 400;
constexpr int TOUCH_RAW_Y_MAX = 3920;

// Measure these three values from the USB log with the trigger at:
// full brake/reverse, released/neutral, and full throttle.
// Measured on the original JR R1 5 kΩ throttle pot (2026-08-09):
// brake/reverse 1442–1452, released 1962–1986, forward 4007–4033.
constexpr uint16_t THROTTLE_ADC_MIN = 1447;
constexpr uint16_t THROTTLE_ADC_CENTER = 1970;
constexpr uint16_t THROTTLE_ADC_MAX = 4015;

// Neutral protection. The module receives neutral until the trigger has remained
// close to centre for ARM_NEUTRAL_TIME_MS after boot.
constexpr uint16_t ARM_NEUTRAL_ADC_WINDOW = 80;
constexpr uint32_t ARM_NEUTRAL_TIME_MS = 1000;

constexpr uint32_t CRSF_BAUD = 400000;
constexpr uint32_t CRSF_FRAME_PERIOD_US = 4000; // 250 Hz initial test rate
constexpr uint32_t USB_LOG_PERIOD_MS = 250;

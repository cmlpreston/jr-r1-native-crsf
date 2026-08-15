#pragma once

#include <Arduino.h>
#include "display_profile.h"

// ESP32-S3 DevKitC-1 pin assignments.
// GPIO19 and GPIO20 are deliberately not used: they are native USB D-/D+.
constexpr gpio_num_t THROTTLE_ADC_PIN = GPIO_NUM_1;
constexpr uint8_t STEERING_ADC_PIN = 2;
// Local transmitter-pack measurement. Connect only through the documented
// 47 kOhm / 10 kOhm divider; never connect a battery directly to this pin.
constexpr gpio_num_t TX_BATTERY_ADC_PIN = GPIO_NUM_5;
constexpr uint32_t TX_BATTERY_DIVIDER_TOP_OHMS = 47000;
constexpr uint32_t TX_BATTERY_DIVIDER_BOTTOM_OHMS = 10000;
constexpr gpio_num_t CRSF_SIGNAL_PIN = GPIO_NUM_17;
constexpr gpio_num_t BUZZER_PIN = GPIO_NUM_4;
constexpr uint8_t BUZZER_LEDC_CHANNEL = 7;

// Waveshare MCP23017 I/O expansion board on the same I2C bus as the selected
// touch controller. With A0/A1/A2 all open, Waveshare pulls all three address
// bits high and the board responds at 0x27.
constexpr uint8_t MCP23017_I2C_ADDRESS = 0x27;
constexpr uint32_t MCP23017_POLL_PERIOD_MS = 5;
constexpr uint8_t MCP23017_TEST_PORT = 0;       // PA0

// Measure these three values from the USB log with the trigger at:
// full brake/reverse, released/neutral, and full throttle.
// Measured on the original JR R1 5 kΩ throttle pot (2026-08-09):
// brake/reverse 1442–1452, released 1962–1986, forward 4007–4033.
constexpr uint16_t THROTTLE_ADC_MIN = 1447;
constexpr uint16_t THROTTLE_ADC_CENTER = 1970;
constexpr uint16_t THROTTLE_ADC_MAX = 4015;

// Neutral protection and output deadband. The module receives neutral until
// the trigger has remained close to centre for ARM_NEUTRAL_TIME_MS after boot,
// and this same ADC region maps to exactly zero command while running.
constexpr uint16_t ARM_NEUTRAL_ADC_WINDOW = 80;
constexpr uint32_t ARM_NEUTRAL_TIME_MS = 1000;
// Plausibility and freshness limits used by the independent CRSF task. A
// disconnected ADC or a stalled control loop can never leave a live throttle
// value latched on the radio link.
constexpr uint16_t THROTTLE_ADC_VALID_MIN = 100;
constexpr uint16_t THROTTLE_ADC_VALID_MAX = 4090;
constexpr uint32_t CONTROL_STALE_TIMEOUT_US = 250000; // 250 ms

constexpr uint32_t CRSF_BAUD = 400000;
constexpr uint32_t CRSF_FRAME_PERIOD_US = 4000; // 250 Hz initial test rate
constexpr uint32_t USB_LOG_PERIOD_MS = 250;

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
// Initial steering calibration. The pot is connected between 3V3 and GND
// with its wiper on GPIO2. A model-specific calibration can replace these
// full-scale defaults once the physical travel has been measured.
constexpr uint16_t STEERING_ADC_MIN = 0;
constexpr uint16_t STEERING_ADC_CENTER = 2048;
constexpr uint16_t STEERING_ADC_MAX = 4095;

// Neutral protection and output deadband. The module receives neutral until
// the trigger has remained close to centre for ARM_NEUTRAL_TIME_MS after boot,
// and this same ADC region maps to exactly zero command while running.
constexpr uint16_t ARM_NEUTRAL_ADC_WINDOW = 80;
constexpr uint32_t ARM_NEUTRAL_TIME_MS = 1000;
// Before a CUT -> LIVE transition, require fresh returned link statistics and
// a useful RF link continuously for this dwell. This is deliberately only a
// pre-arm interlock: a brief telemetry outage after arming must not command an
// unexpected throttle cut; receiver-side failsafe remains responsible then.
constexpr uint8_t ARM_MIN_LINK_QUALITY = 50;
constexpr uint32_t ARM_LINK_STABLE_TIME_MS = 500;
// Plausibility and freshness limits used by the independent CRSF task. A
// disconnected ADC or a stalled control loop can never leave a live throttle
// value latched on the radio link.
constexpr uint16_t THROTTLE_ADC_VALID_MIN = 100;
constexpr uint16_t THROTTLE_ADC_VALID_MAX = 4090;
constexpr uint32_t CONTROL_STALE_TIMEOUT_US = 250000; // 250 ms

constexpr uint32_t CRSF_BAUD = 400000;
constexpr uint32_t CRSF_FRAME_PERIOD_US = 4000; // 250 Hz initial test rate
// A warm ESP32 reset leaves the separately powered Ranger Nano running. Hold
// its inverted one-wire handset bus at physical idle-low for longer than the
// ExpressLRS handset UART watchdog before UART0 starts sending again. This
// lets the module abandon any partial frame/autobaud state just as a full S3
// power disconnect does, without requiring the Ranger itself to be reset.
constexpr uint32_t CRSF_STARTUP_BUS_QUIET_MS = 1500;
constexpr uint32_t USB_LOG_PERIOD_MS = 250;
// The long 4 Hz diagnostic line is useful during timing investigations, but
// USB CDC output on this S3 can disturb the single-wire CRSF task when a host
// opens the monitor. Keep transition/fault messages, but make the continuous
// stream an explicit diagnostic build option.
constexpr bool ENABLE_PERIODIC_USB_DIAGNOSTICS = false;

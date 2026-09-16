# Legacy DevKitC hardware and pin assignment reference

**Active hardware is now the XIAO ESP32-S3 PCB.** Use [the schematic (5) pin
map](xiao-pcb.md) for all current builds and uploads. This document preserves
the old prototype wiring; it does not describe the connected XIAO.

The former controller was an ESP32-S3 DevKitC-1 fitted with an
ESP32-S3-WROOM-1-N16R8 module (16 MB flash and 8 MB octal PSRAM), connected to a
Waveshare 3.5inch RPi LCD (F), the original R1 throttle potentiometer and buzzer,
and a RadioMaster Ranger Nano transmitter module. A build-time profile retains
support for the earlier BIGTREETECH TFT35 SPI V2.1.

## ESP32-S3 GPIO allocation

| ESP32-S3 pin | Direction | Connected device/signal | Electrical notes | Firmware symbol/status |
|---:|---|---|---|---|
| GPIO1 / ADC1_CH0 | Input | Original 5 kΩ throttle-pot wiper | Wiper through 1 kΩ; 100 nF from GPIO1 to GND; pot ends use 3.3 V and GND | `THROTTLE_ADC_PIN`; active |
| GPIO2 / ADC1_CH1 | Input | Original 5 kΩ steering-pot wiper | Reserved production endpoint; same 1 kΩ/100 nF input filtering as throttle | `STEERING_ADC_PIN`; reserved |
| GPIO4 | Output/PWM | Original passive piezo buzzer | 220–330 Ω series resistor; other piezo lead to GND | `BUZZER_PIN`; active |
| GPIO5 / ADC1_CH4 | Input | Local transmitter-pack voltage | Only through 47 kΩ/10 kΩ divider; 100 nF from GPIO5 to GND | `TX_BATTERY_ADC_PIN`; implemented, user-enabled |
| GPIO6 | Input | Keypad `+` | Active-low contact; internal pull-up | production map |
| GPIO7 | Input | Keypad `SELECT` | Active-low contact; internal pull-up | production map |
| GPIO8 | Bidirectional | Display touch `SDA` | Shared 3.3 V I²C | `TOUCH_SDA_PIN`; active |
| GPIO9 | Output | LCD data-command | 3.3 V logic | `TFT_DC_PIN`; active |
| GPIO10 | Unallocated | Waveshare LCD chip select is tied to GND | Available to future firmware; used as `TFT_CS_PIN` only by BTT fallback |
| GPIO11 | Output | LCD `MOSI` | 3.3 V SPI | `TFT_MOSI_PIN`; active |
| GPIO12 | Output | LCD `SCK` | 3.3 V SPI, presently 30 MHz | `TFT_SCK_PIN`; active |
| GPIO13 | Unallocated | Waveshare LCD `MISO` is not routed | Available to future firmware; used as `TFT_MISO_PIN` only by BTT fallback |
| GPIO14 | Input | Keypad `PAGE` | Active-low contact; internal pull-up | production map |
| GPIO15 | Input | Keypad `CLEAR` | Active-low contact; internal pull-up | production map |
| GPIO16 | Input | Display touch interrupt | Active-low, internal pull-up | `TOUCH_IRQ_PIN`; active |
| GPIO17 | Half-duplex UART | Ranger Nano `SIGNAL/CRSF` | Single-wire 400 kbaud 8N1 through 220–470 Ω series resistor | `CRSF_SIGNAL_PIN`; active |
| GPIO18 | Output | Display touch `SCL` | Shared 3.3 V I²C, presently 400 kHz | `TOUCH_SCL_PIN`; active |
| GPIO19 | Reserved | Native USB D- | Do not allocate while native USB programming/monitoring is used | reserved |
| GPIO20 | Reserved | Native USB D+ | Do not allocate while native USB programming/monitoring is used | reserved |
| GPIO21 | Input | Keypad `DOWN` | Active-low contact; internal pull-up | production map |
| GPIO40 | Output | Waveshare LCD reset | Used only by the default Waveshare profile | `TFT_RESET_PIN` |
| GPIO41 | Output/PWM | Waveshare LCD backlight | 5 kHz, 8-bit PWM; used only by Waveshare profile | `TFT_BACKLIGHT_PIN` |
| GPIO42 | Output | Waveshare GT911 reset/address select | Used only by the default Waveshare profile | `TOUCH_RESET_PIN` |
| GPIO47 | Input | Keypad `-` / custom | Active-low contact; internal pull-up | production map |
| 3V3 | Power output | Throttle potentiometer reference | ESP32 3.3 V rail; do not use for TFT or Ranger power | active |
| GND | Common | All low-voltage circuits | ESP32, TFT, Ranger, sensors and regulators require a common ground | active |
| 5V/VIN | Power input | Regulated 5 V for ESP32 | On this DevKit it is treated as an input in the final installation | planned supply |

GPIO3 and the remaining unallocated exposed GPIOs stay free for service and
future hardware. GPIO40/41/42 return to that pool when the BTT profile is used.

## Waveshare MCP23017 input expander

| MCP23017 | ESP32-S3 | Notes |
|---|---:|---|
| `VCC` | `3V3` | Keep the expander and its GPIO at 3.3 V |
| `GND` | `GND` | Common ground |
| `SDA` | GPIO8 | Shared with TFT touch I2C |
| `SCL` | GPIO18 | Shared with TFT touch I2C |
| `INTA`, `INTB` | Not connected | Polling during initial test |

With A0/A1/A2 open, the Waveshare board is expected at `0x27`. All sixteen
ports are configured as active-low inputs with internal pull-ups and are polled
every 5 ms. The currently active prototype allocation consumes exactly one
expander:

| MCP contact(s) | R1 control |
|---|---|
| PA0 | Timer button |
| PA1 | Drive Mode switch |
| PA2 / PA3 | Grip Trim A - / + |
| PA4 / PA5 | Grip Trim B - / + |
| PA6 / PA7 | Throttle Trim - / + |
| PB0 / PB1 | Steering Trim - / + |
| PB2 / PB3 | Aux 3 Trim - / + |
| PB4 | Idle Up switch |
| PB5 | Display switch |
| PB6 | Keypad MODE |
| PB7 | Keypad UP |

PA0 remains bound to CRSF CH4 while held as the current resolver proof. The
other contacts already produce debounced logical events and live monitor state;
their per-model functions are deliberately unassigned until the binding editor
is implemented.

### Planned keypad-PCB allocation — not active yet

The replacement eight-button keypad PCB routes the complete keypad to MCP23017
Port A. These schematic net names are the canonical mapping for that PCB:

| MCP contact | Schematic net | Physical key |
|---|---|---|
| PA0 | `KEY_MODE` | MODE |
| PA1 | `KEY_UP` | UP |
| PA2 | `KEY_PLUS` | `+` |
| PA3 | `KEY_SELECT` | SELECT |
| PA4 | `KEY_MINUS` | `-` |
| PA5 | `KEY_DOWN` | DOWN |
| PA6 | `KEY_CLEAR` | CLEAR |
| PA7 | `KEY_PAGE` | PAGE |

This allocation deliberately conflicts with the active breadboard/prototype
map above, where PA0 is Timer and PA1--PA7 are switches or trim contacts. Do
not replace `kR1InputMap` with the keypad allocation while testing the existing
hardware. Before the new PCB is installed, add a separately selected hardware
profile, allocate the remaining controls across PB0--PB7 and the six available
direct GPIO inputs, then validate every input in the Input Monitor while CUT.

## Display profile selection

`include/display_profile.h` is the single source of truth for panel controller,
pins, touch controller and backlight behaviour. The normal PlatformIO
environment uses the Waveshare profile. Environment
`esp32-s3-devkitc-1-btt` defines the BTT fallback without requiring a source
edit.

## Waveshare 3.5inch RPi LCD (F) connector (default)

The production PCB connector is JST PHD 14-way (2x7, 2.0 mm). Pins 1–13 follow
the panel signal order below and pin 14 is deliberately left unconnected.

| LCD signal | ESP32-S3 connection | Interface/function | Voltage |
|---|---:|---|---:|
| `VCC` | External regulated 3.3 V | Panel supply per official ESP32 example | 3.3 V |
| `GND` | GND | Common reference | 0 V |
| `LCD_MISO` | Not connected | Not required for normal display operation | — |
| `LCD_MOSI` | GPIO11 | LCD SPI data from ESP32 | 3.3 V logic |
| `LCD_CLK` | GPIO12 | LCD SPI clock | 3.3 V logic |
| `LCD_CS` | GND | Permanently selected; LCD is the only SPI peripheral | 0 V |
| `LCD_DC` | GPIO9 | LCD data/command | 3.3 V logic |
| `LCD_RST` | GPIO40 | ST7796 reset | 3.3 V logic |
| `LCD_BL` | GPIO41 | Backlight PWM | 3.3 V logic |
| `TP_SDA` | GPIO8 | GT911 touch I2C data | 3.3 V logic |
| `TP_SCL` | GPIO18 | GT911 touch I2C clock | 3.3 V logic |
| `TP_INT` | GPIO16 | GT911 touch interrupt | 3.3 V logic |
| `TP_RST` | GPIO42 | GT911 reset/address select | 3.3 V logic |

The panel uses an ST7796 at 320x480, rotated to the UI's 480x320 landscape
orientation. The capacitive GT911 is selected at 7-bit address `0x5D`; it shares
the I2C bus with the MCP23017 at `0x27`. Backlight PWM is 5 kHz/8-bit and permits
real brightness control and full backlight shutdown. Join the external 3.3 V
supply ground to ESP32 ground before connecting any signal.

## BIGTREETECH TFT35 SPI V2.1 connector (fallback)

| TFT35 marking | ESP32-S3 connection | Interface/function | Voltage |
|---|---:|---|---:|
| `+5V` | External regulated 5 V | LCD and backlight power | 5 V |
| `GND` | GND | Common reference | 0 V |
| `MISO` | GPIO13 | LCD SPI data to ESP32 | 3.3 V logic |
| `MOSI` | GPIO11 | LCD SPI data from ESP32 | 3.3 V logic |
| `SCK` | GPIO12 | LCD SPI clock | 3.3 V logic |
| `NSS` | GPIO10 | LCD chip select | 3.3 V logic |
| `RS` | GPIO9 | LCD data/command | 3.3 V logic |
| `SDA` | GPIO8 | NS2009 touch I²C data | 3.3 V logic |
| `SCL` | GPIO18 | NS2009 touch I²C clock | 3.3 V logic |
| `IDRQ` | GPIO16 | NS2009 touch interrupt, active low | 3.3 V logic |

The display controller is ILI9486 at 480x320. LVGL 8.4 supplies the UI layer;
Arduino_GFX remains the low-level ILI9486 transport. The touch controller is an
NS2009 on I²C address `0x48`.

Measured bench consumption for this TFT35 SPI V2.1 assembly is **0.25 A at
5.0 V**, or approximately **1.25 W**, with the screen and backlight operating.
Allow additional regulator capacity for the ESP32-S3 and transient/measurement
margin; this measurement is not the total transmitter load.

## Ranger Nano CRSF connection

| Ranger Nano connection | Connect to | Notes |
|---|---|---|
| `SIGNAL/CRSF` | ESP32 GPIO17 through 220–470 Ω | One-wire half-duplex CRSF at 400 kbaud, non-inverted physical UART waveform |
| `GND` | Common GND | Mandatory even when Ranger is USB-powered during bench testing |
| `VCC` | Switched transmitter pack or suitable regulated supply | Ranger specification supports the intended 3S LiFe pack; never connect to ESP32 3.3 V |
| USB | Separate USB source for bench testing only | Leave VCC disconnected when powering the module this way; join grounds |

The series resistor is for bus-contention current limiting. The tested value is
270 Ω. UART1 transmits and UART2 passively receives on GPIO17, with CRSF handled
on ESP32 core 0 and LVGL/application work on core 1.

## Original R1 throttle potentiometer

```text
ESP32 3V3 ---------------- pot end A
ESP32 GND ---------------- pot end B
pot wiper ---- 1 kΩ ------ GPIO1
                              |
                            100 nF
                              |
                             GND
```

The measured pot value is approximately 5 kΩ. Current stored/default ADC points
are approximately 1447 at full brake, 1970 at neutral and 4015 at full throttle.
The touchscreen calibration routine replaces these defaults in NVS.

## Original R1 buzzer

```text
GPIO4 ---- 220–330 Ω ---- passive piezo terminal 1
GND -------------------- passive piezo terminal 2
```

The original part behaves as a passive piezo and is substantially louder above
3 kHz. Current defaults are 3.2 kHz and a 10 ms key click.

## 3S LiFe transmitter-pack sensing

```text
switched pack + ---- 47 kΩ ----+---- GPIO5
                               |
                              10 kΩ
                               |
                              GND

GPIO5 ------------------ 100 nF ---- GND
```

The divider ratio is 5.7:1. A full 3S LiFe pack at 10.8–10.95 V produces about
1.9 V at GPIO5. The firmware defaults to 3S LiFe with a 9.0 V alarm and voltage
sensing disabled. Enable it only after fitting the divider, under
`SETUP > TX BATTERY`. Chemistry, cell count and alarm voltage are stored in NVS.

## Intended power distribution

```text
3S LiFe pack
    |
main R1 power switch
    +---- raw switched pack ---------------- Ranger Nano VCC
    +---- regulated 5 V --------------------- ESP32 5V/VIN
    +---- regulated 3.3 V ------------------- Waveshare LCD (F) VCC
    +---- 47k/10k voltage divider ----------- ESP32 GPIO5

All grounds ------------------------------------------------ common GND
```

Confirm both regulator current ratings and noise performance before fitting
them permanently. Keep the CRSF wire and analogue-pot wiring away from
switch-mode regulator inductors and high-current wiring. The earlier BTT TFT
alone measured 0.25 A from 5 V; measure the new panel before choosing the final
3.3 V regulator.

## Physical-input capacity

The original controls require 22 digital contacts: ten contacts for five
spring-return trims, four maintained/momentary controls, and eight keypad keys.
One MCP23017 supplies 16; six direct ESP32 GPIOs supply the remainder. Steering
and throttle are separate ADC inputs, so a second expander is not required.
The endpoint table in `src/r1_input_map.cpp` is the firmware source of truth.

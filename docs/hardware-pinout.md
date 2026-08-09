# Hardware and pin assignment reference

This document is the authoritative wiring reference for the JR R1 native-CRSF
prototype. The current controller is an ESP32-S3 DevKitC-1 connected to a
BIGTREETECH TFT35 SPI V2.1, the original R1 throttle potentiometer and buzzer,
and a RadioMaster Ranger Nano transmitter module.

## ESP32-S3 GPIO allocation

| ESP32-S3 pin | Direction | Connected device/signal | Electrical notes | Firmware symbol/status |
|---:|---|---|---|---|
| GPIO1 / ADC1_CH0 | Input | Original 5 kΩ throttle-pot wiper | Wiper through 1 kΩ; 100 nF from GPIO1 to GND; pot ends use 3.3 V and GND | `THROTTLE_ADC_PIN`; active |
| GPIO4 | Output/PWM | Original passive piezo buzzer | 220–330 Ω series resistor; other piezo lead to GND | `BUZZER_PIN`; active |
| GPIO5 / ADC1_CH4 | Input | Local transmitter-pack voltage | Only through 47 kΩ/10 kΩ divider; 100 nF from GPIO5 to GND | `TX_BATTERY_ADC_PIN`; implemented, user-enabled |
| GPIO8 | Bidirectional | TFT35 NS2009 touch `SDA` | 3.3 V I²C | `TOUCH_SDA_PIN`; active |
| GPIO9 | Output | TFT35 LCD `RS` / data-command | 3.3 V logic | `TFT_DC_PIN`; active |
| GPIO10 | Output | TFT35 LCD `NSS` / chip select | 3.3 V logic | `TFT_CS_PIN`; active |
| GPIO11 | Output | TFT35 LCD `MOSI` | 3.3 V SPI | `TFT_MOSI_PIN`; active |
| GPIO12 | Output | TFT35 LCD `SCK` | 3.3 V SPI, presently 25 MHz | `TFT_SCK_PIN`; active |
| GPIO13 | Input | TFT35 LCD `MISO` | 3.3 V SPI | `TFT_MISO_PIN`; active |
| GPIO16 | Input | TFT35 touch `IDRQ` | Active-low, internal pull-up | `TOUCH_IRQ_PIN`; active |
| GPIO17 | Half-duplex UART | Ranger Nano `SIGNAL/CRSF` | Single-wire 400 kbaud 8N1 through 220–470 Ω series resistor | `CRSF_SIGNAL_PIN`; active |
| GPIO18 | Output | TFT35 NS2009 touch `SCL` | 3.3 V I²C, presently 400 kHz | `TOUCH_SCL_PIN`; active |
| GPIO19 | Reserved | Native USB D- | Do not allocate while native USB programming/monitoring is used | reserved |
| GPIO20 | Reserved | Native USB D+ | Do not allocate while native USB programming/monitoring is used | reserved |
| 3V3 | Power output | Throttle potentiometer reference | ESP32 3.3 V rail; do not use for TFT or Ranger power | active |
| GND | Common | All low-voltage circuits | ESP32, TFT, Ranger, sensors and regulators require a common ground | active |
| 5V/VIN | Power input | Regulated 5 V for ESP32 | On this DevKit it is treated as an input in the final installation | planned supply |

GPIO2, GPIO3, GPIO6, GPIO7, GPIO14, GPIO15 and the higher exposed GPIOs remain
unassigned in the present firmware. Reserve at least one ADC-capable input for
the steering potentiometer before assigning button inputs.

## BIGTREETECH TFT35 SPI V2.1 connector

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
    |                                      +- TFT35 +5V
    +---- 47k/10k voltage divider ----------- ESP32 GPIO5

All grounds ------------------------------------------------ common GND
```

Confirm the final regulator current rating and noise performance before fitting
it permanently. Keep the CRSF wire and analogue-pot wiring away from switch-mode
regulator inductors and high-current wiring.

## Not yet assigned

The steering potentiometer, programming keypad, five digital trims, drive-mode
switch, idle-up switch, timer button, display switch and remaining internal JST
and 12-way FFC conductors have not yet received final GPIO assignments. Their
functional inventory and proposed CRSF roles are recorded in
[`r1-control-map.md`](r1-control-map.md). Update this file and `include/config.h`
together whenever a new physical input is mapped.


# JR R1 native CRSF controller

This is the first, deliberately small firmware scaffold for replacing the JR R1
computer with an ESP32-S3 DevKitC-1. It reads the original 5 kΩ throttle pot and
sends a standard 16-channel CRSF channels frame to a RadioMaster Ranger Nano.

Only CRSF channel 2 changes. Channel 1 (future steering) and channels 3–16 remain
centred. The BIGTREETECH TFT35 SPI V2.1 provides the first channel-configuration
page with persistent brake endpoint, throttle endpoint, trim and direction.

The consolidated wiring and GPIO allocation is maintained in
[`docs/hardware-pinout.md`](docs/hardware-pinout.md). The original R1 control
inventory and future assignment plan is in
[`docs/r1-control-map.md`](docs/r1-control-map.md).

## Safety

For the first test, disconnect the buggy motor from the ESC or place the buggy on
a secure stand. The firmware holds channel 2 at neutral until the trigger remains
near its configured centre for one second.

Always fit the Ranger antenna before powering the module. Begin at the lowest RF
power setting.

## Throttle potentiometer wiring

Do **not** use GPIO20. On the ESP32-S3 DevKitC-1, GPIO20 is native USB D+, while
GPIO19 is USB D-. Using it can break the USB programming/monitor connection.

Use GPIO1 for the initial analogue input:

```text
ESP32-S3 3V3 ---------------- throttle pot end A
ESP32-S3 GND ---------------- throttle pot end B
throttle pot wiper -- 1 kΩ -- GPIO1
                                  |
                                100 nF
                                  |
                                 GND
```

Reversing the two end wires reverses trigger direction. Do not feed the pot from
5 V: an ESP32 GPIO must never receive more than 3.3 V.

## Ranger Nano wiring

The module-bay interface is a single-wire, half-duplex CRSF bus at 400 kbaud.
A Saleae connected to the physical signal wire decodes it as ordinary
non-inverted 8N1 UART. On this ESP32-S3 implementation, both UART directions
use the GPIO-matrix inversion flag to reproduce/read that measured waveform.

```text
ESP32-S3 GPIO17 -- 220–470 Ω -- Ranger Nano SIGNAL / CRSF
ESP32-S3 GND ------------------ Ranger Nano GND

separate regulated 6–8.4 V ---- Ranger Nano VCC
supply negative --------------- common GND
```

The Ranger Nano's official XT30 specification is 6–16.8 V, while its module-bay
diagram labels the bay supply 5–12 V. Do not power it from the ESP32 3.3 V pin.
For the current bench setup, its main USB port may instead be powered from the
separate USB battery pack, with grounds joined and the VCC pin left disconnected.

The 220–470 Ω series resistor limits contention current if both sides briefly
drive the half-duplex bus. The ESP32 CRSF logic level is 3.3 V.

## Transmitter battery voltage

The local transmitter pack is measured by the ESP32 rather than obtained from
the CRSF receiver telemetry. Connect the switched pack positive to GPIO5 through
a 47 kΩ / 10 kΩ divider, with a 100 nF capacitor from GPIO5 to ground:

```text
switched pack + ---- 47 kΩ ----+---- GPIO5
                               |
                              10 kΩ
                               |
                              GND

GPIO5 ------------------ 100 nF ---- GND
```

This divider is safe through a 4S lithium pack. A fully charged 3S LiFe pack
(about 10.8–10.95 V) presents about 1.9 V to GPIO5. Configure chemistry, cell
count, low alarm and voltage-sense enable under `SETUP > TX BATTERY`. Defaults
are 3S LiFe and a 9.0 V low alarm. Voltage sensing is disabled by default so an
unwired ADC input cannot create false readings or alarms during USB testing.

## Calibration

Tap `CAL` on the TFT35 CH2 page. While calibration is active, CH2 is held at
neutral. Move the trigger to full brake, then full throttle, then release it and
leave it centred for approximately 1.5 seconds. The measured minimum, centre and
maximum are validated and saved in ESP32 NVS, and are restored at the next boot.

The compiled `THROTTLE_ADC_*` values remain as safe defaults if NVS does not yet
contain a valid calibration. Direction is selected separately on the CH2 page.

## BIGTREETECH TFT35 SPI V2.1 wiring

The screen board takes **5 V power**, but its LCD, touch and interrupt signals
are **3.3 V logic** and connect directly to the ESP32-S3.

| TFT35 label | ESP32-S3 DevKitC pin | Purpose |
|---|---:|---|
| `+5V` | `5V`/regulated 5 V | LCD and backlight power |
| `GND` | `GND` | Common ground |
| `MISO` | GPIO13 | LCD SPI data to ESP32 |
| `MOSI` | GPIO11 | LCD SPI data from ESP32 |
| `SCK` | GPIO12 | LCD SPI clock |
| `NSS` | GPIO10 | LCD chip select |
| `RS` | GPIO9 | LCD data/command |
| `SDA` | GPIO8 | NS2009 touch I2C data |
| `SCL` | GPIO18 | NS2009 touch I2C clock |
| `IDRQ` | GPIO16 | NS2009 touch interrupt, active low |

For the first bench test, the TFT `+5V` can use the DevKitC `5V` pin while the
DevKitC is powered by laptop USB. If the backlight causes USB instability, use a
separate regulated 5 V source and retain the common ground. Never connect TFT
`+5V` to ESP32 `3V3`.

The first UI constrains each endpoint to 50–100% and sub-trim to +/-20%.
Sub-trim shifts neutral and both endpoints together instead of clipping travel
at one end. The +/-20% limit retains the normal full travel inside CRSF's
absolute 0–2047 channel envelope. As with the original R1, endpoint positions
should be checked again after changing sub-trim. Touch raw and mapped coordinates
are printed over USB; use those readings to refine the four
`TOUCH_RAW_*` calibration constants in `include/config.h` if required.

The transmitter boots into a scalable main menu. `CHANNELS` and `SETUP` are
active; the remaining roadmap sections are shown dimmed and disabled:

- `MODELS`: select/name/copy/reset models and later transfer/import/export;
- `CHANNELS`: names, reverse, calibration, sub-trim and endpoints per channel;
- `RESPONSE`: dual rate, exponential, throttle curves and speed limiting;
- `MIXES`: programmable mixes and input-to-output routing;
- `DRIVE AIDS`: throttle hold, ABS, idle-up and quick-throttle functions;
- `TIMERS`: run, countdown and lap timers;
- `LINK/TELEM`: ELRS link, failsafe, RF settings and telemetry pages;
- `SETUP`: display, sound, controls, warnings and system preferences.

This reorganises the original R1 System/Function menus without duplicating
their historical navigation constraints. The channel browser exposes all 16
CRSF channels in two banks. CH2 opens the implemented throttle configuration
page; the other channel entries are stable placeholders for future pages.

## Original R1 passive piezo buzzer

The original two-wire buzzer measures open circuit and is treated as a passive
piezo transducer. Connect it directly to GPIO4 through a protective series
resistor:

```text
ESP32-S3 GPIO4 -- 220-330 ohm -- piezo terminal 1
ESP32-S3 GND ------------------ piezo terminal 2
```

Do not connect either piezo terminal to 5 V in this direct-drive arrangement.
The `SETUP` page provides persistent buzzer on/off, key-click on/off, pitch and
click-duration settings plus a test button. These global settings use the
`jr-r1-system` NVS namespace and are independent of the CH2/model settings.
Timed tones are stopped by the ESP32's dedicated tone task, so TFT redraw time
does not lengthen short key clicks.

## Build and monitor

```sh
pio run -t upload
pio device monitor -b 115200
```

UART1 transmits channel frames and UART2 listens continuously on the same GPIO.
The listener sees harmless echoes of the outgoing channel frames plus Ranger
return traffic. The firmware currently decodes CRSF link statistics (RSSI and
link quality) and receiver/battery voltage. A bench capture confirmed LQ 100%,
roughly -35 dBm RSSI, and 6.6 V from the bound ER6G.

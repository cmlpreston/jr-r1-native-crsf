# JR R1 native CRSF controller

This is the first, deliberately small firmware scaffold for replacing the JR R1
computer with an ESP32-S3 DevKitC-1. It reads the original 5 kΩ throttle pot and
sends a standard 16-channel CRSF channels frame to a RadioMaster Ranger Nano.

The throttle role defaults to CRSF channel 2 but can be moved to any of the 16
channels from the TFT. Channel 1 is reserved for future steering input by
default; all inputs not yet implemented remain centred. The BIGTREETECH TFT35
SPI V2.1 provides persistent role, endpoint, trim, direction, rate and
exponential configuration.

The consolidated wiring and GPIO allocation is maintained in
[`docs/hardware-pinout.md`](docs/hardware-pinout.md). The original R1 control
inventory and future assignment plan is in
[`docs/r1-control-map.md`](docs/r1-control-map.md).
Deferred work is tracked in [`TODO.md`](TODO.md).

## Capturing a TFT screenshot

The firmware accepts `SCREENSHOT` over its native USB serial connection. It
reads the ILI9486 display memory through the TFT35 `MISO` connection and streams
a framed 480x320 RGB888 image without allocating a full framebuffer. Capturing
forces throttle CUT because the main control loop pauses during the transfer.

Stop PlatformIO's serial monitor so the capture tool can open the port, then run:

```sh
./.venv/bin/python tools/capture_tft.py dashboard.png
```

The script automatically selects a single `/dev/cu.usbmodem*` device. Use
`--port /dev/cu.usbmodem101` if more than one is connected. If the first capture
has red and blue exchanged, repeat it with `--swap-rb`; `--flip-x` and `--flip-y`
are available for diagnosing controller read orientation.

The dashboard channel monitor is adaptive and displays enabled channels only.
Steering and throttle remain prominent labelled horizontal bars. When enabled
auxiliary outputs are added by the future 16-channel output model, up to six are
shown as a compact vertical bank on the right (`+N` reports further enabled
outputs). Link quality and RSSI share one line, while the Link/Safety card
changes background and border colour for CUT, LIVE, waiting, and fault states.

## Safety

For the first test, disconnect the buggy motor from the ESC or place the buggy on
a secure stand. Every boot starts in **CUT** with the arm request cleared. Select
Walk or Normal, tap the dashboard safety panel, and leave the trigger near its
configured centre for one second. Assignment, role, mode and calibration changes
immediately publish neutral and require this process again.

The CRSF task independently checks the age of the last control-loop update. If
it exceeds 250 ms, it transmits neutral even if the UI/core has stalled.
Implausible ADC values, calibration and a disabled throttle assignment also fail
closed.

Control sampling and response processing run in a dedicated 4 ms high-priority
task, independently of LVGL rendering. Full-screen page changes therefore
cannot interrupt steering/throttle updates; the CRSF task's 250 ms stale check
monitors this control task rather than the display loop.

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

Receiver-pack voltage arrives through ER6G CRSF telemetry. Configure its pack
chemistry, cell count, low-voltage alarm and alarm enable independently under
`SETUP > RX BATTERY`. The RX defaults are 2S LiFe and 6.0 V. LiFe, LiPo,
Li-ion and NiMH profiles are available; changing chemistry or cell count proposes
a suitable threshold, which can then be adjusted manually.

## Calibration

Tap `CAL` on the selected throttle-channel page. While calibration is active,
the selected output is held at neutral. Move the trigger to full brake/reverse,
then full forward, then release it and
leave it centred for approximately 1.5 seconds. The measured minimum, centre and
maximum are validated and saved in ESP32 NVS, and are restored at the next boot.

The calibration state machine and ADC-to-permille mapping live in the generic
`ProportionalInput` component. Calibration belongs to the physical input rather
than its assigned CRSF output, so remapping the trigger from channel 2 does not
move or duplicate its calibration. The trigger uses the `jr-r1-trigger` NVS
namespace; an existing calibration in the former `jr-r1-ch2` namespace is used
as a migration default. Steering and other proportional controls can each use
another `ProportionalInput` instance and their own input-scoped NVS namespace.

The compiled `THROTTLE_ADC_*` values remain as safe defaults if NVS does not yet
contain a valid calibration. Direction is selected separately on the CH2 page.

At 100% rate and endpoint the transmitter sends the standard CRSF travel range
of 172–1811 (992 neutral). ExpressLRS normally maps that to approximately
988–2012 us PWM at the receiver. Although CRSF can encode the wider absolute
range 0–2047, 1811 is conventional full servo/ESC travel; dashboard bars are
scaled to 172–1811 so this is displayed as full scale. Before calibrating an
ESC, confirm the USB log reaches `requested=1811` at full forward and
`requested=172` at full brake/reverse (and the same `sent` values when safely
armed), then teach the ESC those actual receiver endpoints according to its own
calibration procedure.

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

`CHANNELS > ROLES` selects the steering and throttle CRSF channel numbers; the
same channel cannot hold both roles. `RESPONSE`, available from the main menu or
the throttle page, provides independent forward and brake/reverse rate and expo.
Rate is 0–100%. Expo is -100% to +100%; positive expo softens response around
neutral, while negative expo makes it more aggressive. Processing order is:

```text
physical trigger side -> expo -> rate -> endpoint -> direction reverse -> sub-trim
```

This ordering is deliberate: `FORWARD` and `BRAKE/REV` always describe the
physical trigger actions. Reversing the output changes which CRSF end those
actions drive, without accidentally exchanging their endpoint or response
settings. `BRAKE/REV` is used because a car ESC commonly brakes on the first
reverse-trigger command and supplies reverse drive only after stopping.

`CONTROL MAP` is the first functional version of the generic
input-to-function-to-channel assignment layer. It enables/disables the steering
and trigger assignments and maps each to a CRSF channel. Additional buttons and
switches will appear there as their hardware is mapped.

`DRIVE MODES` selects WALK or NORMAL. WALK scales output to an adjustable limit
(35% by default). Arm/CUT is a separate safety state controlled from the
dashboard's `LINK / SAFETY` panel. Every boot and every drive-mode change forces
CUT; the trigger must return to neutral before the user can arm again.

The throttle `SPEED` page configures independent forward and brake/reverse slew
times from neutral to endpoint. Zero milliseconds means direct response.

`SETUP > DISPLAY` provides timeout, software brightness, night palette and
immediate sleep. Touch wakes the panel. Because the TFT35 connector does not
expose backlight control, software dimming changes pixels but does not eliminate
the measured backlight current; that needs a transistor/MOSFET-controlled
backlight or 5 V rail.

The transmitter boots into a scalable main menu. `CHANNELS`, `RESPONSE`,
`CONTROL MAP`, `DRIVE MODES`, and `SETUP` are active; deferred sections remain
dimmed:

Every non-dashboard page reserves the top-right header position for a Home
icon that returns directly to the dashboard. The page-local Back, Cancel, or
primary action remains immediately to its left. Using Home during input
calibration safely cancels calibration, retains the previous calibration, and
returns the transmitter to CUT.

- `MODELS`: select/name/copy/reset models and later transfer/import/export;
- `CHANNELS`: names, reverse, calibration, sub-trim and endpoints per channel;
- `RESPONSE`: dual rate, exponential, throttle curves and speed limiting;
- `CONTROL MAP`: current input enable/assignment and future programmable mixes;
- `DRIVE MODES`: walk/normal now; ABS, 4WS and throttle curves later;
- `TIMERS`: run, countdown and lap timers;
- `LINK/TELEM`: ELRS link, failsafe, RF settings and telemetry pages;
- `SETUP`: display, sound, controls, warnings and system preferences.

The programmable physical-control architecture, proposed default R1 bindings,
safety constraints and implementation checklist are maintained in
[`docs/control-assignment-roadmap.md`](docs/control-assignment-roadmap.md).

This reorganises the original R1 System/Function menus without duplicating
their historical navigation constraints. The channel browser exposes all 16
CRSF channels in two banks. Whichever channel has the throttle role opens the
implemented throttle configuration page; other channel entries remain stable
placeholders until their input and configuration pages are implemented.

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

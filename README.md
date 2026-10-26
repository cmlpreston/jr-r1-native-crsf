# JR R1 native CRSF controller

The active hardware is the **Seeed XIAO ESP32-S3** two-expander PCB, using the
final schematic (5) pin map in [`docs/xiao-pcb.md`](docs/xiao-pcb.md). The default
target is `seeed_xiao_esp32s3-pcb` (Waveshare RPi LCD (F), 480×320).
Use only the XIAO until the user explicitly changes the hardware.

The current control/channel architecture, invariants and staged refactor are
documented in [`docs/architecture-review.md`](docs/architecture-review.md).
Pure host-side modules can be checked with `make -C test run`.

This is the first, deliberately small firmware scaffold for replacing the JR R1
computer with an ESP32-S3 DevKitC-1. It reads the original 5 kΩ throttle pot and
sends a standard 16-channel CRSF channels frame to a RadioMaster Ranger Nano.

The throttle role defaults to CRSF channel 2 but can be moved to any of the 16
channels from the TFT. Channel 1 is reserved for future steering input by
default; all inputs not yet implemented remain centred. A selectable display
profile supports the Waveshare 3.5inch RPi LCD (F) by default and retains the
BIGTREETECH TFT35 SPI V2.1 as a build-time fallback. The touchscreen UI provides
persistent role, endpoint, trim, direction, rate and exponential configuration.

Current wiring and GPIO allocation are in [`docs/xiao-pcb.md`](docs/xiao-pcb.md).
[`docs/hardware-pinout.md`](docs/hardware-pinout.md) preserves the legacy DevKitC wiring. The original R1 control
inventory and future assignment plan is in
[`docs/r1-control-map.md`](docs/r1-control-map.md).
Deferred work is tracked in [`TODO.md`](TODO.md).

The integrated LVGL UI now has configurable Drive widgets and Drive/Tune/Data/Setup
navigation. See the [firmware port and retained-menu audit](docs/ui-firmware-port.md)
for widget controls, telemetry assumptions and remaining hardware validation.

Choose **Setup → Handset → Display → Colour scheme** for Light, Graphite / teal,
Midnight / ice blue, Warm charcoal / ivory, or Slate / soft lilac. The choice
applies immediately and persists across restarts. The assignable **Night mode**
action switches between Light and the last selected dark scheme. Safety colours
and the red JR wordmark remain consistent across schemes.

In **Tune → Steering → Endpoint**, steer left/right or tap the **Left / Right**
value to select a side, then use the on-screen or handset **+ / −** keys.
Returning to centre keeps the selected side. Both drafts are retained while
switching sides; **Apply** saves them and **Cancel** discards them. **Reset**
resets only the selected endpoint. Selection follows physical steering before
channel reversal. Throttle and brake endpoints remain separate Tune tabs.

## Desktop UI simulator

The complete LVGL touchscreen UI can be built and exercised on macOS without
the transmitter, Ranger, receiver or display connected. This is the real
`TransmitterUi` implementation—not a separate visual mock. SDL replaces the LCD
and touch controller, a small compatibility layer replaces Arduino NVS and
timing, and simulated CRSF data supplies link quality and pack voltage.

Install the host dependencies once, fetch the same LVGL 8.4 release used by
PlatformIO, then build and run:

```sh
brew install sdl2 openssl@3
cd simulator
make bootstrap
make run
```

Use the mouse as the touchscreen. Hold `W` for forward or `S` for brake,
press `L` to toggle the simulated ELRS link, and use `[`/`]` to vary receiver
voltage. `F12` saves `simulator/screenshot.bmp`; `Esc` exits. Settings are
persisted beneath `simulator/state/`, so configuration changes survive a
restart. Delete that ignored directory to simulate erased NVS.

For automated layout checks without opening a visible window:

```sh
SDL_VIDEODRIVER=dummy ./build/jr-r1-sim \
  --run-ms 500 --screenshot /tmp/dashboard.bmp
```

Landscape 480×320 is the required transmitter layout. To run the matching
desktop build:

```sh
cd simulator
make run
```

Use `build/jr-r1-sim --page dashboard`, `--page menu`, or `--page calibration`
with the screenshot arguments above for automated landscape captures. The
matching active hardware build is:

```sh
.venv/bin/pio run -e seeed_xiao_esp32s3-pcb
```

Portrait simulator builds (`make portrait-run`) remain available for regression
coverage. Hardware uploads use landscape only, as directed on 2026-10-26.

The simulator also emulates the ExpressLRS UID response used by the bindphrase
page. It does not emulate RF timing, UART electrical behaviour, ESP32 tasks or
receiver failsafe operation; those remain hardware-in-the-loop tests. QEMU is
therefore unnecessary for normal UI work and would provide less useful touch
and screenshot iteration than the shared-code SDL build.

### Browser demo of the current interface

Run `make -C simulator browser`, then open <http://127.0.0.1:8767>.
The browser displays frames rendered by the actual C++/LVGL UI and forwards
touches and keypad actions to the simulator. It is not an HTML recreation of
the transmitter screens. The local server uses Python's standard library and
binds to loopback only. `Ctrl+C` stops it and both simulator processes.

Landscape is the required transmitter orientation from 2026-10-26. Portrait
builds are retained for simulator/regression coverage; hardware uploads use landscape only.
Mouse/touch, MODE/PAGE/CLEAR/SELECT, arrow navigation, +/- edits, simulated arm
requests, steering/throttle sliders, link loss and voltage changes are available.
Inputs return to neutral on browser focus loss. For calibration, uncheck
`Return to centre when released` to hold a sample at an endpoint.

Demo preferences persist separately under `simulator/state-browser/landscape`
and `simulator/state-browser/portrait`. The existing native simulator state and
hardware settings are unaffected. TX configuration and binding replies are
emulated; this demo does not reproduce the ESP32 task scheduler, full physical
binding pipeline, RF timing or receiver/ESC failsafe behaviour.

The standalone browser version is published at
[jr-r1-demo.vercel.app](https://jr-r1-demo.vercel.app/). It runs the same C++/LVGL
UI through WebAssembly and needs no local simulator server. Build and Vercel
update instructions are in [simulator/web/README.md](simulator/web/README.md).

The CRSF output path maintains requested, safe and enabled state independently
for all 16 channels. The control task publishes a complete atomic snapshot to
the CRSF task. A final, host-tested arbiter applies stale-control precedence
immediately before each channel frame and resolves the entire snapshot to
requested values only while Armed. Steering, throttle and proportional AUX
bindings share one response/adjustment/slew pipeline, while the complete
22-contact R1 digital-input layer is mapped through one MCP23017 plus six direct
GPIOs. Unassigned controls generate diagnostic events but cannot affect output
channels until given an explicit binding.

## Capturing a TFT screenshot

The simulator can write screenshots directly. The firmware also accepts
`SCREENSHOT` over native USB when built for the BTT profile, reading display
memory through its SPI `MISO` connection. The production Waveshare PHD cable is
write-only (`LCD_MISO` is not fitted), so physical-panel capture is deliberately
disabled for that profile. Capturing on a supported profile forces throttle CUT
because the main control loop pauses during the transfer.

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
it exceeds 250 ms, the final arbiter transmits the complete safe channel model
even if the UI/core has stalled. Implausible ADC values, calibration and a
disabled throttle assignment also fail closed. These deterministic safe frames
are the first protection layer; if the S3 or CRSF serial stream fails entirely,
the receiver's separately configured ExpressLRS failsafe remains the backup.
Future model-profile persistence will make every safe position explicitly
configurable and allow it to be matched at the receiver.

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
The receive setting has also been checked experimentally: removing its matrix
inversion produces no valid Ranger return frames.

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

### ELRS bindphrase from the touchscreen

With ExpressLRS 4.1 or newer on the addressed device, open
`MENU > LINK / TELEM`. The page implements ExpressLRS's MSP command `0x2D`
inside CRSF `MSP_REQ`/`MSP_WRITE` frames. Its on-screen keyboard accepts up to
51 characters and stores the desired phrase locally in ESP32 NVS.

`SET TX` addresses the locally connected Ranger Nano and therefore works even
when no receiver is bound. The module reboots after the write; firmware waits
1.2 seconds and automatically asks it for its six-byte UID. The displayed UID
is compared with the UID calculated locally from the phrase and reports
`MATCH` or `DIFFERS`. `UID TX` performs the read without changing anything.

`SET RX` and `UID RX` remain disabled until valid RF link telemetry is present:
an unbound receiver cannot receive these commands. When changing a linked pair,
write the receiver first and the transmitter second. All management operations
force CUT and require the normal neutral/arm process afterwards. `CLEAR` only
clears the text entry; blank writes are deliberately refused to prevent an
accidental unbind.

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
Calibration applies an additional exponential moving average (1/8 new sample,
7/8 previous value). After the trigger settles in either endpoint region for
about 120 ms, its endpoint estimate continuously follows that filtered value
for as long as the endpoint remains held. It therefore progressively forgets
the transition toward the stop and converges on the resting endpoint instead
of latching one extreme sample. Neutral likewise uses the settled filtered
value. This rejects transition samples and prevents a single ADC noise spike
from becoming a permanent calibration value. Hold each endpoint briefly—the
longer it remains held, the more completely the estimate settles—before
returning to neutral.

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

## Display profiles

The installed display is selected centrally in `include/display_profile.h`.
The normal build defaults to `DISPLAY_PROFILE_WAVESHARE_RPI_35F`. To reconnect
the original BTT panel without editing source, build or upload the dedicated
fallback environment:

```sh
pio run -e esp32-s3-devkitc-1-btt -t upload
```

Both profiles preserve the common SPI and I2C pins. Controller type, reset,
touch implementation and backlight behaviour are isolated behind the profile.

### Legacy DevKitC Waveshare 3.5inch RPi LCD (F) wiring

For the active XIAO, use [the PCB pin map](docs/xiao-pcb.md).

The production PCB uses a JST PHD 14-way (2x7, 2.0 mm) connector and adapts it
to the panel's 13-way connector; PHD pin 14 is spare. The official ESP32-S3
example powers the panel connector from **3.3 V**. An external regulated 3.3 V
supply is suitable; join its ground to the ESP32 ground before connecting
signal wires.

| LCD signal | ESP32-S3 DevKitC pin | Purpose |
|---|---:|---|
| `VCC` | External regulated `3.3V` | Panel power |
| `GND` | `GND` | Common ground |
| `LCD_MISO` | Not connected | Not required for normal display operation |
| `LCD_MOSI` | GPIO11 | LCD SPI data from ESP32 |
| `LCD_CLK` | GPIO12 | LCD SPI clock |
| `LCD_CS` | GND | Permanently selected; LCD is the only SPI peripheral |
| `LCD_DC` | GPIO9 | LCD data/command |
| `LCD_RST` | GPIO40 | ST7796 reset |
| `LCD_BL` | GPIO41 | Backlight PWM |
| `TP_SDA` | GPIO8 | GT911 touch I2C data |
| `TP_SCL` | GPIO18 | GT911 touch I2C clock |
| `TP_INT` | GPIO16 | GT911 touch interrupt |
| `TP_RST` | GPIO42 | GT911 reset/address select |

The firmware uses ST7796 rotation 3, GT911 address `0x5D`, and real 5 kHz
backlight PWM. Display sleep therefore turns both the controller and backlight
off, and the brightness setting controls the physical backlight.

### BIGTREETECH TFT35 SPI V2.1 wiring (fallback)

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

For a bench test, use a regulated 5 V source and retain the common ground. Never
connect TFT `+5V` to ESP32 `3V3`.

The first UI constrains each endpoint to 50–100% and sub-trim to +/-20%.
Sub-trim shifts neutral and both endpoints together instead of clipping travel
at one end. The +/-20% limit retains the normal full travel inside CRSF's
absolute 0–2047 channel envelope. As with the original R1, endpoint positions
should be checked again after changing sub-trim. Touch raw and mapped coordinates
are printed over USB; use those readings to refine the four
`TOUCH_RAW_*` calibration constants in `include/display_profile.h` if required.

`CHANNELS > ROLES` selects the steering and throttle CRSF channel numbers; the
same channel cannot hold both roles. Every channel fed by an assigned
proportional control exposes its own persistent `RESPONSE / RAMP` editor from
the channel page. The main-menu `RESPONSE` shortcut opens the assigned throttle
channel. Each profile provides independent positive/negative endpoints, rate,
expo and ramp, plus sub-trim and direction. Labels follow the assigned role:
steering uses right/left, throttle uses forward/brake-reverse, and other
proportional channels use positive/negative. The live graph shows the
unmodified linear response in grey, the effective configured response in cyan,
endpoint rails and the translated sub-trim neutral in amber, and the current
input/output point. The output axis spans +/-120% so retained travel after
sub-trim remains visible instead of being clipped at the ordinary +/-100%
bounds.
Rate is 0–100%. Expo is -100% to +100%; positive expo softens response around
neutral, while negative expo makes it more aggressive. Processing order is:

```text
physical input side -> expo -> rate -> endpoint -> direction reverse -> sub-trim
```

This ordering is deliberate: `FORWARD` and `BRAKE/REV` always describe the
physical trigger actions. Reversing the output changes which CRSF end those
actions drive, without accidentally exchanging their endpoint or response
settings. `BRAKE/REV` is used because a car ESC commonly brakes on the first
reverse-trigger command and supplies reverse drive only after stopping.

`CONTROL MAP` is the first functional version of the generic
input-to-function-to-channel assignment layer. Its paged TFT list covers all
nineteen physical controls. Steering and trigger open the primary role editor;
each digital switch, trim or button opens an assignment editor that can enable
or disable the input, select a non-reserved CRSF channel, choose a compatible
momentary/toggle/pulse/two-position/trim behaviour, save it, or remove it.
Spring-return trims can instead target `TRIM` or `SUBTRIM` on any channel. The
same assignment is visible from that channel's configuration page, where one
of the five R1 trim controls can be selected or the parameter left unassigned.
Trim values use 1% steps over +/-20%, survive restarts in `jr-r1-chparam`, and
are applied to the role-owned steering/throttle output paths. Subtrim translates
the full output envelope; ordinary trim remains within conventional travel.
Enabled bindings cannot share an output channel or take the steering/throttle
role channels. Assignments are saved in the versioned `jr-r1-bind` NVS
namespace. Saving or removing one forces CUT and requires neutral re-arm before
the new map becomes live. The Timer button to CH4 proof binding is installed as
the default profile when no valid stored profile exists.

`DRIVE MODES` selects WALK or NORMAL. WALK scales physical forward drive to an
adjustable limit (35% by default) while retaining the fully configured
brake/reverse output. The physical trigger direction is used, so channel
reversal cannot accidentally limit braking instead. Sub-trim remains an
invariant centre offset and is never scaled by WALK. Arm/CUT is a separate safety state controlled from the
dashboard's `LINK / SAFETY` panel. Every boot and every drive-mode change forces
CUT; the trigger must return to neutral before the user can arm again. CUT,
calibration, control reset and stale-control protection all hold the configured
trimmed neutral rather than jumping to the untrimmed CRSF midpoint.
The neutral-interlock ADC window is also the throttle output deadband. Normal
spring-return/pot variation inside that window therefore produces exactly zero
drive demand, while travel outside it is remapped to retain both full endpoints.

The per-channel `RAMP` page configures independent positive and negative
neutral-to-endpoint slew times for any assigned proportional channel. A value
of `0 ms` is direct response. Labels follow the assigned role (`RIGHT/LEFT` for
steering and `FORWARD/BRAKE-REV` for throttle), while other proportional AUX
channels use `POSITIVE/NEGATIVE`.

`SETUP > DISPLAY` provides timeout, brightness, night palette and immediate
blanking. It sends the standard DCS `Display OFF` command (`0x28`) and
wakes with `Display ON` (`0x29`). `Sleep IN` (`0x10`) is deliberately not
combined with blanking because that left this TFT35-SPI V2.1 panel solid white.
Touch wakes the panel. The Waveshare profile also switches its exposed
backlight PWM fully off. Because the BTT connector does not expose backlight
control, its software dimming or blanking may not eliminate the measured
backlight current; that needs a transistor/MOSFET-controlled backlight or 5 V
rail.

The transmitter boots into a scalable main menu. `CHANNELS`, `RESPONSE`,
`CONTROL MAP`, `DRIVE MODES`, and `SETUP` are active; deferred sections remain
dimmed:

Every non-dashboard page reserves the top-right header position for a Home
icon that returns directly to the dashboard. The page-local Back, Cancel, or
primary action remains immediately to its left. Using Home during input
calibration safely cancels calibration, retains the previous calibration, and
returns the transmitter to CUT.

Touch actions are dispatched on release. A gesture beginning during the short
post-action guard interval is discarded through its complete release rather
than being promoted into a tap on the newly drawn page. Coordinates are also
filtered across the gesture to suppress resistive-panel lift-off drift.

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
pio run                                             # XIAO PCB landscape build
pio run -e seeed_xiao_esp32s3-pcb -t upload   # Active XIAO hardware
pio device monitor -b 115200
```

The historical DevKitC build used 145,864 of 327,680 bytes of statically allocated
internal RAM (44.5%) and 822,937 of 6,553,600 application-flash bytes (12.6%).
The editable nineteen-input binding table is small compared with LVGL and its
display buffers, so assignments do not create a practical memory risk. Boot
logging reports total/free/minimum/largest heap, flash size, PSRAM size and the
heap consumed by UI initialisation for ongoing runtime monitoring. The project-
local `esp32-s3-devkitc-1-n16r8` PlatformIO board definition matches the fitted
ESP32-S3-WROOM-1-N16R8: 16 MB quad flash, the standard 16 MB OTA partition table,
and 8 MB octal PSRAM. A flashed-board check reported 16,777,216 flash bytes and
8,386,295 usable PSRAM bytes. The firmware detects PSRAM but does not yet depend
on it.

UART0 operates the Ranger connection as a single-wire half-duplex CRSF bus on
GPIO17. Firmware alternates the GPIO matrix between transmit and receive while
leaving the UART driver installed, closely matching the handset transport used
by ExpressLRS. It currently decodes CRSF link statistics (RSSI and link quality)
and receiver/battery voltage. A bench capture confirmed LQ 100%, roughly -35 dBm
RSSI, and 6.6 V from the bound ER6G.

TX module settings are available under **LINK / TELEM**. See
[ExpressLRS TX configuration](docs/elrs-tx-configuration.md) for controls,
protocol limits and hardware verification.

## Source backup

The reviewed 2026-10-26 working snapshot is described in
[the backup record](docs/backup-2026-10-26.md), including bench settings and
validation limits.

## Receiver S.Port telemetry firmware

The ER4/ER6G receiver-integrated S.Port translator is maintained separately in
[cmlpreston/ExpressLRS-SPort](https://github.com/cmlpreston/ExpressLRS-SPort),
branch `codex/sport-telemetry`, based on ExpressLRS 4.1.0. Its drivers, shared
sensor decoding, host tests, hardware builds and guarded upload workflow have
moved there. This repository retains transmitter CRSF parsing/display/alarms
and its original XIAO S3 upload preflight.

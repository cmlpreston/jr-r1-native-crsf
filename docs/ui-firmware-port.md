# LVGL widget UI port — September 2026

The main browser demo compiles the same `TransmitterUi` C++ implementation as
firmware. Drive, Tune, Data and Setup use a shared day/night palette and fixed
LCD pixel layout. Established detail editors retain their handlers and settings,
with matching colours, fonts and panel styling. The HTML trial stays at `/trial/`.

The default hardware orientation is **320×480 portrait**, with 480×320 landscape
available explicitly. Both orientations expose the existing functionality.
Portrait detail pages use stacked labels/values, paged response controls and
a compact binding keyboard. Setup groups Model and Handset functions in flat
rows. A dark header, active-tab underline and permanent red JR / light R1 strip
follow the approved mockup. Simulator checks do not establish physical LCD contrast,
touch accuracy, electrical correctness, RF timing or receiver failsafe behaviour.

## Functionality audit

| Existing function | Route in the integrated UI |
| --- | --- |
| Dashboard, output state, model and link | Drive; fixed header remains visible |
| Model memories and selection safeguards | Header model or Setup → Models; CUT and neutral required |
| Channel configuration and role assignment | Setup → Model → Channels; role editor retained |
| Rate, expo, forward/brake endpoints and ramps | Tune; independent throttle/brake drafts; detailed channel editors retained |
| Channel trim, subtrim and reverse | Tune second bank and existing channel detail pages |
| Legacy response-profile subtrim | Detailed response editor, separately from channel-parameter subtrim |
| Binding source/context/target/behaviour, add/remove/save | Setup → Model → Control map and binding editor |
| Raw switch and ADC inspection | Setup → Handset → Input monitor |
| Normal/Walk mode and forward limit | Drive mode button or Setup → Drive modes |
| Buzzer, clicks, pitch, duration and test | Setup → System |
| Calibration and startup switch warning | Existing calibration/system entry points and startup flow |
| Brightness, sleep, screensaver and night mode | System → Display |
| TX chemistry, cells, low alarm and voltage sensing | Handset → Batteries → Transmitter |
| RX chemistry, cells and low alarm enable | Handset → Batteries → Receiver |
| Firmware branch, commit, environment and board | System → Firmware info |
| TX module parameter discovery/edit/command | Setup → Model → RF module |
| TX/RX UID and binding phrase | RF module → Binding |
| ESC programming preview | Setup → Model → ESC settings; remains a preview, no new ESC transport |
| Detailed RF/link and module diagnostics | Header LQ → Radio diagnostics |
| Requested/safe channel monitoring | Data → Channels |

Numeric Tune edits are drafts until Apply. Cancel does not change control
settings. CUT is immediate; requesting arm from the header requires a touch
held for at least 800 ms. Keypad selection cannot reuse an earlier long touch.
The output safety arbiter and neutral interlocks remain authoritative. A future
optional power-on PIN unlock is recorded in TODO; it is not implemented here.

## Widgets and persistence

Drive has a two-column, two-row grid. Widgets occupy 1×1, 2×1 or 2×2 cells.
Double-width panels start in the left column; a 2×2 panel starts at top left.
Choose an anchor through **Drive → Layout**, edit and Apply. Applying replaces
any overlapping panels; Cancel preserves the layout. Empty removes a panel.

Available types are timer, paired TX/RX voltage, one/two channels, individual
sensor, GPS trail and model image. Styles are restricted to useful combinations:
power has numbers/bars/dials; channels also offer graph/switch styles; sensors
have numbers/history. Sensor values display unavailable/stale states explicitly.
The default is a full-width timer with paired voltage numbers and CH1/CH2 bars
below. Drive → Layout → Use suggested layout restores it for the active model.

`include/dashboard_widgets.h` holds the bounded descriptor and placement rules.
`widgets-v2` stores four six-byte cells for each of ten model memories (240 bytes).
Older `widgets-v1` choices migrate into the first slot. Layouts are independent
of channel/control settings. Current timer runtime resets on model selection,
continues off the Drive page, and does not stop merely because its widget is
hidden. Timer start/stop and reset are assignable physical actions; existing
button assignments are preserved.

The Hyper 7 uses the existing artwork and is treated as electric. Other models
currently use a fallback icon. Logo uploads, an S3 setup portal and offline map
backgrounds are future work. The GPS display is a bounded relative trail/grid,
with zoom and trail reset; it does not fetch map tiles. Graph histories are short
in-memory samples, not a background telemetry recording/export service.

## Telemetry contract

Firmware decodes CRC-checked CRSF frames for GPS (0x02), RPM (0x0C) and temperature
(0x0D), in addition to existing battery/link frames. GPS coordinates and heading
are range checked. RPM uses signed 24-bit values; temperature uses signed tenths
of a degree C. Only source zero's first RPM/temperature is currently displayed;
TX/RX temperature sources are not silently shown as vehicle temperature.
Actual motor/ESC meaning depends on the vehicle telemetry bridge.

GPS and RPM expire after three seconds, temperature after ten. Position/speed
views additionally require at least four satellites, a display heuristic rather
than a guarantee of fix quality. Invalid input is not replaced with demo values.
The GPS speed decoder uses **0.1 km/h per unit**, matching deployed ELRS and
Betaflight encoding; the current TBS text differs on this scaling.

References: [TBS CRSF specification](https://github.com/tbs-fpv/tbs-crsf-spec/blob/main/crsf.md),
[Betaflight CRSF encoder](https://github.com/betaflight/betaflight/blob/master/src/main/telemetry/crsf.c),
[ELRS MAVLink bridge](https://github.com/ExpressLRS/ExpressLRS/blob/master/src/lib/MAVLink/MAVLink.cpp).

The browser's explicitly labelled sensor checkbox supplies fictional GPS,
speed, temperature and RPM in simulator code only. Hardware requires actual
incoming sensors. Battery current/mAh, trip accumulation and offline maps are
not implemented by this port.

## Validation and hardware trial

```sh
make -C test run
make -C simulator -j6 audit
make -C onboard/sport-crsf test
.venv/bin/pio run -e esp32-s3-devkitc-1-portrait -e seeed_xiao_esp32s3-pcb-portrait
# Activate Emscripten 4.0.15 before:
sh simulator/build_web.sh
```

The native audit uses disposable preferences and exercises actual callbacks:
Setup routes and Model/Handset tabs, numeric Apply/Cancel, independent brake values, widget
placement/persistence, model-switch guards, timer continuity, arm hold, focus
bounds and LVGL free heap. Both orientations cover every established menu. Portrait touch checks also
exercise navigation, role adjustment, response pagination and keyboard entry. Twelve host test binaries cover the control,
input, safety, PCB I/O, TX configuration, telemetry and widget models. The
vehicle bridge has a separate protocol test target.

Before uploading, follow `AGENTS.md`: fetch main, confirm branch ancestry and
connected board/display/pin map, build/test, state the selected profile and use
PlatformIO upload with preflight. Successful DevKitC and XIAO builds do not imply
a firmware image has been flashed or physically tested.

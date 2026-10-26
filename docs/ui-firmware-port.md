# LVGL widget UI port — September 2026

The main browser demo compiles the same `TransmitterUi` C++ implementation as
firmware. Drive, Tune, Data and Setup use a shared day/night palette and fixed
LCD pixel layout. Established detail editors retain their handlers and settings,
with matching colours, fonts and panel styling. The HTML trial stays at `/trial/`.

The default hardware orientation is **320×480 portrait**, with 480×320 landscape
available explicitly. Both orientations expose the existing functionality.
Portrait detail pages use stacked labels/values, paged response controls and
a compact binding keyboard. Setup groups Model and Handset functions in flat
rows. A dark header, active-tab underline and permanent red JR / light R-1 strip
follow the approved mockup. Simulator checks do not establish physical LCD contrast,
touch accuracy, electrical correctness, RF timing or receiver failsafe behaviour.

## Functionality audit

| Existing function | Route in the integrated UI |
| --- | --- |
| Dashboard, output state, model and link | Drive; fixed header remains visible |
| Model memories and selection safeguards | Header model or Setup → Models; CUT and neutral required |
| Channel configuration and role assignment | Setup → Model → Channels → Role / physical input |
| Rate, expo, forward/brake endpoints and ramps | Tune; independent throttle/brake drafts; detailed channel editors retained |
| Channel trim, subtrim and reverse | Tune second bank and existing channel detail pages |
| Legacy response-profile subtrim | Detailed response editor, separately from channel-parameter subtrim |
| Binding source/context/target/behaviour, add/remove/save | Setup → Model → Control map and binding editor |
| Raw switch and ADC inspection | Data → Inputs or Setup → Handset → System → Input diagnostics |
| Normal/Walk mode and forward limit | Drive mode chooser or Setup → Model → Drive modes |
| Buzzer, clicks, pitch, duration and test | Setup → Handset → Sounds / alerts |
| Calibration and startup switch warning | Existing calibration/system entry points and startup flow |
| Brightness, sleep, screensaver and night mode | Setup → Handset → Display |
| TX chemistry, cells, low alarm and voltage sensing | Setup → Handset → TX battery |
| RX chemistry, cells and low alarm enable | Setup → Model → RX battery |
| Firmware branch, commit, environment and board | System → Firmware info |
| TX module parameter discovery/edit/command | Setup → Model → RF module |
| TX/RX UID and binding phrase | RF module → Binding |
| ESC programming preview | Setup → Model → ESC settings; remains a preview, no new ESC transport |
| Detailed RF/link and module diagnostics | Header LQ → Radio diagnostics |
| Requested/safe channel monitoring | Data → Channels |

Numeric Tune edits are drafts until Apply. Cancel does not change control
settings. Manual CUT and enable both require holding Drive for at least 800 ms;
automatic safety CUT remains immediate. Keypad selection cannot reuse an earlier long touch.
The output safety arbiter and neutral interlocks remain authoritative. Boot PIN
unlock is implemented and never requests arming.

## Widgets and persistence

Drive has a two-column, two-row grid. Widgets occupy 1×1, 2×1, 1×2 or 2×2 cells.
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
currently use a fallback icon. Logo uploads, an S3 setup portal and user-uploadable map
backgrounds are future work. The GPS display has a bounded trail, zoom/reset
controls and a built-in Highfield Park OSM vector basemap (see
`assets/maps/README.md`); it falls back outside local coverage and does not
fetch map tiles. Speed widgets include maximum/average, selectable primary
value and an assignable statistics-reset action. Graph histories are short
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
incoming sensors. Battery current/mAh, trip accumulation and arbitrary map uploads remain
outstanding; the built-in local map is implemented.

## Validation and hardware trial

Receiver S.Port firmware and compatibility tests now live in the separate
[ExpressLRS-SPort fork](https://github.com/cmlpreston/ExpressLRS-SPort). Run
`make -C sport test` from that receiver checkout.

```sh
make -C test run
make -C simulator -j6 audit
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

## Navigation refinement — 2026-09-30

Drive no longer carries a Layout button. Dashboard layout lives under
Setup → Model → Dashboard, with an 800 ms long press on the Drive heading
as a shortcut. The Normal/Walk chip opens a focused chooser; changing modes
forces CUT, and the Walk forward limit remains separate from brake/reverse.

Channel configuration has one hub for role/input, response/trim and
mixing/output limits. Role changes refuse conflicting ownership or existing
channel bindings. The global control map remains for transmitter functions and
physical-control overview. Tune and channel response share the focused numeric
editor; auxiliary quick adjustments explicitly select positive/negative travel.
Detailed response settings use the same graph-and-value editor, with independent
positive/negative sides and Basic/More sections. Response offset remains separate
from channel trim and subtrim. Physical trim/subtrim controls use a named picker
with Apply/Cancel; controls already assigned elsewhere cannot be taken over.

Assignments have Assignment and Behaviour tabs. Fields follow the selected target:
functions and drive modes use their action's behaviour, while channel assignments
expose switch/button mode. Contexts include plain-language explanations. Changing
tabs preserves the draft; Cancel discards it and Apply validates before saving.

Drive navigation shows a lock while CUT and a green dot while LIVE, with
a hold progress indicator. All four navigation labels share the same alignment
and size, without hold hints on Drive. Short taps
only navigate; early release cancels output changes. Tap the Drive heading
for blocking reasons, or hold that heading for dashboard layout. Both manual
output changes require a physical 800 ms hold; remote input cannot enable.
Automatic safety CUT remains immediate. Boot PIN and output enable are separate.
Legacy detail headers use a Drive navigation shortcut with the same hold policy.
When enabling waits for a receiver, the title bar shows Link wait and the
output-status page explicitly shows LINK WAIT and the blocking reason. Drive
keeps its centered label and lock until output is actually LIVE.

Leaving Walk mode requires a fresh PIN entry from either the LCD/web mode chooser
or a bound physical switch. Wrong PIN or Cancel keeps Walk active. Walk is retained
across reboot and model changes; successful exit selects Normal with outputs CUT.
An exit requested by a maintained switch is prompted once until that switch returns
to Walk, avoiding repeated prompts after Cancel.

Setup → Handset → System → PIN changes the four-digit PIN by checking the current
PIN and entering the new PIN twice. The authenticated web page also has a PIN form.
The initial PIN remains 1234 until changed. Forgotten PIN recovery requires USB
serial access: send `PINRESET 1234` (or another four-digit PIN) with outputs CUT.
Recovery preserves models, Wi-Fi, maps and Walk mode, and does not unlock or arm
the handset. RF bench authorization uses the saved PIN too.

Mixing uses named sections rather than a circular eight-page walk. Section
changes preserve drafts; Cancel at the overview discards them and Apply checks
and saves them. Preview bars and the curve are linked directly.

GPS gives the map the area below the Data tabs, with a status overlay and compact
zoom/trail controls. After four seconds without interaction it becomes a full-screen
map, keeping North and map attribution. The first touch only reveals controls;
no invisible navigation or output-enable targets remain. Saved raster maps fill
the viewport with their aspect ratio preserved, and zoom works before a GPS fix.
Live GPS uses the same map scale for imagery, trace and position marker.

Wi-Fi connection settings and web credentials have separate landing pages.
Restart belongs to System. Sounds no longer duplicates battery/display links.
TX battery settings are handset-owned; RX battery settings are model-owned.
The browser cannot close its transport through a mirrored navigation gesture;
physical Close ends the manual setup session under the existing network policy.

Validation uses the actual native LVGL callbacks in both orientations. It does
not replace testing touch feel, LCD visibility or RF operation on the handset.


### Four-cell S.Port telemetry (2026-10-18)

The ER4 trial forwards the verified FLVS ADV `0xa1/0x0300` four-cell scan
as CRSF voltage group `0x0e`, source 0, four big-endian millivolt values.
The transmitter accepts complete, plausible cell groups atomically and retains
the last readings for 20 seconds without a valid cell update before showing `--`.
This allows intermittent sensor/RF updates without flickering values. These values are separate from `0x08` receiver supply.
No cell alarm threshold is inferred from the pack type.

Open **Data → Sensors**, then the right arrow to **Drive battery 2 / 3**
for pack total, lowest cell and cells 1–4, shown to three decimal places in volts.
Tap a reading for its history. Sensor dashboard widgets also offer these six
sources after the original six; existing persisted sensor IDs are unchanged.
Missing/stale readings show `--` and do not become zero-voltage measurements.
The read-only USB command `TELEMETRY` reports validity/link/age and, when fresh,
`@CELLS_MV` readings for bench verification without enabling outputs.

The host UI audit feeds an actual-format CRSF frame through the production
parser, checks cards, details, widgets and missing values, and renders the cells
page. RF reception remains a hardware check; receiver WiFi mode stops RF.

### Combined battery cells widget

Dashboard → widget settings → Type **Battery cells** groups the vehicle pack's
individual cell voltages in one panel. All four dashboard sizes are supported.
The widget includes the detected cell count, pack total and lowest cell. Old
single-cell dashboard widgets migrate to this group; pack-total and lowest-cell
sensor widgets retain their original IDs.

**Cell count** defaults to **Auto (telemetry)**, or can be set to 1S–12S in each
widget's saved settings. Auto uses the number of individual cells reported by
CRSF source 0 voltage-group telemetry, not an estimate from total pack voltage.
A manual count specifies expected cells: missing measurements remain `--`, and
a count mismatch is reported rather than inventing voltages. Stale measurements
are cleared from the live display. The transmitter parser accepts 1–12 individual readings;
the current receiver trial forwards 4S, so larger packs require receiver work.


### FAS300 ADV bench forwarding (2026-10-26)

The receiver trial profiles now select the captured `0x22` FAS300 ADV source:
`0x0400` temperature, `0x0200` current (0.1 A), `0x0210` voltage (0.01 V).
Temperature uses the existing sensor card and displays whole degrees C, matching
the S.Port source resolution. CRSF still carries tenths internally (24 C is
encoded as 240); the same whole-degree display applies to details, history
labels and dashboard widgets. FAS voltage and current are on
**Data → Sensors → right arrow twice → FAS300 ADV 3 / 3**, and are available
as dashboard Sensor sources. Existing widget IDs are preserved.

CRSF battery `0x08` ID 1 supplies current; legacy/no-ID and explicit ID 0 retain
receiver supply. General-voltage group `0x0e` source 128 retains FAS voltage
at millivolt precision, separate from the drive cells. Voltage and current become stale
after 3 seconds (Temperature retains its existing 10-second policy), and a zero
remains a valid reported value. Current above 300 A
is displayed with **Check range** for FrSky comparison rather than suppressed.
The USB `TELEMETRY` command includes `@FAS` raw/scaled values. Hardware readings
must still be compared with the known FrSky setup before relying on calibration.


### Steering endpoint direction and editing response

Tune → Steering → Endpoint uses the selected steering channel's reversal when
selecting Left/Right from calibrated input. Labels, stored draft values, preview
curve and Apply share that mapping; the underlying input-side response storage
and channel processing are unchanged. Existing models need no migration.

Side changes and +/−, Reset and Fine/Coarse update existing controls in place,
keeping touch targets alive and limiting redraw to the affected labels/curve.
Apply still commits both drafts and puts outputs in CUT; return to Drive and
re-arm to test the changed travel. Cancel leaves the applied profile unchanged.
Validation covers normal/reversed steering, touch Apply through the production
CRSF output calculation, persistence, and output changes after a fresh arm dwell.
Physical servo travel and handset touch latency remain hardware checks.

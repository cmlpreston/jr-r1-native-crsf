# JR R-1 EVO

**A new computer for a favourite old radio: custom ESP32-S3 transmitter firmware, a replacement PCB, colour touchscreen, and ExpressLRS with S.Port sensor telemetry.**

<p align="center">
  <img src="assets/project/2026-10-31-r1-evo.jpg" width="480" alt="The rebuilt JR R-1 with its original wheel and controls, Ranger Nano antenna and landscape touchscreen dashboard">
  <br><em>The working R-1 EVO, photographed on 31 October 2026.</em>
</p>

The aim is to keep what makes the JR PROPO R-1 enjoyable to use—its pistol grip,
steering wheel, trigger, trims and physical buttons—while replacing the ageing
computer and radio system with electronics and software we can develop ourselves.
The result is an ongoing, working conversion rather than a new transmitter shell:
a custom PCB inside the R-1, a Seeed XIAO ESP32-S3, a 3.5-inch touchscreen and a
RadioMaster Ranger Nano running ExpressLRS.

The project covers **both ends of the link**: independently developed handset
firmware in this repository and a customised ExpressLRS receiver firmware fork
that adds direct FrSky S.Port sensor support. The handset software is **not an
OpenTX or EdgeTX fork**. It implements its own control pipeline, model settings,
LVGL interface and CRSF integration; the RF link relies on ExpressLRS.

[Build story](#from-original-radio-to-r-1-evo) · [Features](#what-we-have-built) ·
[Simulator](#the-same-ui-on-the-desktop) · [Web UI](#the-web-ui-inside-the-transmitter) ·
[Schematics](#hardware-and-schematics) · [Development guide](DEVELOPMENT.md)

## What we have built

This describes the development prototype as of **31 October 2026**. Features
have been built and exercised incrementally; this is not a finished commercial
radio or a claim that every sensor/hardware combination has been validated.
Recent development is also tracked on
[`codex/safety-control-refactor`](https://github.com/cmlpreston/jr-r1-native-crsf/tree/codex/safety-control-refactor).

| Area | Implemented in the prototype |
| --- | --- |
| Controls and outputs | Native 16-channel CRSF output; original steering, throttle, trims, grip controls, switches and eight-button keypad; configurable control assignments and channel roles. |
| Tuning and mixing | Reversal, calibration, trims, rates, expo, independent endpoints, response speed, channel-to-channel mixes, switch conditions and graphs showing the resulting response. |
| Models | Saved model configurations, names, selectable vehicle artwork and primary/accent/base colour choices, including an F1-style car. |
| Dashboard | Configurable widgets in 1×1, 2×1, 1×2 and 2×2 sizes; analogue channel bars, binary switch states, selectable physical inputs, speed dial, timer, power readings and a combined battery-cell panel. |
| Telemetry | CRSF link and sensor decoding, freshness/no-data handling, history views and alarms; custom receiver integration for S.Port cells and FAS sensor measurements. |
| Maps | Address search, pan/zoom and selection of street or satellite imagery in the browser; image packages stored on the handset, GPS trail overlay and a map view that hides its controls when idle. |
| Operating modes | CUT/re-arm workflow with readiness reasons, Normal/Walk selection, PIN-protected exit from Walk, boot PIN and USB PIN recovery. |
| Connectivity | Saved home Wi-Fi with automatic connection/retry, optional setup hotspot, configurable hostname and onboard configuration/remote-control web UI. |
| Development tools | Shared-code desktop/browser simulator, screenshot and layout checks, host tests, boot logs, memory diagnostics, serial input monitor and local-build/Pi-USB flashing workflow. |

The channel monitor can show live control positions while CUT holds transmitted
outputs safe. A display value is therefore not, by itself, proof of servo output.
The transmitter's CUT state also does not replace receiver/ESC failsafe setup.

### Channel setup and hardware-control mapping

Configuration is organised around the output channel and the physical control
that drives it. Steering/throttle roles can be assigned to different CRSF
channels; they are not permanently tied to CH1 and CH2.

| Configuration | Available settings |
| --- | --- |
| Per-channel response | Normal/reversed direction; separate negative/positive endpoints, rates, expo and response times; trim and subtrim; channel setup linked to its control assignment. |
| Physical-input calibration | Steering/trigger calibration and neutral handling belong to the physical input; output travel/endpoints belong to the channel. |
| Mix sources | Two independently enabled mix contributions per destination; physical inputs, CH1–16, or configured steering/throttle as sources; include the existing channel input or use mixes only. |
| Mix calculation | Add or replace; positive/negative weights; follow source trim; constant contribution; final minimum/maximum; auxiliary-channel CUT value; reset to default settings. |
| Mix activation | Always, selected switch low/high, Walk or Normal; proportional source or above/below threshold with trigger point and hysteresis. |
| Switched rates | Select a physical switch; independent negative/positive rates for low and high positions, with configurable transition time. |
| Mix preview | Contribution/output bars and a response graph; sweep an input and inspect switch/mode effects without transmitting the preview. |
| Control assignment | Enable/disable; select a supported transmitter function, model parameter, drive-mode action or CRSF output; choose the channel/parameter and when the binding applies. |
| Control behaviour | Proportional, momentary, toggle, two-position, spring-return three-position, stepped, three-position selector or pulse, as appropriate to the chosen control and target. |
| Assignment range/state | Minimum/maximum, step, reverse control, initial and safe values, pulse duration and retained state where supported. |
| Assignment context | Always, dashboard, menus, CUT only or armed only. Unavailable combinations are rejected rather than silently assigned. |
| Monitor visibility | Choose channels/physical controls shown in All Channels, including timer switch and grip trims; horizontal or vertical analogue bars, binary indicators for suitable switch assignments. |

A practical mix example is **throttle → CH3 gyro gain**: start with a constant
gain, add a negative throttle contribution, and limit the final gain range so
it stays on the intended side of the gyro's mode boundary. A switch can enable
the mix or select rates. This configures the receiver's gain channel; the
handset does not itself implement a vehicle stabilisation gyro.

### Other configuration areas

- **Models and dashboard:** model selection/name, vehicle image and three
  colours; widget type, size, position and display options. Enlarging a widget
  preserves obscured widgets for when it shrinks again.
- **Drive and timer:** Normal/Walk and forward Walk limit; deliberate CUT/re-arm;
  timer start/stop/reset. Some operations require neutral and fresh inputs/link.
- **Telemetry and battery:** selectable sensor widgets and histories, battery
  chemistry/cell-count/alarm settings, transmitter voltage sensing, vehicle-cell
  widget Auto or expected 1S–12S, and stale/missing-data indication. Expected
  cell count does not estimate individual cell voltages from pack total.
- **ExpressLRS module:** device discovery, supported module-provided settings,
  bindphrase/UID workflow and parameter readback. Packet rate, telemetry ratio,
  power and other choices depend on the connected module's advertised parameters.
- **Maps:** street/satellite style, address/location, zoom and download area,
  map name/source attribution, import/export/upload and storage preparation;
  map zoom, trail visibility/reset and fullscreen controls on the handset.
- **Handset:** display orientation, colour scheme, brightness/night behaviour,
  screensaver, PIN change/recovery, input calibration, saved Wi-Fi connection,
  automatic connection option, setup hotspot, hostname and software restart.

Some source enums and prototype menus reserve future actions such as macros,
logging or ESC programming. Their presence in code is not a claim that those
features are complete. Per-receiver output mapping and failsafe configuration
also remain distinct from transmitter channel setup.

## The Drive dashboard: choose what matters for each model

Each model has its own two-column, two-row widget layout. Open **Setup → Model →
Dashboard**, choose a position, then select the widget type, size, style and
source and **Apply**. The resulting widgets appear on **Drive**. Sizes are 1×1,
2×1, 1×2 and 2×2, where they fit from the selected position. A larger widget
covers neighbouring widgets without deleting their saved configuration; reducing
it reveals them again. Use **Empty** to leave a position unused.

| Widget | What it shows and how it can be configured |
| --- | --- |
| **Timer** | Elapsed run time, with Start/Stop and Reset controls. It can also be operated through supported handset timer assignments. |
| **TX / RX power** | Transmitter-pack voltage from the local ADC and receiver supply from telemetry. Choose numbers, bars or dials. These are separate measurements, not the vehicle's individual cell voltages. |
| **Channels** | One chosen channel and an optional second channel. Choose numbers, bars, dials, graph or switch-style presentation. Useful for focusing on steering/throttle or an auxiliary output. |
| **All channels** | Configured channels, physical hardware inputs, or both. Choose which entries appear; analogue values use horizontal/vertical bars or numeric presentation, while suitable binary inputs use switch indicators. Tap to page through entries that do not fit. This can include the timer switch, grip trims and other connected controls; the unwired DISPLAY_SW is not an active input. |
| **Sensor — speed** | Current, maximum or average speed as the large reading, with related statistics. Choose numbers, a dial or graph. Speed requires the relevant vehicle/GPS telemetry; the handset does not infer it from throttle position. |
| **Sensor — other readings** | Temperature, RPM, receiver voltage, link quality, satellite count, drive-pack total, lowest cell, FAS voltage or FAS current. Choose the supported numeric/graph style. Missing or stale readings display as unavailable rather than zero. |
| **Battery cells** | All reported vehicle cells in one widget, with pack total, lowest cell and detected count. Choose Auto or an expected 1S–12S count. Tap to page if needed. The present receiver cell trial forwards 4S; larger UI capacity does not imply larger-pack receiver validation. |
| **GPS map / trail** | A north-up saved street/satellite image or vector map, with vehicle position and recorded GPS trail overlaid when telemetry is available. The map fills the widget with a north indicator and attribution. Tap for the larger GPS page, zoom and trail controls; controls hide when idle and reappear on touch. |
| **Model image** | Current model's name and selected vehicle artwork, with configurable primary, accent and base paint colours. Tap to reach model selection. |
| **Empty** | No widget in that position. |

Tap a sensor for its detail/history view, the power widget for radio diagnostics,
or a map for the GPS view. Styles are filtered to suit the widget: for example,
the speed dial is not offered for an arbitrary temperature sensor.

Channel widgets remain useful while outputs are CUT: they show **requested
control positions**, with a small crossed-out output indicator. Data's channel
monitor distinguishes requested values from the actual safe values sent.

## CUT, Normal/Walk and PIN protection

**CUT inhibits live control output; it does not turn off the RF module or stop
CRSF channel frames.** The control/transport safety layers send configured safe
values instead of the live requested values. Auxiliary CUT overrides can be
configured separately; they are not receiver RF-loss failsafe settings.

The transmitter starts with outputs inhibited. CUT is also used for structural
channel/mix/control edits, model and mode changes, calibration, hardware recovery
and remote interactions that need to enter configuration safely. Faults such as
invalid inputs/configuration or a stale control update keep outputs inhibited.
Ordinary navigation is separate from output enable; changing pages is not an
instruction to arm.

The **Drive** navigation control carries the output-state icon. Deliberately
hold it to request enabling, then release as prompted. The readiness view
explains blockers such as **Link wait** or **neutral required**. Enabling needs
the expected valid inputs/configuration, startup readiness, a qualifying link
and neutral throttle. Holding Drive also protects an intentional return to CUT
from an accidental tap. Unlocking a PIN never arms the transmitter.

**Normal** uses the configured output response. **Walk** limits physical forward
throttle to a configurable percentage (35% by default), while retaining the
configured brake/reverse authority. Mode selection is integrated into Drive;
changing mode forces CUT and requires deliberate re-arm at neutral. Leaving
Walk requires the PIN again. An abandoned Walk-exit PIN prompt times out after
10 seconds, returns to Drive and leaves Walk/CUT in force.

The LCD UI has a **boot PIN**, automatic submission when the correct four digits
are entered, PIN change and a **Forgot PIN?** USB recovery guide. The initial
trial PIN is `1234` and can be changed. Web access has its own authentication:
authenticated configuration can be available while the handset PIN is locked,
and the mirrored LCD supports PIN entry. These are separate access states.
A PIN unlock does not bypass output readiness or receiver/ESC failsafe setup.

Loss of the return telemetry link after arming is currently advisory; receiver
RF-loss behaviour remains the receiver's responsibility. CUT is a transmitter
control feature, not a substitute for configuring and testing that failsafe.

## Retaining and assigning the original hardware controls

We retain the R-1's wheel, trigger, trims, grip controls, switches and eight
keypad buttons. Their identities are separated from GPIO wiring, so control
assignment is not dictated by the label printed on the old case. The Control
map associates supported inputs with channels, transmitter functions, model
parameters or drive-mode actions, with behaviour, ranges and context settings.
For example, a suitable switch can operate an auxiliary channel and a trim can
adjust a channel parameter rather than its original R-1 function.

**Current keypad limitation:** the latest navigation implementation reserves
MODE/PAGE/SELECT/CLEAR/UP/DOWN/+/- before the assignment resolver. Although the
catalogue/web editor includes these buttons, using MODE as a general RF-channel
button requires a further precedence/remapping change. This is an intended
extension, not a capability claimed by the current screenshots.

## A look around the interface

These are **simulator captures**, rendered by the shared C++/LVGL UI from
firmware checkpoint `8dda44e`. They are not fully pixel-perfect reproductions
of the installed LCD: scaling, colours, viewing angle, touch response, timing
and simulated data differ. Example values below are fictional, not a vehicle test.

| Drive: configurable dashboard | Tune: steering, throttle and brake |
| --- | --- |
| ![Simulator Drive dashboard](assets/ui-captures/drive.png) | ![Simulator Tune menu](assets/ui-captures/tune.png) |
| Data: sensors and telemetry | Setup: model configuration |
| ![Simulator Data page](assets/ui-captures/data-sensors.png) | ![Simulator Setup menu](assets/ui-captures/setup-model.png) |

| GPS map / trail (fictional circuit) | Channel mixing curve |
| --- | --- |
| ![Simulator GPS map](assets/ui-captures/gps-map.png) | ![Simulator mixing response graph](assets/ui-captures/mix-curve.png) |

The [screen gallery](docs/ui-gallery.md) also shows handset settings, channel
setup, control mapping, mix settings, dashboard layout, widget configuration,
output status and PIN entry.

## From original radio to R-1 EVO

Initial work began independently, three days before SourceTX's public
[31 July 2026 announcement](https://www.reddit.com/r/rccars/comments/1vbqozz/introducing_sourcetx_modern_touchscreen_rc_car/),
according to the project's development recollection. SourceTX subsequently
became an inspiration for a modern surface-radio interface. The first commit
in this repository is dated **9 August 2026**; it is not the start of the earlier
exploration.

<p align="center"><img src="assets/project/original-r1-manual-keypad.png" width="300" alt="Original JR R-1 manual page showing receiver connections and the eight-button keypad"><br><em>Original JR R-1 instruction manual, printed page 7 (JR Racing / JR PROPO; supplied ManualsLib copy).</em></p>

### August: understand the R-1, then make it talk CRSF

The first step was opening the radio and identifying the controls worth keeping.
Its original electronics, steering assembly, switches and connectors provided
the mechanical starting point. Early prototypes used external wiring and a
breadboard before committing to a replacement PCB.

<p align="center">
  <img src="assets/project/2026-08-07-original-electronics.jpg" width="280" alt="Original R-1 circuit board inside the opened handset">
  <img src="assets/project/2026-08-26-breadboard.jpg" width="280" alt="Opened handset connected to a breadboard and a working colour display">
  <br><em>7 August: the original electronics. 26 August: the breadboard conversion and working boot screen.</em>
</p>

By 9 August, the photographed display already showed a model, receiver supply,
link quality and a 16-channel monitor. The first interface was deliberately
instrument-heavy; later revisions would make everyday driving and tuning simpler.

<p align="center">
  <img src="assets/project/2026-08-09-early-dashboard.jpg" width="600" alt="Early physical LCD dashboard with model, power, link and sixteen channel bars">
  <br><em>9 August: an early dashboard running on the actual display.</em>
</p>

<p align="center"><img src="assets/project/early-throttle-bench.jpg" width="430" alt="Early throttle endpoint, trim and reversal screen working on a breadboard prototype"><br><em>Early throttle setup on the real LCD. This supplied photograph has no reliable capture date.</em></p>

Mechanical work progressed alongside the firmware. PCB CAD and printed fit
checks helped work around the existing shell, trigger and button positions.

<p align="center">
  <img src="assets/project/2026-08-30-pcb-fit-cad.jpg" width="390" alt="CAD view of the replacement board and printed fit-check supports">
  <img src="assets/project/2026-08-31-fit-prototypes.jpg" width="200" alt="Two physical 3D printed board-fit prototypes">
  <br><em>30–31 August: CAD planning and physical fit prototypes.</em>
</p>

### September: a board shaped to fit the original handset

The replacement board uses a XIAO ESP32-S3 and two MCP23017 I/O expanders to
retain the many original buttons, switches and trims. Much of the construction
is through-hole, making hand assembly and troubleshooting practical. The
original steering and trigger mechanisms and small associated boards remain.

<p align="center">
  <img src="assets/project/2026-09-13-pcb-cad.jpg" width="600" alt="CAD render of the custom PCB, showing the XIAO, two DIP expanders and eight buttons">
  <br><em>13 September: the replacement PCB design.</em>
</p>

<p align="center">
  <img src="assets/project/2026-09-22-pcb-fit.jpg" width="390" alt="Bare manufactured PCB fitted into the R-1 case">
  <img src="assets/project/2026-09-22-keypad.jpg" width="390" alt="Hand-soldered replacement button bank and expander">
  <br><em>22 September: the manufactured board fits the case, and assembly begins.</em>
</p>

The 16 September JLCPCB update recorded manufacture in progress. A
[19 September RF bench video](assets/project/video/2026-09-19-rf-bench.mp4)
shows the working radio-link prototype before final PCB assembly.

<p align="center"><img src="assets/project/2026-09-16-pcb-production.jpg" width="250" alt="JLCPCB production progress on 16 September"><img src="assets/project/2026-09-16-ui-progress.jpg" width="250" alt="Historical portrait simulator UI from 16 September"><br><em>16 September: PCB manufacture and UI development proceeded in parallel.</em></p>

The screen and PCB could then be tested together. Photos from this period show
portrait experiments; the installed transmitter now uses **480×320 landscape**.
The UI evolved towards Drive, Tune, Data and Setup, with configurable widgets
and more consistent editors and navigation.

<p align="center">
  <img src="assets/project/2026-09-25-pcb-and-lcd.jpg" width="390" alt="Working portrait touchscreen beside the populated replacement PCB">
  <img src="assets/project/2026-09-25-control-wiring.jpg" width="260" alt="Replacement board installed with original control wiring connected">
  <br><em>25 September: the new computer and display working together, and wiring the retained controls.</em>
</p>

### October: bring the hardware and interface together

A printed display enclosure brought the screen into the handset. Debugging was
not all software: LCD chip-select and I²C solder joints caused real faults that
were isolated with display tests, boot logs and logic-analyser captures. The
working installed display has since been exercised at an 80 MHz SPI setting.

<p align="center">
  <img src="assets/project/2026-10-03-display-mount.jpg" width="270" alt="Handheld R-1 with its new LCD enclosure fitted">
  <img src="assets/project/2026-10-25-assembled-radio.jpg" width="270" alt="Assembled R-1 running the landscape dashboard">
  <br><em>3 October: display enclosure fitted. 25 October: the assembled transmitter operating with its new dashboard.</em>
</p>

One especially persistent input fault was a solder bridge from a grounded
XIAO pin to a nearby via on an input track. The
[close-up photograph](assets/project/2026-10-25-solder-bridge.jpg) records how
small the eventual cause was. The
[25 October steering/throttle video](assets/project/video/2026-10-25-steering-throttle.mp4)
records the first combined input test. More assembly photos and the working-PCB
and touchscreen videos are in the [progress gallery](docs/project-progress.md).

By 31 October, work had moved on to harmonising typography, menus, paging and
keypad navigation, while continuing receiver telemetry development. The old
R-1's controls are part of the interface, rather than decorations around a
separate touchscreen computer.

These images document successive prototypes, so older screen layouts and
wiring should not be treated as current build instructions. Photo dates come
from capture metadata where available; additional WhatsApp exports use their
filename dates, as recorded in the [image provenance](assets/project/README.md).

## Custom receiver firmware: S.Port sensors over ExpressLRS

The companion [ExpressLRS-SPort repository](https://github.com/cmlpreston/ExpressLRS-SPort/tree/codex/sport-telemetry)
contains our receiver changes, based on ExpressLRS 4.1.0. This adds a direct
S.Port poller/parser and sensor-to-CRSF forwarding to selected PWM receivers.
It addresses a capability absent from the upstream version we started with:
connecting existing FrSky S.Port sensors directly to the receiver and bringing
their measurements back to this handset, without a flight controller or an
additional telemetry bridge.

```text
R-1 controls → XIAO handset firmware → CRSF → Ranger Nano → ExpressLRS RF → receiver → servos
                                                                            ↑
                                                                     S.Port sensor bus
                                                                            │
R-1 telemetry UI ← CRSF telemetry ← Ranger Nano ← ExpressLRS RF ← custom receiver decoder
```

Work includes **ER4/ESP8285** and **ER6G/ESP32** implementations. The ER4 uses
two UART pins joined into the sensor bus; the ER6G implementation supports a
single shared signal pin. Exact wiring and output assignments belong to the
[receiver documentation](https://github.com/cmlpreston/ExpressLRS-SPort/blob/codex/sport-telemetry/sport/README.md),
not to a generic pinout shared between receivers.

The development has used FLVS ADV cell telemetry and FAS300 ADV measurements
for voltage, current and temperature. Bench captures, parser tests and handset
displays are documented separately from end-to-end RF validation. Some sensor
paths and the ER6G profile still require further hardware checks; GPS forwarding
is not selected in the current receiver trial. The handset can display 1–12
individual cell readings, but the current receiver cell trial forwards **4S**.
Selecting a higher cell count cannot create measurements the receiver has not sent.

This is a **custom fork**, not a claim of upstream ExpressLRS S.Port support or
universal support for every S.Port sensor. See the
[receiver status and test instructions](https://github.com/cmlpreston/ExpressLRS-SPort/blob/codex/sport-telemetry/sport/README.md)
for the precise scope.

## The same UI on the desktop

The simulator builds the actual C++ `TransmitterUi` and LVGL screens, with SDL
standing in for the LCD and touch controller. It supplies simulated inputs and
telemetry, and persists model/settings state locally. We can navigate, tune,
try dashboard layouts and capture screenshots without having the handset on
the desk. A browser bridge shows those rendered frames and sends touch and
keypad events back to the simulator.

```sh
brew install sdl2 openssl@3
make -C simulator bootstrap
make -C simulator browser
# Open http://127.0.0.1:8767
```

![Full simulator browser interface, including keypad, input sliders and telemetry controls](assets/ui-captures/simulator-browser.jpg)

*The complete simulator web page, with simulated input controls alongside the shared firmware UI.*

Native SDL operation, screenshot commands and regression checks are in the
[development guide](DEVELOPMENT.md#desktop-ui-simulator). The simulator is useful
for checking layout and behaviour, but does not reproduce the LCD's viewing
angle, colour, touch electronics, SPI timing, radio timing or receiver failsafe.
The hardware photographs above are deliberately presented as photographs.

## The web UI inside the transmitter

The XIAO also runs its **own embedded web server**. This is distinct from the
desktop simulator: it connects to the real handset, its saved settings and its
running UI. Home Wi-Fi can connect automatically at boot; an optional setup
hotspot supports local configuration.

The browser provides model/channel/control configuration, mixing settings,
map search and download, map storage/upload, hostname settings and restart.
It also provides an authenticated live view with remote touch interaction and
PIN entry, making setup possible when the physical screen is out of reach.
Configuration and output-changing actions retain their respective safety checks.

The live view uses pixels rendered by the firmware. It **does not read back the
physical Waveshare LCD**, whose installed connection is write-only. We can
capture the rendered interface, but a framebuffer capture cannot verify that
the physical screen is displaying it correctly. The simulator and live web
view serve different purposes, even when they look similar.

The [web UI gallery](docs/ui-gallery.md#embedded-web-ui) shows model and hardware-control
configuration and the map selection/preview/upload flow. Those captures use the
real browser frontend with a **local simulated handset backend**; no live handset
was altered. The map example is Silverstone, a public circuit.

| Hardware-control assignments | Satellite selection and upload |
| --- | --- |
| ![Web UI control assignments with documentation fixtures](assets/ui-captures/web-controls.png) | [![Satellite map preview and example save confirmation](assets/ui-captures/web-map-upload.png)](assets/ui-captures/web-map-upload.png) |

### Map sources and acknowledgements

- **Street maps and geographic features:** [OpenStreetMap contributors](https://www.openstreetmap.org/copyright),
  with OSM data under the Open Database License (ODbL). The browser uses OSM
  tiles for selection and Overpass queries for the street-image/vector package workflow.
- **Place/address search:** [Photon](https://github.com/komoot/photon), the
  OpenStreetMap-based geocoder, using komoot's public Photon service.
- **OSM feature queries:** the [Overpass API](https://wiki.openstreetmap.org/wiki/Overpass_API).
- **Satellite/aerial photography:** [Esri World Imagery](https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer),
  credited in the current selector as **Esri, Vantor, Earthstar Geographics, and
  the GIS User Community**. Available imagery, detail, dates and required
  provider credits vary by area.

Provider attribution is retained in downloaded packages and displayed with
maps. Image access is subject to each provider's terms; these sources are not
our own imagery. The fictional circuit in the native simulator captures is
labelled separately. Stored images load into PSRAM for display; PSRAM itself
is volatile and is not the persistent map store.

## Hardware and schematics

| Component | Current installation |
| --- | --- |
| Handset | JR PROPO R-1, retaining the original controls and enclosure |
| Main processor | Seeed Studio XIAO ESP32-S3 |
| Additional I/O | Two MCP23017 expanders, K1 and K2 |
| Display | Waveshare 3.5-inch RPi LCD (F), touch, 480×320 landscape |
| RF module | RadioMaster Ranger Nano, ExpressLRS, native CRSF connection |
| Power monitoring | Switched transmitter-pack voltage divider feeding the XIAO ADC; separate 5V_SYS supply rail |
| Mechanical work | Replacement PCB and printed display enclosure/fit prototypes |

The **[revision (5) schematic PDF](docs/hardware/r1-evo-schematic-rev5.pdf)** is
published with the project. The [hardware notes](docs/hardware/README.md) explain
its provenance and how to interpret it alongside the
[current XIAO GPIO and expander pin map](docs/xiao-pcb.md). Earlier prototype
hardware remains documented for history; the approved transmitter build is
`seeed_xiao_esp32s3-pcb`, landscape only.

## Built with extensive Codex / ChatGPT collaboration

This project is also an example of what sustained collaboration with coding
models can produce. Codex/ChatGPT was used extensively for firmware development,
architecture review, UI design and iteration, protocol research, receiver
telemetry work, tests, simulator tooling, diagnostics and interpreting logic
captures. The work crossed embedded C++, browser code, hardware documentation
and the development/upload tools rather than stopping at a screen mock-up.

The project owner supplied the goals and operating requirements, made the
hardware and mechanical decisions, assembled and soldered the boards, connected
sensors and controls, and tested the radio. Photos, measurements, serial logs
and captures fed back into the next software revision. The result came from
that repeated hardware/software loop; a successful build or simulator run was
not treated as a substitute for testing the actual transmitter.

For historical context, these were the model generations during the build:

| Period | Model context and evidence |
| --- | --- |
| July–August 2026 | GPT-5.6 Sol, Terra and Luna were available across Codex/ChatGPT Work during the early project period. This records contemporary availability; the retained local session metadata does not establish which model handled every early discussion. |
| 10–30 September 2026 | The retained project session metadata records `gpt-6-astra` during this period. |
| 30 September–31 October 2026 | The retained project session metadata records `gpt-6.1-sol`. OpenAI's changelog dates the GPT-6.1 Sol introduction to 29 September. |

Availability is documented in OpenAI's
[July product history](https://learn.chatgpt.com/docs/whats-new) and
[ChatGPT/Codex changelog](https://learn.chatgpt.com/docs/changelog).
The usage dates above come from local session model identifiers, not an inference
that a model must have been used because it had been released. Private session
transcripts and credentials are not part of this repository. Model names date
the experiment; they are not a dependency required to build or run the radio.

## Credits and influences

- **[ExpressLRS](https://github.com/ExpressLRS/ExpressLRS)** supplies the RF
  foundation and device ecosystem. Our receiver work extends that project;
  it does not replace its contribution or imply upstream endorsement.
- **[SourceTX](https://www.reddit.com/r/rccars/comments/1vbqozz/introducing_sourcetx_modern_touchscreen_rc_car/)**
  was an inspiration after its public introduction, particularly the idea of
  a modern touchscreen surface radio driving an ExpressLRS module natively.
  R-1 EVO began independently; it is not a SourceTX firmware fork.
- **[EdgeTX](https://manual.edgetx.org/color-radios/user-interface)** and
  RadioMaster TX16S interface references informed configurable widgets,
  model organisation, input/mix/output thinking and clear parameter editing.
  OpenTX and EdgeTX are important precedents, but this handset's firmware is
  independently implemented rather than derived from their codebases.
- **[DJI RC Plus](https://enterprise-insights.dji.com/blog/dji-rc-plus-top-5-features)**
  informed our review of readable operating views, persistent status and
  physical shortcuts.
- **[Spektrum iXSR / RaceWare](https://www.spektrumrc.com/product/spektrum-ixsr-12-channel-dsmr-transmitter-with-sr515-receiver/SPM6425.html)**
  informed the review of surface-radio tuning, telemetry organisation and
  channel/curve displays. Our [design notes](docs/ui-design-direction.md)
  distinguish references and proposals from implemented behaviour.
- **[LVGL](https://lvgl.io/)** provides the embedded UI framework;
  **[Arduino_GFX](https://github.com/moononournation/Arduino_GFX)** provides
  the display driver layer. SDL supports the desktop simulator.
- **JR PROPO** created the original R-1 hardware and identity we are preserving.
  This is an independent conversion project, with no claimed affiliation with
  JR, RadioMaster, FrSky, SourceTX, DJI or Spektrum.

## Explore or contribute

Start with the [development guide](DEVELOPMENT.md),
[current PCB wiring](docs/xiao-pcb.md), [control inventory](docs/r1-control-map.md),
[architecture review](docs/architecture-review.md) and [TODO list](TODO.md).
The [receiver project](https://github.com/cmlpreston/ExpressLRS-SPort/tree/codex/sport-telemetry)
is maintained separately. Build and upload instructions must be matched to the
actual hardware; the repository's [upload rules](AGENTS.md) apply to firmware work.

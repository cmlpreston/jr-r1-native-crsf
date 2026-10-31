# Building the prototype R-1 and developing its firmware

[Project overview](../README.md) · [Current interface gallery](ui-gallery.md) ·
[Build and development guide](../DEVELOPMENT.md) · [Roadmap](../TODO.md)

This is the illustrated history of the R-1 EVO conversion: understanding the
original radio, building and debugging its replacement electronics, and evolving
the transmitter and receiver firmware together. The main [README](../README.md)
describes the project and its capabilities today.

These are successive prototypes. Older screen layouts and wiring are historical
evidence, not current assembly instructions. Use the [current PCB pin map](xiao-pcb.md),
[revision (5) schematic](hardware/r1-evo-schematic-rev5.pdf) and development guide
when building hardware. Supplied bench videos are embedded as GitHub-hosted
players, with repository copies linked beneath them.

## Origins and inspirations

Initial work began independently, three days before SourceTX's public
[31 July 2026 announcement](https://www.reddit.com/r/rccars/comments/1vbqozz/introducing_sourcetx_modern_touchscreen_rc_car/),
according to the project's development recollection. SourceTX subsequently
became an inspiration for a modern surface-radio interface. The first commit
in this repository is dated **9 August 2026**; it is not the start of the earlier
exploration.

<p align="center"><img src="../assets/project/original-r1-manual-keypad.png" width="300" alt="Original JR R-1 manual page showing receiver connections and the eight-button keypad"><br><em>Original JR R-1 instruction manual, printed page 7 (JR Racing / JR PROPO; supplied ManualsLib copy).</em></p>

## August: understand the R-1, then make it talk CRSF

The first step was opening the radio and identifying the controls worth keeping.
Its original electronics, steering assembly, switches and connectors provided
the mechanical starting point. Early prototypes used external wiring and a
breadboard before committing to a replacement PCB.

<p align="center">
  <img src="../assets/project/2026-08-07-original-electronics.jpg" width="280" alt="Original R-1 circuit board inside the opened handset">
  <img src="../assets/project/2026-08-26-breadboard.jpg" width="280" alt="Opened handset connected to a breadboard and a working colour display">
  <br><em>7 August: the original electronics. 26 August: the breadboard conversion and working boot screen.</em>
</p>

By 9 August, the photographed display already showed a model, receiver supply,
link quality and a 16-channel monitor. The first interface was deliberately
instrument-heavy; later revisions would make everyday driving and tuning simpler.

<p align="center">
  <img src="../assets/project/2026-08-09-early-dashboard.jpg" width="600" alt="Early physical LCD dashboard with model, power, link and sixteen channel bars">
  <br><em>9 August: an early dashboard running on the actual display.</em>
</p>

<p align="center"><img src="../assets/project/early-throttle-bench.jpg" width="430" alt="Early throttle endpoint, trim and reversal screen working on a breadboard prototype"><br><em>Early throttle setup on the real LCD. This supplied photograph has no reliable capture date.</em></p>

Mechanical work progressed alongside the firmware. PCB CAD and printed fit
checks helped work around the existing shell, trigger and button positions.

<p align="center">
  <img src="../assets/project/2026-08-30-pcb-fit-cad.jpg" width="390" alt="CAD view of the replacement board and printed fit-check supports">
  <img src="../assets/project/2026-08-31-fit-prototypes.jpg" width="200" alt="Two physical 3D printed board-fit prototypes">
  <br><em>30–31 August: CAD planning and physical fit prototypes.</em>
</p>

## September: a board shaped to fit the original handset

The replacement board uses a XIAO ESP32-S3 and two MCP23017 I/O expanders to
retain the many original buttons, switches and trims. Much of the construction
is through-hole, making hand assembly and troubleshooting practical. The
original steering and trigger mechanisms and small associated boards remain.

<p align="center">
  <img src="../assets/project/2026-09-13-pcb-cad.jpg" width="600" alt="CAD render of the custom PCB, showing the XIAO, two DIP expanders and eight buttons">
  <br><em>13 September: the replacement PCB design.</em>
</p>

### 16 September: manufacture and interface development

The JLCPCB production update recorded manufacture in progress, with an estimated
finish of 17 September. The accompanying UI capture shows the portrait dashboard
of that period, including its earlier CUT and Layout buttons. Hardware and
firmware development proceeded in parallel.

<p align="center"><img src="../assets/project/2026-09-16-pcb-production.jpg" width="250" alt="JLCPCB production progress on 16 September"><img src="../assets/project/2026-09-16-ui-progress.jpg" width="250" alt="Historical portrait simulator UI from 16 September"></p>

### 19 September: working RF bench prototype

The opened handset, breadboard, external RF module, display and receiver/servo
setup show the working radio-link prototype before final PCB assembly.

https://github.com/user-attachments/assets/d969b3ab-47aa-40d7-9dc7-c1d16479c7ca

[Open/download the original RF bench clip](../assets/project/video/2026-09-19-rf-bench.mp4).

### 20 September: configuration from a browser

![Historical web control configuration](../assets/project/2026-09-20-web-controls.jpg)

The earlier web editor exposed original handset controls, output selection and
enable state. Browser-based configuration became part of the handset itself,
alongside the desktop simulator used to develop the shared firmware interface.
See the [interface gallery](ui-gallery.md#embedded-web-ui) for later web examples.

### 22–28 September: assembling and operating the PCB

<p align="center">
  <img src="../assets/project/2026-09-22-pcb-fit.jpg" width="280" alt="Bare manufactured PCB fitted into the R-1 case">
  <img src="../assets/project/2026-09-22-keypad.jpg" width="280" alt="Hand-soldered replacement button bank and expander">
  <img src="../assets/project/2026-09-22-xiao-assembly.jpg" width="280" alt="XIAO and DIP sockets installed on the replacement board">
  <br><em>22 September: the manufactured board fits the case, and assembly begins.</em>
</p>

The screen and PCB could then be tested together. Photos from this period show
portrait experiments; the installed transmitter now uses **480×320 landscape**.
The UI evolved towards Drive, Tune, Data and Setup, with configurable widgets
and more consistent editors and navigation.

<p align="center">
  <img src="../assets/project/2026-09-25-pcb-and-lcd.jpg" width="390" alt="Working portrait touchscreen beside the populated replacement PCB">
  <img src="../assets/project/2026-09-25-control-wiring.jpg" width="260" alt="Replacement board installed with original control wiring connected">
  <br><em>25 September: the new computer and display working together, and wiring the retained controls.</em>
</p>

**25 September — working replacement PCB**

https://github.com/user-attachments/assets/c674a6c4-4e64-43a7-bc6d-176065c3f0c9

[Open/download the original working-PCB clip](../assets/project/video/2026-09-25-working-pcb.mp4).

**28 September — touchscreen working**

https://github.com/user-attachments/assets/dca95e1c-1022-4f89-bfe4-26ae98233780

[Open/download the original touchscreen clip](../assets/project/video/2026-09-28-touchscreen.mp4).

## October: bring the hardware and interface together

A printed display enclosure brought the screen into the handset. Debugging was
not all software: LCD chip-select and I²C solder joints caused real faults that
were isolated with display tests, boot logs and logic-analyser captures. The
working installed display has since been exercised at an 80 MHz SPI setting.

<p align="center">
  <img src="../assets/project/2026-10-03-display-mount.jpg" width="270" alt="Handheld R-1 with its new LCD enclosure fitted">
  <img src="../assets/project/2026-10-25-assembled-radio.jpg" width="270" alt="Assembled R-1 running the landscape dashboard">
  <br><em>3 October: display enclosure fitted. 25 October: the assembled transmitter operating with its new dashboard.</em>
</p>

### 25 October: finding the small fault, then testing both inputs

<p align="center"><img src="../assets/project/2026-10-25-solder-bridge.jpg" width="350" alt="Solder bridge between a grounded XIAO pin and a nearby input-track via"></p>

One persistent input problem came from a solder bridge between a grounded
XIAO pin and a nearby via on an input track. It took considerable investigation
to find: the close-up records the eventual physical cause, rather than a
firmware defect. The following video records the first combined steering and
throttle input test.

https://github.com/user-attachments/assets/b78660c5-05b0-463d-b80c-2710eca7bff4

[Open/download the original steering/throttle clip](../assets/project/video/2026-10-25-steering-throttle.mp4).

By 31 October, work had moved on to harmonising typography, menus, paging and
keypad navigation, while continuing receiver telemetry development. The old
R-1's controls became part of the interface, rather than decorations around a
separate touchscreen computer.

## Firmware development alongside the prototype

The handset software grew from the CRSF/control bench prototype into its own
control pipeline and LVGL interface. It is independently implemented, rather
than an OpenTX, EdgeTX or SourceTX firmware fork. ExpressLRS supplies the RF
foundation; the companion receiver work extends ExpressLRS itself.

| Stage | Software development and evidence |
| --- | --- |
| Early August | Physical LCD views already showed a model, receiver supply, link quality and 16-channel monitoring. Throttle endpoints, trim and reversal were exercised on the breadboard prototype. |
| September | The interface moved towards Drive/Tune/Data/Setup, configurable dashboard widgets and browser configuration. The shared-code simulator enabled iteration while the PCB was being manufactured and assembled. |
| October | Original controls, simultaneous steering/throttle, calibration, channel response, mixing and hardware diagnostics were brought together. UI/navigation refinement continued alongside the soldering and display investigations. |
| Parallel receiver work | The separate ExpressLRS-SPort fork added receiver-integrated S.Port polling/decoding and CRSF telemetry forwarding, with ER4 and ER6G paths. Prepared GPS forwarding and individual sensor paths still require their documented hardware checks. |
| 31 October–1 November | Module-link approval and the authenticated OTA updater were added (`b57162d`). The updater was installed over USB; a complete wireless upload/reboot test was deferred at this checkpoint. |
| 1 November | USB capture of the real handset's rendered pixels (`7d13dbf`) produced the native screenshot gallery, replacing simulator previews in the project overview. The captures do not read back the physical LCD. |
| 1 November | Per-model Up/Down/Lap timers, 100-lap histories, Ready-based throttle start, alarms, accumulated on-time and web CSV export were implemented (`4ada81d`). Host, web and native UI tests and the XIAO build passed; the transmitter had not yet been flashed with this timer revision at that checkpoint. |

The [current feature catalogue](../README.md#what-we-have-built),
[receiver status](https://github.com/cmlpreston/ExpressLRS-SPort/blob/codex/sport-telemetry/sport/README.md)
and [roadmap](../TODO.md) distinguish implemented code, bench observations and
hardware validation still outstanding. A dated milestone is not a claim that
every radio, sensor or failsafe condition had been tested.

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

## Media handling

Photo dates come from capture metadata where available; additional WhatsApp
exports use their filename dates, not independently verified camera timestamps.
The [image provenance](../assets/project/README.md) records the source of the
earlier photographs.

Published JPEG copies omit EXIF/XMP/IPTC/comment metadata. Videos were remuxed
without source metadata or chapters and without re-encoding audio/video. Dates
remain in these captions and filenames rather than embedded location metadata.
The videos document bench milestones; they do not establish every RF failsafe,
electrical or operating condition.

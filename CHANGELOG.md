# R-1 EVO change record

## 15 November 2026 — trip, run summary and battery load/balance widgets

- Append optional Trip computer, Run summary and Battery load / balance kinds
  16–18 without changing saved layout bytes, previous IDs or default dashboards.
- Share a resettable Trip/Run session for GPS distance, moving time/average,
  observed top speed, elapsed time, CU01 peak power, used-capacity delta and Wh.
  Integrate only fresh observed intervals; mark gaps and sensor counter restarts.
  Use a consistent 1 km/h threshold for moving time and distance.
- Keep battery load and cell readings independent, with observed minimum pack
  voltage/peak current and live lowest cell/spread. Retain peaks on telemetry loss
  and expire live values. No remaining-charge or battery-health inference.
- Use shared compact/expanded numeric cards and confirmed local resets, with
  UI-owned cold storage and no changes to control, RF or sensor settings.
- Host suite, approved XIAO build, both simulator builds/full UI audits and
  focused render/reset/freshness/allocation-failure tests pass. Real sensor bench
  validation remains pending. This candidate has not been flashed.

## 15 November 2026 — automatic GPS map/speed presentation

- Upgrade the existing GPS map widget with reusable map/speed subviews, selected
  by fresh returned fields. Preserve saved kind5, trail, map, sizes and statistics.
- Use a three-second last-usable-position grace, acquisition/waiting status for
  low satellites or incomplete position data, and unavailable readings on loss.
  Preserve legacy zero-as-unknown position data with an unconfirmed amber map.
- Keep the dedicated Speed card and full Data/GPS page behaviour. Reject delayed
  browser taps after actual presentation switches without rebuilding the screen.
- This infers a useful display, not the sensor's physical mode. Blue10Hzspeed is
  documented; its actual field set remains unverified. The user is not ready for
  a passive hardware check. Host tests, approved XIAO landscape build, both
  simulator builds/full UI audits and focused GPS/map/Speed checks pass. Six
  fixed-Speed before/after captures are byte-identical. Candidate is unflashed.

## 15 November 2026 — power/energy and control-history widgets

- Append optional Power / energy and Control history widget kinds, preserving
  existing saved layouts and supporting every size and maximise behaviour.
- Derive electrical watts, observed peak and sampled Wh from CU01 readings;
  estimate Wh/km from matched fresh GPS-speed coverage after 100 m. Exclude
  stale/invalid intervals, mark incomplete totals and confirm local resets.
- Plot ten seconds of calibrated steering/throttle inputs with fixed percentage
  axes, distinct colours and timestamp gaps. CUT retains valid input traces;
  traces do not claim servo motion or distinguish braking from reverse.
- Keep histories and chart arrays in UI-owned PSRAM-first storage. No per-tick
  LVGL object creation, sensor commands or output-setting changes. Preserve real
  control-sample timestamps rather than restamping cached samples at UI delivery.
- Full host suite, approved XIAO landscape build, both simulator builds and full
  UI/rotation/typography audits pass. Focused widget checks also pass with cold
  storage failure, gaps, resets and unchanged-render guards. Synthetic previews
  are recorded in the manual. Installed source `dfd57c4` through the guarded
  approved XIAO landscape workflow; device hash verification passed.
  Real-sensor and handset bench validation remains pending.

## 14 November 2026 — editor reuse, smaller fonts and display comparison

- Refresh ordinary value editors in place, retaining their header, controls,
  focus targets and page epoch. The measured 320-action fixture renders 90.2%
  fewer pixels and creates no new LVGL objects; Apply/Cancel and safety rules
  are preserved. Broader cross-page chrome caching remains deferred.
- Replace complete fallback fonts with required glyph subsets. Fallback storage
  falls from 209,046 to 25,513 B; the preferred 95,116 B font set is unchanged.
  Net firmware flash falls by 176,740 B (172.6 KiB); static RAM rises by 48 B.
- Add an opt-in Standard/Trial LCD transfer comparison using the same commands,
  pins, mode 3 and 80 MHz clock. Standard remains the default after every boot.
  Authenticated changes require CUT, an unlocked handset and released holds.
- Installed source `fff44ca` through the guarded approved XIAO landscape upload;
  full host checks and landscape UI audit passed again, with device hash verified.
  Both simulator builds, focused checks and full portrait audit also passed.
  Initial hardware A/B shows no clear gain (Standard median72.97ms, Trial74.30ms
  for identical full redraws); user confirms normal Trial LCD. Standard restored.
  Record web/live-screen task gaps for investigation and broader qualification.

## 14 November 2026 — live expo and shared feedback panels

- Keep bounded expo adjustments live, including Tune draft preview, Apply,
  Cancel/navigation restoration, keypad adjustment/reset and legacy expo rows.
  Preserve saved asymmetric expo on entry; save committed changes after CUT.
- Keep other structural edits and fault/CUT protections. Expo intentionally
  propagates through shaped mixer sources; zero and endpoints are retained.
- Harmonize trim/subtrim and hold-to-enable/CUT feedback using a shared centred
  panel, preserving hold timing and touch/arming guards.
- Add a combined CU01 drive-pack widget for volts, amps, used mAh and estimated
  sensor-reported remaining percentage, with unavailable/stale states.
- Installed source `488d94b` through the approved XIAO landscape workflow;
  complete tests/UI checks and device hash verification passed.

## 14 November 2026 — GPS widget credit strip

- Remove the source-credit strip from dashboard GPS widgets, including maximized
  widgets. Retain source metadata, credits on Data / GPS and missing-map warnings.
  Installed source `c6218d2` through the approved XIAO landscape workflow; full
  tests/UI audit and device hash verification passed.
- A reported neutral beep at displayed -1% could not be reproduced; user then
  confirmed the cue at 0%. Keep the existing zero/sign-cross detection unchanged.

## 14 November 2026 — live steering centre and GPS widget fit

- Keep steering parameter subtrim live, moving neutral with fixed endpoints.
  Remove its CUT request and independent structural re-arm while retaining CUT
  for throttle/auxiliary subtrim, other structural edits and actual faults.
  Save the live adjustment only after CUT; remove the steering popup's CUT hint.
- Fit saved satellite imagery to GPS widgets with a minimum cover scale and
  bounded following near image edges. Keep the marker/trail aligned with the
  fitted viewport and retain the full GPS page's zoom/pan behaviour.
- Initial previous-build startup/CUT, steering and motor-output bench checks
  passed by user report. Installed source `0053322` through the approved XIAO
  landscape workflow; tests/UI checks and device hash verification passed.

## 14 November 2026 — high-priority memory and responsiveness fixes

- Publish safe outputs before logging; replace critical-task USB/formatting with
  bounded events and a low-priority worker. Serialize machine USB replies.
- Sample physical controls independently with serialized I²C, queued UI delivery,
  immediate latched CUT, model/safety epochs and a hardware-health watchdog.
- Move RF cache/history to PSRAM-first cold storage, retain limits and provide
  internal fallback and explicit exhaustion handling. Static RAM 76.8% -> 62.2%;
  new worker stacks/task overhead must be included in runtime measurements.
- Guard unchanged graph geometry/styles and cache raster X/trail projections.
  Pixel-equivalent host sampling benchmark improves ~8.6x, excluding LCD work.
- Report allocator LVGL peak and worker stack margins; add race, backpressure,
  input, I²C, allocation-failure and graph/map regression coverage.
- Installed source `dd53728` on the approved XIAO landscape profile after build,
  host tests and full UI checks; guarded PlatformIO upload and device hash
  verification passed. This includes ABS, Off by default. UI/model edit callbacks
  remain UI-owned; hardware timing and safe bench qualification are pending.

## 14 November 2026 — ABS pulsed-braking candidate

- Add per-model ABS under Tune / Brake, Off by default: activation, release
  depth, frequency and delay; 50% strong / reduced pulse pattern.
- Require a matched verified ESC Run mode read/write showing Forward/Brake.
  Invalidate eligibility and disable ABS on a new programming session or run-mode
  write. No automatic ESC mode changes; externally changed mode needs re-reading.
- Run modulation in the control task after shaping, slew and limits, with CUT,
  input/model validity and final stale-control arbitration remaining authoritative.
- Add checksummed settings, per-model isolation, enabled header marker and
  host/UI checks. Actual receiver PWM and ESC braking need bench validation.

## 14 November 2026 — live steering endpoints

- Installed through guarded XIAO landscape upload; complete tests/UI audit passed
  and uploaded firmware hash verified. Physical servo validation remains.

- Preview steering endpoint drafts through the shared control snapshot, so wheel
  position and keypad +/- adjust actual travel immediately while enabled.
- Keep Apply live, restore previous travel on Cancel, and save after CUT.
- Exempt only valid mechanical steering endpoint changes from structural re-arm;
  retain throttle, direction, input, mixing, ESC and fault interlocks.
- Add production-engine checks for reversed travel, CUT, input faults and
  unchanged throttle through logical-channel/configured-steering mixes.
- The reported red lock disappeared after Ranger power-off. Its exact fault
  reason was not captured and is not claimed diagnosed or fixed.

## 14 November 2026 — ESC programming workflow

- User validated the installed Program / power cycle / Read ESC / Drive /
  power-cycle flow. Record the continuous long press needed to turn this ESC
  fully off; earlier entry failures remain unexplained. No settings changed.

- Installed on the R1 from `0066113`: approved XIAO landscape build, full tests
  and UI audit passed; guarded PlatformIO upload and device hash verified.

- Keep Program, Read ESC and Drive in a fixed action bar on every ESC tab and
  while editing. Keep Apply one and Cancel separate above that bar.
- Show the numbered sequence: Program and wait for acknowledgement; ESC off
  until the RX LED is dark, wait five seconds, on once; Read ESC before editing;
  select Drive and restart before waiting for Drive confirmation.
- Dim unavailable actions, retain their operation guards, and report why a
  programming request cannot begin instead of silently retaining the idle text.
- Bench-validate the paired custom ER6G r14/r8 path: all 15 reads, one drag-brake
  write from 0% to 5%, acknowledgement and readback, independent card/persistence
  confirmation, and restoration to 0% with acknowledgement and readback. The
  user waived the final card check after restoration. BEC remained 6.0 V.
- The receiver uses the normal output-5 ESC lead for PWM or half-duplex serial,
  selected for the whole boot. The small startup stage remains read-only; live
  writes require a verified programming session, RF/model/CUT gates and matching
  readback. Failed or ambiguous writes are not retried automatically.


## Installed on R1 — 5 November 2026: ER4 programmer build

- Flash the tested CH12 programmer UI/session/interlocks on the approved XIAO
  landscape profile. Startup checks passed; transmitter confirmed CUT.
- ER4 remains RF-only; receiver handover/failsafe commissioning is pending.

## Installed on ER4 — 5 November 2026: RF-only programmer bench image

- Upload the dedicated ESP8285 ER4 image at 192.168.1.130 through guarded
  PlatformIO Wi-Fi OTA, retaining binding and network settings.
- Verify the rebooted identity, source/version, pin layout and executable
  readback. ESC access and writes remain disabled; R1/ER6G unchanged.
- Add receiver-specific artifact/live-device guards and retain the existing
  XIAO transmitter upload restrictions. Preserve original S.Port recovery backup.

## Bench prototype — 5 November 2026: dedicated ER4 ESC programmer

- Reserve CH12 (low ER6G, high ER4) and preserve selection through CUT output
  substitution. Add UI/control/serial-task arming and request guards.
- Replace the ESC preview examples with all 15 measured Surpass settings,
  read-only unknown states, per-setting drafts and explicit one-setting Apply.
- Add a bounded nonce/sequence request/reply client, receiver transaction engine,
  write replay protection, ACK plus readback verification, and ER6G identity
  confirmation on return. Error/cancel/navigation restores selection low and CUT.
- Add pinned ELRS ER4 integration and reproducible RF-only bench packaging.
  Default receiver build touches no ESC GPIO and cannot write settings.
- Keep radio modes, installed receivers and installed R1 firmware unchanged.
  Electrical behaviour, RF handover and programming re-entry need bench tests.
  See docs/esc-programmer-er4.md.

## Ready for review — 5 November 2026: fixed-endpoint subtrim

- On user confirmation, make channel subtrim adjust neutral while retaining
  the configured endpoints, rescaling each side of the response. Ordinary trim
  still shifts neutral and endpoints together with final output clipping.
- Respect reversal, asymmetric response, switched rate, Walk and clipped travel;
  constrain neutral to available travel. A collapsed side pins neutral.
- Vertically centre the transient trim/subtrim overlay in both orientations.
- Preserve stored values, live trim, subtrim CUT and shared control/preview math.
  This changes the response of existing nonzero subtrim settings; recheck travel.

## Ready for review — 5 November 2026: remaining setup pages

- Move channel roles, throttle detail, response/ramp, firmware information,
  ELRS binding, ESC preview and calibration onto the common page scaffold and
  shared row/tab/button builders in both orientations.
- Replace boxed settings/toggles and coordinate-based callbacks with semantic
  row actions. Remove the separate legacy portrait detail renderer.
- Give response curves a full-width live plot and separate settings tabs;
  retain numeric edits, immediate CUT, parent routes and live input updates.
- Retain binding link/blank-phrase guards, inert ESC read/write, calibration
  acquisition/cancellation and all stored control settings.
- Remove the V2 badge from the menu bar and reclaim model-name space in native
  and browser trial headers.


## Installed — 4 November 2026: shared menu presentation

- Use one menu-row builder for navigation lists, settings, choices and read-only
  input rows. It owns shared margins, typography, divider lines, value alignment,
  truncation and touch/focus rectangles; existing Setup/Tune lists use it too.
- Move Control map, assignment editor, Input monitor, Colour scheme and RF module
  settings onto the current shared header/navigation scaffold. Replace Control
  map's separate arrow/button footer with centred dots and swipe paging; retain
  an Inputs shortcut and channel-context return path.
- Preserve draft-only assignment edits, Apply/Cancel, input monitoring, RF
  readback/confirmation and all existing CUT/PIN checks.

Guarded Pi USB upload verified on `codex/r1-v2`, approved XIAO/Waveshare LCD F
landscape profile. Local firmware, host/web tests and full UI/rotation/typography
audit passed. Boot #632 passed K1/K2, touch, LCD and UI checks; INPUTMON confirmed
CUT with no RF frames. Record `/tmp/r1-flash-4m3lbvul`; log `/tmp/r1-menu-flash.log`.

## Installed — 4 November 2026: one configurable Drive dashboard

- Remove the separate fixed Overview and the persistent Normal/Walk row; the
  saved per-model widget layout now fills the space below the title bar.
- Add optional Controls (Normal/Walk), Trim / rates (steering trim/rate,
  throttle trim and brake rate), and RF margin (known-mode margin, LQ and RF
  power) widgets. Existing layout IDs and saved/covered widgets are retained.
- Controls use the existing mode state, CUT behavior and PIN-protected Walk exit;
  trim/rate buttons open the existing guarded editors. RF margin opens radio
  diagnostics and displays unavailable/stale readings explicitly.
- Retain Run review's held maxima/minima and explicit reset; add a direct entry
  on Data → Sensors alongside its existing configurable Quick shortcut.

Guarded Pi USB upload verified on `codex/r1-v2`, approved XIAO/Waveshare LCD F
landscape profile. Local build, host/web tests and full UI/rotation audit passed.
Boot #631 passed peripheral/UI checks; INPUTMON confirmed CUT. Record:
`/tmp/r1-flash-u96zzme0`; log `/tmp/r1-widget-dashboard-flash.log`.

## V2 native UI trial — 3 November 2026

- Preserve V1 at `codex/r1-v1` / `r1-v1-2026-11-03`; develop V2 separately.
- Add real-telemetry Drive overview, live trim/rate strip, GPS/Widgets shortcuts,
  graph pause/time cursor, sensor setup links, per-model editable Quick and run
  review with explicit statistics reset. Preserve native safety/control paths.
- Embed DM Sans and Manrope bitmap fonts, retain icon fallback and use the
  approved trial's light palette. Keep the top model label informational.
- Remove gearing calculator from the browser trial and restore a GPS demo route;
  native GPS map/trail/zoom and per-model widget layouts remain available.

Record user-visible changes here as they are implemented. Update the operating
manual periodically rather than regenerating it for every firmware change.
Entries below distinguish prepared source from firmware already installed.

## Installed 3 November 2026 — Setup menu scope and course retirement

Guarded Pi USB PlatformIO upload verified, approved XIAO/Waveshare landscape
profile, branch `codex/safety-control-refactor`, HEAD `91cf80c` plus current source.
Fresh main ancestry, firmware build, host/web tests and full UI/rotation audit
passed. Boot #628 passed startup checks and reported CUT, arm_request=no.
Record `/tmp/r1-flash-8huchyfh`; log `/tmp/r1-setup-scope-flash.log`.

- Move live RF module configuration to Handset, labelled Shared. Handset now
  groups display, sounds, RF module, TX battery, network and system settings.
- Model groups channels, assignments, dashboard, drive modes, vehicle batteries,
  per-model RF warnings, ESC settings and ERS sensors. Remove the TX battery
  shortcut from Model's battery submenu; RX and drive pack remain there.
- Remove the model-warning shortcut from handset sound settings. RF diagnostics
  retains an explicitly labelled Model alarms shortcut. Warning Back navigation
  returns to its originating page. Existing settings and persistence retained.
- Not pushed or published.

## Installed 3 November 2026 — retire GPS course widget

- Remove GPS course from the widget picker and renderer because the installed
  sensor chain does not provide usable course values. Saved course slots become
  empty; retired kind ID 9 remains reserved and other saved IDs stay unchanged.
  Speed, satellite count, GPS position/map and telemetry decoding remain intact.
- Included in the verified upload above; not pushed or published.

## Installed 3 November 2026 — taller sensor graphs and RF warnings

Guarded Pi USB PlatformIO upload verified, approved XIAO/Waveshare landscape
profile, branch `codex/safety-control-refactor`, HEAD `91cf80c` plus current source.
Fresh main ancestry, firmware build, host/web tests and full UI/rotation audit
passed. Boot #626 passed startup checks and reported CUT, arm_request=no.
Record `/tmp/r1-flash-ft6v9kuz`; log `/tmp/r1-graph-link-flash.log`.

- Compact sensor readings/statistics above the graph; remove the history footer.
  Landscape plot height increases from 62 to 115 pixels, portrait from 202 to
  255. Add top/mid/bottom numeric Y-axis labels and grid lines, with units in the
  statistics line, padding for flat signals, and explicit empty-history labels.
- Configurable per-model RF warnings under Sounds / alerts and RF diagnostics.
  2.4 GHz ELRS 4.x sensitivity table is default (recorded receiver is 4.1.0);
  3.x selectable. Unknown RF modes suppress RSSI-margin alarms and retain LQ.
  Defaults: margin at/below 8 dB or LQ at/below 90%, one-second dwell, recovery
  hysteresis and five-second repeat. Warn only while output is requested.
- Missing link statistics are separately identified as unavailable telemetry,
  never treated as a measured weak RSSI. SNR/margin diagnostics clear on stale
  data. Warning display/sounds do not change control or module power settings.
- Not pushed or published.

## Installed 3 November 2026 — inaugural-drive follow-up

Guarded Pi USB PlatformIO upload verified, approved XIAO/Waveshare landscape
profile, branch `codex/safety-control-refactor`, HEAD `91cf80c` plus current source.
Fresh main ancestry, firmware build, host/web tests and full UI/rotation audit
passed. Boot #624 passed startup checks and reported CUT, arm_request=no.
Record `/tmp/r1-flash-pooe1kt_`; log `/tmp/r1-first-drive-flash.log`.

- Steering/throttle parameter controls respect channel reversal. Existing stored
  output offsets and neutral remain unchanged; UI/control signs use physical
  input direction. Rebase parameter bindings when reversal changes.
- Configurable per-model drive-pack and lowest-cell warnings, 4S LiPo initial
  preset 14.00 V / 3.50 V, with LiFe/LFP, Li-ion and NiMH presets, independent
  enables and adjustable thresholds. Fresh pack telemetry prioritizes cells,
  then ERS, then FAS; cell alarms require actual individual-cell telemetry.
  Two-second debounce, hysteresis, ten-second beep repeat and visible banner.
- All sensor details offer 30-second/session max and average plus Reset;
  all sensor widgets can select Current/Maximum/Average and reset their source.
  RPM defaults to session max hold. Statistics ignore duplicate/stale samples;
  generic averages are fresh-packet sample means; speed keeps its existing
  time-weighted session average. Graph histories remain the last 30 samples.
- Drive capacity defaults to the user's 6500 mAh. ERS Remaining links to CU01
  settings and stages this capacity on selection, requiring guarded Apply and
  sensor readback. The reported remaining percentage is never fabricated.
- GPS position uses a directionless dot. Map zoom spans 1/8x to 8x; wider
  uploaded imagery is still necessary for coverage beyond the offline map.
- RF diagnostics label receiver antenna, RF mode and decoded RF output mW.
  No electrical current/power measurement is available from current hardware.
- Not pushed or published.

## Installed 3 November 2026 — remove temperature display, trim-neutral cue

Guarded Pi USB PlatformIO verified upload, approved XIAO/Waveshare landscape
profile, branch `codex/safety-control-refactor`, HEAD `91cf80c` plus working-tree
changes. Fresh main ancestry, firmware build, host/web tests and full UI/rotation
audit passed. Boot #618 passed startup checks and confirmed CUT.
Record `/tmp/r1-flash-tmcw2qe1`; log `/tmp/r1-trim-neutral-flash.log`.

- Hide temperature from Data → Sensors and sensor-widget choices, keeping all
  persisted source IDs stable. Existing temperature widgets become empty slots.
  Temperature telemetry decoding remains available internally.
- Trim/subtrim changes beep for 100 ms when a nonzero value reaches or crosses
  zero in either direction. Leaving zero and unchanged values are silent.
  Respects buzzer mute; navigation clicks cannot truncate the neutral cue.
- Not pushed or published.

## Installed 3 November 2026 — transient trim/subtrim feedback

Guarded Pi USB PlatformIO upload verified on the approved XIAO/Waveshare
landscape environment, branch `codex/safety-control-refactor`, HEAD `91cf80c`
plus working-tree changes. Main ancestry, firmware build, host/web tests and
full UI/rotation audit passed. Boot #616 passed startup checks and confirmed
CUT. Record `/tmp/r1-flash-vlgyrjtu`; log `/tmp/r1-adjustment-overlay-flash.log`.

- Steering/throttle trim or subtrim changes show a touch-through overlay for
  1.2 seconds after the last adjustment. A fixed ±100% scale shows response
  endpoints in cyan and neutral in amber, with signed values and clipping.
  Uses the configured proportional response before mixes, without advancing
  slew or changing output behaviour. Page rebuilds retain the brief expiry.
- Both channel trim and channel subtrim currently add offsets to the response,
  clipped at ±100%; subtrim does not preserve endpoints. Existing trim-live and
  subtrim-CUT behaviour is retained and the subtrim popup says CUT requested.
- Not pushed or published.

## Installed 3 November 2026 — live Tune values and mix monitor

Guarded Pi USB upload from `codex/safety-control-refactor`, HEAD `91cf80c`
plus working-tree changes, using `seeed_xiao_esp32s3-pcb`, Waveshare LCD F
landscape 480×320, schematic (5). Fresh main ancestry, build, host/web tests and
full UI/rotation audit passed; PlatformIO verified the flash. Boot #614 passed
K1/K2/touch/LCD/UI checks and acknowledged CUT. Record `/tmp/r1-flash-p2emb46t`;
log `/tmp/r1-live-tune-mix-flash.log`.

- Tune page two refreshes trim/subtrim from the channel model while assigned
  physical controls are used. Signed values show 0.1% resolution, with centred
  bars over the existing ±20% adjustment range. No page rebuild is needed.
  Existing live-trim and subtrim-CUT behavior is retained.
- Mixer inputs and curve pages default to Live controls. Show named input
  sources, mix effects and the draft target, separately from the resolved Sent
  value and CUT/LIVE state. The transfer curve has an amber draft marker and
  a green Sent marker at the current input;
  other inputs stay at their current values while the selected axis is swept.
  Test values remain an explicit mode, adjusted on the Inputs page. Apply is
  still required to save a draft; monitoring never changes outputs or settings.
- The read-only draft calculation reuses control-pipeline source shaping and
  channel dependencies, including independent activation switches. It displays
  a settled target; rate transitions, slew and threshold latch history remain
  owned by the real control engine. Missing/stale input snapshots are unavailable.
- This work has not been pushed or published to Vercel.

## Installed 2 November 2026 — stacked speed-widget labels

Guarded Pi USB upload from `codex/safety-control-refactor`, HEAD `91cf80c`
plus the layout changes below, using `seeed_xiao_esp32s3-pcb`, Waveshare LCD F
landscape 480×320. Fresh main ancestry, build, host/web tests and full UI audit
passed; PlatformIO verified the flash. Boot #609 passed all K1/K2/touch/LCD/UI
checks, and USB acknowledged CUT. Source/artifact record:
`/tmp/r1-flash-qsfjcxc5`; log `/tmp/r1-speed-sats-stacked-flash.log`.
This layout correction is not yet published to Vercel.

- Put the speed widget's satellite count on a separate line beneath km/h,
  right-aligned, so the count cannot read as part of the speed unit. Reserve
  space in Numbers, Dials and Graph layouts, including compact widgets.

## Simulator published 2 November 2026

- Rebuilt both WebAssembly orientations with Emscripten 4.0.15 and deployed the
  current working tree to https://jr-r1-demo.vercel.app/. Includes speed-widget
  Reset, delayed Drive hold, satellite captions and stationary-map follow/redraw
  improvements. Updated browser help with demo PIN 1234 and current hold timing.
- Vercel deployment `dpl_Hp2VAXz3m7LAd5izEEsiW4q8fKRJ`, build ID
  `dc893780c85b791d`; production HTML, app loader and both WASM hashes matched
  local output. Portrait interaction and both orientation renders checked.
- This publication does not upload transmitter firmware.

## Installed 2 November 2026 — satellite/map follow upload

Guarded Pi USB upload of the current working tree from
`codex/safety-control-refactor` / HEAD `2954bec`, environment
`seeed_xiao_esp32s3-pcb`, Waveshare LCD F landscape 480×320.
Fresh main ancestry, local firmware build, host/web tests and full UI audit
passed; PlatformIO verified the flashed images. Startup #608 passed all
K1/K2/touch/LCD/UI checks and USB acknowledged CUT. Vehicle/CU01 role persisted.
No receiver link or packets were present in the post-flash snapshot; live map
responsiveness remains for the user's hardware trial.
Source/artifact record `/tmp/r1-flash-6t_8bh17`; log
`/tmp/r1-satellite-map-follow-flash.log`.

- GPS page and map widgets keep the image stationary while the position marker
  moves within the central 60% of the viewport. First position, zoom changes or
  reaching an outer 20% margin recenter it. Dashboard maps keep a fixed scale
  instead of repeatedly fitting the trail. Marker/trail use the viewport centre
  for alignment; duplicate positions and unchanged overlay geometry/visibility/
  opacity no longer trigger repeated redraws. Trail objects use their actual
  bounds instead of extending to the map origin. Synthetic raster UI audit:
  a five-pixel movement redrew 1,620 pixels versus an 88,320-pixel landscape
  map area; unchanged updates redrew zero. Live hardware timing remains to be
  checked during the on-device trial.

- Show a small satellite count in dashboard GPS course and speed widgets,
  including maximized views. Unavailable or stale satellite reports show
  `-- sats`, independently of speed, position and course availability.

## Installed 2 November 2026 — speed reset and Drive hold upload

Guarded Pi USB upload from `codex/safety-control-refactor`, HEAD `2954bec`
plus the preserved working-tree changes, using `seeed_xiao_esp32s3-pcb`
(Waveshare LCD F, landscape 480×320). Build, host/web tests and full landscape
UI audit passed; PlatformIO verified all flashed images. Post-flash startup
#607 passed K1/K2, touch, LCD driver and UI checks; USB acknowledged CUT.
Local source/artifact record: `/tmp/r1-flash-a2p6pgzg`; upload log:
`/tmp/r1-widget-drive-delay-flash.log`. Receiver link was absent at the post-flash
snapshot, so this upload does not include a fresh live telemetry validation.

- Physical Drive-tab holds wait one second before showing progress, followed
  by the existing 0.8-second hold before release can request output or CUT.
  Normal navigation taps do not flash the hold overlay or footer progress bar.

- Add a Reset button directly to dashboard speed widgets, clearing the shared
  session maximum, average and speed graph history without leaving Drive.

- Decode GPS speed, position, course and satellites independently. CRSF all-FF
  unavailable fields no longer become map coordinates or block fresh speed.
  Speed widgets/statistics use their own freshness; maps retain the last usable
  point and resume only when valid position fields return. USB GPSSTATUS shows
  speed availability and age. Prepared from EdgeTX's missing-field convention;
  blue-mode hardware behaviour and effective RF update rate still need testing.


- In CU01/vehicle-battery mode, decode stock ExpressLRS voltage-group source 128
  as the receiver's independent ADC supply reading. CU01 0x08 packets continue
  to populate vehicle data; CV01 cell groups remain separate. Legacy FrSky FAS
  source-128 interpretation remains available in receiver-battery mode.


- Hide dedicated FrSky FAS voltage/current cards and new-widget choices during
  ERS testing, retaining persisted source IDs and legacy decoding.
- Data → Sensors → Current / energy adds a CUT-only, saved choice for ordinary
  CRSF battery packets: Receiver supply or Vehicle pack / CU01. This changes
  handset interpretation only; it sends no discovery or configuration commands.
- USB TELEMETRY includes ERS battery role, voltage/current/capacity/remaining and RPM.


- USB `GPSSTATUS` and `TELEMETRY` now distinguish GPS packet reception, rejected
  payloads, stale data and usable position. Include cumulative GPS counters and
  packet/update ages without printing coordinates.

- Remove alternating fix-unknown/stale text from the GPS page and map widget.
  Show No GPS position only until the first usable point; retain the last point
  with a muted, translucent marker while stale. Freshness checks are unchanged.

- First usable GPS position triggers a non-blocking, three-note rising chime
  (C7/E7/G7, about 440 ms total), once per boot. Respects Buzzer On/Off;
  telemetry dropouts/model changes do not retrigger it. Timer warnings and
  longer alerts take priority; missed notes are not replayed after a stall.
- This announces usable reported coordinates, not confirmed GPS fix quality.
- Deferred README/manual update: describe GPS acquisition sound.

## Latest installed updates

### 1 November 2026 - R1 dc3d100 / ER6G e8e9fff9

- R1 uploaded through the Pi USB connection; ER6G uploaded over Wi-Fi.
- GPS position continues when the S.Port sensor omits course. Receiver sends
  an optional R1 course-validity marker; the handset distinguishes unavailable
  course from a valid zero-degree/north report.
- New GPS course widget: numeric or compass dial, cardinal direction and degrees.
  Shows direction of travel, with no arrow for missing/stale position/course or
  speed below 1 km/h. Supports the existing sizes and temporary maximisation.
- The Pi upload helper now uses a stable artifacts-only directory. Git ancestry,
  firmware build and tests run on the Mac; the Pi verifies transferred files
  and hardware configuration, then uses guarded PlatformIO nobuild/upload.
- README/manual follow-up: describe course availability, the new widget and
  add screenshots after handset validation. No manual refresh in this change.

## Installed transmitter firmware

### 1 November 2026 - 06b6928 (Wi-Fi OTA)

- Authenticated OTA upload accepted and image validation confirmed; handset
  restarted and the updated web frontend was verified at its home-network IP.
- Model 1 / HYPER 7 retained; handset reported CUT/configuration access after
  restart. Physical backlight brightness still needs owner confirmation.
- Host tests, landscape build and both-orientation UI/rotation checks passed.

#### PlatformIO Wi-Fi upload transport

- Explicitly select PlatformIO's custom protocol for authenticated web OTA.
  Setting only the upload command allowed the ESP32 platform builder to replace
  it with its default serial uploader. USB retains its original transport.

#### GPS controls stay visible

- Temporarily disable the GPS page's automatic edge-to-edge fullscreen mode.
  Its map controls, title bar and navigation stay visible while idle.
- The dashboard map widget still supports temporary long-press expansion and
  the overlaid minimise chevron.

#### backlight brightness after sound

- Separate the buzzer and backlight LEDC hardware timers. Their old channel 6/7
  pairing allowed a beep to change the backlight timer from 8-bit to 10-bit,
  making the 100% brightness setting behave like roughly 25%.
- Add a compile-time guard against assigning both peripherals to one timer.
- Confirm physical brightness after upload; the simulator does not model shared
  ESP32 PWM timer registers.

#### dashboard widget focus

- Hold a widget body for about 0.65 seconds, then release to maximise it.
- The expanded widget uses the area below the title bar down to the bottom
  navigation. The extra heading and Restore row are omitted, and the widget's
  own title is hidden while enlarged.
- A small overlaid down-chevron returns to the grid. Back or another body hold
  also minimises. Short taps and timer Start/Reset buttons keep their usual jobs.
- Expansion is temporary: model layout, sizes and covered widgets are retained.
  All channel/input visibility settings and widget sources still apply.
- The embedded web mirror supports the same hold gesture, retaining its existing
  authentication, CUT and separate output-enable hold checks.
- Verified with XIAO landscape builds and simulator coverage of both orientations,
  widget types and cell positions. Physical handset validation follows upload.

### 1 November 2026 - 0b05af7

- Countdown buzzer cues: one beep at 20 seconds remaining, two at 10 seconds,
  one per second from 5 through 1, and an 800 ms end beep at zero.
- Non-blocking sound scheduling; pause, reset and mute handling prevent replay
  of old cues. Requires Buzzer and Race-end alarm enabled.
- Uploaded through Pi USB; startup checks passed for K1/K2, touch, LCD and UI.

## Documentation refresh - 1 November 2026

- Updated README and operating manual (edition 1.3) to installed firmware 06b6928.
- Documented verified authenticated OTA, guarded PlatformIO web upload, CUT,
  package validation, restart and USB recovery limitations.
- Added clearly labelled simulator examples of the saved dashboard and enlarged
  All channels, GPS and Timer widgets; documented the minimise chevron and
  preserved layout, plus portrait/landscape support and future auto-rotation.
- Updated countdown sound cues, visible GPS controls and GPS status explanations;
  distinguished installed ER6G trial firmware from unconfirmed live GPS delivery.
- Rebuilt and visually checked the printable manual; preserved image proportions.

## Documentation follow-up

- Deferred README request (1 November 2026): show a range of Drive dashboard
  arrangements and widget contents in screenshots, including different sizes,
  combinations of channels/inputs, maps/course, timer, speed, power/cells and
  model artwork. Prefer native handset captures; label simulator examples.

- Replace widget-focus simulator examples with native handset captures once the
  handset is reachable and the new layout has been checked on hardware.
- Confirm physical backlight brightness after the PWM timer fix.
- Continue recording changes here for periodic README/manual refreshes.

### Prepared ERS telemetry support — 2026-11-01

- RadioMaster ERS GPS/CV01/CU01/RB01 commissioning: authenticated web discovery,
  settings reads, GPS mode, motor poles and battery capacity.
  Confirmed replies/timeouts, bounded storage, CUT/module-link gating and no write retries.
- ERS battery voltage/current/used mAh/remaining readings in Data and dashboard Sensor widgets.
  External UART pack reports use trial battery ID 2 to preserve receiver supply and S.Port FAS.
- CV01 unused trailing zero cell slots supported; malformed/interleaved zero data rejected.
- Receiver UART assignments require explicit configuration; no automatic servo remapping.
- Physical ERS sensor/chain and configuration acknowledgement tests remain outstanding.
  Commissioning notes: docs/ers-sensor-commissioning.md. Include in the next periodic README/manual refresh.
- Stock ExpressLRS CU01 packets are supported through an explicit, saved standard-battery role selector; no inference from current or voltage.

### Native ERS commissioning — 2026-11-01

- Setup → Model → ERS sensors adds discovery, sensor list, settings reads and
  GPS mode / motor pole count / current-sensor capacity editing on the LCD.
- Shares the web client's RF queue, CUT/module-link guards and acknowledged
  status; works without Wi-Fi. Drafts remain unconfirmed until the sensor replies.
- Retained landscape/portrait layouts, keypad focus and normal Back navigation.
- Include native sensor commissioning in the next README/manual refresh.

- Fixed Pi binary-only USB uploads after OTA: restore the normal bootloader,
  partition and OTA-slot-selection writes when PlatformIO uses `nobuild`.
  Application-only verification could otherwise leave the previous OTA slot running.
  NVS/model settings and map storage are preserved.

- ERS discovery now includes the manufacturer's all-device heartbeat broadcasts
  and accepts heartbeat registrations over an eight-second scan. Initial ping
  alone could miss devices with other IDs. Added bounded USB configuration-path diagnostics.

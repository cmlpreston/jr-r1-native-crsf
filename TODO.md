# JR R1 controller consolidated backlog

This is the single high-level backlog for the project. Detailed programmable
input/control work is tracked in
[`docs/control-assignment-roadmap.md`](docs/control-assignment-roadmap.md) and
must not be duplicated here. Items are ordered roughly by implementation and
safety priority.

The authoritative architecture-refactor execution order is recorded under
`Agreed execution order` in
[`docs/architecture-review.md`](docs/architecture-review.md). Follow that order
before beginning later driving-control features.

## Current milestone: complete the two primary controls

- [ ] Generalise proportional-input calibration and deadband beyond the
  throttle trigger. Calibration belongs to the physical input; response
  settings belong to the assigned CRSF output channel.
- [ ] Map and connect the 5 kOhm steering pot from the FPC to its reserved ADC
  input and give it the same sampling, filtering and fault detection as the
  throttle trigger.
- [ ] Route steering through the existing per-channel reverse, sub-trim,
  endpoint, rate, expo and ramp profile.
- [ ] Add steering-specific turn and return ramp controls.
- [ ] Verify throttle and steering simultaneously at the 250 Hz CRSF control
  rate, including rapid TFT interaction and telemetry traffic.

## Safety before driving the electric buggy

- [ ] Map and require a dedicated, exclusive physical arm/CUT control.
- [ ] Guarantee CUT overrides every channel binding, future mix, macro, hold
  and ABS output.
- [ ] Bench-test receiver PWM during TX loss, receiver reboot and deliberate
  brownout with the motor disconnected; save Saleae captures and pulse widths.
- [ ] Select and verify the ESC throttle failsafe pulse and receiver failsafe
  configuration.
- [ ] Add boot self-tests for ADC plausibility, calibration validity, CRSF
  output, receiver failsafe configuration and selected model.
- [ ] Add explicit startup and failsafe values for retained accessory channels.
- [ ] Prevent safety-critical functions from sharing unsafe secondary bindings.
- [ ] Log ADC/input faults, stale-control cuts, receiver link loss and
  brownouts.

## Finish the physical R1 input hardware

- [ ] Trace and document all twelve steering FPC conductors.
- [ ] Connect and verify every original button, maintained switch,
  spring-return trim and grip trim through direct GPIO or the MCP23017.
- [ ] Add a physical-input diagnostic/calibration page for raw ADC/GPIO,
  debounced state and detected faults.
- [ ] Document the final interface PCB, connectors, filtering, protection,
  power rails and BOM.

## Assignment system and models

See the detailed roadmap for individual binding features.

- [ ] Dispatch the remaining transmitter, model-parameter and drive-mode
  actions exposed by the assignment model.
- [ ] Add TFT editors for binding step, range, initial value, failsafe value,
  pulse duration and retained-state behaviour.
- [ ] Add configurable trim/sub-trim ranges and step sizes; the current fixed
  implementation is +/-20% in 1% steps.
- [ ] Add restore-default-control-profile.
- [ ] Add model select, name, copy and reset, with model changes allowed only
  while CUT and throttle-neutral.
- [ ] Move bindings and response settings into versioned per-model profiles.
- [ ] Add versioned JSON profile import/export and a Wi-Fi configuration UI.
- [ ] Later: shareable setup codes/files, excluding binding credentials and
  transmitter-specific calibration; validate channel roles and endpoints on import.
- [ ] Later: quick handover between two vehicles, with explicitly verified
  parked/braked receiver behaviour for the inactive vehicle and neutral re-arm.
- [ ] Later: multi-action macros, conditions and general mixes with direction,
  weight and offset.

## Driving controls

Complete these after steering shares the generic proportional pipeline:

- [ ] Add an optional five-point response curve with monotonic validation and
  editing on the existing live response graph.
- [ ] Add assignable throttle hold, commanding configured neutral and requiring
  neutral before release resumes drive.
- [ ] Add brake-side ABS with configurable activation point, depth and
  frequency; CUT, failsafe and hold always take priority.
- [ ] Allow a physical dial or digital trim to adjust the Walk-mode forward
  limit while retaining full brake authority.
- [ ] Later: race-start countdown with an optional randomised start delay and
  user-defined buzzer macros.
- [ ] Deferred until there is a vehicle use case: 4WS and gyro-gain controls,
  including gradual speed-dependent gyro assistance using valid telemetry.

## Display and interaction

- [ ] Evaluate the overall interface proposal in
  [`docs/ui-design-direction.md`](docs/ui-design-direction.md): light daytime
  palette, Drive/Tune/Setup navigation, persistent output status and direct
  parameter editing, inspired by DJI's settings and operating screens.
- [x] Audit portrait and landscape menu navigation, edit drafts, widget persistence, model
  guards, timer operation, arm hold and focus bounds in the native simulator.
- [x] Rebuild advanced menus for portrait and make portrait the default.
  Use Model/Handset tabs, flat menu rows and a permanent JR R1 strip.
- [ ] Add touchscreen lock and dashboard short/long/double-press shortcuts.
- [ ] Evaluate optional power-on PIN unlock before arming (user idea). Unlock
  once per power cycle; retain neutral/arming interlocks and immediate CUT at
  all times. Define recovery and failed-attempt behavior before implementation.
- [ ] Add dim-before-sleep plus dedicated sunlight and night palettes.
- [x] Port Drive/Tune/Data/Setup into LVGL with a per-model 2×2 widget grid:
  timer, paired TX/RX power, one/two channels, sensor, GPS trail and model image.
  Support 1×1, 2×1 and 2×2 sizes, applicable bars/dials/graphs/switches styles,
  Apply/Cancel editing, stale telemetry and persistent model/link/CUT status.
- [ ] Add asset uploads and offline map backgrounds through the S3 setup portal.
- [ ] Add quick-adjust overlays for dashboard widgets.
- [ ] Measure and record Waveshare awake, controller-off and backlight-off
  current.
- [ ] Decide whether the production interface needs a MOSFET-controlled
  backlight or switched TFT 5 V rail.

## Telemetry and vehicle peripherals

- [x] Decode CRSF GPS, source-zero temperature and RPM with freshness checks.
- [ ] Extend battery telemetry beyond voltage to current and consumed capacity.
- [ ] Define and prototype the vehicle-side bridge for GPS, RPM, current,
  voltage and temperature sources, including any required S.Port/i-BUS to CRSF
  conversion.
- [ ] Build the driving telemetry page: speed/max speed, RPM, pack voltage,
  current, consumed mAh, receiver voltage and temperatures.
- [ ] Add runtime, trip distance, odometer and consumed-capacity accumulation.
- [ ] Add the GPS/map page with heading, track history and offline OpenStreetMap
  tiles for selected driving areas and zoom levels.
- [ ] Log telemetry and export it over Wi-Fi/BLE.
- [ ] Investigate ER6G/ELRS serial sensors and safe remote ESC configuration.
- [ ] Later: maintenance reminders, battery-cell-count suggestion with user
  confirmation, camera control and external trainer/wheel/pedal inputs.
- [ ] Later: vehicle inclination and jump-height telemetry, subject to suitable
  vehicle-side sensing and validation; low priority for the Hyper 7.

The later ideas above include the SourceTX discussion reviewed on 2026-09-15:
<https://www.reddit.com/r/rccars/comments/1vbqozz/introducing_sourcetx_modern_touchscreen_rc_car/>.
They are deferred possibilities, not commitments for the current PCB/firmware release.

## Local browser setup portal (future revision)

- [ ] Add an ESP32-S3-hosted setup portal, opened deliberately from handset
  Setup and usable from a phone or computer over local Wi-Fi without a cloud
  account. Keep the browser and handset interfaces in the same design language.
- [ ] Support model creation, duplication, naming and backup/import. Share a
  versioned model schema and validation with the handset; assess the current
  ten-model limit and twelve-character model names before extending them.
- [ ] Upload model logos with browser-side cropping, resizing and a preview at
  the actual LCD pixel size. Store media separately from model configuration,
  referenced by asset ID, with a built-in fallback image.
- [ ] Enter an explicit maintenance/CUT mode for configuration and asset
  uploads; require deliberate exit and normal arming checks before driving.
  Limit portal access to the local setup session and validate uploaded files.
- [ ] Assess available flash/filesystem capacity and any additional storage
  before choosing asset limits. Stage and validate uploads before replacing
  active files so an interrupted transfer preserves the previous model/assets.
- [ ] First deliver models, logos and backup/restore; then add bounded offline
  map/track packages with geographic bounds or other georeferencing metadata,
  storage budgets and required map attribution. An image alone is insufficient
  to place live GPS coordinates correctly.

## ExpressLRS management validation

- [ ] Bench-test the implemented ExpressLRS 4.1 `SET TX`/`UID TX` flow against
  the Ranger Nano and save a logic-analyser request/response capture.
- [ ] With a linked 4.1 receiver, test receiver-first `SET RX`, then `SET TX`,
  and verify both returned UIDs against the locally calculated bindphrase UID.

## Automated testing and release engineering

- [ ] Split more safety and response processing into host-testable modules.
- [ ] Add automated tests for response profiles, slew limits, safety
  transitions, assignments, drive modes and saved-settings migration.
- [ ] Add scripted SDL screenshot/layout comparisons.
- [ ] Produce a reproducible hardware/firmware build guide.
- [ ] Decide public-release scope and licence.

## Completed foundation

- [x] Initial holistic control/channel architecture audit, ownership matrix and
  staged behaviour-preserving refactor plan.
- [x] Extract pure channel-response processing and add a unified host-test
  runner with characterization coverage.
- [x] Extract generic proportional-input profile/runtime conditioning and
  migrate throttle EMA, validity and deadband mapping with legacy-equivalence
  tests.
- [x] Native CRSF transmission with a 16-channel output model.
- [x] Independent 250 Hz control processing and TFT/telemetry tasks.
- [x] Throttle sampling, filtering, calibration, neutral interlock and CUT.
- [x] Per-channel reverse, response sub-trim, endpoints, rate, expo and output
  ramp for assigned proportional controls.
- [x] MCP23017/direct-GPIO input layer, debounce, event generation and Input
  Monitor.
- [x] Persistent editable bindings for CRSF channels, trim/sub-trim, initial TX
  functions, drive modes and contexts.
- [x] Shared semantic touch/keypad navigation and editing across current pages.
- [x] Startup maintained-switch/spring-trim warning and arm interlock.
- [x] Dashboard, enabled-channel monitor and basic ELRS link telemetry page.
- [x] Waveshare display profile, controller/backlight blanking and guarded
  touch-to-wake.
- [x] SDL2 simulator with persistent settings and screenshot capture.

# JR R1 controller backlog

This is the maintained backlog for ideas deliberately deferred from the current
proof of concept.

## Safety and validation

- Bench-test the implemented ExpressLRS 4.1 `SET TX`/`UID TX` flow against the
  Ranger Nano and save a logic-analyser request/response capture.
- Once a receiver is linked on 4.1, test receiver-first `SET RX`, then `SET TX`,
  and verify that both returned UIDs match the locally calculated phrase UID.

- Bench-test ER6G PWM output during boot, TX loss, RX reboot and deliberate
  brownout with the motor disconnected; save Saleae captures and pulse widths.
- Add a physical throttle-cut/arm control after mapping the R1 switches.
- Add boot self-tests for ADC plausibility, calibration, CRSF, receiver failsafe
  configuration and selected model.
- Log input faults, stale-control cuts, brownouts and link loss.
- Choose the ER6G throttle failsafe pulse after testing the actual ESC.

## Inputs, assignments and models

- Follow the detailed checklist in `docs/control-assignment-roadmap.md`; it is
  the source of truth for the programmable control-system feature.
- Map the steering FPC and all original R1 buttons, toggles and trims.
- Extend the TFT assignment editor beyond the now-implemented CRSF outputs,
  per-channel trim/subtrim, first transmitter functions and first drive-mode
  selections to editable contexts and multi-action bindings.
- Add configurable step/range limits for channel trim/subtrim; the first
  implementation uses +/-20% with 1% steps.
- [x] Feed enabled auxiliary outputs and their live values into the dashboard's
  right-hand vertical channel bank; keep steering/throttle as horizontal bars.
- Support transmitter functions, channels, modes and multi-action macros.
- Add TFT editors for behaviour-specific range, step, initial, failsafe and
  pulse-duration values (the resolver already supports them).
- Add model select/name/copy/reset and safe switching through neutral.
- Export/import versioned JSON profiles through a Wi-Fi web interface.
- Add general multi-input/output mixes with direction, weight and offset.

## Driving functions

- Generalise endpoints, trim, expo, rate and speed to every proportional input.
- Add steering-specific turn and return speed.
- Add optional throttle curves, ABS, 4WS, gyro gain and throttle hold.
- Allow a physical dial to adjust the Walk-mode limit.
- Add race-start countdown and user-defined buzzer macros.

## Display and interaction

- Add a MOSFET-controlled TFT backlight or switched 5 V rail so brightness and
  sleep reduce the measured 0.25 A TFT load.
- Add dim-before-sleep and dedicated sunlight/night palettes.
- Add configurable dashboard widgets and quick-adjust overlays.
- [x] Replace page-specific keypad coordinate emulation with a registered UI
  control layer shared by touch, Select and +/- actions. Channels and
  Assignments include their bank controls; throttle, Drive Mode, Setup,
  channel roles, batteries, display and assignment editing are migrated.
- Extend the registered-control layer to ELRS text entry and Input Monitor
  paging, then add simulator navigation regression cases.
- Add touch and physical-input diagnostic/calibration pages.
- [x] Bind the Timer button to next-dashboard-page in a new default R1 profile
  while retaining existing user assignments and alternate selectable actions.

## Telemetry and peripherals

- [x] Add a dedicated live telemetry page for ELRS link state, LQ, RSSI, SNR,
  antenna/RF fields, TX/RX voltage and CRSF stream diagnostics.
- Extend the CRSF parser for standard GPS and battery/current telemetry frames.
- Add a driving telemetry page with speed, maximum speed, RPM, main-pack
  voltage, current, consumed mAh and motor/ESC/battery temperatures.
- Add a GPS/map page with position, heading, track history and offline
  OpenStreetMap tiles for selected driving areas and zoom levels.
- Define the vehicle-side telemetry bridge for GPS, RPM, ESC/current and
  temperature sources, including FrSky S.Port/i-BUS conversion into CRSF.
- Add run time, trip distance, odometer, maximum speed and consumed capacity.
- Add motor, ESC, battery and servo temperatures plus RPM/wheel speed.
- Add maintenance reminders based on run time or distance.
- Log telemetry and export it over Wi-Fi/BLE.
- Investigate ER6G serial sensors and safe remote ESC configuration.
- Suggest battery cell count, requiring explicit user confirmation.
- Investigate camera control and external trainer/wheel/pedal inputs.

## Release engineering

- [x] Run the real LVGL UI in an SDL2 desktop simulator with mouse touch,
  simulated control/telemetry values, file-backed settings and screenshots.
- Add scripted navigation/layout regression cases to the SDL simulator once
  page-level test hooks are defined.
- Split control processing into host-testable modules.
- Test CRSF packing, safety transitions, slew limits, assignments, modes and
  saved-settings migration automatically.
- Document an interface PCB, BOM, enclosure and reproducible build.
- Decide which portions can be released publicly and under which licence.

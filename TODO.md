# JR R1 controller backlog

This is the maintained backlog for ideas deliberately deferred from the current
proof of concept.

## Safety and validation

- Bench-test ER6G PWM output during boot, TX loss, RX reboot and deliberate
  brownout with the motor disconnected; save Saleae captures and pulse widths.
- Add a physical throttle-cut/arm control after mapping the R1 switches.
- Add boot self-tests for ADC plausibility, calibration, CRSF, receiver failsafe
  configuration and selected model.
- Log input faults, stale-control cuts, brownouts and link loss.
- Choose the ER6G throttle failsafe pulse after testing the actual ESC.

## Inputs, assignments and models

- Map the steering FPC and all original R1 buttons, toggles and trims.
- Expand control assignments from steering/trigger to every physical input.
- Support transmitter functions, channels, modes and multi-action macros.
- Add momentary, toggle, three-position and proportional behaviours.
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
- Add touch and physical-input diagnostic/calibration pages.

## Telemetry and peripherals

- Add run time, trip distance, odometer, maximum speed and consumed capacity.
- Add motor, ESC, battery and servo temperatures plus RPM/wheel speed.
- Add maintenance reminders based on run time or distance.
- Log telemetry and export it over Wi-Fi/BLE.
- Investigate ER6G serial sensors and safe remote ESC configuration.
- Suggest battery cell count, requiring explicit user confirmation.
- Investigate camera control and external trainer/wheel/pedal inputs.

## Release engineering

- Split control processing into host-testable modules.
- Test CRSF packing, safety transitions, slew limits, assignments, modes and
  saved-settings migration automatically.
- Document an interface PCB, BOM, enclosure and reproducible build.
- Decide which portions can be released publicly and under which licence.

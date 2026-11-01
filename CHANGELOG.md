# R-1 EVO change record

Record user-visible changes here as they are implemented. Update the operating
manual periodically rather than regenerating it for every firmware change.
Entries below distinguish prepared source from firmware already installed.

## Pending transmitter upload

### 1 November 2026 - GPS controls stay visible

- Temporarily disable the GPS page's automatic edge-to-edge fullscreen mode.
  Its map controls, title bar and navigation stay visible while idle.
- The dashboard map widget still supports temporary long-press expansion and
  the overlaid minimise chevron.

### 1 November 2026 - backlight brightness after sound

- Separate the buzzer and backlight LEDC hardware timers. Their old channel 6/7
  pairing allowed a beep to change the backlight timer from 8-bit to 10-bit,
  making the 100% brightness setting behave like roughly 25%.
- Add a compile-time guard against assigning both peripherals to one timer.
- Confirm physical brightness after upload; the simulator does not model shared
  ESP32 PWM timer registers.

### 1 November 2026 - dashboard widget focus

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

## Installed transmitter firmware

### 1 November 2026 - 0b05af7

- Countdown buzzer cues: one beep at 20 seconds remaining, two at 10 seconds,
  one per second from 5 through 1, and an 800 ms end beep at zero.
- Non-blocking sound scheduling; pause, reset and mute handling prevent replay
  of old cues. Requires Buzzer and Race-end alarm enabled.
- Uploaded through Pi USB; startup checks passed for K1/K2, touch, LCD and UI.

## Next manual refresh

- Document temporary widget maximisation and the overlaid minimise chevron.
- Refresh affected dashboard captures after hardware validation.
- Reconcile examples and firmware revision identifiers with the selected release.

## Next README refresh

Record requested README additions here; apply them during a periodic README
update rather than editing the README for every firmware change.

### Requested 1 November 2026

- Describe temporary widget maximisation: hold a widget body, then minimise
  with the overlaid down-chevron, Back or another hold; the saved grid is
  preserved. Include paired normal-grid and maximised screenshots, with useful
  examples such as All channels, GPS map and Timer. Prefer native handset
  captures after upload/validation; label any simulator examples clearly.
- Explain that a complete portrait UI is maintained alongside landscape, with
  simulator/build coverage of both orientations. This supports rotating the
  handset screen; a manual orientation control already exists. Automatic
  rotation is a future option requiring an accelerometer that has not yet been
  added, plus orientation detection and automatic-rotation firmware. Do not
  describe accelerometer-based auto-rotation as implemented.

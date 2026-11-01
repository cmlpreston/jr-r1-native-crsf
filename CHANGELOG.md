# R-1 EVO change record

Record user-visible changes here as they are implemented. Update the operating
manual periodically rather than regenerating it for every firmware change.
Entries below distinguish prepared source from firmware already installed.

## Pending transmitter upload

No firmware changes pending immediately after the verified OTA update below.

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

- Replace widget-focus simulator examples with native handset captures once the
  handset is reachable and the new layout has been checked on hardware.
- Confirm physical backlight brightness after the PWM timer fix.
- Continue recording changes here for periodic README/manual refreshes.

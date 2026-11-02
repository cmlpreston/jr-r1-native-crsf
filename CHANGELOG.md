# R-1 EVO change record

Record user-visible changes here as they are implemented. Update the operating
manual periodically rather than regenerating it for every firmware change.
Entries below distinguish prepared source from firmware already installed.

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

# Sensor history widget

Choose a dashboard slot, select **Sensor**, the desired source, and **Graph**
style. Existing saved Graph widgets automatically use the new presentation.
The chart shows current readings over the last 60 seconds on a fixed time axis,
with automatic vertical limits labelled in the sensor's units. Zero is a valid
reading. Sensor values retain 32-bit precision before rendering; large RPM
values are normalized to LVGL's chart coordinate range.

Collection uses at most one actual observation per second in a bounded
61-sample ring for each sensor, held in PSRAM-preferred UI storage. Collection
continues away from the dashboard. UI stalls do not fabricate intervening
samples; missing/invalid telemetry and readings older than three seconds leave
gaps. Earlier valid sections remain visible until they scroll out. Chart arrays
stay at stable addresses and refresh only when their contents change.

History is live session data, not a power-cycle log. Model changes and sensor
resets clear it and exclude the cached packet at the reset boundary. Saved run
statistics, extrema, GPS routes and their checkpoint format remain unchanged.
Number and Dial styles retain their existing statistics and reset controls.
For Graph style the displayed reading and plot always use the current source;
the large-value Current/Max/Average selector is omitted.

The host suite checks sample timing, fixed-window gaps, stale/reset packets,
zero/negative/high RPM values, capacity limits and clock wrap. GitHub also
builds the simulator and runs a focused LVGL audit when the history collector,
renderer, backing storage or focused audit changes. It covers all widget sizes,
both display orientations, scaling, gaps, reset, typography and chart bounds.
Other changes skip this simulator step. A successful focused audit is explicitly
named in the artifact manifest; it does not claim a full application UI audit.

## Installed and validated — 17 November 2026

The user-authorized upload installed `929620cb3f3421b71776b4a1a669ef68c36332e4`
from `codex/r1-v2`, environment `seeed_xiao_esp32s3-pcb`, XIAO ESP32-S3 /
Waveshare LCD(F), schematic (5), 480x320 landscape. The temporary bench startup
baseline is unchanged. [Hosted validation](https://github.com/cmlpreston/jr-r1-native-crsf/actions/runs/44070306542)
passed the firmware build, complete host suite and focused LVGL audit. All eight
size/rotation screenshots were generated; compact landscape, compact portrait
and full landscape previews were visually checked. The initial audit mixed
synthetic timestamps with page initialization's real clock; seeding after page
creation corrected the fixture without weakening its gap assertions.

Firmware-only ccache results: 388/392 cacheable calls hit (98.98%), four misses;
build time 50.39 s. The entire hosted job, including this feature's simulator
build/audit and packaging, took 3m37s. RAM is 204284 bytes and flash 2243733 bytes.
Firmware SHA256:
`0124ba1615a2c1560d97b71a706bd24eba5db41cf269e950c542e1cb430bcb61`.

Fresh main ancestry, matching USB identity, embedded commit, artifact hashes,
hardware profile/pins and framework boot image were checked on the Pi. Reusing
`.pio/cloud-upload`, guarded `nobuild` upload took 39.87 s, with 26.0 s application
transfer; all flash hashes verified. No local compilation or full suite rerun.
Read-only status at uptime 10.4 s reports CUT, both MCPs online, zero read errors,
safe outputs 992/992, no RF bench authorization and no arm request.

To use it, edit a **Sensor** dashboard widget, choose the source, and select
**Style: Graph**. The graph is always the current sensor stream; the Number
style's large-value Maximum/Average setting is hidden while Graph is selected.
Real driving/telemetry observation remains a handset follow-up.

Logs/artifacts: `/tmp/r1-sensor-history-validated`,
`/tmp/r1-sensor-history-upload.log`; preview PNGs:
`/tmp/r1-sensor-history-landscape-0.png`,
`/tmp/r1-sensor-history-landscape-3.png`,
`/tmp/r1-sensor-history-portrait-0.png`.

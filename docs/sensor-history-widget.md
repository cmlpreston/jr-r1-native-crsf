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

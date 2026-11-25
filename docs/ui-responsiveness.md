# UI responsiveness — first pass, 2026-09-19

## Finding and change

The integrated UI refreshes every 80 ms. LVGL 8.4 invalidates labels and styles
when setters are called, even when their content is unchanged. The dashboard
and integrated page updates were repeatedly sending identical text, font and
colour values, causing unnecessary dirty-region rendering and SPI transfers.

`include/ui_refresh.h` compares live values with their current LVGL properties
before setting them. Page construction remains unchanged. Formatted values use
a small stack buffer with an allocation fallback for longer text, without
truncation. Speed widgets now set only their chosen primary value, avoiding an
instantaneous-value write followed by a max/average write on every tick.

## Measured validation

The native SDL audit installs an LVGL display monitor callback and counts pixels
actually rendered across 20 identical dashboard updates after the initial draw.
The fixture contains paired voltage readings and channel bars, with history
sampling held fixed. It does not model SPI duration or measure touch latency.

| Orientation | Before | After |
| --- | ---: | ---: |
| Landscape | 413,160 pixels | 0 pixels |
| Portrait | 425,400 pixels | 0 pixels |

Regression checks also require changed voltage, channels, link quality and safety
status to redraw; verify stale and low-voltage warnings; and cover unchanged
dials, a stopped timer, max-speed display, and long formatted text. Existing
portrait/landscape interaction audits and host tests pass. The active XIAO
portrait firmware builds successfully.

Commands:

```
make -C test run
make -C simulator -j6 audit
.venv/bin/pio run -e seeed_xiao_esp32s3-pcb-portrait
```

## Limits and next measurements

No change to the 250 Hz control path, bus speeds, 80 ms display-update cadence,
touch debounce, arming or output safety. This result is specific to unchanged
numeric/bar/dial widgets; graph histories and GPS trails can still invalidate
unchanged geometry. Full-page creation still requires a full display transfer.

Next: measure real flush time/pixel counts and touch-to-visible response on the
XIAO, then profile graph/map geometry updates and full-page transitions. The first pass was flashed and the user reported it worked without an obvious
speed improvement or regression.

## Second pass: skip unchanged LCD rows

Every page rebuild invalidates the whole screen. The flush path now compares
RGB565 rows against the last transferred image and sends only runs of changed
rows. The XIAO cache uses 307,200 bytes (300 KiB) of its 8 MiB PSRAM, plus one
validity byte per screen row. No full frame is allocated in internal/DMA RAM.
Allocation failure falls back to the original transfer behaviour. Initial
render and wake force a transfer; partial writes cannot validate unknown rows.

Native SDL measurements for Setup → Tune (same UI and data):

| Orientation | LVGL rendered pixels | Pixels sent after cache | Reduction |
| --- | ---: | ---: | ---: |
| Landscape | 153,600 | 85,920 | 44.1% |
| Portrait | 153,600 | 83,200 | 45.8% |

Rebuilding an identical Setup page transfers zero pixels. Simulator audits
exercise both cases, full transfer after invalidation, and unchanged live
values. Cache unit tests cover initial/partial writes, changed data, isolated
rows, invalidation, and allocation-failure fallback. Optional
`JR_R1_AUDIT_CACHE_IMAGES=/tmp/r1-cache` writes cached and forced-full redraw
BMPs for pixel equivalence checks.

`UI PERF` serial lines are emitted after a refresh following a page rebuild or
accepted touch. They report integrated-page construction time, rendered/sent
pixels, transfer count, time inside the display driver, elapsed page time and
accepted-touch-release-to-refresh-completion time. Touch time excludes the
physical hold and release debounce; it is not first-contact latency. The SPI
field in the simulator measures SDL work, not hardware SPI. Logs are emitted
outside the flush callback; serial logging itself can affect subsequent timing.

These changes retain the working bus speeds and independent control task.
Real hardware transfer/latency measurements and subjective validation of this
second pass remain pending. Row comparison and fragmented transfers have their
own overhead; pixel savings do not establish a corresponding latency reduction.
Full-page layout/render CPU work remains unchanged.

## Hardware observations and A/B instrumentation

The user found no obvious subjective speed change with either optimisation.
The row-cache build logged real transitions of 347–381 ms from accepted touch
release, including 62–72 ms page construction and 151–161 ms driver transfer.
An identical Tune-page rebuild transferred no pixels but still took 196 ms.
Actual navigation transferred 122,560–130,240 of 153,600 pixels: the synthetic
Setup→Tune fixture's savings should not be generalised to all pages.

The next candidate adds temporary serial commands `UICACHE OFF` and `UICACHE ON`.
OFF bypasses comparisons and copies and sends each LVGL buffer in one transfer.
Switching modes invalidates the cache; ON's first frame primes it. The selection
is not saved, and reboot restores ON. This does not change radio output, display
clock or touch handling. Tests verify re-enabling cannot reuse stale pixels and
that on/off output images match in both orientations.

Additional `UI PERF` fields:

- `layout`: explicit pending screen layout before the LVGL handler on page builds.
- `flush-overhead`: total flush callback time minus driver-transfer time, including
  row comparison, cache copies and transfer bookkeeping.
- `lvgl-other`: time inside `lv_timer_handler` minus flush callback time, including
  rendering, LVGL timers and any remaining layout.

These are wall-clock measurements, so task preemption is included. Construction
may itself cause some immediate layout work; the fields are measured call scopes,
not an exhaustive decomposition of CPU usage. `page` also includes intervening
UI updates/scheduling. The timestamp is captured before printing the log.

Hardware comparison procedure (pending):
1. With the handset safely in CUT, record Drive→Tune→Data→Setup→Data→Tune→Drive
   with the cache ON, repeat several times.
2. Send `UICACHE OFF` followed by Enter and repeat the same sequence/data state.
3. Send `UICACHE ON`, navigate once to prime it, then repeat as a consistency check.
4. Compare medians per transition, not different pages or simulator timings.
   Keep bus speed, theme, widget layout and logging identical.

No additional page-retention optimisation is included in this measurement pass.

## Safety refactor comparison — 2026-10-11

The safety refactor keeps control independent of LVGL and defers/coalesces
touch channel-settings writes until after the accepted gesture has painted and
output is safe. Mixer graphs are compiled on structural configuration changes,
rather than revalidated repeatedly on each 4 ms cycle.

An identical native SDL performance fixture was applied to checkpoint `4cc5872`
and the refactor. Seven rounds measured median page action → forced layout →
completed redraw, with the same state and cache settings:

| Orientation / page | Checkpoint median | Refactor median |
| --- | ---: | ---: |
| Landscape / Tune | 316.667 ms | 316.668 ms |
| Landscape / Data | 316.666 ms | 316.665 ms |
| Landscape / Setup | 316.666 ms | 316.669 ms |
| Landscape / Drive | 316.667 ms | 316.664 ms |
| Portrait / Tune | 449.960 ms | 449.976 ms |
| Portrait / Data | 466.654 ms | 466.670 ms |
| Portrait / Setup | 450.006 ms | 449.983 ms |
| Portrait / Drive | 450.001 ms | 449.990 ms |

These differences do not establish a measurable latency change. Native SDL
transfer/vsync dominates the fixture; this excludes physical touch scanning,
hold/release debounce and hardware SPI. Unchanged dashboard updates and repeated
identical menu draws still transfer zero pixels. Setup → Tune still renders
153,600 pixels and sends 85,440 landscape / 82,240 portrait pixels in this fixture.

A separate `-O2` native chained-AUX mixer benchmark (seven runs of 20,000 cycles)
measured 1,229 ns/cycle before and 990 ns/cycle with cached validation/order and
combined inspection, about 19% less time, with identical output checksums. This
is representative host CPU work, not an ESP32 deadline measurement.

The focused `--ui-perf-audit` also verifies immediate CUT for response edits,
preserved live trim, CUT for sub-trim, postponement of due writes while armed,
and a coalesced save after safe output/painting. Full audits retain interaction,
rotation, display equivalence and memory checks in both orientations. Hardware
touch-to-visible timing and safe bench validation of the new firmware remain
pending; this change has not been flashed.

## Ordinary-editor reuse and LCD trial — 2026-11-14

Ordinary value-editor +/-/Fine/Reset now update existing labels and curve points.
Their header, controls, focus targets and screen epoch remain alive through the
edit; true navigation still rebuilds and advances the epoch. Apply/Cancel, live
expo/endpoints, deferred saving and CUT/fault protections are unchanged. This is
editor reuse, not a cache of every page or shared chrome across different pages.

Before/after measurements use the same 320 actual touch actions across 80
field/role/orientation cases:

| Quantity | Before | Candidate |
| --- | ---: | ---: |
| Rendered pixels | 48,144,963 | 4,706,282 |
| New LVGL objects during adjustment | 7,764 | 0 |
| Aggregate host controller-call time | 200,826 µs | 27,002 µs |

Rendered work falls by 90.2% in this fixture. This is not a measured hardware
latency or frame-rate improvement. Allocation blocks/bytes remain stable through
each four-action sequence; ten unchanged Resets produce zero rendered pixels
and new objects. Sixteen before/after editor screenshots are byte-identical,
including with the new fallback font subsets. The preferred fonts are unchanged;
the [dated performance review](CODE-PERFORMANCE-REVIEW-2026-11-14.md) records exact
font and firmware size savings.

The approved XIAO display backend also offers an opt-in **Trial** pixel path.
It retains the verified commands, pins, mode 3 and 80 MHz clock. Standard is used
on every boot. For ideal aligned full-frame writes, outer SPI API calls change
from 4,800 to 20 landscape/30 portrait; the approximately 4,800 FIFO loads remain.
This is synchronous transfer, not DMA. Host checks cover exact RGB565 byte order,
odd counts, alignment, bounded scratch handling and guarded mode changes.

### Hardware comparison procedure

Use the authenticated web **Display comparison** panel. Changing the method or
resetting its counters requires an unlocked, awake handset in acknowledged CUT,
with holds released and no firmware update or ESC programming interlock.

1. Select **Standard**, allow its initial full redraw, then choose **Reset display
   counters**. Keep the row-cache setting and sensor state consistent.
2. Repeat a fixed navigation/editor/map sequence. Record pixels/bytes, API calls,
   pixel-transfer time, longest write and the control/radio/input gap counters.
3. Select **Trial**, allow its initial redraw and reset counters, then repeat the
   identical sequence. Return to Standard and repeat as a consistency check.
4. Compare like-for-like transfer totals. Check colours, partial/odd rectangles,
   wake/rotation and extended panel/control stability before considering a new
   default. Initial full-redraw results are recorded below; broader tests remain.

Pixel-transfer time excludes drawing and address-window commands; preemption can
be included. Task maximum gaps and dropped-event counts are since boot and are
not reset by **Reset display counters**. Row-cache fragmentation and alignment
can raise API-call counts above the ideal full-frame figures.

The same authenticated diagnostics use `GET /api/display-test`; guarded writes
use `POST` with `action=mode&mode=standard|trial` or `action=reset` through the
existing session/token mechanism. This avoids opening USB for routine comparison.
Host tests, both simulator builds, focused checks and full portrait/landscape
audits passed. Guarded upload installed `fff44ca` on the approved XIAO landscape
target, with device hash verification. Broader hardware qualification remains
pending; no Trial transport speed improvement is claimed.

### Initial hardware comparison — 14 November

Source `fff44ca` was tested through the authenticated web panel, staying CUT with
the module link off. Remote live-screen requests were intermittent, so the
navigation experiment was discarded. Instead, each method selection invalidated
the row cache and forced the same unchanged menu screen to redraw. Live viewing
was disconnected before these six alternating samples. Every sample transferred
exactly 153,600 pixels / 307,200 bytes in 20 pixel writes. Stats were read after
the redraw; no serial connection was opened.

| Method | Pixel-transfer times (ms) | Median (ms) | API calls per redraw | Longest writes (ms) |
| --- | --- | ---: | ---: | --- |
| Standard | 74.98, 72.97, 67.11 | 72.97 | 4,800 | 4.17, 4.24, 4.66 |
| Trial | 73.15, 75.27, 74.30 | 74.30 | 20 | 4.88, 4.42, 4.77 |

There is no clear transfer-time gain. These small samples include preemption,
exclude drawing/address commands and do not measure touch latency. The user
confirmed normal physical LCD colours/text without broken lines or flicker in
Trial. Standard was restored, not promoted or changed persistently. A final
Standard redraw measured 71.06 ms. Partial/odd rectangles, wake/rotation and long
stability still need hardware qualification before considering a new default.

New timing counters also exposed material gaps during web/live-screen activity.
Before using the live mirror, since-boot maxima were control 5.97 ms, radio
82.91 ms and physical inputs 25.61 ms. Standard/mirror activity raised control to
85.36 ms and inputs to 96.26 ms; by the first Trial selection, radio was 83.55 ms
and inputs 97.87 ms. No later maximum arose during the remaining warmed redraws.
All final inputs were healthy and dropped input/diagnostic counters stayed zero.
Those facts do not establish timely control: the sampler gap is close to its
100 ms heartbeat limit, and scan gaps are not identical to checked sample age.
The maxima are cumulative, so neither the cause nor a per-method attribution is
established. Isolate mirror allocation, network activity, persistence and warmed
rendering before qualifying live driving under this workload. Keep this as an
unresolved measured concern, not a diagnosed SPI regression or a passed safety
benchmark.

After this light test: internal free 84.3 KiB, minimum 60.1 KiB, largest 72.0 KiB;
PSRAM free 7,045.5 KiB, minimum 7,034.2 KiB. Minimum unused stacks were UI/web
1,432 B, control 2,420 B, radio 2,928 B, physical inputs 2,052 B and diagnostics
2,256 B. These are observations, not worst-case margin qualification. Evidence:
`/tmp/r1-medium-display-comparison-2026-11-14.png` and ignored
`tmp/medium-performance-2026-11-14/hardware-comparison.json`.

## Live web screen transfer

The browser now requests up to four contiguous changed rows at a time. A
landscape reply carries at most 3,840 bytes of pixels. If two nonadjacent rows
change, the viewer requests each in its own strip. A fixed revision per scan
lets the next scan collect changes that arrived behind the cursor. The browser
paints a new page only after it has received all its rows and keeps coordinates
and holds tied to the current page epoch.

Live screen GET responses use the socket's nonblocking send operation once per
response piece. A short write closes that response and the browser retries the
same cursor. This removes the WiFiClient write retry waits from screen GETs.
HTTP request parsing and other portal endpoints still run on the UI loop. After
25 seconds without an authenticated screen poll, the device frees the 300 KiB
capture buffer unless it is in use for a USB screenshot.

Host tests cover the strip size cap, distant dirty rows, changes during a scan,
lease timeout, page changes and retry after a truncated response. The original
timings above predate this change. The next handset test should measure the
effect on touch response and input, control and radio timing with live viewing,
including slow Wi-Fi, full page changes, reconnect and USB capture.

## Shared navigation and ERS draft reuse — 19 November 2026

Compatible pages retain their header and bottom navigation while replacing the
page body. Each transition still advances the mirrored screen epoch and registers
fresh focus targets. Changes in orientation, theme, model, header layout or
navigation selection rebuild the shared objects. Tune uses a full rebuild to
keep its chevron edges identical across display flush boundaries.

The Pi's native SDL simulator compared 49 transitions per orientation through
Display, TX Battery, Clock, Data and Channel Monitor. The two paths produced
identical framebuffer pixels at every step. Times are totals for that sequence
with forced full rebuild first and retained objects second.

| Orientation | Objects rebuilt | Pixels drawn | Construction (ms) | Layout (ms) | Flush (ms) |
| --- | ---: | ---: | ---: | ---: | ---: |
| Landscape | 2,674 → 1,953 | 7,526,400 → 7,119,651 | 459 → 361 | 450 → 361 | 8,747 → 8,666 |
| Portrait | 2,919 → 2,198 | 7,526,400 → 7,250,628 | 486 → 385 | 494 → 402 | 13,211 → 12,840 |

Changing a local ERS draft now updates the existing value, state and capacity
step labels. An equivalent forced redraw test across 26 GPS, RPM and capacity
actions created 1,099 objects and drew 3,993,600 pixels. In-place edits created
zero objects and drew 263,804 pixels, a 93.4% reduction. Both routes produced
identical frames. The in-place edits retain focus and epoch for the current page.

These measurements compare simulator rendering and SDL flushes. Handset touch
latency and SPI transfer times need separate measurements on hardware.

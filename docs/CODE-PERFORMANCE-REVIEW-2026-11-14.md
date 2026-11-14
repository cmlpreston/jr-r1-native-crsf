# R-1 EVO memory and performance review — 14 November 2026

Reviewed source: `c792652` on `codex/r1-v2`, including the unflashed ABS candidate.
Target: Seeed XIAO ESP32-S3, Waveshare LCD (F), landscape 480×320, schematic (5).

This is a review, not an optimisation implementation. No firmware, RF settings,
ESC settings or live hardware were changed. Source, pinned dependencies, target
ELF/map/DWARF, existing hardware observations and a fresh simulator performance
run were examined. The review covers control/RF, memory, rendering/maps, the
setup portal and current receiver programming paths. It is not a formal proof
of every safety property. Estimates below are explicitly distinguished from
measurements.

## Implementation update — editors, fonts and LCD transfer trial

The subsequent medium-priority candidate implements ordinary-editor reuse and
font subsetting, plus an opt-in LCD transport comparison. It has not yet been
hardware-qualified. Host tests, both simulator builds, focused audits and full
portrait/landscape UI audits passed. Guarded upload installed `fff44ca` using
the approved XIAO landscape environment, with device hash verification; record
`/tmp/r1-flash-zdb5wpgs`, log `/tmp/r1-medium-flash-2026-11-14.log`.

Ordinary value-editor +/-/Fine/Reset now retain the complete editor tree,
including header, focus targets and screen epoch. Only changed text and curve
points update. Actual navigation still advances the epoch and clears page-owned
pointers. Existing live expo/steering endpoint, Apply/Cancel, deferred persistence
and CUT/fault behaviour are retained. Broad cross-page chrome reuse is deferred.

| Same 320-action editor fixture, 80 field/role/orientation cases | Before | Candidate |
| --- | ---: | ---: |
| LVGL rendered pixels | 48,144,963 | 4,706,282 |
| LVGL objects created during adjustment | 7,764 | 0 |
| Host controller-call processing, aggregate | 200,826 µs | 27,002 µs |

This is **90.2% fewer rendered pixels** in that fixture, not a whole-UI speedup.
The host processing timings exclude the hardware LCD/input path. Used allocation
blocks/bytes remain stable over each four-action sequence; repeated unchanged
Reset produces zero pixels and objects. All 16 before/after editor screenshots
are byte-identical, including after font subsetting.

The six preferred DM Sans/Manrope fonts remain **95,116 B**. Fallback storage
falls from **209,046 to 25,513 B**: a font-only saving of **183,533 B (179.23 KiB,
87.8%)**, rather than the original 204 KiB gross upper bound. Required symbol
glyphs and a compatible default font are retained; glyph descriptors, bitmaps,
kerning, source coverage and rendered pages are checked.

| Approved-target build | Installed `488d94b` | Candidate |
| --- | ---: | ---: |
| Application flash | 2,390,145 B | 2,213,405 B |
| Static internal RAM | 203,932 B | 203,980 B |

Net firmware flash falls by **176,740 B (172.6 KiB)** after the added code and
diagnostics. Static RAM rises by **48 B**, including an 8 B font cache increase.
These are build measurements, not runtime heap/stack qualification.

The XIAO-only Trial transport uses the native RGB565 SPI pixel API, preserving
commands, pins, mode 3, reset handling and 80 MHz clock. Standard remains the
default at every boot; Trial is neither persisted nor selected automatically.
Ideal aligned full-frame outer API calls fall from 4,800 to 20 landscape or 30
portrait. The same approximately 4,800 hardware FIFO loads remain: this is not
DMA/asynchronous drawing or a wire-rate increase. Partial/unaligned/odd writes
have bounded handling and may require more API calls.

The authenticated web **Display comparison** panel exposes the trial and transfer/
task-gap counters. Changes require acknowledged CUT, an unlocked/awake handset,
released holds and no firmware update/programming interlock. Initial paired
hardware full-redraw samples show no clear benefit: Standard median72.97ms,
Trial74.30ms, identical pixels; physical appearance normal by user report.
Standard was restored. Newly observed web/live-screen task gaps remain unresolved
and broader panel/control qualification remains pending. See
[UI responsiveness](ui-responsiveness.md) for the evidence and procedure.

## Implementation update — first and high-priority work

The subsequent implementation addresses critical-task logging, independent
physical sampling/CUT, RF/history memory placement, graph invalidation and map
sampling. The review findings below describe the original `c792652` baseline.
Source `dd53728` was installed on user request through the guarded approved XIAO
landscape workflow. Build, host tests and full UI checks passed again; PlatformIO
upload succeeded with device hash verification and reset. Record
`/tmp/r1-flash-ym377dwe`, log `/tmp/r1-priority-fixes-flash-2026-11-14.log`.
Physical postboot behaviour and hardware qualification are still pending. ABS is
included and remains Off by default; medium-priority changes await bench results.

| Build measurement | Before | Installed |
| --- | ---: | ---: |
| Static internal RAM | 251,668 B (76.8%) | 203,932 B (62.2%) |
| `TransmitterUi` | 70,400 B | 17,936 B |
| `CrsfTxConfig` | 36,164 B | 848 B, plus lazy cold storage |
| Application flash | 2,381,897 B | 2,386,905 B |

Static RAM falls by **47,736 B**. This is not the net runtime heap gain: two new
worker stacks consume 4,096 B each, plus FreeRTOS task overhead. The RF cache and
history allocate in PSRAM first, with internal fallback; fallback reduces the
headroom saving and allocation failure has explicit safe UI behaviour. Sensor,
parameter and history limits are retained. The raster/projection caches add
approximately 1.2 KiB rather than another framebuffer.

Implementation details:

- Critical control/RF producers now enqueue fixed-size events without allocation,
  formatting, USB access or waits for queue space. New safe outputs publish
  before diagnostic bookkeeping. A low-priority worker formats/writes logs and
  reports dropped events. USB screenshot/machine replies serialize with that
  worker to preserve their framing.
- A dedicated input task owns expander sampling/debounce and direct CRSF binding
  updates. Touch and MCP transactions share a static recursive priority-inheriting
  I²C mutex. UI/model callbacks receive queued events. Queue overflow, missing
  input chips or an expired 100 ms sampler heartbeat force CUT. The heartbeat
  measures hardware health, not just task activity.
- Physical CUT is latched across UI configuration publication. Only a new,
  disarmed snapshot acknowledging its exact sequence releases the mailbox veto.
  Sampled safety/model epochs reject old queued events and prevent an edge from
  resolving against a newly installed model. Local/remote/keypad holds are
  invalidated on physical CUT; pending captured releases cannot turn into
  synthetic long/repeating presses while UI delivery is backlogged.
- UI-owned trim and menu callbacks still wait for the UI owner to finish current
  work. Independent sampling prevents missed edges and keeps direct channel
  bindings/CUT responsive, but does not promise a fixed latency for every edit.
  Bounded/asynchronous rendering and network work remain later improvements.
- RF parameter storage is lazy and pointer-stable. UI history is one cold bundle.
  Both use PSRAM first and internal fallback; history exhaustion preserves live
  readings/controls and reports unavailable statistics/graphs instead of
  dereferencing null. RF exhaustion reports a retryable discovery error.
- Graph setters compare independently generated points and counts before
  invalidation. Widget geometry resolves pending layout first, avoiding a second
  settling redraw. Paused cursors and related styles/labels also avoid no-op
  writes. The new fixtures require exactly zero rendered pixels over 20 unchanged
  updates in both orientations, while changed/range/stale cases still redraw.
- Raster maps cache source-X indices using the original double/floor formula;
  scanlines then do integer lookups. Trail projection uses generation/viewport
  keys and one cosine per changed projection. Pixel-equivalence tests include
  2,500 randomized clipping/pan/zoom cases. Five host runs measured median sampling
  time **9,196 → 1,073 µs** for 100 full frames (about 8.6×), identical output.
  This excludes LVGL and SPI and is not a handset frame-rate claim.
- Memory diagnostics now report LVGL's allocator `max_used` and the added worker
  stack high-water marks. `INPUTSTATS` reports sampler freshness/maximum gap,
  queued/dropped input events and dropped diagnostic events without changing
  output state.

Validation includes host tests, actual production-input-pipeline integration
with mocked hardware/task seams, deliberate temporary mutation failures for
three race/health regressions, concurrent I²C repeated-start tests, exhaustion
fixtures, full UI regression, both simulator builds and the approved landscape
hardware build. Target UI object sizes use Xtensa DWARF. A fresh performance
fixture still reports zero pixels for unchanged dashboard updates; its ~316.66 ms
SDL paint median remains vsync-limited and is not evidence of hardware latency.

Local logs use `/tmp/r1-priority-*2026-11-14.log`; reproducible evidence is copied
under ignored `tmp/priority-performance-2026-11-14/` after verification. Before
normal driving, bench-check real scan gaps, task stack margins, disconnected/
slow USB, shared I²C traffic, model changes and PWM/CUT behaviour on the handset.
No clock, pin map, control/RF rate, receiver firmware or ESC settings changed.

## Main conclusions

1. Remove blocking diagnostics from time-critical tasks before tuning speed.
2. Stop unchanged graph redraws and replace repeated raster-map coordinate maths.
3. Recover about **52 KiB of internal RAM** by relocating UI-only RF settings and
   history to PSRAM; this changes placement, not total bytes allocated.
4. Improve page/editor lifetime and benchmark the SPI transport on the handset.
5. Use measured allocation peaks before shrinking LVGL or task stacks.

The most promising visual improvements are rendering less and reducing transfer
work. Increasing refresh frequency or SPI clock is not the first step.

## Baseline and limits

Latest local approved-target build:

| Quantity | Measured value | Meaning |
| --- | ---: | --- |
| Static internal `.data + .bss` | 251,668 B / 327,680 B (76.8%) | Excludes task/driver/runtime heap allocations |
| Firmware flash use | 2,381,897 B / 3,342,336 B (71.3%) | Build's application-image allocation |
| LVGL allocator reservation | 98,304 B | Already included in static RAM |
| `TransmitterUi` instance | 70,400 B | Already included in static RAM |
| `CrsfTxConfig` within UI | 36,164 B | Subset of UI, not additional to it |
| UI histories/statistics bundle | 17,346 B | Subset of UI |
| DMA draw buffer | 15,360 B | Keep in internal DMA-capable memory |

Target types/symbols were measured using the pinned Xtensa GDB and `nm`, not
host ABI sizes. The linker reports `.data=27,372` and `.bss=224,296`. Do not add
LVGL or UI bytes to the static total again; do not interpret GNU size's aggregate
flash/dummy mapping sections as runtime RAM. DMA heap is a subset of internal
heap, not an independent pool.

Fresh native SDL performance run:

| Fixture | Result |
| --- | ---: |
| 20 unchanged dashboard updates | 0 rendered pixels |
| Setup → Tune | 153,600 rendered; 77,280 sent after row cache |
| Same transition construction | 2,069 µs on this host |
| Tune/Data/Setup/Drive action-to-paint medians | Approximately 316.66 ms each |

The near-identical paint timings are dominated by SDL/vsync and do **not** measure
R1 touch latency or SPI performance. Zero-pixel dashboard coverage does not imply
unchanged graph pages are efficient. Older physical measurements in
[ui-responsiveness.md](ui-responsiveness.md) are useful historical observations,
not current 80 MHz/ABS-build benchmarks.

Local evidence is retained under ignored `tmp/code-review-2026-11-14/`: performance
log, target layout reports and fingerprints of the ELF/map/binary. Reproduce the
performance fixture with a disposable preference directory:

```sh
JR_R1_SIM_STATE="$(mktemp -d /tmp/r1-review.XXXXXX)" \
SDL_VIDEODRIVER=dummy simulator/build/jr-r1-sim --ui-perf-audit
```

## Findings and recommended changes

### 1. High priority: logging can delay safety publication and RF deadlines

Evidence: `src/main.cpp:544–568` logs a safety transition before publishing its
new output snapshot. RF-task diagnostics also execute directly at lines 306,
310 and 647. The pinned ESP32 HWCDC writer can wait on a TX mutex (default
100 ms timeout) and a connected host/ring buffer. Formatted messages of this
length also allocate through `Print::printf`.

This is a code-supported blocking risk, **not a reproduced 100 ms stall on the
handset**. Disconnected USB takes a different FIFO/drop path. Nevertheless, a
slow or contended logger must not precede publication of newly safe outputs.
This is particularly relevant to faults first detected inside the control task.
UI-requested CUT already has an immediate mailbox veto, which must be retained.

Publish safety outputs first, and replace critical-task formatting/writes with a
fixed-size nonblocking event queue. Drain and format outside control/RF tasks;
record dropped events rather than waiting for space. Moving the print after
publication alone improves ordering but still leaves later control cycles exposed.

Validate connected-host backpressure, TX-lock contention, event overflow and
4 ms deadline/PWM behaviour. Preserve the existing final safety mailbox and
stale-output arbitration; do not make a diagnostic write count as fresh control.

### 2. High priority: physical buttons/trims still share the blocking UI loop

Evidence: MCP polling, input processing, keypad dispatch and binding events are
in `src/main.cpp:1101–1169`; UI processing/flush follows at line 1222. The separate
250 Hz ADC/control task does not make these digital inputs independent. A long
page render or synchronous web response can delay the next physical-input poll;
brief presses may be delayed or missed.

First bound display/network work per UI iteration. If polling still misses its
budget, introduce one input owner and a bounded event queue. This needs an
explicit serialized I²C policy because expanders, touch and reset handling share
Wire. Do not simply create concurrent I²C users.

Measure maximum input-scan gap, press-to-recognition latency and queue loss while
changing pages, rendering maps, using Wi-Fi/live view and saving settings. Preserve
pre-render CUT and existing debounce/held-switch/fault semantics.

### 3. High return: move the cold RF parameter cache out of internal RAM

Evidence: `include/crsf_tx_config.h:19–21,54`, UI ownership at
`include/transmitter_ui.h:741`. Sixty-four parameter slots each reserve 552 B,
including 384-byte options, 48-byte name and 112-byte information strings. The
array alone is 35,328 B; the complete client is 36,164 B.

Allocate this UI-owned cache once in PSRAM, or lazily on discovery with a stable
lifetime. Preserve parameter IDs, the current supported limit, readback and
no-retry-on-mutation behaviour. Handle allocation failure with a usable error;
do not allocate/free on each refresh or keep dangling parameter pointers.

Expected benefit: roughly **35 KiB internal RAM headroom**, without reducing
features. It is not a total-memory saving or a proven speed improvement.

### 4. High return: update graph geometry only when it changes

Evidence: `src/transmitter_ui_design.inc:1229,1595,1671` calls
`lv_line_set_points` at the 80 ms UI cadence, while history advances once per
second at line 1131. Paused cursor updates do likewise in
`src/transmitter_ui_v2.inc:85`. LVGL 8.4's line setter unconditionally refreshes
size/invalidation even when passed the same points pointer.

Use history/viewport/cursor generations or point-array comparisons before the
setter. Include range changes, stale-validity changes, pause/unpause and resize
in the invalidation key. This can remove roughly 11–12 redundant geometry
updates per second per history-only graph; it is not a percentage improvement
for the whole UI. The row cache saves some SPI bytes but cannot undo LVGL CPU
work already performed.

Extend the zero-redraw fixtures to static/paused graphs and cursor positions,
then confirm real new samples and scale changes still redraw correctly.

### 5. High return: make raster-map sampling an affine integer/index operation

Evidence: `src/gps_basemap.cpp:35–57` computes longitude and performs repeated
double arithmetic/division/floor for every dirty image pixel. For a fixed
viewport, the mapping is affine and source X indices are independent of Y.

Precompute source X indices and per-row Y indices, or use carefully rounded
fixed-point stepping. The inner loop then becomes bounds checks and pixel
indexing. A full-width X mapping can be computed 480 times rather than once
per pixel on every row. Prefer this small working set over another full-frame
cache initially.

Also cache unchanged trail projection (`src/transmitter_ui_design.inc:83–99`):
up to 48 points are projected before equality is checked, with repeated cosine
through `include/gps_map_projection.h:9`. Precompute longitude scale and key
projection by trail generation, viewport, map revision and geometry.

Require pixel-equivalence cases for map edges, outside-map coordinates, negative
rounding, pan/zoom, clipping, both orientations and no-data/stale positions.
Measure map draw time independently from LCD transfer.

### 6. Medium/high return: update editors in place and reuse page chrome

Evidence: `src/transmitter_ui_design.inc:364` calls `startScreen`, which deletes
the whole LVGL tree at `src/transmitter_ui_lvgl.cpp:481`. Ordinary value-editor
+/− redraws the page at `src/transmitter_ui_design.inc:1047`. Steering endpoints
already provide an in-place example.

Update existing values/curves for ordinary drafts first. Then retain a shared
header/navigation shell while replacing content. Reusable styles can reduce
repeated local-style allocations. Avoid keeping every page alive: that would
trade speed for unnecessary RAM.

Preserve focus registration, touch bounds, page epochs, remote hold cancellation,
draft Apply/Cancel semantics and pointer-reset lifetimes. Measure construction,
layout, LVGL rendering and transfer separately.

### 7. Medium/high potential: benchmark a narrower bulk SPI pixel path

Approved XIAO uses `Arduino_HWSPI` (`src/transmitter_ui_lvgl.cpp:183`). Its pinned
GFX Library 1.5.6 `src/databus/Arduino_HWSPI.cpp:345–379` swaps/copies RGB565 in
32-pixel chunks, each followed by `SPIClass::writeBytes`. A full frame produces
approximately **4,800 outer chunk calls** despite only twenty 16-row LVGL flushes.

The pinned Arduino ESP32 `SPIClass::writePixels` accepts **byte length**, performs
RGB565 swapping into FIFO words and may remove outer copies/API overhead. It
still uses 64-byte FIFO transfers and busy-waits: this is **not DMA/asynchronous
rendering**, and does not remove those FIFO transfers.

A narrow A/B implementation is preferable to replacing the verified display
backend. Preserve mode 3, grounded CS and reset settling
(`src/transmitter_ui_lvgl.cpp:194–224`). Do not increase clock: the active target
is already 80 MHz (`include/display_profile.h:98–101`). Its ideal RGB565 wire-only
full-frame floor is 30.72 ms; commands, copying and drawing add overhead.

Test colour byte order, odd counts, partial rectangles, wake, rotation, long-run
panel stability and task/input timing on the actual hardware. No speedup is
claimed until that comparison exists.

### 8. Medium return: relocate UI histories; measure LVGL before shrinking it

UI statistics/history occupy 17,346 B (`include/transmitter_ui.h:673–678`):
13,536 B statistics, 2,160 B graph readings, 540 B validity flags, 960 B channel
history and 150 B paused snapshot. A PSRAM bundle could recover about **17 KiB
internal RAM**. Preserve all-time versus rolling statistics, duplicate/stale
handling and reset semantics. Do not narrow 64-bit sums without overflow bounds.

The larger 96 KiB LVGL pool is another candidate, but shrinking it now would be
poorly supported. `src/memory_diagnostics.cpp:19,27` samples usage every two
seconds and synthesizes a sampled peak, missing short-lived construction peaks.
LVGL already exposes its true allocator `max_used`; report that first. A future
96→64 KiB reduction would recover 32 KiB **only if** fragmentation and worst-case
page construction/portal workflows establish adequate margin.

Keep the 15,360 B draw buffer internal. Do not shrink 4,096/6,144 B RF/control
task stacks from static estimates; collect their true FreeRTOS high-water marks.

### 9. Medium return while using Wi-Fi: reduce live-screen retention and blocking

`include/remote_screen.h:20–24` turns changed rows into one full-width span from
first changed row to last. Two small changes far apart can therefore send almost
a full 307,200-byte frame. `src/setup_portal.cpp:365–368` sends that synchronously
on the UI loop; server processing runs at lines 758, 762 and 775.

Use bounded/incremental network work and evaluate dirty rectangles, row runs or
tiles. Preserve a consistent revision/epoch snapshot and remote input freshness
and arming rules; an asynchronous network task must not access mutable LVGL state.

A live-view buffer is allocated lazily but browser Disconnect/pagehide only stops
polling (`include/setup_portal_page.h:330,439`), not server capture. It is freed
when the web service stops (`src/setup_portal.cpp:722–723`), which may be much
later with home Wi-Fi enabled. A view lease/explicit release could stop redundant
memcmp/copy work and reclaim 300 KiB PSRAM while the server remains available.
Coordinate multiple viewers and USB screenshots before releasing shared storage.

The legacy vector-map loader also allocates a 48,000-byte JSON document on the
general heap (`src/setup_portal.cpp:320`); PSRAM allocation or bounded streaming
would reduce transient internal pressure. Current image packages take a different,
smaller metadata path, so this is not a claim that every map upload needs 48 KiB.

### 10. Medium/low return: reduce unnecessary control-loop copying/evaluation

Target sizes: configuration snapshot 952 B; telemetry 416 B. The control task
copies both every 4 ms (`src/main.cpp:514,528`) although it only needs link-valid
and LQ from telemetry. Use compact link state and revision-aware configuration
reads. The UI also builds/compares a complete snapshot every loop at lines 455–462;
a dirty-generation approach could avoid that work. Preserve atomicity, resets
and configuration-revision mailbox ordering.

`compileConfiguredMixing` appends all 16 channels (`src/control_configuration.cpp:383`),
then inspection and processing each evaluate that order. Prune unused nodes while
retaining role channels, bound AUX outputs, configured mixes and transitive
sources. Inspection uses fresh non-live runtime while processing advances live
runtime: simply merging passes could break neutral qualification/rate transitions.
A two-role model could reduce 32 evaluations to four in that stage; this is **not**
an 87.5% overall CPU speedup.

Subtrim adjustment geometry (`src/control_configuration.cpp:158`) recalculates
centre and endpoints repeatedly. Cache static terms by revision or share per-cycle
terms, accounting for live endpoint and switched-rate changes. These are sensible
cleanups, but not established ESP32 bottlenecks compared with display work.

### 11. Medium flash saving: replace complete fallback fonts with symbol subsets

The custom six-font set occupies 95,116 B of flash. Its six full Montserrat
fallbacks retain a further **209,046 B**. See `include/lv_conf.h:17–26` and the
`.fallback` references in `src/fonts/r1_*.inc`.

Generate symbol-only fallbacks for the required sizes. The 204 KiB is a gross
upper bound before retaining required symbols, not a guaranteed net saving.
Audit every LV_SYMBOL/non-ASCII character and screenshot all pages/themes.
This preserves the preferred DM Sans/Manrope appearance. It saves flash, not
that amount of internal RAM, and does not establish faster drawing.

## Lower-priority opportunities and boundaries

- The model recolour buffer reserves 115,200 B PSRAM on first use
  (`src/model_image.cpp:23–31`); generic assets need 51,840 B. Allocate exact
  source size or release it once no object references the generated image.
- `OfflineMap::Package` reserves 4,376 B including vector points even for image
  maps (`src/offline_map.cpp:7`). Tagged storage/PSRAM relocation is possible.
- Timer histories (4,640 B) and focus/widget registries are smaller targets.
  Discarding other models' lap histories would change features.
- Several raw label/style setters still bypass guarded helpers:
  `src/transmitter_ui_lvgl.cpp:4408`, `src/transmitter_ui_design.inc:1572,1625,1654`.
  Move invariant style setup to construction and guard changing values. LVGL's
  position/bar-range setters already compare values; avoid wrapping everything.
- Row-cache fragmentation can cause multiple small SPI transactions
  (`src/transmitter_ui_lvgl.cpp:330–356`). Benchmark merging short unchanged gaps,
  using existing UICACHE ON/OFF and flush-overhead/transfer-count measurements.
  Do not remove a bandwidth-saving PSRAM cache based on memory totals alone.
- The 400 kbaud 26-byte CRSF transmission occupies about 650 µs, roughly 16% of
  one core at 250 Hz, with busy waiting for prompt bus release. A TX-done ISR is
  a higher-risk later project: delayed turnaround previously lost Nano replies.
- Quarter-EMA filtering adds approximately 10 ms half-response / 32 ms 90%
  settling at 250 Hz. Per-axis tuning could improve feel if ADC noise allows;
  retain the current default and review fault semantics separately. Filtered
  rail validity is not proof of disconnected-wire detection.
- Current ER6G Drive mode bypasses ESC UART polling. Optimising the 800 baud
  programming loop will not speed driving. Deferred full-app startup-peer hash
  verification and a smaller receiver diagnostic profile are separate later
  receiver projects; retain exact-image checks before programming boot selection.
- ER4/standalone receiver source is not linked into the transmitter. Deleting
  those experiments will not reduce transmitter RAM or flash.

## Suggested execution order and acceptance criteria

1. Nonblocking critical-task diagnostics; publish safety before diagnostic work.
2. Report true LVGL high-water and add graph/map/input-gap/deadline instrumentation.
3. Relocate the RF parameter cache, with allocation-failure and lifetime tests.
4. Guard unchanged graphs/styles and generalise in-place editors.
5. Replace raster-map inner-loop maths and cache trail projection.
6. A/B the SPI pixel path and row-run merging on the handset.
7. Relocate UI history; prune fallback fonts after a complete glyph audit.
8. Reassess UI/input ownership, snapshot/mixer costs and conditional LVGL sizing.

For each change record before/after internal minimum heap and largest block,
true LVGL peak/fragmentation, stack margins, rendered/sent pixels, transfer count,
construction/layout/render/flush times, touch/keypad latency, maximum physical
scan gap and control/RF deadline distribution. Test map/live web/settings/OTA
stress cases as well as normal Drive. Keep comparable data, theme and layout.

Run existing host tests and both-orientation interaction/layout checks for the
changed path. Performance acceptance must include unchanged channel output,
CUT/failsafe/neutral/re-arm, stale input, model isolation and allocation-failure
behaviour. Host timing is useful for CPU comparisons but cannot qualify physical
SPI or RF/PWM timing. None of these proposals requires weakening upload guards,
raising RF/control rate, changing the verified display pin map or abandoning the
working paired ESC startup design.

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

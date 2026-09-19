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
XIAO, then profile graph/map geometry updates and full-page transitions. Hardware
feel and stability have not yet been validated for this change; the firmware
has not been flashed as part of this first software pass.

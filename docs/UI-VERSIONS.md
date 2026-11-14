# R1 interface versions

## V1 — preserved 3 November 2026

- Source: `codex/r1-v1`, tag `r1-v1-2026-11-03`, commit `76a7e6b`.
- Contains the latest previously installed native UI, including battery/RF
  warnings, graph axes, live trim feedback and the Model/Handset reorganization.
- Exact last verified upload artifacts and manifest are retained locally in
  ignored `tmp/releases/r1-v1-2026-11-03/` (from `/tmp/r1-flash-8huchyfh`).
- Browser Design 05 was a separate prototype at that point, not V1 firmware.

## V2 — native trial development

Branch: `codex/r1-v2`. Native widget dashboard at 480×320, DM Sans / Manrope bitmap
fonts and sage/teal light palette. Model name in the top bar is informational.
No gearing calculator.

Drive uses the saved per-model widget layout, with no separate Overview or fixed
Normal/Walk row. Optional Controls, Trim / rates and RF margin widgets provide
mode selection, guarded quick editors and known-mode RF margin/LQ/power.
Controls reflects the same mode as the hardware switch and keeps the existing
PIN-protected Walk exit. Holding Drive retains the output enable/CUT gesture
and startup checks. Run review is accessible from Data → Sensors and Quick.

Data retains Sensors / GPS / Channels / Inputs. Sensor graphs can be paused
and inspected with the time cursor while live telemetry and statistics keep
collecting. Setup shortcuts lead to actual battery/RF/ERS commissioning pages;
ERS changes still require the native Apply/readback flow. All sensor statistics
can be reviewed together and explicitly reset without resetting the sensor's
consumed charge, timer or GPS trail.

Quick is per-model, in a separate V2 preference namespace. Edit → tap a slot
cycles its destination; `^` moves it earlier. Select Empty to remove a slot.
Key labels identify the physical keys that open each shortcut. MODE still opens
Quick and holding MODE returns to Drive. Models remain accessible through a
Quick slot (select Models while editing); the top bar no longer opens them.

V1 dashboard layouts, uploaded GPS map data and control settings retain their
existing formats. Widgets shows the original per-model layout and its map
widgets, including maximize. The browser
trial's illustrative control diagram is not yet a native control-map replacement.
The established native assignment and live mix editors remain available.

## Returning to V1

When requested, switch to `codex/r1-v1` and use the same guarded upload workflow
for `seeed_xiao_esp32s3-pcb` landscape. The source baseline and exact previous
artifacts are preserved. There is no automatic downgrade or in-screen firmware
switch. A newer `origin/main` still requires the normal ancestry/diff review.

V2 adds its own Quick preferences and does not migrate V1 control/layout data.
Any user changes to the shared model settings remain shared on returning to V1.

# R1 EVO — Design 05

Separate interactive browser proposal, 3 November 2026. This is not LVGL or
firmware, and never connects to the transmitter, module or sensors. Design 04
in `../trial/` and the production simulator are preserved.

Preview from the repository root:

```sh
python3 -m http.server 8771 --bind 127.0.0.1 --directory simulator/web
```

Open http://127.0.0.1:8771/trial-next/ . The standard web build includes this
folder at `/trial-next/` for a future explicitly requested deployment.

## Review flows

- Drive: four telemetry tiles, optional trim/rate strip, run-review shortcut.
  Pack/cell and RF warnings highlight affected tiles. Lost telemetry suppresses
  current values while preserving historical records.
- Sensor: larger graph with labelled Y axis, pause and scrub cursor, independent
  current reading, max/average scope, explicit sensor-statistics reset. Shortcuts
  lead to battery warnings, pole count, RF warnings and capacity drafts.
- Quick: pin/unpin with the star, reorder and remove entries with Edit order.
  Eight entries maximum; model-specific browser persistence.
- Tune: wheel and throttle bench inputs move a live marker and numeric output.
  Response view plots CH1 versus wheel; mix view plots CH2 versus wheel at the
  current throttle input. Mixing is a simplified illustrative model, not a
  replacement for the firmware mixer or its safety pipeline.
- Trim: physical bench buttons produce a short overlay and a neutral beep.
  Reverse is reflected in the response graph. The bar depicts a neutral offset;
  endpoint editing and the full native trim/subtrim pipeline are not implemented.
- Control map: Grip A/B identification, local assignment drafts and a duplicate
  steering-trim indication. Diagram positions are illustrative; buttons here
  identify the control rather than operate its assigned function.
- Run review: independent session extrema, used charge and worst RF margin;
  explicit reset confirmation. Statistics reset does not reset consumed charge
  at the simulated sensor.
- Setup: model battery/alarms/assignments versus shared handset RF hierarchy.
  Gearing calculator starts with 64 km/h, 16T → 17T and supports live changes.

## Scope and verification

Telemetry and alarm scenarios are fabricated, including RPM, pole-count defaults
and RF data. Threshold warnings are illustrative, without native debounce or
audio scheduling. Pole count and ERS capacity saves are local drafts only.
Throttle trim/subtrim and RF-module configuration are explanatory placeholders;
this is a navigation/design trial rather than a complete transmitter emulator.

Browser settings use `r1-design05-models`, separate from Design 04. Statistics
are held in memory until reset/reload/model change. Graph inspection does not
stop collection. Each per-sensor reset preserves other sensor histories.

Verified in the browser: sensor → settings → Apply → graph; model-specific
thresholds; graph freeze/cursor; live mixed output saturation; trim screen fit;
Quick pin/reorder/remove; identify/assign/duplicate feedback; 16T → 18T estimate
of 72 km/h. Screen contents use native 480×320 coordinates; the enlarged preview
scales them for browser review. Browser fonts approximate a possible native UI.

# Native handset captures

Captured from the real XIAO ESP32-S3 on 1 November 2026, firmware `7d13dbf`.
All PNGs are unmodified 480×320 LVGL flush-pixel exports over USB. They use
the same pixel buffer as the web mirror and do not read the physical LCD.
Outputs remained CUT and the module link paused; no telemetry was injected.

Temporary themes, model images and three-layer paint were selected in the
same model memory, so HYPER 7 remains the model name in the artwork examples.
The original Graphite theme, Buggy artwork, original paint and timer widget
were restored. See [the gallery](../../docs/ui-gallery.md) and
[capture manifest](manifest.json) for details and checksums.

PNG files contain only IHDR/IDAT/IEND chunks, with no location metadata.
Wi-Fi credentials, private network names and installed map imagery are absent.
The older simulator and web-fixture captures remain in `assets/ui-captures`.

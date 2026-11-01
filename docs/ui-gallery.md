# R-1 EVO interface gallery

The native screenshots below were captured on **1 November 2026** from the
**real XIAO ESP32-S3**, running firmware **`7d13dbf`**, in landscape 480×320.
The USB screenshot command exports the actual LVGL flush pixels also used by
the handset's web mirror. These are neither simulator frames nor physical LCD
readback: cable/panel faults, viewing angle and reflections cannot appear in them.

The handset remained **CUT**, with the module link paused. Telemetry is the
handset's real current state, including no-data readings; no sensor values were
injected. Temporary artwork, paint and theme examples use the same HYPER 7
model memory, so its name remains visible even when F1 or another image is
selected. Original artwork, paint, theme and timer widget were restored afterward.

## Main screens

| Drive dashboard | Tune |
| --- | --- |
| ![Drive](../assets/handset-captures/drive.png) | ![Tune](../assets/handset-captures/tune.png) |
| Data / Sensors | Setup / Model |
| ![Data sensors](../assets/handset-captures/data-sensors.png) | ![Model setup](../assets/handset-captures/setup-model.png) |
| Setup / Handset | Display settings |
| ![Handset setup](../assets/handset-captures/setup-handset.png) | ![Display settings](../assets/handset-captures/display-settings.png) |

## Configuration on the handset

| Control assignments | Channel setup |
| --- | --- |
| ![Control map](../assets/handset-captures/control-map.png) | ![Channel setup](../assets/handset-captures/channel-setup.png) |
| Mixing settings | Mixing response graph |
| ![Mix settings](../assets/handset-captures/mix-settings.png) | ![Mix curve](../assets/handset-captures/mix-curve.png) |
| Dashboard positions | Widget settings |
| ![Dashboard layout](../assets/handset-captures/dashboard-layout.png) | ![Widget configuration](../assets/handset-captures/widget-settings.png) |
| Drive modes | CUT / output status |
| ![Normal Walk modes](../assets/handset-captures/drive-modes.png) | ![CUT output status](../assets/handset-captures/cut-status.png) |
| TX battery settings | Live hardware inputs |
| ![TX battery settings](../assets/handset-captures/tx-battery-settings.png) | ![Hardware inputs](../assets/handset-captures/hardware-inputs.png) |
| PIN entry | USB PIN recovery |
| ![PIN keypad](../assets/handset-captures/pin-entry.png) | ![PIN recovery](../assets/handset-captures/pin-recovery.png) |

Authenticating never enables outputs by itself. The mixing screenshots show
configuration/preview, not a live servo test; existing channel settings were
not changed for the captures.

## Colour schemes

| Light | Graphite / teal |
| --- | --- |
| ![Light](../assets/handset-captures/theme-light.png) | ![Graphite teal](../assets/handset-captures/theme-graphite.png) |
| Midnight / ice blue | Warm charcoal / ivory |
| ![Midnight](../assets/handset-captures/theme-midnight.png) | ![Warm charcoal](../assets/handset-captures/theme-warm.png) |
| Slate / soft lilac | Scheme selector |
| ![Slate lilac](../assets/handset-captures/theme-slate.png) | ![Scheme selector](../assets/handset-captures/colour-schemes.png) |

## Vehicle artwork and paint

| Hyper 7 | Buggy |
| --- | --- |
| ![Hyper 7](../assets/handset-captures/model-hyper7.png) | ![Buggy](../assets/handset-captures/model-buggy.png) |
| Truggy | Short-course truck |
| ![Truggy](../assets/handset-captures/model-truggy.png) | ![Short course](../assets/handset-captures/model-short-course.png) |
| Monster truck | Crawler |
| ![Monster truck](../assets/handset-captures/model-monster-truck.png) | ![Crawler](../assets/handset-captures/model-crawler.png) |
| Touring car | Formula / F1 |
| ![Touring car](../assets/handset-captures/model-touring.png) | ![F1](../assets/handset-captures/model-f1.png) |
| Primary, accent and base paint | Recoloured buggy |
| ![Paint settings](../assets/handset-captures/model-paint-settings.png) | ![Custom paint](../assets/handset-captures/model-custom-paint.png) |

These examples use a temporary Model image widget in the original timer's
position. Artwork also changes the small title-bar icon. Tyres/mechanical parts
retain their shading; primary, accent and base paint affect the painted regions.

## GPS illustration from the simulator

![Fictional GPS map and trail](../assets/ui-captures/gps-map.png)

This particular image remains a **simulator example**, from checkpoint
`8dda44e`, with a fictional circuit and simulated GPS data. It is deliberately
separate from the real-device screenshots above.

## Complete simulator web page

![Full simulator browser interface](../assets/ui-captures/simulator-browser.jpg)

This is the local simulator browser bridge, including the firmware screen,
eight-key navigation controls, extra Home utility, steering/throttle sliders
and simulated telemetry controls. It is separate from the XIAO's web server.

## Embedded web UI

The following captures use the **actual embedded browser frontend with a local
documentation fixture backend**, not a connected transmitter. The example
network/address, model data and resource figures are illustrative. The map
selector fetched real Esri imagery of Silverstone, a public circuit; the save
confirmation is explicitly a fixture result. No firmware was flashed, no model
was altered and no map was uploaded to physical hardware for these captures.

### Model configuration

![Web UI model configuration](../assets/ui-captures/web-model.png)

### Original handset controls

![Web UI hardware-control assignments](../assets/ui-captures/web-controls.png)

The web catalogue includes the original keypad controls. In the current
firmware, reserved keypad navigation takes precedence over editable bindings;
MODE-to-channel remapping needs a further change, as described in the README.

### Select, prepare and upload a satellite map

Choose Street or Satellite, browse/search a location and position the square
selection. **Download selected area** prepares the package in the browser.
Review the handset image and provider credit, then **Save map to handset**.
The two steps are distinct: downloading a preview is not a completed upload.
The screenshot below includes the example save confirmation.

[![Satellite selection, package preview and example upload result](../assets/ui-captures/web-map-upload.png)](../assets/ui-captures/web-map-upload.png)

Satellite imagery: **Esri, Vantor, Earthstar Geographics, and the GIS User Community**.
The README's [map acknowledgements](../README.md#map-sources-and-acknowledgements)
also cover OpenStreetMap contributors, Photon/komoot and Overpass.

[Open the full web configuration page capture](../assets/ui-captures/web-portal-full.jpg)
for the complete page, including channel parameters, mixing, maps, memory,
PIN and hostname settings.

## Capture and privacy notes

The native handset PNGs are unchanged 480×320 RGB exports, with only PNG
IHDR/IDAT/IEND chunks: no EXIF, XMP, GPS, device identity or capture-location
metadata. Their [capture manifest](../assets/handset-captures/manifest.json)
records source, dimensions and checksums. No Wi-Fi password or private network
name is visible in the published native screens.

The older simulator/browser-fixture assets retain their own provenance labels.
Their map geography is a public circuit or a clearly fictional track. The
hardware captures do not publish the handset's installed map image.

## Temporary widget focus (simulator)

These examples use the shared UI at firmware `06b6928`, captured on
1 November 2026 with isolated simulator settings and simulated inputs. They
show the saved grid and the expanded All channels, GPS and Timer widgets.
The small top-centre down-chevron restores the unchanged grid. The map has no
live GPS position; the images are not evidence of a GPS fix or physical LCD
appearance.

| Saved dashboard | All channels expanded |
| --- | --- |
| ![Simulator dashboard](../assets/ui-captures/widget-focus/dashboard.png) | ![Simulator All channels expanded](../assets/ui-captures/widget-focus/all-channels-expanded.png) |

| GPS map expanded | Timer expanded |
| --- | --- |
| ![Simulator GPS expanded](../assets/ui-captures/widget-focus/gps-expanded.png) | ![Simulator Timer expanded](../assets/ui-captures/widget-focus/timer-expanded.png) |

# Shareable R1 simulator

Public demo: https://jr-r1-demo.vercel.app/

Interactive design trial: https://jr-r1-demo.vercel.app/trial/
Published with user approval on 2026-09-15; Design 04 includes auxiliary channel
configuration, simulated telemetry/GPS and model artwork. The main demo now runs the LVGL widget dashboard; the S3 setup portal
remains a roadmap item.
Local preview: `python3 -m http.server 8769 --bind 127.0.0.1 --directory simulator/web/trial`

The `/trial/` page is a separate HTML/CSS/JavaScript interaction prototype of the
proposed interface, rather than the compiled firmware UI. It offers day/night
palettes, native 480x320 and 320x480 layouts, Apply/Cancel numeric editors, live
response previews, physical-adjustment feedback, control learning, per-model
preferences and a simulated radio status. Design 03 adds independent brake
rate/expo/speed, centred trim indicators and direct Monitor navigation.
`pixel-preview.js` renders the DOM into a native-resolution canvas (no
devicePixelRatio scaling); LCD pixels and 1:1 size are on by default. Enlarging
the canvas preserves pixels. Browser font metrics and antialiasing still differ
from LVGL, and physical display size/contrast are not reproduced. It keeps its settings under the
separate `jr-r1-interface-trial-v2` browser key. The full build script includes
it; edits to the trial alone require copying the five files from `web/trial/`
to `web/dist/trial/` before deploying, without recompiling WebAssembly.

Design 04 adds Setup → Channels (CH3–16 auxiliary assignment, switch/proportional
behavior, endpoints, reverse and failsafe), and Data → Sensors / GPS / Channels.
The sensor dashboard and history are generated locally; the map is an authored
fictional circuit with synthetic positions, trail, zoom and home distance. No
geolocation, remote map tiles or live telemetry are accessed. Missing GPS and
stale/missing sensor streams can be tested from the bench controls. The existing
Hyper 7 artwork is embedded in `channels-data.js` for the model thumbnail.

The C++/LVGL simulator is compiled to WebAssembly. Vercel serves static files;
no Mac process, Python server, radio hardware or shared settings database is
needed. Each visitor's preferences are stored in their own browser's local
storage. Portrait is the default; both orientations expose the existing functions.
Radio replies and telemetry are simulated, and sound is not emulated.

## Build

Install and activate Emscripten SDK **4.0.15**, then from the repository root:

```sh
make -C simulator bootstrap
sh simulator/build_web.sh
python3 -m http.server 8768 --bind 127.0.0.1 --directory simulator/web/dist
```

Open http://127.0.0.1:8768/. Both orientations are included. The build uses the
current working files, including uncommitted UI changes. Rebuild after editing
the firmware interface. For a completely fresh build, remove only the generated
`simulator/build-web` and `simulator/build-web-portrait` directories first.

## Publish an update

After building and testing, deploy **only** the generated output directory:

```sh
npx vercel@59.17.0 deploy --cwd simulator/web/dist --scope cmlprestons-projects --prod --yes
```

The initial deployment linked that directory to `jr-r1-demo`. If its ignored
`.vercel` settings are missing, first run `vercel link` in the output directory
and select the existing project. Publishing is manual; GitHub automatic
deployment is not configured. Do not deploy the repository root or native demo
state directories.

`web_platform.cpp` provides the canvas and input bridge, while `main.cpp` uses
the browser animation loop. Native SDL builds remain available. The MD5 helper
is blueimp-md5 2.19.0 (MIT), used solely for the existing ELRS UID calculation;
its licence and the LVGL licence are included in the public build.

Verified: both WebAssembly builds, both native builds, all twelve host test
suites, touchscreen navigation, keypad navigation, saved response settings
after reload, and portrait switching.

## LVGL widget dashboard

Use **Drive → Layout** to select an anchor position and edit its type, size,
style and sources. Apply saves the layout for the current model and replaces
panels covered by the new size. Cancel leaves it unchanged. Use suggested layout restores a full-width timer with power and channel panels.
Timer, model image,
paired TX/RX voltage, one/two channels, individual sensor and GPS trail panels
are available. Sensor and channel graphs retain a short live history.

Enable **Simulate electric vehicle sensors + GPS** to exercise Data and map
widgets. These values and the route near fictional coordinates 0,0 are generated
only in the simulator. With the box off, hardware-style unavailable states are
shown. Real firmware needs incoming telemetry from the receiver/vehicle bridge.
There is no offline basemap or logo-upload service yet.

The default orientation is portrait 320×480. The detail menus are rebuilt for
this shape, including calibration, binding and TX module settings. Fonts and graphics use
LVGL's LCD pixel grid; an enlarged browser view cannot reproduce panel contrast,
physical touch feel or sunlight readability. Hold the on-screen CUT button for
0.8 seconds to request arming; tap it to cut immediately.

Run the isolated native integration audit with `make -C simulator audit`.
See [the firmware port audit](../../docs/ui-firmware-port.md) for retained menus,
telemetry assumptions and hardware validation still required.

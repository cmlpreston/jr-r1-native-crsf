# R1 interface design direction

Design exploration, 2026-09-15. This is a proposal, not an implemented firmware
interface. The user requested an overall UI/UX review; sunlight readability is
one consideration, not the sole objective.

User review: the initial light concept felt like a beginner iOS app. It has not
been accepted as a redesign direction. Compare against the real current UI in
the local browser simulator before proposing another visual treatment.

## References reviewed

- [DJI RC Plus 2 interface guide](https://www.youtube.com/watch?v=lCIimugncqs):
  sampled the preflight screen at 0:28, checklist at 7:45 and operating view at
  8:40–8:46, and read the chapter list. This was not a full viewing of the video.
- [DJI RC Plus design rationale](https://enterprise-insights.dji.com/blog/dji-rc-plus-top-5-features):
  physical shortcuts, visible status and outdoor display hardware.
- Existing R1 portrait dashboard and menu simulator captures in `assets/`.

## Proposed structure

1. **Drive:** selected model, explicit output state, mode, a small set of useful
   readings and direct access to steering/throttle quick adjustments. A timer
   can be the primary reading until vehicle speed telemetry is available.
2. **Tune:** Steering, Throttle and auxiliary channels. Group rate, expo,
   endpoints, subtrim and speed by the thing the user is adjusting. Advanced
   channel routing remains available beneath these role-based controls.
3. **Setup:** models, control assignments, radio link/TX/RX configuration and
   handset/display/calibration. Separate per-model settings from global settings.

Telemetry details and channel diagnostics are available by tapping the relevant
readout; the default driving screen need not permanently show raw channel bars.
Keep a stable place for Home/Back and selected-model identity. Physical MODE,
PAGE, SELECT, CLEAR and +/- must invoke the same actions as touch controls.

## Interaction and visual language

- Persistent output state and reason: CUT, ARMED, link loss or input fault.
  Keep drive mode separate from output eligibility. A lock icon alone is
  ambiguous because it can also mean touchscreen lock.
- Show editable values as controls with consistent affordances. A tap opens a
  focused editor with a large value, coarse/fine steps and an appropriate graph.
- Changes have clear Save/Cancel and visible applied/pending feedback. On real
  hardware, use the existing CUT policy for configuration, neutral re-arm and
  verified module readback. Do not imply that a setting was applied prematurely.
- Preserve physical arming and allow an authenticated, timed remote Drive hold
  as authorised on 2026-10-02. A readiness screen explains blockers; it must not
  falsely certify receiver/ESC failsafe behaviour from transmitter link status.
- Use restrained neutral surfaces, one accent for focus/selection, and warning
  colours for actual conditions. Pair status colour with text. Increase primary
  type size; reduce outlines, repeated labels and decorative icon area.
- Use a few generous touch targets on each screen, and a visible keypad focus.
  Avoid relying on swipes, long-presses or icon-only actions for essential tasks.
- Design natively for 320x480 portrait and separately reflow for 480x320
  landscape. Do not shrink DJI's seven-inch interface to fit the R1 display.
- Day/night palettes should preserve the same layout. Assess readability on the
  actual Waveshare screen, outdoors and at operating distance; a browser preview
  does not validate sunlight readability, touch ergonomics or LCD performance.
- The initial concept uses a light daytime palette: very light grey background,
  white control surfaces and near-black text, with restrained blue selection.
  This follows the user's interest in DJI's predominantly white settings UI.

## First implementation slice, if adopted

Shared chrome and navigation, then Drive, a single Steering rate/expo editor,
and the readiness/status detail. Validate touch plus keypad navigation, saved
settings behaviour, stale/missing telemetry, longest labels and output safety in
the simulator before extending to all pages or testing on hardware.

The conversation prototype uses illustrative telemetry and timer values. Its
readiness indicators are demonstration states, not hardware test results.

## TX16S MK3 MAX / EdgeTX reference review

Reviewed 2026-09-15: RadioMaster's official MK3 MAX product imagery and EdgeTX's
current online manual, including its input editor and model-manager screenshots.
This was a documentation and image review, not hands-on radio testing. Manual
screenshots illustrate EdgeTX patterns; the installed version and theme can
change their appearance. The MK3 has an 800x480 display, versus R1's 480x320
landscape display, so its screen density should not be copied directly.

Useful adaptations, proposed only:

- A shared numeric editor: tap an entire parameter row to edit a large value,
  with coarse/fine adjustment and reset. Keep R1's existing live response graph.
  This would replace the crowded small +/- targets on its response page.
- A short, non-blocking value readout after a physical adjustment, for example
  `Steering rate 85%` or `Steering trim +3`. Reserve stronger alerts for output
  and link conditions. EdgeTX's global-variable change popup is a precedent.
- Explicit control learning in Control Map: select Assign, actuate a control,
  show the detected control and proposed binding, then confirm. Learning must
  be a deliberate configuration mode rather than run during ordinary driving.
- Two useful dashboard presets, Drive and Diagnostics, using a stable model/
  output/link header. EdgeTX supports configurable widgets; a full widget
  designer would add unnecessary scope for R1 at this stage.
- Make model-specific versus handset-wide settings clear in the page heading
  and navigation. Preserve touch/keypad parity and the current focus highlight.
- Model duplication and a few surface-model templates could reduce repetitive
  setup later. Active-model identification must remain unambiguous.

Visual lesson: aligned rows, a stable section header, clear editable fields and
restrained accent colour can make the current UI more coherent. A light-grey
settings surface is worth comparing with a refined dark palette, but the earlier
light concept remains unaccepted. Use colour for selection and meaningful state;
avoid decorative dashboard imagery, excessive icon-only tabs, and shrinking a
large general-purpose aircraft interface onto the R1 screen.

The subsequent interactive design trial is in `simulator/web/trial/`, prepared
for `/trial/` on the Vercel app. Public deployment is awaiting explicit approval.
It uses a fixed dark status strip, aligned settings rows, restrained teal focus,
and day/night surfaces. Primary flows work in both orientations: Drive/Monitor,
Steering/Throttle tuning with preview and Apply/Cancel, control learning,
model selection, model/handset setup, and simulated RF settings. This remains
a proposal; the firmware implementation has not adopted these screens.
Verification covered changing and cancelling/applying expo, assigning Grip B+
to steering rate, using the assignment, on-screen adjustment feedback,
persistence after reload, and both palette/orientation combinations.

Sources:

- https://radiomasterrc.com/products/tx16s-mk3-max-radio-controller-elrs-m2
- https://manual.edgetx.org/color-radios/user-interface
- https://manual.edgetx.org/color-radios/user-interface/virtual-keyboards
- https://manual.edgetx.org/color-radios/model-settings/inputs-mixes-and-outputs/inputs
- https://manual.edgetx.org/color-radios/model-settings/global-variables
- https://manual.edgetx.org/color-radios/screen-settings
- https://manual.edgetx.org/color-radios/select-model

## Spektrum iXSR+ / RaceWare reference review

Reviewed 2026-09-15 against the SPM6425 product page and official English
manual. Visually inspected dashboard, setup, channel monitor, steering curve
and custom-curve screens (manual pages 29-31 and 52-53); also read guided setup
and telemetry panel configuration (pages 39 and 117). This is a documentation
review, not a hands-on evaluation. No trial or firmware behavior was changed.

The dashboard pairs a prominent timer with centred trim bars, steering and
braking/reverse rates. Setup groups channel functions separately from other
settings. The channel monitor aligns channel numbers, bars and values. Custom
curves select a point before adjusting its input and output. Guided vehicle
setup ends with a receiver connection map. Telemetry panels can be configured.

Proposed R1 adaptations:

- Make the Drive screen more useful during a run: steering rate and brake
  rate as the two quick adjustments, with expo in Tune. Add compact centred
  trim indicators and retain the model, mode, link and output-state header.
  Brake rate is a proposal requiring an explicit independent brake parameter;
  do not relabel the existing throttle expo or rate as brake rate.
- Give Monitor direct access and show AUX channels as well as steering and
  throttle, retaining the distinction between input and transmitted output.
- Offer Race and Trail dashboard presets with a small set of selectable
  telemetry readings; avoid a full gauge designer initially. Missing or stale
  readings need an explicit unavailable state.
- Later, add a short vehicle setup wizard ending with a receiver channel map.
  Keep supported receiver capabilities separate from UI assumptions.
- For future custom curves, use point selection followed by numeric editing,
  with touch and keypad parity, rather than requiring precise point dragging.

Visual judgment: the iXSR's charcoal, white and orange palette gives stronger
instrument character than generic app cards. Adapt its hierarchy and contrast,
but retain R1's flat rows and restrained accent. Its gradients, angular button
decoration and dense scrolling pages would consume scarce R1 display space.
Prioritize an alternative Drive screen before expanding setup functionality.

Sources:

- https://www.spektrumrc.com/product/spektrum-ixsr-12-channel-dsmr-transmitter-with-sr515-receiver/SPM6425.html
- https://www.spektrumrc.com/on/demandware.static/-/Sites-horizon-master/default/dw09a9a66f/Manuals/SPM6425_MANUAL_EN.pdf

## Design 03: shared visual language

The user requested a blend of useful patterns rather than a copy of one radio.
The local trial now combines the aligned editors and explicit assignments from
the earlier study with an iXSR-inspired driving hierarchy. The rules are:

- Stable model/link/output header and Drive, Tune, Monitor, Setup navigation.
- Flat surfaces, restrained borders, consistent row spacing and tabular values.
- Teal denotes selection and editing. Amber/red denotes output/link attention;
  the explicit branding exception is the JR wordmark, which is always red.
- Whole parameter rows are touch targets. All numeric changes use the same
  preview, coarse/fine steps, Reset, Cancel and Apply flow.
- Drive emphasizes the timer, steering and brake rates, and centred trim bars.
  Brake response is independent of forward throttle and has its own Tune tab.
- Monitor shows input and transmitted output separately. It explicitly labels
  its two simulated channels rather than implying a complete hardware map.

The browser trial defaults to 1:1 CSS size and a native 480x320 or 320x480 pixel
canvas. Enlarging it preserves pixels instead of rerendering sharp vector text.
DOM controls retain touch, keyboard and accessibility behavior under the canvas.
This is still a layout study: browser font rendering differs from LVGL and the
preview does not reproduce the panel's physical size, contrast or refresh rate.

Verified independent brake response (80% brake gives -80% at full brake while
forward throttle stays +100%), Apply/Cancel, link loss returning output to
neutral, both orientations, native canvas dimensions and no clipped buttons on
the inspected tuning screens. Browser console was clean. The native simulator
build passed after applying red JR text to firmware identity/header labels.
The layout changes remain confined to the trial; no device was flashed and no
public deployment was performed.

## Design 04: auxiliary channels, telemetry and model identity

The local trial now exposes CH3–16 under Setup → Channels, in four-channel
pages. Each auxiliary slot has a function preset, explicit control learning,
enable state, toggle / three-position / proportional behavior, direction,
independent low/high endpoints and failsafe. Numeric output settings reuse the
shared Apply/Cancel editor. Button reassignment identifies conflicts before
confirmation. Analog controls may feed multiple outputs. Endpoints preserve
neutral even when their magnitudes differ; failsafe applies on CUT/link loss
in this prototype. These are UI proposals, not updates to the hardware map.

Data replaces Monitor in the primary navigation, with Sensors, GPS and Channels
as secondary tabs. Sensors include speed, motor temperature/RPM, receiver
voltage, link quality and satellites. Tap a reading for recent history and
minimum/maximum values. Bench controls simulate stale/missing data and lost GPS
fix. All telemetry is generated locally and marked simulated.

GPS uses an authored fictional test circuit, a synthetic vehicle position,
heading and trail, home distance, zoom/fit controls and a scale bar. Landscape
places controls beside the map; portrait places them below. No real location,
geolocation request, online tile provider or sensor connection is involved.

The existing Hyper 7 buggy artwork is restored in the persistent model header
and model picker. The crawler uses a distinct schematic vehicle icon. JR stays
red and the native-resolution preview remains enabled by default.

Verified button and proportional assignments, saved channel settings after
reload, high-endpoint limiting, failsafe on link loss, pagination through CH16,
sensor history, stale/no-data and no-GPS states, map zoom, and both orientations.
An isolated logic check covered asymmetric endpoints, neutral, reverse,
failsafe and disabled output. JavaScript syntax and diff checks passed; browser
console had no errors. No firmware or public deployment changed in this step.

## Proposed dashboard widgets and firmware migration

The Drive run-timer area should become a per-model widget slot with fixed
templates: timer, one sensor, a compact sensor pair, or a mini GPS map. Choose
the widget and source through Setup; tap the displayed widget for its full
page. Keep model identity, link and CUT status outside this configurable area.
Changing the visible widget must not stop the timer. Units, freshness and
missing-data states should be consistent with the Data pages. Start with one
slot rather than a free-placement dashboard at this screen size.

The standalone HTML trial is a design specification, not firmware UI code.
Recreate its shared components and pages in the existing LVGL 8.4 C++ UI,
using the established configuration and control interfaces. First port the
header, navigation, Drive layout and timer/sensor widget; then port Tune,
channel editors and Data views. Map rendering and offline asset storage are
separate later work. Do not transfer the trial's synthetic telemetry or its
simplified control/safety behavior into production logic.

Validate each slice in the existing native and WebAssembly simulators, which
compile the firmware UI source, before bench validation on the LCD. This
provides actual LVGL fonts and layout in the browser. Confirm the intended
DevKitC/Waveshare or XIAO PCB environment and pin/display profile before any
upload; check touch targets, legibility, redraw latency, memory use and control
task timing on hardware. Preserve saved model settings through explicit schema
migration when adding widget choices.

## Implemented portrait direction

Portrait 320×480 is now the default for the LVGL UI and standalone demo. The
approved mockup informs the dark slate header, readable values, Model/Handset
Setup tabs, flat separator rows, active navigation underline and permanent JR R1
branding. The Drive layout remains a configurable 2×2 grid supporting 1×1,
2×1, 1×2 and 2×2 panels; the earlier single-slot proposal above is superseded.
Detailed functionality is inventoried in [the port audit](ui-firmware-port.md).
Existing control/persistence handlers are reused behind portrait-specific
layouts. No firmware upload or physical display verification is implied.

### Mixed channel/switch dashboard monitoring

All Channels → Source selects Channels, Hardware switches, or Channels + switches.
Channel entries show the actual output, including CUT behaviour. A single binary
CRSF binding (momentary/toggle/two-position/pulse) is shown as a read-only switch;
proportional inputs, trim controls, stepped or three-position assignments and
mixed channels retain numerical/analogue presentation. Intermediate values are
marked MID instead of being misrepresented as OFF. Endpoint/reverse/trim settings
are considered when matching the two binary positions.

Hardware entries show Drive mode and Idle up contact states independently of
output/CUT. Unmapped/faulted contacts show --. DISPLAY_SW is excluded. Tapping a
widget still pages the monitor; the drawn switch indicators cannot operate a
channel or physical switch. Layout storage v4 migrates existing All Channels
widgets to channels-only before enabling the new source options.

## Card spacing

Separate cards and boxed setting rows use `kPanelGap` (6 native LCD pixels)
in both orientations. Dashboard, sensor grid, RF diagnostics, assignment/input
rows, mixing rows, colour schemes and firmware cards share that gutter.
Joined segmented tabs and navigation controls deliberately have no gutter.
RF diagnostics uses the dashboard’s 10-pixel outer margin and derives its
column widths and stacked positions from the shared spacing constants.


### Manual orientation trial (2026-10-02)

Display settings page 2 includes an Orientation / Rotate control. It switches
between 480×320 landscape and 320×480 portrait at runtime, including LCD
rotation, GT911 touch mapping, LVGL layouts and the remote-screen dimensions.
The framebuffer and maximum-width 16-row draw buffer are reused. Rotation is
deferred until touch release and refused during an arm request, calibration
or remote enable hold. Firmware logs free heap, largest DMA block and PSRAM
after each switch. The trial is not saved: reboot restores the environment's
default orientation. No accelerometer is required for this test.

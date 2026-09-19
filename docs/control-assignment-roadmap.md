# Programmable control system roadmap

This document is the source of truth for the R1's programmable physical-input
system. It preserves the feature ideas discussed during the proof of concept and
tracks implementation progress. A physical input, its logical function and a
CRSF output channel are deliberately separate concepts.

## Architecture

```text
GPIO / ADC / FPC contact
          |
          v
debounced physical input with stable ID
          |
          v
event: value, press, release, long/double press, trim -/+
          |
          v
one or more context-sensitive bindings
     |          |          |          |
     v          v          v          v
TX function  parameter  drive mode  CRSF channel / macro
          \       |       /
           safety and conflict resolver
                    |
                    v
             16-channel CRSF frame
```

The control task must remain independent of display rendering, touch handling
and telemetry decoding. Throttle CUT and stale-input protection always have
priority over bindings and macros.

- [x] Proportional control sampling/processing runs independently of TFT/LVGL
  rendering; page changes cannot create a synthetic neutral output.

## Proposed default R1 profile

| Input | Short/default action | Additional configurable actions |
|---|---|---|
| Idle Up switch | CUT / armed request | Throttle hold or profile enable |
| Drive Mode switch | Walk / Normal | Profile A/B; optional two-position CRSF output |
| Display switch | Display and backlight off/on | Day/night mode |
| Timer button | **Next dashboard page** | Long: Quick panel; timer/lap/log marker; momentary/toggle/pulse channel |
| Grip Trim A | Throttle rate/power limit +/- | Walk limit; stepped channel |
| Grip Trim B | Steering rate +/- | Gyro gain; stepped channel |
| Throttle trim | Coast/drag-brake adjustment | Throttle trim or brake setting |
| Steering trim | Steering trim | Steering subtrim |
| Aux 3 trim | Disabled until assigned | Stepped, selector or spring-return CRSF channel |
| MODE key | Main menu | Dashboard shortcut |
| PAGE key | Page/tab | Previous/next dashboard page |
| SELECT key | Confirm | Quick panel |
| CLEAR key | Back/cancel | Long: safe CUT; alarm acknowledgement |
| UP/DOWN/+/- keys | Menu navigation/editing | Dashboard-only shortcuts and safe long-press actions |

Timer operation is intentionally not privileged. The conveniently placed Timer
button is a general input and defaults to dashboard page change because that is
likely to be useful while driving.

## Supported binding targets

- Transmitter/UI function: menu, dashboard, page, Quick panel, display,
  touchscreen lock, alarm acknowledgement, resets, timers and logging.
- Model parameter: trims, rates, walk limit, expo, braking, ABS and brightness.
- Drive state/profile: CUT/arm, Walk/Normal and profile A/B.
- CRSF channel 1..16.
- Macro: an ordered or simultaneous set of the targets above, subject to safety
  validation.

## Supported input behaviours

- Proportional axis.
- Maintained two-position switch.
- Momentary while held.
- Toggle on each press.
- Timed pulse.
- Spring-return three-position (`- / centre / +`).
- Retained stepped value with configurable range, step and repeat rate.
- Retained three-position selector.

Each binding records context, range, initial value, failsafe value, persistence,
inversion and whether it is safety-critical. One physical input may have several
bindings only when the combination is made explicit in the UI.

## UI plan

### Input Monitor

- [x] Stable IDs and descriptors for all nineteen assignable R1 controls.
- [x] Initial paged Input Monitor screen and mapped/unmapped state.
- [x] Hardware-independent GPIO/MCP23017/ADC endpoint descriptors.
- [x] Debounced button/switch processing with short, long, double and repeat events.
- [x] Spring-return trim processing with hold-repeat and impossible-state detection.
- [x] Raw/debounced state store suitable for the monitor and diagnostics UI.
- [x] Show raw GPIO/ADC state and debounced logical state live.
- [x] Show press, hold, double-press and trim repeat events.
- [x] Add wiring/GPIO identity and fault state.
- [x] Add MCP23017 polling input driver and prove PA0 through the resolver to CRSF CH4.
- [x] Add production MCP23017 mappings, polling and state-store updates.
- [x] Add direct-GPIO sampling drivers and feed the state store.

### Bindings

- [x] Data model for input events, contexts, targets and behaviours.
- [x] Binding range/step/initial/failsafe/persistence fields and validator.
- [x] Host-testable binding resolver with context filtering and conflict checks.
- [x] Versioned NVS binding store for the current Model 1 profile.
- [x] Binding list grouped by physical input.
- [x] Add/edit/delete/enable binding screens for CRSF channel targets.
- [x] Let spring-return trims target either an independent CRSF channel or a
  retained per-channel trim/subtrim parameter from the same editor.
- [x] Add a channel-centric trim/subtrim control selector, with ownership
  conflict checks shared with the input-centric editor.
- [x] Persist all sixteen channels' trim/subtrim values independently of the
  physical-control binding profile.
- [x] Move the versioned binding store into each model profile. Ten model
  memories now isolate bindings, roles, response profiles, trim/subtrim,
  receiver-battery settings and Walk limit while preserving the legacy Model 1
  namespaces. Stored-schema migrations beyond the current versions remain
  future work.
- [x] Detect duplicate/conflicting CRSF channel writers.
- [ ] Provide restore-default-profile command.

### Outputs

- [x] Replace the single-throttle CRSF builder with a 16-channel output model.
- [x] Resolve local functions and parameters into a typed action queue.
- [x] Add momentary, toggle, pulse and switch output resolvers.
- [x] Add trim jog, stepped and three-position output resolvers.
- [x] Dispatch per-channel trim/subtrim actions to the live model pipeline.
- [ ] Dispatch the remaining UI/model/drive-mode local actions. The first
  usable slice is complete: menu, dashboard, Quick panel, display on/off,
  night mode, arm/CUT and Walk/Normal can now be selected on the TFT and are
  dispatched at runtime.
- [x] Feed sampled physical-input events into the resolver on hardware.
- [ ] Show live CRSF value and receiver-output mapping notes.

### Contexts and shortcuts

- [x] Runtime Dashboard, menu, CUT and armed context selection.
- [x] Make binding contexts editable on the TFT. The assignment editor exposes
  the unambiguous single-context presets Always, Dashboard, Menu, Cut only and
  Armed only. Context is stored with the existing per-model binding data and
  enforced by the resolver; compound OR masks remain supported internally but
  are intentionally not presented until their semantics can be labelled well.
- [x] Keypad navigation consumes keypad events while a menu is open. Page
  controls now register one semantic callback plus focus, touch, decrement and
  increment hitboxes; touch and the keypad therefore invoke the same action
  instead of duplicating page-specific coordinate switches. Channels,
  Assignments (including their bank controls), Drive Modes, Setup, throttle
  endpoints/sub-trim, rate/expo/speed, Channel Roles, TX/RX Battery, Display
  Setup and the complete assignment editor use this registry.
- [x] Extend registered controls to the Dashboard, channel configuration,
  throttle header actions, Input Monitor paging, ELRS action buttons and the
  complete on-screen keyboard. Telemetry and Calibration register their
  standard Back/Home controls; Main Menu retains its complete dedicated focus
  implementation.
- [ ] Add simulator navigation regression cases.

### UI control contract

New interactive pages must call `beginPageFocus()` and `finishPageFocus()`.
The standard form automatically appends keypad-focusable Back and Home controls.
Use `makePageActionButton()` for buttons so drawing, touch activation, keypad
focus and Select activation are declared together. Use `addPageEditorTarget()`
for values with decrement/increment regions. Both routes dispatch the same
semantic callback for touch, Select, `+` and `-`; do not add page-specific
coordinate handling to the keypad dispatcher. Custom headers may call
`beginPageFocus(false)` but must explicitly register every header action.

During development, attempting keypad navigation on a page without registered
controls emits `UI: page ... has no registered keypad controls` on Serial and
sounds the short error beep.
- [ ] Dashboard short/long/double-press shortcuts.
- [x] Timer button defaults to next dashboard page for a new/erased binding
  profile; existing user-edited bindings are preserved.
- [ ] Touchscreen lock and display on/off actions. Display on/off is complete;
  touchscreen lock remains.

### Safety

- [ ] Dedicated, exclusive arm/CUT binding.
- [x] The final CRSF-task safety arbiter makes CUT and stale control select the
  complete safe model after every binding, modifier and channel processor.
- [x] Binding edits force CUT and neutral re-arm.
- [x] Startup switch-position warning and arm interlock for maintained switches,
  spring-centred trims and unknown/faulted MCP23017 input states. The TFT names
  each displaced control and does not allow the warning page to be dismissed.
  Retained output-state warnings remain part of future model/profile work.
- [x] Model change only while CUT and throttle-neutral. Loading a model resets
  the output path, reloads roles and bindings, and requires neutral re-arm.
- [ ] Explicit initial and failsafe values for retained accessory channels.
- [ ] Critical functions cannot share an unsafe secondary binding.

### Driving-control implementation order

Implement these in order so steering and throttle share one deterministic,
testable signal path rather than accumulating channel-specific exceptions:

1. [x] Introduce a per-proportional-axis profile. Persistent per-channel
   reverse, sub-trim, endpoints, rate, expo and output ramp are implemented and
   legacy CH2 settings migrate to the assigned throttle channel. Calibration
   and deadband belong to generic physical-input profiles; throttle and steering
   use the same conditioning runtime. The interactive calibration controller
   still needs extraction from the UI.
2. [x] Connect steering to the shared proportional conditioning/channel pipeline
   and expose independent calibration/response settings, plus a persisted
   steering-disable option for bench tests.
   - [ ] Validate the actual steering pot and simultaneous throttle/steering.
   - [ ] Add separate steering turn/return speed controls.
3. [ ] Add optional editable response curves. Begin with a five-point curve,
   monotonic validation and a live raw-versus-output graph. Forward and brake
   throttle halves remain independently configurable.
   The per-channel response editor now has the live bipolar graph foundation:
   linear reference, configured response and live input/output marker. It also
   edits and plots role-labelled endpoint rails plus a translated sub-trim
   neutral line on a +/-120% output scale. The remaining work in this item is
   the optional five-point custom-curve editor and its monotonic validation.
4. [ ] Add assignable throttle hold. It must command the configured neutral,
   override drive demand, indicate HOLD prominently and require neutral before
   release can resume drive.
5. [ ] Add optional ABS on the physical brake side only, with configurable
   activation point, depth and frequency. CUT, hold, stale-input and failsafe
   handling always take priority over ABS modulation.
6. [ ] Allow a physical dial or digital-trim binding to adjust the Walk-mode
   forward limit while preserving full configured brake authority.

Every stage must preserve the 250 Hz CRSF control task, avoid LVGL work in the
control path, save settings per model, and include live monitor values before
the next stage starts. Defer 4WS and gyro-gain controls until there is a
concrete vehicle use case. Race-start countdown and user-defined buzzer macros
remain later enhancements.

### Later configuration and interchange

- [ ] Versioned profile export/import.
- [ ] Wi-Fi web editor presenting an input-to-action/channel matrix.
- [ ] Optional BLE companion interface.
- [ ] Multi-action macros, conditions and mixes.

## Display blanking

The current Waveshare 3.5inch RPi LCD (F) profile has an exposed PWM backlight.
Controller Display OFF plus backlight PWM has been verified, including guarded
GT911 touch-to-wake. The original TFT35-SPI notes below are retained for the
fallback display profile.

- [x] Waveshare controller sleep, PWM backlight-off and fresh-touch wake.
- [ ] Measure and record Waveshare awake/sleep current.

### Original TFT35-SPI fallback

The current firmware sends the ILI9486 `Display OFF` (`0x28`) command and wakes
with `Display ON` (`0x29`). This blanks pixel drive but does not switch the
TFT35-SPI backlight. `Sleep IN` was intentionally removed because this specific
panel showed a solid white image when both commands were combined.

Reports from Linux/KlipperScreen users confirm that DPMS can turn the image
black while the module backlight remains powered. Therefore software controller
sleep is useful for display life and a dark UI, but it is not the final power
saving solution for the measured 5 V / 0.25 A panel load.

- [x] Controller sleep/wake commands with touch-to-wake.
- [ ] Verify sleep/wake repeatedly on the actual panel and record current draw.
- [ ] Locate a backlight-enable node or add a transistor/MOSFET backlight stage.
- [ ] If backlight access is impractical, switch the TFT 5 V rail and perform a
  complete reinitialisation when the maintained Display switch is turned on.
- [ ] Keep the ESP32 and CRSF control path alive while the TFT is powered down.

Reference: <https://github.com/bigtreetech/TFT35-SPI/issues/2>

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
- [ ] Move the versioned binding store into each future model profile and add
  migrations when the stored schema changes.
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
- [ ] Make binding contexts editable on the TFT.
- [ ] Keypad navigation consumes keypad events while a menu is open.
- [ ] Dashboard short/long/double-press shortcuts.
- [ ] Timer button defaults to next dashboard page.
- [ ] Touchscreen lock and display on/off actions. Display on/off is complete;
  touchscreen lock remains.

### Safety

- [ ] Dedicated, exclusive arm/CUT binding.
- [ ] CUT overrides every mix, macro and output.
- [x] Binding edits force CUT and neutral re-arm.
- [ ] Startup switch-position and retained-state warnings.
- [ ] Model change only while CUT and throttle-neutral.
- [ ] Explicit initial and failsafe values for retained accessory channels.
- [ ] Critical functions cannot share an unsafe secondary binding.

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

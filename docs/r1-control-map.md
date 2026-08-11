# JR R1 control inventory and assignment plan

Source: JR Racing R-1 manual, especially Control Identification (manual page 4),
The Key Pad (page 7), and Function Select (page 15).

## Driving controls

| Manual no. | Physical control | Electrical/interaction type | Original JR role | Recommended initial native-CRSF role |
|---:|---|---|---|---|
| 18 | Steering wheel | 5 kΩ analogue potentiometer, spring centred | Proportional channel 1 steering | Fixed default: CH1 steering; calibratable and optionally remappable |
| 19 | Throttle/brake lever | 5 kΩ analogue potentiometer, spring centred | Proportional channel 2 throttle/brake | Fixed default: CH2 throttle/brake with neutral safety interlock |
| 8 | Timer button | Momentary pushbutton | Timer or throttle hold; could be inhibited | General shortcut; next dashboard page by default, with timer/channel actions optionally assigned |
| 9 | Drive Mode switch | Two-position maintained switch | Selects drive-mode-dependent settings; assignable as Drive Mode or Idle Up | Local drive profile by default; optionally two-position channel |
| 10 | Grip Trimmer A | Two-way momentary digital trim | Assignable digital trim | Power/throttle rate adjustment by default; optionally stepped channel |
| 11 | Grip Trimmer B | Two-way momentary digital trim | Assignable digital trim; factory steering dual-rate control | Steering dual rate by default; optionally stepped channel |
| 12 | Throttle digital trimmer | Two-way momentary digital trim | Assignable; commonly throttle/coast-brake trim | Throttle trim or drag-brake setting; never bypass neutral safety |
| 13 | Steering digital trimmer | Two-way momentary digital trim | Assignable; commonly steering trim | Steering trim by default |
| 14 | Auxiliary 3 Channel (CH3) | Two-way momentary digital trim/step control | CH3 digital trim or switched-step output | CH3 stepped proportional output by default; may instead adjust a local parameter |
| 15 | Idle Up switch | Two-position maintained switch | Idle-up selection; assignable as Idle Up or Drive Mode | Throttle cut/arm or auxiliary mode, with startup-position warning; optionally CH5 |

The manual explicitly treats controls 10-14 as **five digital trims**, controls 9
and 15 as **two assignable switches**, and control 8 as the **thumb-operated
assignable button**.

### Functions offered by the original Function Select menu

| Control class | Original assignable functions |
|---|---|
| Digital trims | Throttle trim (coast brake), steering trim, throttle brake (panic brake), steering rate, ABS swing, ABS point, Aux 3 channel, inhibit |
| Maintained switches | Drive mode, idle up, inhibit |
| Thumb button | Timer, throttle hold, inhibit |

The original firmware allowed a function to be assigned to only one physical
control. Our implementation need not retain that restriction: one input can
drive a local parameter and/or an output channel if the UI makes the combination
explicit and safe.

## Programming keypad

The keypad contains eight separate keys. The underlined manual labels were
Direct Mode shortcuts rather than additional physical inputs.

| Physical key | Original normal/menu action | Original Direct Mode shortcut | Recommended new role |
|---|---|---|---|
| MODE LIST | Enter/advance through function lists | None | Home/menu or enter menu |
| UP | Previous item/increase navigation | Steering exponential | Up; long press may repeat |
| `+` | Increase/edit value | Model Select | Increase/right |
| SEL | Select field/channel | Throttle curve | Select/OK |
| PAGE | Change page/field | Travel adjust | Page/back-tab |
| CLR | Clear/reset selected value | None | Back; hold for reset-to-default confirmation |
| DOWN | Next item/decrease navigation | Trim position | Down; long press may repeat |
| `-` / CUSTOM | Decrease/edit value | User-selected shortcut; timer at factory default | Decrease/left; configurable long-press shortcut |

The touchscreen can duplicate these functions, but retaining the keypad gives
reliable eyes-off operation and avoids making safety-critical actions dependent
on touch input.

## System, mechanical and output items

| Manual no. | Item | Classification | Proposed treatment |
|---:|---|---|---|
| 1 | Antenna | RF hardware | Replaced by Ranger Nano antenna arrangement |
| 2 | RF transmission indicator | Indicator/output | Reuse or replace with ELRS link/RF status LED |
| 3 | Power switch | Maintained system switch | Master power; not a channel assignment |
| 4 | Display indicator | Indicator/output | Optional power/UI state LED |
| 5 | Charge jack | Power connector | Retain only if compatible with the new battery/charger design |
| 6 | D.S.C. jack | Legacy wired trainer/direct-servo port | Candidate service/debug connector; not initially required |
| 7 | Display switch | Maintained system switch | Display/backlight off/on by default; optionally day/night mode if electrically accessible |
| 16 | Steering tension adjustment | Mechanical adjustment | Retain unchanged; no ESP32 input required |
| 17 | Dot-matrix display | Output | Replaced by TFT35 SPI touchscreen |
| 20 | Keypad assembly | Eight digital inputs | UI navigation and configurable shortcuts |

## Suggested six-channel default model

This is a safe starting layout for the ER6G, not a hard-coded restriction.

| CRSF channel | Default source | Output behavior | Initial state |
|---:|---|---|---|
| CH1 | Steering wheel | Proportional | Enabled |
| CH2 | Throttle/brake lever | Proportional, neutral interlock and failsafe | Enabled |
| CH3 | Aux 3 digital trim | Stepped proportional/three-position, configurable | Disabled until explicitly configured |
| CH4 | Drive Mode switch | Two-position | Disabled until explicitly configured |
| CH5 | Idle Up switch | Two-position | Disabled until explicitly configured |
| CH6 | Timer button | Momentary, toggle, or pulse | Local timer only until explicitly configured |

## Assignment types the firmware should support

Each physical input should have a stable input ID independent of its current
assignment. An assignment can then target one of the following:

| Assignment type | Suitable inputs | Key settings |
|---|---|---|
| Proportional channel | Steering/throttle pots | Channel, calibration, reverse, endpoints, centre/sub-trim, curve/expo |
| Two-position channel | Drive Mode or Idle Up switch | Channel, normal/reverse values, startup warning, failsafe |
| Momentary channel | Timer button or one direction of a trim | Channel, active value, pulse duration |
| Toggle channel | Momentary button | Channel, two values, initial/failsafe state |
| Three-position channel | Digital trim or paired buttons | Channel, low/mid/high values, wrap/clamp behavior |
| Stepped proportional channel | Any two-way digital trim | Channel, step size, repeat rate, minimum/maximum, reset value |
| Local parameter adjustment | Any two-way digital trim | Trim, dual rate, expo, brake rate, power limit, ABS parameter, etc. |
| UI action | Keypad or timer button | Page, timer, reset maximum, telemetry page, shortcut |
| Inhibited | Any assignable input | No action |

## Recommended implementation order

1. Give every physical control a stable ID and show its live state in an
   **Inputs Monitor** page before assigning functions.
2. Implement the two analogue axes, then the two maintained switches and Timer
   button.
3. Decode all five two-way digital trims, including press-and-hold repeat.
4. Add per-model assignments using the types above.
5. Add startup switch warnings, throttle neutral checks, channel failsafes, and
   conflict warnings before enabling accessory channels.
6. Add keypad shortcuts only after the core navigation behavior is stable.

Detailed status, default bindings, safety requirements and TFT blanking work are
maintained in [control-assignment-roadmap.md](control-assignment-roadmap.md).

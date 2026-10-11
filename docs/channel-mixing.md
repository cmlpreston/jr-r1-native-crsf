# Channel mixing

LCD: Setup → Model → Channels → CHx → Mixing / switched rates. Pages contain
Mix 1, Mix 2, optional switched rates, each mix's activation, Bench preview, Baseline / limits, and Mix curve.
Save commits the channel; Cancel/Back discards its draft. The built-in web page
has equivalent controls under Channel parameters → Mixing.

Each destination has an explicit **Existing channel input** contribution (100%
or None) and two ordered additional rules. Existing input means the assigned
steering/throttle input or auxiliary channel binding. Defaults preserve normal
channel behavior. Disable Existing input for a destination composed solely of
mix rules. Add accumulates; Replace discards the preceding total when active.
The resulting demand is limited to ±100%.

## Sources and order

- **Channel 1–16:** another channel's mixed logical signal, including its
  expo/fixed rate and optional switched rate, before mechanical endpoints,
  reversal, sub-trim, output slew and transmitted/CUT substitution.
- **Configured steering/throttle:** calibrated input with its role channel's
  expo/fixed rate, without that channel's mixes or mechanical settings.
- **Raw physical control:** calibrated wheel/trigger position; trims are
  −100/0/+100%; digital contacts are released −100%, held +100%. These are
  physical positions, not a binding's toggle/step state.

Positive/negative weights are independent signed percentages (−100 to +100).
Follow source trim optionally adds the source channel's ordinary trim before
weighting; it does not inherit mechanical sub-trim. Raw physical sources have
no channel trim to follow. Upstream mixes that deliberately include trim retain
that contribution in downstream channels.

The destination applies its own expo/rate, endpoints, reversal, sub-trim, trim,
Walk throttle limiting and slew to the combined demand. Thus destination expo
can further shape a configured source. Keep destination rate at 100% and expo
at zero when only the source response should shape the result.

Dependencies are evaluated in order regardless of channel number. Direct and
indirect cycles are rejected even if one branch is currently switch-inactive.
Missing active input dependencies cause an input fault; disabled steering or
throttle cannot silently become a valid channel source. Saving forces CUT.
The existing physical throttle neutral interlock remains required, with an
additional mixed-throttle neutral veto. CUT/safe outputs do not follow live mixes; auxiliary channels may use an explicit CUT value.

## Example

For CH3 = 60% steering CH1 + 40% throttle CH2:

1. Select CH3; set Existing channel input to None.
2. Enable Mix 1, source Channel 1, both weights +60%, Add, Always.
3. Enable Mix 2, source Channel 2, both weights +40%, Add, Always.
4. Set CH3 mechanical endpoints/reversal as required, then Save.

Rear steering: CH3 existing input None, Mix 1 Channel 1, both weights −70%.
Use +70% for crab steering; activation may use a maintained switch or drive mode.

## Bench preview

LCD and web show signed bars for existing input, both contributions, combined
value and final output. Adjust test source positions, switch level and mode to
try a draft without moving a receiver. Samples represent signals at each mix
contribution input (after source shaping), not simulated ADC positions. Channel
source trims are included when requested. The two sliders track together when
both rules select the same source. Bars show contributions even when a later
Replace discards them; Mixed/Output show the ordered result.

The preview uses the same C++ mixer and destination response as control output.
It is not a live telemetry monitor; source dependency chains are not driven by
these test sliders. Switches share the selected test high/low state. Rates are
settled and slew/transition timing is not animated. The web preview does not
write settings, reset safety or transmit any test data. Save is separate.

## Mix curve

LCD: use Page > to reach **Mix curve**. The web editor shows the graph above
the contribution bars. Choose Existing input, Source 1 or Source 2 for the
horizontal axis (−100% to +100%). The vertical axis is final channel output
after response, reversal, trim and final limits. The amber marker shows the
current test input/output. Other inputs stay at their test values; rules using
the same source sweep together.

The graph samples the shared C++ preview at 41 positions. On the LCD, set test
switch and drive mode on the bars preview page; their current values are shown
above the graph. This is a settled response curve, not a time/history plot or a
simulation of upstream source shaping. It never transmits the test values.

## Optional switched rates

A maintained switch chooses negative/positive Low and High percentages.
For 50%/100% forward throttle with full braking, use Low positive 50%, High
positive 100%, both negative 100%. Transition is milliseconds per 100 percentage
points. Rates scale shaped travel about neutral and do not rewrite endpoints.

## Persistence and compatibility

Per-model `v3` blobs hold the extended settings. Exact 28-byte-per-channel `v2` records retain their settings with zero constant, full output limits and no CUT override. Exact 20-byte-per-channel `v1`
records are migrated in memory, preserving raw sources, original single weight
on both sides, Add/Replace, base inclusion and switched rates. The old blob is
left intact. Only an explicit Save writes v3; other model data is untouched.

Validated with host mixer/response/safety tests, portrait and landscape simulator
navigation/persistence audits, web editor checks and XIAO portrait build. Hardware
validation is required after the next requested flash.

## Baseline, final limits and gyro gain

A constant contribution (−100% to +100%) is added to the existing input before
the ordered rules. Replace discards that contribution too. Final minimum and
maximum clamp the requested command after endpoints, reversal, trim and slew.
They do not change the logical source passed to downstream channel mixes.
Steering/throttle limits must include zero.

For a gyro on CH3, after confirming that positive CH3 selects the desired mode:
disable Existing input, set Constant +60%, add Channel 2 with positive weight
−40% and negative weight 0%, Always. With default destination response this gives
+60% at neutral, +40% at half forward throttle, +20% at full throttle, and +60%
under braking/reverse. Set final limits +20% to +60% to stay on that side of
centre. Verify the gyro mode LED; these percentages are channel commands, not
calibrated gyro sensitivity. Do not assume the sign selects AVCS on every gyro.

An optional auxiliary CUT override sends a separate final channel command,
bypassing response and requested-output limits. Steering/throttle cannot use
this override, including when an auxiliary channel is reassigned to those roles.
Saving still forces CUT and normal arming checks remain required.

The CUT override takes effect after model settings load, not during the earliest
boot frames. It does not configure receiver RF-loss failsafe. The HK401B expects
AVCS during initialization: power the receiver after the transmitter is ready
and verify its mode indicator; configure receiver failsafe separately.

## Reset mixing

Use **Reset mixing** on the web editor or **Reset** on the LCD mixing pages to
restore the selected channel's mixing draft: existing input included at 100%,
both rules off, constant zero, final limits −100%/+100%, CUT override off and
switched rates off. Rule weights and inactive rate settings also return to their
factory defaults. Select **Save** to apply; leaving without saving discards the
reset. Channel endpoints, expo, reversal, trims and physical control assignments
are separate settings and are preserved.

## Threshold switches (brake lights and other auxiliary functions)

Each rule's activation page now includes **Source mode**, **Trigger point** and
**Hysteresis**. Source mode can be Proportional, Switch above, or Switch below.
A threshold rule contributes **0% while OFF and +100% while ON**. Proportional
weights are ignored in switch modes. Add/Replace, optional source trim, activation
switches and destination response/limits still apply. Use a destination with
100% rates/endpoints, zero expo/trim/sub-trim, no reversal or slew for a direct
0/+100 output. Threshold sources can be any physical control, configured input,
or logical channel. Threshold rules cannot feed steering/throttle, including
through other channels, so their neutral safety checks remain stateless.

For CH3 brake lights:

1. Set Existing channel input to None; leave constant at 0%.
2. Enable Mix 1, source Raw throttle trigger, operation Replace, activation Always.
3. On Mix 1 activation, select Switch below, trigger **-5%**, hysteresis **3%**.
4. Leave Mix 2 and switched rates disabled; use the normal 0% auxiliary CUT value.

The lights command turns ON strictly below -5%, remains ON within the hysteresis
band, and turns OFF at or above -2%. Switch above turns ON strictly above the
trigger and OFF at or below trigger minus hysteresis. Hysteresis is a width in
percentage points (0–100), and the release point must remain inside -100–+100%.
At the trigger itself an OFF switch stays OFF. With zero hysteresis it releases
at the trigger. Latches are independent per destination/rule and reset on CUT,
configuration/model changes, invalid inputs, or inactive rule activation.
Thresholds describe trigger position or logical channel demand; brake and
reverse may share the same negative trigger range depending on the vehicle ESC.

Bars/curve previews start OFF on every sample and show the activation edge;
they do not simulate retained hysteresis history. Saving uses a v4 model blob;
v1/v2/v3 settings migrate with all source modes set to Proportional.

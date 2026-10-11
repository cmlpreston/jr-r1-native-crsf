# Live serial input dashboard

The full transmitter firmware accepts the read-only `INPUTMON` command. Run
the ANSI terminal client from the project directory (after flashing a build
that includes this command):

```sh
.venv/bin/python tools/input_monitor.py
```

It auto-detects a single Espressif USB device. To choose explicitly:

```sh
.venv/bin/python tools/input_monitor.py --port /dev/cu.usbmodem21201
```

Close any other serial monitor first. Use an ANSI terminal such as macOS
Terminal, at least 80 columns by 33 rows. Ctrl+C restores the terminal and
closes USB. Opening USB serial can restart the XIAO even with DTR/RTS low.
This is a terminal program, not a PlatformIO monitor filter.

The screen shows K1/K2 availability, GPIO words and cumulative read errors;
throttle raw/filtered ADC, steering filtered ADC, pre-CUT input percentages;
battery ADC millivolts when sensing is enabled; and mapped controls with raw
contacts, debounced positions and last events. RAW 1 means an active contact
(normally electrically LOW); trim pairs are negative/positive. Both active
contacts are invalid. An offline device is not shown as healthy/released.
DISPLAY_SW is shown as unmapped. Unconnected analog inputs may float.

Snapshots are requested up to five times per second, with only one request
outstanding. Values are marked STALE after two seconds without a complete
snapshot. Ordinary logs do not scroll over the dashboard. Last-event fields
retain brief presses that occur between refreshes; this is not an event log.

The monitor doesn't arm, calibrate, change assignments or save settings.
Physical controls still perform their normal configured actions. PIN unlock
is not needed. If hardware startup fails, HALTED_SAFE mode still samples the
available expanders and raw analog inputs, without starting the control task
or dispatching button actions. Percentages/calibration are unavailable in that
mode. Repair the wiring and reset to retry normal startup. Failed expanders
are not reconfigured automatically by this halted-state monitor.

In this recovery state, saved home Wi-Fi auto-connect and the authenticated web
configuration/remote display also remain available. The UI shows HW FAULT; PIN
unlock allows menu navigation but cannot arm outputs. If the LCD driver itself
failed, LVGL renders only to the web mirror. Touch polling is disabled during
recovery. CRSF continues sending safe frames; no live input/mixing control task
is started. A reboot retries the hardware and is required to leave recovery.

## Live RF bench testing with missing LCD/touch

Use the explicit opt-in client mode:

```sh
.venv/bin/python tools/input_monitor.py --rf-test
```

Enter the current saved PIN (initially `1234`) when prompted. This enables a temporary test session
after normal startup or in hardware recovery. When recovering, it starts the
normal input/control tasks, using the saved calibration, model,
channel assignments and mixes. It does not automatically arm. For a servo-only
test, disconnect the propulsion motor/ESC output being driven.

- **A**: request arm with controls centred. Both expanders and startup controls
  must be healthy; calibration and the setup hotspot must be closed. The normal
  receiver-link and throttle-neutral dwell checks still apply.
- **Space** or **C**: CUT immediately.
- **Q** or **Ctrl+C**: CUT and exit.

The client sends a heartbeat only while fresh input snapshots arrive. Two
seconds without it latches CUT, independently in the control and CRSF tasks.
A resumed connection does not re-arm: press A again. Faulted inputs and model/
configuration reset actions also CUT. The mode and PIN authorization are not
saved, and reboot always disables them. Raw serial equivalents are `RFTEST
1234`, `RFKEEP`, `RFARM`, `RFCUT`; use the client to supply the heartbeat.

The terminal shows actual safety status, arm request, receiver link/LQ, CRSF
frame count and throttle/steering CRSF values. A growing frame count alone
doesn't prove receiver/servo operation. Web configuration stays available while
CUT and becomes read-only during live output. Browser taps cannot arm. The
LCD/browser badge shows BENCH CUT / BENCH LIVE where appropriate; the touch
controller remains disabled in recovery. Reboot after fixing hardware to return
to the normal operating path.

Protocol v1 uses tab-separated `@INPUT_BEGIN`, `@INPUT_MCP`, `@INPUT_ROW`, and
`@INPUT_END` records. Incomplete or malformed snapshots are discarded.

## Forgotten PIN recovery

With outputs CUT, send `PINRESET 1234` over USB serial to set the PIN to 1234,
or replace those digits with another four-digit PIN. This does not unlock the
handset, enable RF output, leave Walk mode, or erase models/maps/Wi-Fi.
PIN changes normally use System → PIN or the authenticated web PIN form.

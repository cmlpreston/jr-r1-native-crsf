# ExpressLRS TX configuration

Open **Main Menu → LINK / TELEM** to discover the local TX module. The handset
uses CRSF device discovery and parameter reads, so names, IDs, choices, limits
and visibility come from the installed ExpressLRS firmware. No fixed Ranger
parameter IDs or assumed RF power levels are used.

Use **PREV / NEXT** to page through a folder. Select a folder to open it and
**BACK** to return. Select a setting, choose **PREVIOUS VALUE / NEXT VALUE**, then
**APPLY**. Choosing a value alone does not send it. The handset reads the setting
back, checks the returned value, and reloads the parameter tree because one
setting can alter the available choices for another. Values belong to the TX
module; they are not saved as an independent, potentially conflicting handset
copy.

This covers the module's exposed selection parameters, including packet rate,
telemetry ratio, Model Match, maximum RF power and dynamic power. A disabled or
missing setting is not invented by the handset. Numeric/string types other than
text selections are displayed read-only/unsupported; binding phrase changes
remain on the existing **BINDING** page.

Commands such as **WiFi Connectivity → Enable WiFi** use **RUN COMMAND**. If the
module asks for confirmation, read its message and press **CONFIRM**, or
**CANCEL / BACK**. Commands are queried for status; an unacknowledged start or
confirmation is never automatically repeated. Wi-Fi activation can stop the
normal CRSF/RF link, so a lost-response message can follow successful activation.
Reconnect and use **REFRESH** to establish the actual module state.

The control task holds outputs in its existing calibration/configuration safety
inhibit while this settings page is open. The CRSF task continues sending safe
channel frames between bounded management requests. Leaving the page uses the
existing neutral/re-arm interlock. Configuration does not require a connected RX.
The receiver will need compatible RF settings to reconnect after relevant changes.

## Protocol and robustness

- Local TX address 0xEE, handset 0xEA; discovery 0x28/0x29, parameter read/info
  0x2C/0x2B, parameter write/command 0x2D.
- CRC validation occurs before configuration replies enter the telemetry mailbox.
- One outstanding request; replies are checked against destination, origin, ID
  and chunk countdown. Missing reads are retried with bounded timeouts.
- Up to 64 parameters, 640 bytes per assembled parameter, 384 bytes of choices.
  Excessive or malformed data produces an explicit error and disables writes.
- Hidden fields and empty choices cannot be selected. Regular device pings detect
  disconnection. Apply is refused for stale state.
- Binding-phrase MSP support has separate ExpressLRS-version requirements; these
  standard parameter operations do not depend on that binding extension.

Protocol references:
https://github.com/ExpressLRS/ExpressLRS/blob/master/src/lua/elrs.lua
https://github.com/ExpressLRS/ExpressLRS/blob/master/src/lib/tx-crsf/TXModuleParameters.cpp

## Verification

`make -C test run` includes protocol and telemetry transport tests for discovery,
chunk assembly, source/ID matching, CRC rejection, selection validation, readback,
confirmation/cancellation, timeouts, stale state and timer wraparound.

The desktop simulator provides a representative fake TX module:

```
make -C simulator
simulator/build/jr-r1-sim --page tx-config
```

Simulator responses are fixtures, not a claim that the attached Ranger runs those
exact options. Hardware acceptance remains: discover the actual Ranger's menu,
change and read back a setting, power-cycle to check persistence, and exercise
Wi-Fi confirmation with the vehicle unable to move. No hardware was flashed as
part of this implementation.

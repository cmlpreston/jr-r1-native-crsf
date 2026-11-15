# RadioMaster ERS commissioning — 2 November 2026

Prepared for ERS-GPS, ERS-CV01, ERS-CU01 and ERS-RB01. Software tests and builds
are not a substitute for the first physical sensor/chain test. Sensor calibration,
accuracy have not yet been fully verified with this set. Delivery-rate evidence
is recorded below; it does not validate measurement accuracy.

## Current baseline and remaining checks

The ER6G is now running official ExpressLRS 4.1.0 (a9d4a9cb), without our S.Port
bridge. GPS telemetry returned after the stock upload. The exact failure cause
is not isolated; do not use R1 ERS discovery/configuration during baseline testing.
A working chain, real packet source IDs, engineering scales and physical accuracy
still require one-at-a-time checks for CV01, CU01 and RB01. Decoder fixtures alone
are not proof that this sensor set is commissioned.

Installed R1 changes hide dedicated FAS cards/new-widget choices without reusing
persisted source IDs. Data → Sensors → Current / energy includes a saved,
CUT-only Standard CRSF battery selector: Receiver supply or Vehicle pack / CU01.
This is local handset interpretation; it sends no command to the sensors.
Under stock ELRS, the ordinary battery packet is assigned to CU01. Receiver
supply instead comes from the stock receiver ADC voltage group (source 128,
first reading), so the two voltages can be displayed independently. This profile
must not be combined with the legacy FrSky FAS source-128 bridge.

### Measured delivery at 250 Hz / telemetry ratio 1:4

On 2 November, the user changed the Ranger Nano telemetry ratio from Std to 1:4.
A 30-second R1 USB sample received 211 GPS packets (about 7.0 Hz), all decoded
with zero rejects and zero CRSF CRC errors. Maximum sampled ages at 200 ms
polling were GPS 203 ms, CV01 272 ms and CU01 258 ms. These are sampled ages,
not exact maximum interpacket gaps. No stale events were sampled. The earlier
Std baseline delivered GPS/CV01/CU01 about every 4.8 seconds, exceeding the
three-second GPS/CU01 freshness limit. Retain the existing freshness policy.
The user subsequently reported a good backyard position trace in green mode.

CU01 measurement leads were not connected inline; its zero readings do not
validate current, voltage or consumption accuracy. GPS mode-specific accuracy
and full 10 Hz delivery are not established by this measurement.

## Receiver connection

Stock ExpressLRS already supports ERS. The custom S.Port-trial battery identities
described below are historical; they are not present in the current stock baseline. ERS uses non-inverted CRSF UART,
420000 baud, separate TX and RX, not the inverted single-wire S.Port connection.
Daisy-chain the four ERS modules on one UART. Sensor TX connects to receiver RX;
sensor RX connects to receiver TX. Use the marked connector pinout, not wire colour.
Power the chain from a common supported sensor supply (5 V is suitable for all four),
with common ground. The CU01's high-voltage measurement input is distinct from its
5–8.4 V logic supply. Never connect the 3S TX pack to these sensor UART supply pins.

The ER6G primary UART is physical output 2 / GPIO1 (TX) and output 3 / GPIO3 (RX).
In its receiver WebUI, select CRSF as the primary serial protocol and configure
these outputs as serial TX/RX. They stop being PWM outputs. Keep output 5 / GPIO2
as the single-wire secondary S.Port port if needed. **Do not change this mapping
with servos attached.** It is not changed automatically by this firmware.

A possible three-servo arrangement is output 1 (steering), output 4 (throttle),
output 6 (gyro; requires the optional header), leaving outputs 2/3 for ERS and 5
for S.Port. Match PWM channel assignments to the vehicle before enabling output.
An ERS-only arrangement can free the S.Port output for PWM after explicit
receiver configuration. Do not attach two GPS sources, or two cell-source-0
sensors, simultaneously: source arbitration between those buses is not implemented.

## Setup on the R1 touchscreen

Open **Setup → Model → second page → ERS sensors**. Tap **Discover**, allow eight
seconds, then select a sensor. **Read settings** retrieves its confirmed values.
Use minus/plus to edit GPS mode, total motor pole count or battery capacity, then
**Apply**. Capacity offers steps of 100 or 1 mAh. The draft stays unconfirmed until
the sensor acknowledges it; a timeout leaves it available for review and retry.
**Sensor list** discards an unapplied draft. CV01 currently offers settings readback
only. The **Link** shortcut opens module-link controls without starting Wi-Fi.

Discovery and settings use the receiver RF link. Keep CUT and the module link
and receiver powered. Wi-Fi is not required. Touch and the normal keypad focus/
select/plus/minus navigation are supported in landscape and portrait.

## Setup on the R1 web page

1. Leave the R1 in CUT but enable its module link and establish an RF link.
2. Under **RadioMaster ERS sensors**, select **Discover sensors**; allow eight seconds.
3. Select each sensor and **Read sensor settings**.
4. GPS: select Global mode (2) for position/maps, or Speed mode (1) for speed-only.
5. RB01: enter the motor's total magnetic pole count, **not pole pairs**.
6. CU01: enter actual battery capacity in mAh. Remaining percentage needs this setting.
7. RPM/cell displays currently use source 0. Automatic multi-sensor ID reassignment
   is intentionally not exposed: test one sensor of each type.
8. Apply individually and wait for **Sensor confirmed**. A timeout is not success;
   read settings before retrying. Writes are never automatically repeated.

Sensor settings are stored by the sensor. The R1 exposes only these commissioning
operations, not calibration, firmware update, reset, LED or sensor power commands.
Discovery is bounded to eight sensors. Initial physical acknowledgement behaviour
and settings persistence still need bench verification. Do not run another sensor
configurator at the same time.

## Displays

- GPS: existing map/trail, speed, satellites and GPS-course widgets.
- CV01: Battery cells widget, per-cell voltages, total and lowest cell; fixed-width
  zero-padded reports are accepted. The sensor supports up to 8S; R1 storage supports 12S.
- RB01: RPM sensor display / Sensor widget, source 0.
- CU01: Data → Sensors → third page: ERS pack voltage, current, used mAh and remaining %.
  The four readings are also selectable Sensor-widget sources, in every widget size.
  Standard CU01 scaling is 0.1 V, 0.1 A and integer mAh. Stale data displays `--`.

The updated receiver tags the first external UART battery source as battery ID 2
while preserving the original readings and CRC. Receiver supply remains ID 0;
S.Port FAS current remains ID 1. Additional external battery IDs are ignored until
receiver reboot; one external current sensor is supported. The source convention is specific to this trial. With stock ExpressLRS,
select **Standard CRSF battery packet → Vehicle pack / CU01** in the R1 web UI.
This decodes ordinary ID-0 packets directly; their voltage is not also labelled
as receiver supply. The choice is saved on the R1; default remains receiver
supply for compatibility with the existing FrSky trial.
The receiver's normal analog-voltage reporting remains enabled when ERS is attached.
RPM, GPS, cells and configuration replies otherwise use ordinary CRSF forwarding.

## Verification before driving

Test one sensor at a time, then the chain: compare voltage against a meter, zero
current with no load, capacity after a known run, RPM with the correct pole count,
and GPS outdoors. Check receiver voltage stays independent of drive-pack voltage.
Unplug a sensor to verify stale indications, reconnect, and power-cycle to confirm
settings. Then confirm servo channel mapping and failsafe with the vehicle secured.
No receiver or servo remapping is performed by sensor discovery/configuration.

## Protocol references

- [RadioMaster ERS sensor family](https://radiomasterrc.com/collections/ers-series-sensor)
- [RadioMaster configurator V1.3](https://cdn.shopify.com/s/files/1/0701/8066/7584/files/SENSOR_LUA_2026_7_31.zip?v=1785485572)
- [RadioMaster cells widget](https://cdn.shopify.com/s/files/1/0701/8066/7584/files/CELL_LUA_2026_7_31.zip?v=1785485571)
- [CRSF specification](https://github.com/tbs-fpv/tbs-crsf-spec/blob/main/crsf.md)

Protocol facts were implemented independently; the manufacturer Lua code is not bundled.

Discovery follows the manufacturer configurator: initial type-255 / ID-1 PING,
then type-255 / ID-255 HEART_BEAT broadcasts every two seconds during an
eight-second scan. Both PING and HEART_BEAT registration replies are accepted.
USB `ERSSTATUS` reports only configuration frame counts/header metadata, not GPS
coordinates. `ERS TX` serial lines identify requests that reached the UART sender.

## Installed GPS speed-field handling

EdgeTX's CRSF decoder treats each all-FF GPS field as unavailable independently.
R1 now follows that convention: fresh speed has its own timestamp and validity;
missing/out-of-range coordinates cannot move or extend the stored map trail.
Missing heading and satellite fields do not invalidate speed. A live position
resumes map updates normally; stale speed still expires after three seconds.
GPSSTATUS includes speed_packets, speed_available and speed_age_ms for rate checks.
This is not a sensor-mode detector: hardware validation is still needed in blue
and red modes. No sensor discovery, setting writes or RF-rate changes are used.
Reference: https://github.com/EdgeTX/edgetx/blob/main/radio/src/telemetry/crossfire.cpp

The dashboard GPS map/speed card now selects a useful presentation from these
independent fields. Position selects the map; speed-only data can select speed
after a three-second last-position grace. Fresh known low satellite counts or
position-shaped data lacking a usable position retain an acquiring/waiting map.
Satellite availability is separate from value: an omitted field is unknown.
Legacy S.Port bridges also put zero in this field, including untagged15B packets
when heading is available. Zero plus fresh non-placeholder coordinates therefore
retains the compatibility map with a visible Position unconfirmed caption and
amber marker. Zero with missing/placeholder coordinates remains acquisition.
This still does not establish the physical sensor mode. CRSF0x02 labels its count
as satellites in view, with no mode/fix flag; setting15 of the ERS config reply is
the explicit mode confirmation. No automatic discovery/configuration was added.
Reference: https://github.com/tbs-fpv/tbs-crsf-spec/blob/main/crsf.md#0x02-gps

As of2026-11-15, the real blue-mode field set remains unverified. The RadioMaster
manual documents blue10Hzground speed, but the speed-only/all-FF-field fixtures
are synthetic decoder/UI tests, not a capture from this sensor. The user is not
ready for a hardware check. A later passive blue/red comparison must verify field
presence, satellite values and update rate without sensor discovery or writes.
Manufacturer manual:
https://cdn.shopify.com/s/files/1/0701/8066/7584/files/ERS-GPS-1.3_6cc91247-3d22-44be-ae09-e0db5077a1f1.pdf?v=1770618392

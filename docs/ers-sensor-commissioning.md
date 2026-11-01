# RadioMaster ERS commissioning — 1 November 2026

Prepared for ERS-GPS, ERS-CV01, ERS-CU01 and ERS-RB01. Software tests and builds
are not a substitute for the first physical sensor/chain test. Sensor calibration,
accuracy and update rates have not yet been verified with this set.

## Receiver connection

Stock ExpressLRS already supports ERS. Our updated ER6G S.Port-trial firmware
adds independent receiver/vehicle battery identities for this setup. ERS uses non-inverted CRSF UART,
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

Open **Setup → Model → second page → ERS sensors**. Tap **Discover**, allow four
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
2. Under **RadioMaster ERS sensors**, select **Discover sensors**; allow four seconds.
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

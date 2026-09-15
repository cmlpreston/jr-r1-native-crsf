# ER6G / XIAO C3 / FrSky sensor bridge

Proposed bench design, 2026-09-11. Not wired or validated on hardware.

User-confirmed equipment: RadioMaster ER6G, XIAO ESP32-C3, FrSky GPS,
FAS300 used only for motor temperature, and an unidentified 4S cell monitor.
FAS300 ADV is consistent with the temperature-probe description, but the exact
revision is not confirmed. FLVSS/MLVSS are candidates for the cell monitor.
Receiver power is normally a BEC; bench testing uses a nominal 2S 6.6 V pack.
The BEC output voltage and the test pack's chemistry/full-charge voltage are
not yet confirmed.

## Proposed pin allocation

| Connection | XIAO label | C3 GPIO | Other end |
| --- | --- | --- | --- |
| CRSF telemetry TX | D4 | 6 | ER6G output 3 signal, configured Serial RX |
| CRSF receive | D5 | 7 | ER6G output 2 signal, configured Serial TX |
| S.Port poll TX | D2 | 4 | TX input of the bus interface below |
| S.Port reply RX | D3 | 5 | RX output of the bus interface below |
| S.Port TX enable | D1 | 3 | Active-low TX-buffer enable |
| Logic reference | GND | — | Receiver, sensors and buffer ground |
| C3 power | USB-C / 5V | — | USB for initial bench work; regulated 5 V for installed operation |

These are proposed UART matrix assignments, not the XIAO's default D6/D7 UART
pins. They leave the boot UART pins and GPIO2/8/9 straps out of the bridge wiring.
Use non-inverted CRSF, normally 420000 8N1 on the receiver side; verify the
installed receiver setting before connection. S.Port uses 57600 8N1 with both
UART TX and RX inverted in software for this non-inverting buffer design.

The ExpressLRS ER6G target selects `Vario 2400.json`, which lists PWM GPIOs
`[14,1,3,15,2,4]`. Thus physical outputs 2/3 expose the primary ESP32 UART
TX/RX GPIOs 1/3. Confirm that the installed firmware's Model page offers those
Serial TX/RX assignments. Current ESP32 ELRS versions can also offer Serial2;
that is an alternative, not assumed in this primary-UART plan.

Converting these outputs to serial consumes both outputs. A proposed car layout
is steering on physical output 1 and ESC on physical output 4, with output 4
mapped to the existing throttle *input channel*. Physical socket number and
transmitted channel number are different. Confirm existing channel roles and
failsafe values before changing any receiver settings. No settings were changed.

## Proposed S.Port interface

Use an **SN74LV125AN**, the 14-pin through-hole PDIP version, powered from C3
3V3. This replaces the earlier surface-mount SN74LVC2G125 proposal because the
user needs parts suitable for hand assembly. Use two of its four buffers:

- Buffer 1 input from D2; its output goes through a 220-ohm series resistor to
  S.Port SIGNAL. Its active-low enable goes to D1 with a 10-kilohm pull-up to
  3V3, so reset leaves the transmitter disconnected from the bus.
- Buffer 2 input from S.Port SIGNAL; its output goes to D3. Tie its active-low
  enable to ground so replies and local TX echo can be received.
- Place 100 nF across buffer VCC/GND. Join grounds. Component pin numbers must
  follow the chosen package's datasheet, not a generic 74xx125 footprint.
- Provisional idle bias: 47-kilohm pull-down on S.Port SIGNAL for the inverted
  UART's idle-low state. Check the actual sensor-chain bias and waveform before
  settling this value. Verify both logic thresholds and cable-edge timing.

Through-hole pin connections (SN74LV125AN, top-view numbering):

| IC pin | Function | Connection |
| --- | --- | --- |
| 1 | /1OE | C3 D1 / GPIO3, plus 10 kΩ pull-up to 3V3 |
| 2 | 1A | C3 D2 / GPIO4 |
| 3 | 1Y | Through 220 Ω to S.Port SIGNAL |
| 4 | /2OE | GND |
| 5 | 2A | S.Port SIGNAL |
| 6 | 2Y | C3 D3 / GPIO5 |
| 7 | GND | Common GND |
| 8, 11 | Unused outputs | Leave unconnected |
| 9, 12 | Unused inputs | GND |
| 10, 13 | Unused /OE inputs | 3V3 (disabled) |
| 14 | VCC | C3 3V3 |

Place the 100 nF capacitor close to pins 14 and 7. A 14-pin DIP socket can be
used on stripboard. Select the exact LV125A family and N package; a generic
74HC125 is not an electrically equivalent substitute for 5 V tolerant inputs
while powered at 3.3 V.

This is an engineering proposal, not a manufacturer-validated S.Port reference
circuit. TI specifies 5.5 V tolerant inputs and tri-state outputs for this part;
that protects the C3 receive input within those limits. It does not establish
the actual sensor signal voltage or guarantee that every module accepts the
3.3 V driven level. Measure the sensor bus and verify response to polls first.
Do not connect S.Port SIGNAL directly to a C3 GPIO or confuse sensor supply
voltage with signal voltage.

During each poll the driver enables TX, sends delimiter + physical ID, waits
for the final stop bit to leave the UART, then disables TX before the sensor
reply. RX observes the complete poll and reply including local echo; do not
feed a second software copy of the poll in that mode. The C3 is the only bus
master. GPS, FAS300 and cell monitor share the daisy-chained sensor bus.

## Power arrangement

The ER6G normally runs from a BEC and uses a nominal 2S 6.6 V pack for testing.
Nominal pack voltage is not the maximum voltage: select any regulator against
the actual full-charge voltage and the BEC's measured output range.

Initial bench arrangement:

```text
2S test pack ----------------------> ER6G receiver supply
USB-C ----------------------------> XIAO C3
XIAO 3V3 -------------------------> S.Port buffer VCC
All circuit grounds --------------> common ground
Receiver positive -- NO CONNECTION --> XIAO 5V/3V3
```

Use an ER6G supply connection approved by its manual (outputs 1–5; do not use
output 6 as a power input). The C3-to-ER6G serial connections carry signal and
ground only; omit the receiver-positive wire from the bridge harness.

Installed operation adds a regulated 5 V supply for the C3. A buck regulator
is suitable only if its input stays sufficiently above 5 V throughout operation.
If the BEC can be 5 V or lower, use an appropriate buck-boost regulator instead,
or a verified direct 5 V supply arrangement. The final choice awaits the BEC
voltage. Do not feed the 6.6 V test pack, a 4S drive pack or a high-voltage BEC
directly into XIAO 5V/3V3 or its single-cell battery connection.

The proposed sensor rail is regulated 5 V, separate from the 3.3 V logic rail.
FAS300 ADV and FrSky GPS V2 specify 4–10 V supply, so 5 V fits those models.
Confirm the actual GPS and cell-monitor models before powering the chain.
The cell monitor's 4S balance connector measures the drive pack; it is not a
substitute for identifying the module's S.Port power requirements. Do not join
the receiver supply positive and regulated sensor 5 V positive.

When the C3 runs from USB for development, disconnect its external 5 V feed.
Keep common ground. This avoids paralleling USB power and an external regulator.

## Telemetry handling

`VehicleSensors` now decodes standard S.Port GPS fields, T1/T2 temperature and
4S cell-pair packets. It retains per-field freshness, and reports pack total
and minimum only after a complete fresh 4S scan. Actual application IDs and
physical IDs still need a bus capture; FAS300 temperature is not assumed to be
T1 until observed. Sensor decoding is tested on the host, not captured hardware.

The earlier VFAS/current `BatteryBridge` is not enabled for this installation:
the FAS300 is not measuring current, and the ER6G already supplies receiver
battery telemetry. Keep receiver voltage and drive-pack cells distinct.

Next, choose the over-air encoding for individual cells and temperature after
checking the installed ELRS version's supported frame forwarding. Standard GPS
0x02 is the likely GPS transport; its satellite count cannot be invented from
standard FrSky GPS fields. GPS validity also needs care: receipt freshness alone
does not prove a current satellite fix. The transmitter still needs matching
vehicle telemetry decoding/display. None of these values are sent over RF yet.

## Sources

- [ExpressLRS targets](https://github.com/ExpressLRS/targets/blob/master/targets.json)
- [ER6G hardware layout](https://github.com/ExpressLRS/targets/blob/master/RX/Vario%202400.json)
- [ExpressLRS serial configuration](https://www.expresslrs.org/software/serial-protocols/)
- [XIAO C3 pin reference](https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/)
- [ER6G manufacturer manual](https://www.flyingtech.co.uk/wp-content/uploads/2023/11/ER6G_Receiver_User_Manual.pdf)
- [FrSky GPS V2 manufacturer manual](https://www.intermodel.fr/fichiers/noticegpssensorv2.pdf)
- [TI through-hole buffer ordering code](https://www.ti.com/product/SN74LV125A/part-details/SN74LV125AN)
- [TI buffer specification and pinout](https://www.ti.com/lit/ds/symlink/sn74lv125a.pdf)
- [FrSky FAS300 ADV manufacturer manual](https://www.flyingtech.co.uk/wp-content/uploads/2023/11/FAS300-ADV-Manual.pdf)
- [OpenTX S.Port decoding](https://github.com/opentx/opentx/blob/2.3/radio/src/telemetry/frsky_sport.cpp)

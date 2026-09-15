# Onboard XIAO ESP32-C3: S.Port to CRSF

Status: host-tested conversion core, not yet flashable firmware. No GPIOs or
receiver output mappings have been changed. This folder is independent of the
transmitter build and its existing working-tree changes.

The user has since confirmed ER6G, FrSky GPS, a FAS300 used for temperature,
and an unidentified 4S cell sensor. See [the proposed wiring plan](wiring-plan.md)
for the C3 pin allocation, buffer circuit and remaining hardware questions.
`VehicleSensors` adds tested GPS, standard T1/T2 and 4S cell decoding. These
readings are not yet encoded or sent over CRSF; the VFAS/current bridge below
remains a separate first-stage fixture, not the selected vehicle configuration.

Intended path:

```text
S.Port sensors <--> C3 bus interface/poller --> CRSF RX on RadioMaster receiver
                                             |
                                          ELRS RF
                                             |
                                    Ranger Nano --> transmitter XIAO S3
```

The C3 acts as a telemetry producer on the CRSF side. It does not generate RC
channels or participate in the steering/throttle control path.

## Implemented and tested

- Streaming S.Port decoder: poll delimiter and physical ID, byte unstuffing,
  folded checksum, data primitive validation and partial-packet timeout.
- Generic decoded samples retain physical ID, application ID and raw value.
- Explicitly selected VFAS/current sources produce CRSF battery frames (0x08).
  VFAS centivolts become decivolts; current remains deciamps. Source selection
  includes both physical and application ID, avoiding automatic pack mixing.
- Both values must be valid and no more than 3 seconds old. Output is limited
  to 5 Hz; elapsed-time checks handle millis rollover.
- Capacity and remaining percentage are encoded as zero placeholders, not
  measurements. No consumption integration or charge estimate is implemented.
- Tests feed generated frames through the existing transmitter parser, and
  exercise malformed bytes, checksums, truncation, escaping, wrong sources,
  freshness, range checks, output bounds and timer rollover with sanitizers.

Run from the repository root:

```sh
make -C onboard/sport-crsf test
make -C test run
```

Example core use (sensor IDs must come from the actual installation):

```cpp
onboard::SportParser parser;
onboard::BatteryBridge bridge({0x22, 0x0210}, {0x22, 0x0200});
onboard::SportSample sample;
if (parser.feed(receivedByte, nowMs, sample)) bridge.accept(sample, nowMs);
uint8_t frame[12];
const size_t length = bridge.nextFrame(nowMs, frame, sizeof(frame));
// A future UART driver writes frame only when length != 0.
```

## Hardware integration still required

Confirm the RadioMaster receiver model and ELRS version, the S.Port sensor
models (including whether there is VFAS/current or only cell-voltage telemetry),
and the C3's existing wiring and interface circuit before assigning pins.

S.Port is a 57600-baud half-duplex bus. Its inverted signalling and bus release
need an appropriate interface; a normal UART TX/RX connection is not a complete
S.Port electrical design. A standalone sensor chain needs the C3 to poll it.
If an existing master is polling, the C3 must listen rather than add a second
master. The driver must handle turnaround and TX echo; when echo is disabled,
feed the transmitted delimiter/physical ID to the parser before the reply.

On the receiver side, select a supported CRSF serial input and verify its
baud rate. For a PWM receiver this can require output remapping, so establish
which outputs currently drive servos before changing it. Confirm logic levels,
common ground, sensor supply and the bidirectional S.Port interface against the
actual parts. No final pin map is assigned here.

The current transmitter calls every 0x08 battery voltage `receiverVoltage`.
If the receiver also produces its own battery packets, separate onboard-pack
telemetry can alternate with those readings and affect the RX alarm. Standard
0x08 packets have no battery-instance field. Before enabling bridge output,
choose one authoritative battery source or implement a separately identifiable
telemetry path with matching transmitter decoding. Current is transmitted by
this core but the existing transmitter does not yet expose it.

Next implementation: verified C3 UART/polling driver and PlatformIO environment,
sensor-specific mappings (cells/GPS/RPM/temperature as needed), battery-source
policy and transmitter display support, then a bench capture through the RF
link. Hardware builds and uploads have not been performed. Keep uploads on the
repository's PlatformIO preflight path; do not bypass it with direct esptool.

## Protocol references

- [FrSky telemetry bus overview](https://ethos-doc.frsky-rc.com/model-setup/telemetry/)
- [OpenTX S.Port IDs and units](https://github.com/opentx/opentx/blob/2.3/radio/src/telemetry/frsky_sport.cpp)
- [INAV S.Port electrical overview](https://github.com/iNavFlight/inav/blob/master/docs/Telemetry.md)
- [ExpressLRS receiver wiring](https://www.expresslrs.org/quick-start/receivers/wiring-up/)
- [ExpressLRS serial protocol configuration](https://www.expresslrs.org/software/serial-protocols/)
- [Betaflight deployed CRSF telemetry encoding](https://github.com/betaflight/betaflight/blob/master/src/main/telemetry/crsf.c)
- [Seeed XIAO C3 pin reference](https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/)

Battery units follow deployed legacy CRSF/Betaflight and this repository's
transmitter decoder. The live TBS specification currently describes different
battery units, so do not change scaling without checking the installed ELRS
firmware and captured packets.

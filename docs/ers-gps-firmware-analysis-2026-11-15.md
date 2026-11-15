# ERS-GPS binary inspection — 15 November 2026

The binary analysis below was read-only. A later, user-authorized update on the
same day installed the unmodified v1.9 image; a normal-mode settings query then
reported v1.9. Hardware follow-up evidence is recorded separately below.

## Images and reproducibility

- GPS-specific product download labelled v1.8:
  https://cdn.shopify.com/s/files/1/0701/8066/7584/files/RM_ERS_GPS-updata-v1.8.bin?v=1780640563
  `/tmp/r1-ers-gps-official-v1.8.bin`, 109,168 bytes;
  SHA-256 `56ad3c570c9fffb1f6aee2008ff84d93875b5d7326e4f7649cd11fa0ddffdb79`.
- ERS-series image named v1.9, linked by a preserved copy of the GPS product listing:
  https://cdn.shopify.com/s/files/1/0701/8066/7584/files/ERS_Sensors_V1.9.bin?v=1778047269
  `/tmp/r1-ers-sensors-v1.9.bin`, 79,980 bytes;
  SHA-256 `532482edc3afea7c3362d1ed9dd7f068196604ef630c8ae610d69235aa55f54f`.

Both contain directly disassemblable ARM Thumb code. App base 0x08004000 is
confirmed by string literal pointers (v1.8: file offset 0x399c contains
0x08017120, pointing to `$POLCFGMSG,0,5,0` at file offset 0x13120).
Minimal inspection-only ELF wrappers retain the unchanged raw bytes as one
executable .text section with address 0x08004000:
`/tmp/r1-ers-gps-v1.8-inspection.elf` and
`/tmp/r1-ers-sensors-v1.9-inspection.elf`.
Use Apple's `objdump --triple=thumbv7m-none-eabi -D --start-address=... --stop-address=...`.
The triple selects Thumb disassembly, not an assertion that the MCU is Cortex-M3.
The exact MCU model has not been established.

Filename labels do not establish which file is the latest compatible image.
The v1.8 image contains strings `GPS01` and `v1.3`; the latter is copied into
shared identification data, so it must not be treated as the package version.
The pre-update installed version was not established. After the update described
below, a settings reply explicitly reported version 19 (v1.9).

## Main result: both modes send full GPS frames, with different useful fields

Both images maintain two adjacent 16-byte records:

| Offset | Meaning | Record A, RAM 0x20000004 | Record B, RAM 0x20000014 |
| --- | --- | --- | --- |
| +0 | latitude int32 | updated from GGA | initialized zero; not updated by reviewed GPS parsers |
| +4 | longitude int32 | updated from GGA | initialized zero; not updated by reviewed GPS parsers |
| +8 | speed uint16 | updated from VTG | copied from record A |
| +10 | course uint16 | updated from VTG | initialized zero; not updated by reviewed GPS parsers |
| +12 | CRSF altitude uint16 | GGA altitude +1000 | initialized 1000 (encodes 0 m) |
| +14 | satellites uint8 | updated from GGA | copied from record A |

The GPS mode selector chooses A for internal mode 0 and B for internal mode 1.
Both call the same serializer, which emits address 0xEC, length 0x11, type 0x02,
all 15 GPS payload bytes in CRSF big-endian order, then CRC.
There is no shorter blue-mode packet or special unavailable marker here.

Mode setter accepts 0xAA => internal 0 and 0xBB => internal 1. Configuration
command 15 value 1 selects 0xBB; value 2 selects 0xAA. The existing official Lua
and R1 labels identify 1 as Speed and 2 as Global / position. Thus the semantic
mapping to blue/red follows those documented settings; binary GPIO addresses
alone do not reveal physical LED colours.

Critical addresses:

| Path | GPS v1.8-labelled | ERS v1.9-labelled |
| --- | --- | --- |
| VTG parser | 0x08007AAC | 0x08007AAC |
| GGA parser | 0x08007B70 | 0x08007B74 |
| GPS serializer | 0x08007D04 | 0x08007E48 |
| Mode setter | 0x08007D70 | 0x08007EB8 |
| Receive/parse and record-selection path | 0x08007EA8..0x08007F56 | 0x08008070..0x08008166 |
| Initial-data flash source | 0x080173DC | 0x080177AC |

Reset copies 0xC0 initialized bytes to RAM 0x20000000. Initial-data bytes at
source+0x14..+0x22 establish record B as zero coordinates, speed and course,
altitude 1000 and satellites zero. This is direct initializer evidence, not an
assumption about zero-filled BSS. The received-data path explicitly selects B
for Speed. No GNSS rate/configuration command is sent in that mode setter.

With speed zero and six satellites, the Speed record would serialize as:
`00 00 00 00 00 00 00 00 00 00 00 00 03 E8 06`.
This exact six-satellite example is a prediction from the inspected code. Later
direct captures from the user's unit confirmed the same placeholder pattern
with four satellites, as recorded below.
Consequently, a decoder that accepts coordinate 0,0 plus nonzero satellites can
report position availability even though Speed mode is providing no useful
position. The previous stationary 15-byte/availability-flag samples do not
prove equivalent red/blue content. The later passive zero/nonzero checks did
discriminate the modes without retaining private coordinates. Do not universally
reject 0,0 as impossible: it is a real geographic location.

## Fix and speed gating

The GGA parser extracts the standard fix-quality character but the reviewed
position path gates on the GGA satellites-used count. Counts <=3 clear record A
coordinates, reset altitude to 1000 and clear satellites in both records.
Counts >=4 allow position conversion; counts >50 are discarded. This is not an
explicit CRSF fix flag and is not a measurement of satellites in view.

VTG processing copies speed into both records. Values below 1 km/h are coerced
to zero; values above 400 km/h are ignored. These comparison interpretations
follow the linked floating-point comparison wrappers and constants 1.0/400.0.
The live stationary zero-speed samples therefore cannot reveal this behaviour.

## Rates and UTC

Both versions have one transmit scheduler with a 100-tick interval and no mode
branch around that interval (v1.8 0x08007E44, v1.9 0x08007FA0). Additional tracing
establishes a nominal millisecond counter in v1.9: SysTick handler 0x0800E030
calls the increment at 0x0800E1C4, adding the initialized byte 1 at RAM
0x20000028 to the counter at 0x200031A8; getter 0x0800D410 returns that counter.
Timer initialization 0x0800E150 calculates reload as SystemCoreClock divided
by (1000 / tick-increment), then 0x0800EE24 programs SysTick LOAD=reload-1 and
CTRL=7 (core-clock, interrupt and enable). Thus the reviewed v1.9 scheduler is
nominally 100 ms, approximately 10 Hz, in both modes. Its strict greater-than
deadline check and loop delays can make actual intervals slightly longer.
No mode-specific rate change was found in the reviewed v1.8 paths either, but
its clock setup was not separately traced. This is an attempted serial telemetry
transmit cadence, not independent GNSS solution frequency or delivered RF packet
frequency; parsing can regenerate a frame from partly cached values.

The v1.9 image adds an RMC parser at 0x08007D0C which extracts UTC date and time
into a separate structure at RAM 0x2000054C. Its transmit scheduler constructs
a CRSF type 0x03, 9-byte time payload through 0x080064F8 and queues it alongside
GPS telemetry. This path is outside the GPS mode selection, so both modes can
send date/time with that image. It does not populate record B coordinates.
The inspected v1.8 GPS path parses VTG/GGA and has no corresponding RMC/time path.
This matches version-dependent date/time capabilities reported externally.
The later settings query and UTC capture establish the installed v1.9 result
independently of the static analysis.

## Hardware follow-up — direct capture and v1.9 update

Before updating, normal telemetry was captured from the GPS OUT socket at
420000 baud. The user selected blue Speed mode with a fix: 197 full GPS `0x02`
frames in 20.004844 seconds (9.847615 Hz), four satellites throughout, and zero
latitude/longitude/course with encoded altitude 1000 in every frame. In red
Global mode, 197 frames in 20.007650 seconds (9.846234 Hz) had four satellites,
nonzero latitude/longitude and non-placeholder altitude throughout. Both
stationary samples had zero CRC failures and no `0x03` time frames. These
coordinate-free measurements confirm the predicted field distinction on the
actual sensor. They do not establish its old firmware version or independent
GNSS solution rate.

The first updater probes received no UART bytes. After the user replaced the
serial jumper wires and powered up holding BOOT with the lead in IN, the
guarded updater received pure `C` requests at 460800 baud. It transferred the
exact 79,980-byte v1.9 image identified above, with all 79 data blocks
acknowledged, followed by acknowledged transfer termination. This verifies
the guarded CRC/ACK transfer, not a device flash hash readback. No firmware
bytes were modified. The successful attempt needed no additional raw `0x33`
menu command; the previous failures do not establish a firmware defect.

With the lead returned to OUT and normal power restored, a read-only heartbeat
and whole-settings query at 420000 baud returned `Gps Sensor`, ID 0 (the
default/unassigned ID), Global mode value 2, and version tag 18 = little-endian
19, or v1.9. That query had zero CRC errors. No PING/ID assignment or setting
write was sent. R1 discovery/configuration through the receiver remains a
separate check.

The subsequent passive 20.003629-second capture received 199 GPS frames
(9.948195 Hz) and 189 valid UTC `0x03` frames (9.448286 Hz). It reported seven
satellites throughout and non-placeholder position, with UTC advancing from
`2026-11-15T11:32:06.900Z` to `2026-11-15T11:32:26.700Z`. No UTC payloads were
rejected. There were two CRC failures and 23 discarded bytes; this was not an
error-free stream. These are observed serial packet rates, not proof of fresh
GNSS solutions at those rates. Coordinates and raw GPS payloads were not saved.

Post-update blue mode was then captured with the same OUT wiring and power:
198 GPS frames and186 valid UTC frames in20.007928 seconds (9.896077 and
9.296315 Hz). Five to six satellites were reported, yet every position/course
field was zero and encoded altitude was1000. UTC advanced from
`2026-11-15T11:38:31.000Z` to `2026-11-15T11:38:50.900Z`, with no rejected time
payloads. Five CRC candidate failures, one invalid length and42 discarded bytes
were recorded. This confirms the separate speed record and mode-independent
time path on the updated unit; satellite count alone is not a mode discriminator.

Evidence files:

- `/tmp/r1-gps-uart-out-blue-fix-2026-11-15.json`
- `/tmp/r1-gps-uart-out-red-fix-2026-11-15.json`
- `/tmp/r1-gps-v19-new-wires-probe-2026-11-15.log`
- `/tmp/r1-gps-v19-upload-2026-11-15.log`
- `/tmp/r1-gps-uart-version-after-v19-2026-11-15.json`
- `/tmp/r1-gps-uart-after-v19-2026-11-15.json`
- `/tmp/r1-gps-uart-blue-after-v19-2026-11-15.json`

The R1 UTC feature in `8818e6a` was subsequently installed as source `43db14f`;
the user observed the clock on the physical screen through the receiver chain.
A follow-up display-only Sydney/Melbourne timezone/header-order correction is
prepared but still awaiting upload at this record point. Underlying GPS UTC
telemetry and `GPSTIME` remain unchanged.

## Sources and scope

- [Current RadioMaster GPS product page](https://radiomasterrc.com/collections/wt100/products/ers-gps-precision-gps-sensor-for-compatible-expresslrs-pwm-receivers) links the GPS-labelled v1.8 download.
- [Preserved vendor listing](https://rcdrone.top/products/radiomaster-ers-gps-sensor) retains the official RadioMaster CDN link for the ERS-series v1.9 package.
- [Owners' update reports](https://www.rc-network.de/threads/sensoren-von-radiomaster-f%C3%BCr-elrs.12103124/page-10) describe the v1.9 package reporting GPS v1.9 and other sensors reporting different versions. These establish historical use, not which image is recommended today.
- [Current official updater source](https://github.com/radiomaster-rc/RM-Web-Page/blob/main/Connection.html) implements serial entry probes and YMODEM. That is a transport reference, not evidence that the installed ER6G already supports sensor flashing.

The mode/record/serializer findings above are our own static binary analysis.
The v1.8 initializer, mode selection and serializer were independently reviewed.
The hardware follow-up used the unmodified vendor v1.9 image. Correcting R1's
automatic GPS presentation for the confirmed blue-mode placeholders remains
separate implementation work.

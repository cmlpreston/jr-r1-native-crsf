# XIAO ESP32-S3 PCB profile

This profile implements the wiring in `Schematic PDF_[No Variations] (5).pdf`
reviewed on 2026-09-13. Build explicitly:

```sh
.venv/bin/pio run -e seeed_xiao_esp32s3-pcb
```

PlatformIO board: `seeed_xiao_esp32s3`; display: Waveshare RPi LCD (F).
The default `esp32-s3-devkitc-1` target still uses the original prototype wiring.
Existing upload preflight restrictions remain in place: the XIAO is not yet
approved as connected hardware. This change builds firmware; it does not upload.

| Function | XIAO pin | ESP32 GPIO |
|---|---|---:|
| Throttle | D9 | 8 |
| Steering | D8 | 7 |
| Piezo PWM | D6 | 43 |
| LCD DC | D3 | 4 |
| I2C SDA | D0 | 1 |
| I2C SCL | D1 | 2 |
| CRSF half-duplex | D7 | 44 |
| LCD backlight PWM | D2 | 3 |
| LCD SCK | D4 | 5 |
| Battery divider ADC | D10 | 9 |
| LCD MOSI | D5 | 6 |

LCD CS is grounded and MISO unused. The direct reset/IRQ pins are `-1`:
no DevKitC reset/IRQ GPIO is driven by this profile.

| Expander | Pins | Function |
|---|---|---|
| K1 0x20 | GPA0/2 | Throttle trim plus/minus |
| K1 0x20 | GPA1/3 | MODE / PAGE |
| K1 0x20 | GPA4 / GPB6 | Steering trim minus/plus |
| K1 0x20 | GPB0..5 | CLEAR, MINUS, DOWN, PLUS, SELECT, UP |
| K2 0x21 | GPA0/2 | Grip A minus/plus |
| K2 0x21 | GPA1/4 | Timer / drive mode |
| K2 0x21 | GPA3 / GPB5 | Grip B minus/plus |
| K2 0x21 | GPB0 | Touch reset |
| K2 0x21 | GPB1/2 | AUX3 minus/plus |
| K2 0x21 | GPB3 | LCD reset |
| K2 0x21 | GPB4 | GT911 INT/address select |
| K2 0x21 | GPB6 | Idle-up |
| K2 0x21 | GPB7 | RUN LED: reserved output, held off |

The separate legacy display switch is unmapped because it is absent from this
schematic. GPA7/GPB7 are not used as inputs on the PCB. The RUN heartbeat itself
is not implemented in this change.

## Startup and recovery

After starting Wire on GPIO1/2 at 400 kHz, configure both expanders before
initialising touch or the LCD. K1 enables internal pull-ups on all eight keys
(and its trim contacts) before input sampling. K2 enables pull-ups on its input
contacts and INT. Output latches are loaded before output directions, so reset
and LED lines start at deliberate levels.

GT911 reset is asserted on K2 GPB0, with GPB4 driven low. Release reset with
INT low to select 0x5d, wait for startup, then return GPB4 to input with pull-up.
Pulse LCD reset on GPB3 before Arduino_GFX begins. Touch IRQ is read through K2;
GT911 status-register polling remains the primary touch/wake source.

If expander/display/touch startup fails, stop before starting the control task
and keep transmitting safe frames. Fix the hardware and reset to retry startup.
During normal input polling, address-specific snapshots keep K1 and K2 separate.
Missing-chip inputs are marked faulted and sampled as released, never from stale
pressed data. Reconnection restores the last directions/pull-ups/latches without
pulsing the display reset lines or reverting K2 to an all-input device.

Host tests exercise keypad pull-ups and mapping, independent expander failure,
reset/INT configuration, reconnect preservation, and failed reset writes.
Physical reset timing, touch, button operation and CRSF still require bench
verification on the assembled PCB before use.

## Final optimiser pass

The change report supplied with schematic (5) agrees with the final assignments.
Although named `.XLS`, that report is a PDF. Its final pass moved steering,
throttle and battery ADC to D8/D9/D10 respectively; all three are ADC-capable.
D6 (buzzer) and D7 (CRSF) remain fixed digital assignments. The firmware has a
compile-time ADC-capability check to catch accidental future use of D6/D7.
R9's pin reversal and pull-up branch swaps need no firmware changes.

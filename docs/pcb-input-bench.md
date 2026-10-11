# PCB assembly input test

Temporary headless mode is selected by `JR_R1_INPUT_BENCH=1` in
`include/input_bench_mode.h`. Build the usual
`seeed_xiao_esp32s3-pcb-portrait` environment: same schematic (5) pins,
not a legacy display diagnostic. Set that macro to 0 and rebuild/upload to
restore the complete transmitter firmware.

The test does not initialise UART/CRSF, control tasks, LCD, touch, Wi-Fi or model
storage. CRSF stays an input with pulldown. It leaves saved configuration intact.
I2C runs at 100 kHz during assembly tests. K1 and K2 are polled independently;
one failed expander does not prevent testing the other. Inputs use the normal
pullups and debouncing. Missing chips cannot complete a press/release test.

Open USB serial at 115200. Press and release one button at a time. Live lines
show its name, endpoint, event and value. The KEY checklist repeats every ten
seconds. `PRESS+RELEASE OK` means both transitions were observed since boot or
CLEAR; it is not an electrical continuity or long-term reliability guarantee.

| Signal | K1 port |
|---|---|
| KEY_MODE | GPA1 |
| KEY_UP | GPB5 |
| KEY_PLUS | GPB3 |
| KEY_SELECT | GPB4 |
| KEY_PAGE | GPA3 |
| KEY_CLEAR | GPB0 |
| KEY_DOWN | GPB2 |
| KEY_MINUS | GPB1 |

Serial commands (Enter to send):
- `STATUS`: checklist and expander status.
- `INPUTS`: raw K1/K2 GPIO levels; switch inputs are active low.
- `CLEAR`: clear the test checklist without changing saved model settings.
- `ANALOG`: toggle half-second raw throttle/steering/battery ADC samples.
- `HELP`: command reminder.

Trim directions, timer button and maintained switches also produce live events;
only the eight keypad buttons have a completed-test checklist. Unconnected ADCs
float. DISPLAY_SW is unmapped and excluded by the existing PCB input map.

On the currently connected Mac:

```sh
screen /dev/cu.usbmodem2101 115200
```

Exit screen with Ctrl+A, then backslash, then confirm. The device path may change
when moved to another USB port. Power off before soldering further components.

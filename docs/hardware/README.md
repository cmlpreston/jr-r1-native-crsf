# R-1 EVO hardware documents

[Download the revision (5) schematic](r1-evo-schematic-rev5.pdf).

This is the single-page PDF supplied during development as
`Schematic PDF_[No Variations] (5).pdf`, dated 13 September 2026. It is the
reference used for the XIAO/two-MCP23017 replacement PCB documented in
[the current pin map](../xiao-pcb.md). The published PDF is an unchanged copy.

The board retains the original R-1 controls and brings together the XIAO,
eight keypad buttons, two expanders, filtered control inputs, LCD/touch
connections, CRSF module connection and transmitter battery divider.

Read the schematic together with the GPIO table and assembly notes. The XIAO
symbol includes generic/legacy alternate-function labels; use the ESP32 GPIO
numbers in the firmware pin map rather than assuming those labels describe
ESP32 peripheral routing. The battery input `VBAT_SW` and regulated `5V_SYS`
are separate rails. This revision does not include an onboard pack-to-5V buck
regulator. LCD CS is grounded and LCD MISO is not fitted in the installed profile.

This publication includes the schematic PDF, not an editable CAD project,
Gerber package or complete manufacturing release. CAD renders and dated
assembly photographs are in the [project story](../../README.md). Hardware
changes made during assembly must be checked against the actual board.

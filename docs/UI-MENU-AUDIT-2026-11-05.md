# Native menu consistency audit — 5 November 2026

The 4 November cleanup converted selected menus, not all pages. Receiver battery
was missed. Functional navigation/bounds/typography checks did not establish
visual consistency, and the earlier completion claim was too broad.

## Vehicle battery correction

Receiver battery and Drive pack now share `drawVehicleBatteryContent`, the same
native page scaffold, Battery / Warnings tabs, and shared settings rows. Both
use Chemistry, Cell count, Low voltage warning, Voltage threshold and On/Off.
Voltages display in V with two decimals. Drive adds capacity (mAh), Lowest-cell
warning and Cell threshold. Receiver help identifies telemetry/BEC threshold
configuration; Drive help distinguishes warnings from ESC cutoff.

The old receiver-specific boxed renderer, separate portrait layout and
coordinate-based settings callback were removed. Existing storage formats,
limits, thresholds, chemistry defaults, warning enables and warning cadence
remain unchanged. Both pages return to Vehicle batteries; the existing direct
sensor route back from Drive pack is retained.

## Setup conversion follow-up — 5 November 2026

On user request, all setup pages in the remaining inventory now use the common
`drawDesignPage` scaffold and shared `drawMenuRow` / `drawSettingsRow` /
`drawPageTabs` builders through `transmitter_ui_setup_pages.inc`.

| Pages | Current implementation |
| --- | --- |
| Model / Handset roots; Display; Sounds; TX battery; Network; System | Common scaffold and shared rows |
| Receiver battery; Drive pack | Common battery renderer, tabs and rows |
| Control map; assignment editor; Inputs; colour schemes; RF module | Common scaffold and rows (4 November cleanup) |
| Channel roles; throttle detail; output ramp | Shared settings rows and semantic callbacks; old boxed rows removed |
| Response curve and settings | Common Curve / positive / negative tabs; full-width live curve, settings in shared rows |
| Firmware information | Shared read-only rows; long metadata wraps in portrait |
| ELRS binding | Shared settings/action rows; phrase keyboard uses common buttons inside the common scaffold |
| ESC programmer | Shared tabs/rows, guarded CH12 handover and per-setting drafts; RF-only bench ER4 has electrical access and writes disabled |
| Calibration | Common scaffold, Cancel and read-only live rows; acquisition, neutral hold and cancellation logic retained |

The old separate portrait detail file and obsolete setup row/toggle builders
have been removed. Specialized content (curve plotting, keyboard keys and live
calibration data) remains within the common page system. Response/ESC/battery
tabs support PAGE and swipes; focused numeric settings use the common +/- path.

The live RF diagnostic screen belongs to Data and retains its telemetry cards;
it is not an outstanding setup menu conversion. Boot/PIN and dashboard widgets
also have intentional specialized presentations. This is not a claim that all
screens in the firmware have identical content layouts.

## Verification

Focused battery audit exercises actual row targets and both tabs for receiver
and drive, both orientations, persistence, Back, bounds, and unchanged arm/CUT
request state. It is also included in the full UI audit. Optional unlocked,
disposable-simulator captures cover the remaining legacy paths above.

Logs: `/tmp/r1-battery-menu-{build,focused,tests,firmware,full,inventory}.log`.
Battery captures: `/tmp/r1-battery-menu-{landscape,portrait}-{receiver,drive}-{battery,warnings}.bmp`.
Legacy captures: `/tmp/r1-menu-inventory-{landscape,portrait}-*.bmp`.

Installed on user request via guarded Pi USB upload on 5 November 2026.
Record `/tmp/r1-flash-rrvk_nqi`; boot #635 passed inputs/touch/display/UI,
and INPUTMON confirmed CUT. See the handoff for full upload evidence.

## Setup-conversion verification

The new focused audit runs real touch targets in both orientations, checks
Back routes and bounds, verifies the live input indicator through the normal
update loop, checks CUT on configuration edits, exercises binding keyboard /
receiver-link / blank-phrase guards, verifies the uncommissioned ESC CUT guard and safely
cancels calibration. The full UI audit includes it.

Screenshots for all converted pages in both orientations are generated with
`JR_R1_SETUP_DETAILS_IMAGES=/tmp/r1-setup-details`; logs are
`/tmp/r1-setup-conversion-{build,focused,tests,firmware,full}.log`.
This setup-conversion follow-up has not been flashed.

Both simulator builds, focused touch/keyboard/swipe/persistence/safety checks,
host tests, full UI/rotation/typography regression and approved XIAO landscape
build passed. Screenshot review also caught and corrected transient heading
text corruption from DOT layout. The normal update-loop throttle-input test
caught a dispatch regression during migration; that was corrected and the full
regression passed on rerun. Final menu-bar V2 badge removal was rechecked in
both dimensions and in firmware compilation.

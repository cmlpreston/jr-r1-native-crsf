# Team Race telemetry-slot fix — review candidate, 6 November 2026

## Bench evidence

With ER6G off, ER4 r3 returned nine consecutive Hello/keepalive acknowledgements
(seq5–13) at LQ100 with no retries. R1 stayed CUT, CH5=992 and CH12=1811 while
selected. Both-powered tests had accepted requests and queued ER4 replies but
unreliable return delivery. No ESC electrical access or writes occurred.

ER6G was restored after a double-blink binding indication cleared with one
ordinary power cycle. R1 confirmed RM ER6-G identity and LQ100, lowered CH12 and
cleared the programmer interlock. Team Race readback after the power cycle is
AUX8/CH12, position1/Low. Temporary commissioning acknowledgement was cleared.

## Upstream finding and proposed change

[ExpressLRS PR3819](https://github.com/ExpressLRS/ExpressLRS/pull/3819), commit
`e0f3ad1ce621420bde5dc8f5a40672f9599f4465`, describes a silent Team Race receiver
hearing the selected receiver's telemetry and decoding it as RC data. It can
therefore become active briefly. This closely matches the isolation result;
paired hardware verification of the fix remains pending.

The proposed ER6G image is the clean pinned official 4.1.0 source
`a9d4a9cb5b5687c4c9d7e9e7fbdf44ad93651da6` plus the exact upstream 11-line
RXdoneISR guard. Connected receivers ignore packets arriving in their telemetry
slots; acquisition/binding and telemetry-off behaviour remain unchanged. Build
version metadata identifies the candidate as `4.1.0-teamrace-slot-fix`. This is a
local backport of an open PR, not an official release binary.

No S.Port experiment is included. PWM, UART/ERS, binding, failsafe and radio
implementations remain the pinned stock baseline. The matching ER4 r4 candidate
adds the same receive-slot guard to the RF-only bridge; ESC interface/writes and
normal ER4 PWM/serial outputs remain disabled.

## Validation and prepared artifacts

- ExpressLRS native suite: 94/94 tests passed.
- R1 host/guard/integration suite passed, including receiver identity, source,
  image, recovery, binding, mapping, board and RF-domain rejection checks.
- Actual prepared RXdoneISR tested across all256 nonces, eight telemetry ratios,
  connected/acquisition states, duplicate handling and timer guards. The same
  test fails on the unpatched handler (negative control).
- ER6G ESP32 and ER4 ESP8285 firmware builds passed. Neither candidate uploaded.
- ER6G source diff: `/tmp/r1-er6g-teamrace-slot-2026-11-06/artifacts/rx-slot.diff`.
- ER6G generic candidate: `/tmp/r1-er6g-teamrace-slot-2026-11-06/artifacts/`.
- ER4 r4 generic candidate: `/tmp/r1-er4-rf-r4/artifacts/`.
- Logs: `/tmp/r1-er6g-teamrace-slot-build.log`,
  `/tmp/r1-er6g-teamrace-native-tests.log`,
  `/tmp/r1-teamrace-final-host-tests.log`.

Fresh ER6G recovery snapshot and a separate private candidate preserving UID and
Wi-Fi are saved under ignored, permission-restricted `tmp/` directories. These
contain credentials and must not be published. The offline upload guard passes.
The upload-only ER6G PlatformIO profile checks the live stock4.1.0 identity,
SX128X/ISM2G4 domain, GPIO[14,1,3,15,2,4], current binding/mapping/configuration,
source/patch hashes, image and exact recovery snapshot. No force/unsafe override.

## Upload hold

Current installed hardware remains R1 temporary startup boot642, ER4 r3 and
stock ER6G4.1.0. Both receivers are back on RF. R1 stays CUT. The ER6G candidate
requires an explicit upload request for the identified ER6G, and telemetry/servo
leads disconnected with receiver power retained, before any upload command.
The receiver safeguards are recorded in
`/Users/pre126/Documents/Codex/ExpressLRS-SPort/AGENTS.md`.

## Authorized ER6G upload and readback — 6 November

The user subsequently explicitly requested the ER6G upload, confirmed only
receiver power connected (servo/telemetry leads removed, drive pack disconnected),
and put it into Wi-Fi mode. The guarded PlatformIO upload succeeded. Installed
version is now `4.1.0-teamrace-slot-fix`, target `UNIFIED_ESP32_2400_RX`,
SX128X/ISM2G4. Readback matches every packaged image byte; all saved config and
hardware JSON match the preflash backup exactly. The download endpoint includes
unused bytes beyond the packaged image, so its whole-file hash is not the image
hash. Private verification evidence is retained with the deployment artifact.
Reboot to RF acknowledged HTTP200. ER4 is still r3: the r4 candidate remains
unflashed, and paired handover validation remains pending. This supersedes the
ER6G upload hold above. Further uploads require fresh current-version backup and
guard review; the current guard deliberately expects the preflash stock image.

Post-reboot R1 is CUT, Idle/interlock0, selector CH12low. Its module link is
manually paused; RF reacquisition requires normal physical module-enable and
has not yet been verified.

## Authorized matching ER4 r4 upload — 6 November

User authorized ER4 update and Wi-Fi mode. Fresh r3 recovery snapshot saved,
guard updated to that snapshot while retaining original recovery protection.
Host suite and actual ISR tests passed. Guarded PlatformIO ER4 ESP8285 Wi-Fi
upload succeeded. Postflash version `4.1.0-er4-esc-bench-r4` and expected custom
ER4 target verified; every packaged image byte matches readback. Config,
binding and hardware preserved exactly. Reboot to RF acknowledged HTTP200.
Both receivers now have the receive-slot filter; paired hardware handover testing
remains pending. R1 module is manually paused and needs normal physical enable.
ESC interface/writes and normal ER4 outputs remain disabled. Private evidence:
`tmp/er4-private-deploy-2026-11-06-rf-r4/verification.json`.

## Paired RF validation passed — 6 November

With both updated receivers powered, module enabled, R1 CUT and drive pack
disconnected, three Begin/Finish cycles passed. Across the sessions, all31
Hello/keepalive requests were acknowledged with no retransmissions. Each Finish
restored ER6G identity, selectorlow, Idle/interlock0 and100LQ. CUT and neutralCH5
were maintained throughout. Logs:
`/tmp/r1-paired-slot-fix-handover-2026-11-06.log` and
`/tmp/r1-paired-slot-fix-repeat-2026-11-06.log`.
Brief selection LQ dips recovered; no timeout or Fault. Temporary commissioning
acknowledgement cleared. This supersedes paired-validation-pending notes above;
it validates RF handover only. ESC electrical access/writes remain disabled and
physical output/failsafe commissioning remains outstanding.

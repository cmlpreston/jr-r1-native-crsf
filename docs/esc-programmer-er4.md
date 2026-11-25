# Surpass ESC programming through a dedicated ER4

## 8 November: user defers installation; r11/r6 diagnostic candidate BUILT/TESTED ONLY

User asked whether RXcanbeoffwhilefirmwareprepared; saidyes, switchoffESC/drive
packsoESCandRXbothoff; evidencealreadycaptured, sessionmayend. Userexplicitly:
“we can install the firmware when I come back”. NOinstallationorhardwaretesting
untiluserreturns. Actualpoweroffnotobserved; don'tassumesessionstillalive.
Installedremainsr10/r5, R1unchangedFault/CUT, TXmodulepausedforRXWiFi.
AllSaleaecapturesSTOPPED/saved; noactivehostjobs/no newpoweredrequests.

RetainedWiFi/configdiagnostics pinpointsmini result4(ReadFailed), bootValue255,
starterReadyUs53236/elapsedUs1867535, validr5peer/currentbank andhandoffaccepted.
FullappcorrectlykeepsGPIO2reservedandInterfacebitOFF. Capture6wireprovessuccessful
ESChello/read0reply1 evenwhilemini softwarefailedverification. Stillnotknownwhy
mini missed/invalidatedreply; doNOTclaimhandofffailed orpinmatrixcause.

OnefullWiFi read0diagnosticrequestabortedearly: received202queued, fulltxStarted1
buttxCompleted0/echo0/bytes0; status “RF session ended; pin stays reserved until
power cycle”. Capture7STOPPED/saved/analyzer10032, onwireonlytruncatedFF, NOTa
complete1C0000. SourceconfirmedEr6gEscModeLoop unconditionallyabortedreader
whennotRFlinked, includingWiFimode. ThisistheconfirmedseparateWiFi-readbug.
NoESCwrites. Durableevidencecopiedunderignored
 tmp/er6g-r10-read-verification-2026-11-08/
(capture6/7sal/raw/decoded/summary andfilteredreceiverdiagnostics).

Preparedfull4.1.0-er6g-esc-mode-r11 + matchingmini er6g-startup-r6:
- RFmodeguardabortsonlyRF-ownedreading transactiononlinkloss, preservingexplicit
  WiFibenchreads. Reader'sownmode/busy/header/motor-cardconfirmationgatesremain.
- Mini recordsreceived-bytecount,last4byteshex,andreadProgress (0notstarted,
  1helloattempt,2hello/echoaccepted,3readsent/echoaccepted,4replyheaderseen,
  5replypairreceived) andrawreplyvalueevenoninvalidvalue. Normalprotocoltiming,
  one-shotselection/releaseorder unchangedfromr10/r5; no guessedGPIOfix.
- Sealedhandoffschema2/48bytes(previousschema1/36), includesdiagnosticfieldsin
  checksum; fullpeer exposesstarterRxCount/starterRxTail/starterReadProgress.
  Exactpairedapphash/SWreset/currentbank/freshnessvalidationretained. Oldschema
  cannotbeusedasproof. R1interfaceprotocolunchanged; noR1flashneeded.
- r10releasednormalstartup/PWMfixretained. Settingwritesdisabled everywhere.

FullhostsuitePASS /tmp/er6g-r11-final-verified-host-2026-11-08.log, including
WiFibenchpreservedvsRFlinklossabort andtraceCRC/retainedfields/startupsuccess/
no-replyprogress tests. ActualpreparedPWM/allocator/RXISRtestsPASSduringbuild.
Minir6buildPASS2.02s /tmp/er6g-startup-r6-build-2026-11-08.log, artifacts
/tmp/r1-er6g-startup-r6-artifacts-2026-11-08/.
Fullr11buildPASS22.858s /tmp/er6g-read-diagnostics-r11-build-2026-11-08.log;
/tmp/r1-er6g-read-diagnostics-r11-2026-11-08/artifacts/; finalsource/imagehashes
matchcurrentcheckout. Genericbin/manifestdurablecopy
 tmp/er6g-r11-generic-artifacts-2026-11-08/ (noUID/WiFicredentials).
No privateuploadimagescreated; nofirmwareuploads/commits/pushes/deployments.

NextinstallationONLYwhenuserreturnsandrequests: ER6Gpoweronly/WiFi, ESCoff,
allESC/analyser/servo/telemetryleadsunplugged, separateRXpackforupload. Updated
modeguards nowexpectinstalledr10/r5Drive/peer1/armed0 andfreshprivatebackup
 tmp/er6g-private-backup-2026-11-08-mode-r10-paired/;
fullr11 mustverifyexactbytes/Drive/peer0/armed0 andfreshfullsnapshot
 tmp/er6g-private-backup-2026-11-08-mode-r11/
beforematchingr6stageupload. UseguardedPIOmodeenvironments only; noboottable/
bootloaderwrites orforce. Rebuildfrompinnedcleansourcesif/tmpbuilddirsremoved.
Afterverifiedpair, checkedsoleBECwiring/motoroff/cardoff/CUT; freshProgramack+
Saleaeonepowercycle, comparewirevsnewminiRXtrace. WiFireadcanthenbeattempted
withoutRFguardabortifneeded, butdon'tclaimstartupverificationfixedbydiagnostics.


## 8 November: released-startup r10/r5 pair INSTALLED + VERIFIED

User explicitly authorized ER6Gflash, WiFimode andnothingelseconnected; normal
power-only readiness confirmed. Freshorigin/mainfetch/status/ancestryPASS,
codex/r1-v2/3924d2b. Re-ranfullhostsuitePASS
/tmp/er6g-r10-pre-upload-tests-2026-11-08.log, actualpreparedservo/allocator/ISR
regressionsPASS andgenericartifactsourcehashesmatchedfinalbuild. No newsource
edits thisuploadturn. Freshr9/r4DrivebackupverifiedbeforeprivateUID-preserved
packaging. GuardedPIO er6g-esc-mode-wifi fullr10 uploadSUCCESS24.931s,
/tmp/er6g-r10-upload-2026-11-08.log. Exactallimagebytesreadbackmatched; settings,
hardwaremap, WiFi15s preserved; bootpinreleased/high, noUARTTX, nopeer/armed0
(expectedbeforestage). Savedfreshfullr10recoverybeforesecondappupload.

GuardedPIO er6g-esc-mode-startup-wifi r5 uploadSUCCESS17.182s,
/tmp/er6g-startup-r5-upload-2026-11-08.log. Returnedtofullr10WiFi; exactfullimage
readbackmatches, r5peerSHAverified, starterArmed0/defaultDrive, noUARTTX,
pinreleased/high, configuration/hardware/WiFi15spreserved. This verifiespower-only
software recovery, NOT yetconnectedESCwaveform/BECpersistence ornewprogramentry.
No bootloader/partitionwrites, forceorwrite retries. R1 unchanged/staysCUT; no
ESCsettingswrites ornewPreparecommand; allSaleaecapturesstopped.

Privatefiles (donotprintUID/WiFi):
 tmp/er6g-private-backup-2026-11-08-mode-r9-paired/
 tmp/er6g-private-backup-2026-11-08-mode-r10/
 tmp/er6g-private-deploy-2026-11-08-mode-r10/
 tmp/er6g-private-deploy-2026-11-08-mode-startup-r5/
full-verification.json/paired-verification.json andpairedconfig/hardware/options/
firmwaredownload inprivatefullfolder. FullSHA b5025df6a3be8d5b699b54ed57b13e8ae78a9cd32aa43ab830fe890d132ac07e
SmallSHA 4a8a64829d8ff5d18e05cbb5b53fbe5a53c6f4c27417b311fc0a178fb5fbe6b5
Currentfullrunningbank 2031616, otherbank 65536.
Migrationguards nowrejectreuploadagainstcompletedpair untilreviewednextmigration.

Next userALLpowerOFF, removeseparateRXpack, reconnectCHECKED/undamagedESClead
redBEC/signal/GNDoutput5, motor/cardoff; SaleaeCH0signal/GNDcommonground ifusing.
Userpreviouslycheckedwiringanddeclinedsignalvoltagemeasurement despiteoriginal
Logic5.25V normalinputlimit; donotclaimverifiedvoltagecompatibility. LeaveESCoff,
confirmwiredready, thenstartSaleaeBEFOREonepoweron. No newprogrammingrequest:
firsttestnormalDrive waveformshouldlackprior1.869sLOW andBECshouldremain~6.2V
(item15=1 setwithoriginalcard). Do notinfercauseofpriorharnesssmokeorBECsetting
changefromsuccessfulflash. No commits/pushes/deployments. Noactivehostjobs.


## 8 November: released-startup r10/r5 candidate BUILT/TESTED, NOT FLASHED

User “Ok” authorized addressing observed~1.869s normalGPIO2LOW. Implemented
receiver-only candidate full4.1.0-er6g-esc-mode-r10 + matching mini er6g-startup-r5.
No live hardware commands/uploads duringthischange. Installed REMAINSr9/r4;
ESCsignal disconnected, power-onlyRXonebeep/freshRX6.1V fromprecedingtest.
Actualcause of smokedharness/unexplainedBECsettingchange stillUNPROVEN.

Normal fullboot now explicitly releasesGPIO2 toINPUT+pullup with outputdisabled
BEFORElatch/matrixchanges. NormalServoPWM(50..400Hz) initialize leavesoutput5
INPUT_PULLUP, noOUTPUTLOW. It doesnotallocate/attachPWM onconnectionevent or
pre-inputfailsafe; only usablechanneldata fromconnected/modelmatchedpacket
allowsfirstoutput. Allocationandfirstpulse happeninsamereceiverwritecall.
Other5 outputs retainexistinginitialization. Configuredpositional/no-pulse/link/
model/timeoutfailsafe afterfirstPWM remainsstock; WiFicleanupreleasesGPIO2.
Programmingboot stillownsGPIO2; noPWMstealsit. UARTreleaseorder alsodisables
outputbeforelatchchange; one-shot/timing/readprotocol logicunchanged.

Added drive_pwm_prepare.py applied ONLY toselectable-mode disposableupstream.
Review foundobsoleteTeamRacegate stillinservosUpdate (main/RFhadalreadybypassed
it); removedthatonegate forsingleRX, retainsconnection/model checks. Toavoid
lazyallocationresettinganotherservo'ssharedMCPWMtimer, newPWM.allocateLedc API
usesLEDC-only forESC, leaveslegacyallocate/MCPWM pathunchanged. LEDCinitialduty
ispreloadedfromfirstrequestedus atattachment, avoidingintermediatezero-duty
initialization. Rootactualreaderpin testupdated toassertreleasedGPIO/noUART.
NoR1source/UIupdate neededforthisreceiverchange.

Build includesmandatoryactualpreparedsource tests: updated
receiver/er6g/test_listen_reservation.py exercisesactualservoinitialize/event/
update/write functions; testsreleasedstartup, noinput/unsetchannel/modelmismatch,
allocationfailure/retry, 50..400Hz, stale/LQ/link/model/positional/no-pulse
failsafe, WiFirelease, programmingreservation andother5outputs. New
receiver/er6g/test_pwm_allocator.py exercisesactualESP32allocator/microseconds
functions: ESCLEDCfirstduty matches1500us, noMCPWMinit/reset/fallback, other
LEDCchannels/timerspreserved, exhaustionfailure. RXISRregressionpasses.
FullhostsuitePASS /tmp/er6g-released-startup-final-checks-2026-11-08.log.
Smallr5buildPASS2.18s /tmp/er6g-startup-r5-build.log; artifacts
/tmp/r1-er6g-startup-r5-artifacts-2026-11-08/.
FINALfullr10buildPASS24.314s /tmp/er6g-released-startup-r10-final-build.log;
/tmp/r1-er6g-released-startup-r10-final-2026-11-08/artifacts/.
Manifestsource/imagehashesmatchcurrentsources. Earliernon-finalr10folderis
superseded(firstpulseinitializationrefinedafterit). NoGUItestneededreceiveronly.

Reviewednextpairedmigrationguards (23guardtestsPASS) nowtargetr10/r5 fromr9/r4,
oldfullDrive/Peer1/Armed0 exactbanks, freshbackuprequired
 tmp/er6g-private-backup-2026-11-08-mode-r9-paired/.
FullnewappmustbyteverifyDrive/Peer0/Armed0 andfreshsnapshot
 tmp/er6g-private-backup-2026-11-08-mode-r10/
beforematchingr5stageupload. Guardsstillrejectwrongtargets/forcedoroldprofiles.
No privateuploadimages/freshlivebackupscreatedyet; RXnotWiFi anduserhasNOT
explicitlyrequestedthisupload. BeforefutureER6Gflashgetidentifiedauthorization
andusualESCoff/allESCservo/sensor/analyserharnessdisconnected/RXpackpoweronly/
WiFireadiness. UseguardedPIOmodeenvironments,nobootloader/partitionchanges.
AfterpairverifiedneedsactualsignalwaveformcapturewithmotorleadsOFF, cardOFF,
checkedharness, nosecondRXpackwhenBECrestored; confirmBECsetting1/voltage~6.2V
persists. SaleaeoriginalLogicinputvoltagecompatibilityhasNOTbeenmeasured;
userpreviouslydeclinedandacceptedpriorworkinguse. DoNOTclaimnewfixprovescause
orphysicaloperation; nofurtherpowerswitchesrequestedthisturn. No commits/pushes.


## 7 November: selectable r9/r4 ER6G pair INSTALLED; matching R1 transport fix FLASHED

User explicitly reported ER6G ready to flash, continuing the established
power-only/Wi-Fi upload procedure. Fresh origin/main fetch/ancestry/status passed
on codex/r1-v2 / 3924d2b. Verified pinned clean upstream a9d4a9, complete host
suite, exact-source PWM ownership and RX ISR tests; added 23 migration guard
checks including source/hash/recovery/target/slot/peer rejection cases.

New dedicated guarded PIO environments er6g-esc-mode-wifi and
er6g-esc-mode-startup-wifi use tools/er6g_mode_upload_preflight.py. Full migration
requires installed r8c/r3 pair + fresh private recovery; stage migration requires
exact byte-verified full r9 Drive app + another fresh recovery. No force uploads
or bootloader/partition writes. Stage peer hash is checked on ordinary full-app
Drive boot too, allowing post-install verification without preparing programming.

First full OTA was REJECTED for environment-generated target name
R1_ER6G_ESC_MODE_BUILD. Receiver remained r8c. Fixed build-only env to
Unified_ESP32_2400_RX_via_R1_ESC_MODE, which both classifies it RX and preserves
canonical UNIFIED_ESP32_2400_RX target identity. New guard tests reject incorrect
target markers. Next attempt hit Not Enough Space because ELRS leaves the
rejected transaction open. Inspected source, confirmed old full app still running,
explicitly cancelled that stale transaction via action=cancel (Update.abort, NOT
force confirmation), then normal guarded OTA succeeded. No blind write retries.

Final full r9 build /tmp/r1-er6g-esc-mode-r9-targetfix-2026-11-07/ PASS25.467s.
Full upload SUCCESS38.931s /tmp/er6g-mode-r9-clean-session-upload.log. Byte readback,
configuration and hardware preserved; Wi-Fi15s, Drive, zero UART TX. Saved fresh
full r9 recovery snapshot before stage. Small r4 upload SUCCESS30.404s,
/tmp/er6g-mode-startup-r4-upload.log. Post-stage returned to exact full r9 image,
startup peer SHA verified (starterPeerReady1), starterArmed0, default Drive,
zero UART TX/writes disabled, all configuration/hardware unchanged. Current full
running0x10000; small r4 in0x1f0000; next boot full r9 unless explicitly prepared.
Small stage automatic no-ticket bypass worked; programming entry timing with its
new NVS work is still UNTESTED on the powered ESC.

Private recovery/config artifacts (do not print UID/Wi-Fi):
 tmp/er6g-private-backup-2026-11-07-read-r8c-paired/
 tmp/er6g-private-backup-2026-11-07-mode-r9/
 tmp/er6g-private-deploy-2026-11-07-mode-r9-targetfix/
 tmp/er6g-private-deploy-2026-11-07-mode-startup-r4/
Paired verification JSON and all image bytes retained. Earlier failed generic
r9 candidates under /tmp/*install* or *verified* have the wrong target name;
use ONLY *targetfix* artifacts for this installed image. Migration guards now
intentionally reject another upload against the installed completed pair; a
future update needs an explicitly reviewed new installed-state migration.

During upload review found src/main.cpp still vetoed ESC commands unless CH12
was high, despite the new single-RX client no longer selecting CH12. Removed that
obsolete check; independent stale-control/CUT/non-live checks remain in actual
RF task through include/esc_rf_policy.h and host tests. Matching R1 follow-up
flash succeeded after full build/host suite/UI audit, XIAO/Waveshare F/schematic5
480x320 landscape, PIO seeed_xiao_esp32s3-pcb, 34.867s/hash verified. Record
/tmp/r1-flash-aosp14np; log/tmp/r1-esc-single-rx-transport-pi-flash.log. USB status
confirmed safety=CUT and single ER6G UI after restart. Bench startup bypass still
on; no arm command or ESC programming command sent. No ER4 upload.

Next: with all power off, reconnect ESC/BEC normal lead to ER6G output5, keep
programming card and motor leads disconnected, remove separate receiver pack,
optional Saleae CH0/GND shared signal. Bench-test default Drive/PWM, R1 Program
then sole-BEC power cycle, verified entry/read all15, R1 Drive then power cycle
and PWM return. Confirm RF link enabled and CUT throughout. No actual motor,
new-stage programming timing, live RF mode transition or PWM waveform validated
yet. Setting writes remain disabled. No commits/pushes/deployments.


## 7 November: selectable single-ER6G Drive / programming candidate (NOT INSTALLED)

Implemented the next development step after the single-BEC read success. The R1
ESC programmer now selects Program / Read ESC / Drive on one ER6G. It no longer
uses the ER4, reserves CH12, or exposes setting edits. All setting writes remain
disabled. An unsafe Program tap only requests CUT; a second tap after confirmed
CUT starts mode selection. Calibration and arming are excluded.

Receiver candidate is `4.1.0-er6g-esc-mode-r9` with `er6g-startup-r4`. Normal
full-app power-on leaves output 5 / GPIO2 to the existing ELRS PWM implementation,
including configured channel mapping and failsafe. No UART/GPIO bench setup is
run in Drive. Single-receiver integration bypasses the obsolete Team Race
selection checks, retaining RF/model-match checks and the RX-slot ISR fix.

A CUT-gated RF Prepare command saves a one-shot NVS ticket and selects the exact
hashed small OTA peer for the next boot, without restarting or taking PWM away
in the current Drive boot. User cycles ESC/BEC power. The small app consumes the
ticket, durably locks programming ownership, then runs the proven read-only
40 ms quiet / hello / read0 sequence. It returns to the matching full app, which
accepts only a fresh sealed SW-reset handoff and allows RF reads only after a
verified startup read. An absent/corrupt ticket sends no serial commands.

The programming GPIO lock survives software, watchdog and external resets and
link loss; only a fresh POWERON clears it for normal Drive. A failed/missing
handoff keeps the pin reserved. Drive cancels any next-entry ticket, selects the
full bank, and leaves the live programming pin reserved until power is cycled.
The handset remains CUT until an RF response confirms Drive with no pending
programming request. Timeouts/link loss never silently re-arm; no ESC read or
setting write is retried automatically. Reads are blocked on an ordinary Drive
boot, and writes are always rejected by both client and receiver.

Sources: include/esc_drive_programmer.h; receiver/er6g/reader/Er6gEscMode.*;
startup/peer/reader sources; listen_prepare.py integration. Build explicitly with
`receiver/er6g/build.py --esc-drive-mode` (old --esc-read-only is retired).
Generic artifact mode is esc-drive-mode, filename er6g-esc-drive-mode.bin.

Validation: complete host suite; new actual-source RF and GPIO ownership tests;
startup one-shot ticket/no-ticket/storage failure; peer lock/cold/watchdog reset;
client mode transitions/read interruption/Drive acknowledgement; actual prepared
PWM init/write/event tests in both reserved and normal modes; actual RX ISR
regression; landscape XIAO firmware and both-orientation simulator builds and
focused native touch/typography audits. Logs /tmp/r1-esc-mode-host.log,
/tmp/r1-esc-mode-safe-entry-r1-build.log, /tmp/r1-esc-mode-final-ui-audit.log,
/tmp/r1-esc-mode-details-audit.log. Final receiver artifacts:
/tmp/r1-er6g-esc-mode-r9-verified-2026-11-07/artifacts/; small stage artifacts:
/tmp/r1-er6g-startup-r4-artifacts-2026-11-07/. No live hardware commands or
uploads occurred during this implementation.

Next: explicit ER6G upload authorization + fresh power-only/Wi-Fi readiness,
review/extend the exact paired OTA migration guards (currently intentionally
accept only the previously reviewed r8c/r3 pair and reject this new mode), save a
fresh private recovery snapshot, install/verify both new apps, then flash R1 and
bench-test Program/power-cycle/all15 reads/Drive/power-cycle/PWM and interruption
with motor leads disconnected. NVS lookup adds startup work, so the new stage
still needs a physical Saleae timing check; do not assume the earlier r3 timing
proves r4. Installed firmware REMAINS r8c/r3 automatic-entry bench firmware with
PWM disabled. No normal-driving hardware validation yet. No commits or pushes.


Status: R1 programmer build and RF-only ER4 firmware installed on 5 November
2026. End-to-end RF handover/failsafe and ESC electrical access are not yet
bench-commissioned. User approved
an additional ER4 with custom firmware on 5 November 2026. CH12 replaces the
initial CH16 suggestion to retain the current Hybrid/Wide radio mode.

## Architecture and channel ownership

R1 -> Ranger Nano -> selected receiver:

- CH12 low: stock ER6G, normal vehicle controls and ERS telemetry.
- CH12 high: custom ER4, Surpass configuration request/reply.
- Both receivers share the binding phrase and compatible ELRS major version,
  packet rate, switch mode and model-match settings.
- Configure **ER6G Team Race: Channel AUX8 (CH12), Position 1/Low**.
- Custom ER4 selection is compiled as CH12 high, with an independent RF-send
  gate. Its PWM/normal serial drivers are disabled: do not attach servos to it.
- CH12 is reserved by the R1 control engine. Its requested AND CUT-safe value
  are set after mixes, response, reversal and trim. It cannot become a neutral
  midpoint or a stale user binding when outputs are CUT. The R1 refuses to
  start a session if CH12 has a role, binding or destination mix.
- CH12 is transmitted in Hybrid/Wide and Full Resolution 12/16-channel modes.
  Full Resolution 8-channel mode cannot select the programmer. The bridge
  advertises capability only for a supported OTA mode. No RF setting is changed
  automatically. CH16 was rejected because it normally carries RSSI outside
  Full Resolution 16-channel mode.

Use current ELRS documentation as well as the source pinned by the build:
https://www.expresslrs.org/software/teamracing/
https://www.expresslrs.org/software/switch-config/

## Receiver configuration access

The shared ELRS parameter page now supports both TX (EE) and RX (EC), with
separate reply-origin checks and one outstanding request. Setup -> Vehicle ->
ESC settings -> Link -> ER6G settings opens the receiver page while outputs
are inhibited. Numeric output/input mapping fields and selection values use
one write followed by readback; refresh retains the selected device. Saved enum
values outside current advertised options are displayed as unlisted and retained
without automatic writes (ER6G secondary protocol12 is one known case). Apply
still requires an advertised, nonempty choice. Back from
the receiver root returns to the ESC menu. ERS polling pauses on this page.
Authenticated `/api/rf-config` exposes the same client, with normal boot unlock,
CUT/configuration-page gates, token checks and revision matching for changes.
It provides no raw CRSF injection or PIN bypass.

## R1 operation

Setup -> Vehicle -> ESC settings now shows Link / Drive / Brake / Motor / Limits.
All fifteen choices follow the measured card order, not numerical sorting.
Unknown values display `--`; stored preview examples are removed.

1. Configure and physically verify the receiver pair, CH12 transport, and the
   ER6G throttle failsafe. When deselected, Team Race invokes receiver failsafe;
   CUT alone does not certify the receiver/ESC output. Failsafe must be neutral
   at the actual ESC signal, including its selected mode and absolute position.
2. Enter CUT. On Link, mark **Pair / failsafe commissioned** for this boot.
   This is an operator commissioning acknowledgement, not an automatic test.
3. Begin requests CUT again, selects CH12 high and waits at least 750 ms before
   the bridge handshake. A fresh session nonce/sequence identifies replies.
4. The RF-only bench ER4 reports **electrical interface locked**. It does not
   touch the data pins, read ESC settings or allow writes.
5. With a separately validated interface build, Read ESC scans all fifteen
   settings. Tap a setting, edit its draft and Apply one. Each write is sent
   once; the bridge requires the observed ACK AND matching readback before
   reporting success. Invalid values are rejected at both ends.
6. Finish selects CH12 low, waits 750 ms and probes for ER6G device identity.
   ER4/link-statistics packets do not confirm the return. Navigation away,
   timeout or lost module link also select low. A failed return stays CUT and
   blocks arming until Finish succeeds. No automatic re-arm occurs.

The arming veto is in both UI and control engine. The CRSF task also checks
actual CUT, current control freshness and matching CH12 selection before
sending an ESC request. The management queue drops stale/competing UI commands
while a programmer session owns it. ERS requests and TX configuration polling
are suspended. A write already delivered to the ESC cannot be undone by Cancel;
read settings again after an uncertain outcome. No automatic write retries.

## Custom wire transport

This is a new R1/ER4 bridge envelope, **not part of the measured Surpass bus**:

- CRSF PARAMETER_WRITE 0x2D, destination EC, origin EA, reserved index F0.
- Payload after addresses: `F0 53 50 01 op seqLE16 sessionLE32 parameter value`.
- Replies: PARAMETER_SETTINGS_ENTRY 0x2B, destination EA, origin EC, same body,
  then status and capability flags. Normal CRSF CRC applies.
- Single-flight Hello / Read / Write / Close. Hello alone may be resent twice at
  700ms with identical session/sequence; original reply timeout is unchanged.
  Read/Write are not retransmitted. Bounded timeouts; monotonic
  sequence guards within a session. Duplicate requests return cached results
  without executing another write. A selected-session lease expires after
  3.5 seconds without a request. Read/write operations time out after 1.8 s.
- R1 waits 2.5 seconds for a reply, and 6 seconds for return confirmation.
- Existing ELRS parameter routing is used. Source inspection and compilation
  establish the route; actual Ranger/ER4 RF delivery remains a bench test.
- ER4 additionally requires the ELRS OTA arming flag to be disarmed. Verify
  the module's configured arming method agrees with R1 CUT; do not assume CH5
  is free if it is currently used for a gyro or another proportional output.

## ER6G same-pin read-only TX bench build r2 — 7 November

User authorized testing transmit on output5 with settingwritesdisabled and ER6G
flash whenready; confirmed isolatedreceiverpower/WiFi. Installed
`4.1.0-er6g-esc-read-r2` using guarded `er6g-esc-read-wifi`, image/settings/hardware
readback verified. UART2 RX/TX can share GPIO2, TXopen-drain only for explicit
bench requests, then GPIOreleased/input. No throttlePWM/restoration yet. Device
starts idle: no automaticTX. OtherPWM/ERSbaseline and PR3819filter retained.

Bench HTTPPOST `/esc-read-test` accepts only `operation=handshake` (1CFF) or
`operation=read&parameter=0..14` (1Cid00), no arbitrary bytes or settingwrites.
Requires Wi-Fi state and `X-R1-ESC-Bench: motor-disconnected-card-removed` header.
Only one request at a time; operate sequentially and wait for idle diagnostics
before the next request. NonblockingFIFO transmit, strictselfecho, validated
ESC1Bvalue reply, timeout/noautoretry and releaseonfault/modeexit. Handshake echo
confirms only sending/loopback, not ESCacceptance. Counters/status/RX+TXtrace are
under `/config` esc-listener. This is an isolatedWiFibenchtest, not an R1 UI
programmer and not a drivingfirmware. Programmingcard must be removed for TX.

Hardwarebuild/fullhostsuite/actualPWMreservationandISRtests passed. Fresh
listen-r1 recovery backup and r2 deployment verification saved privately under
`tmp/er6g-private-backup-2026-11-07-listen-r1/` and
`tmp/er6g-private-deploy-2026-11-07-read-r2/`. PoweredESCtest pending physical
reconnection. No ESCsettingwrite capability has been added.

## ER6G tap and UART RX validated — 7 November

Repeated working-card capture with ER6G output5 signal tap and a single shared
ESC/BEC supply succeeded; card displayed settings, and analyser decoded identical
handshake/all15reads to baseline. Prior card ERR was not reproduced; its cause
has not been isolated. ER6G listener captured75bytes with noUARTerrors, and its
retained64bytes exactly matched the analyser's read/reply suffix. It did not
capture the early handshake (startupcounter0), plausibly boot/initialization timing.
This validates receiving programming traffic through physicaloutput5/GPIO2 in
this arrangement, not transmission, automatic entry, PWM restoration or failsafe.
No settings changed; current firmware still disables output5PWM/TX. Evidence and
comparison summary: ignored `tmp/esc-card-er6g-tap-2026-11-07/`.

## Working-card baseline and MCP — 7 November

With ER6G disconnected, card works normally. Connecting ER6G tap caused ERR on
card; the receiver socket/input path is not established as electrically passive.
User prefers one supply: drive pack -> ESC/BEC -> receiver and card. Removing a
required red power wire is inappropriate in that arrangement; isolate separate
positive supplies only when actually using separate supplies.

Saleae original Logic CH0 captured the normal card/ESC session at2MS/s. Capture
stopped/saved, then decoded through working Saleae Logic2.4.48 built-in MCP on
`http://127.0.0.1:10530`. Prefer MCP over nativeUI: control/capture/add_analyzer/
export operations work. `tools/saleae_mcp_client.py` provides JSONRPC initialize,
notification and tool-call transport without adding global Codexconfiguration.
The appserver must be enabled under Settings/Automation. Discover fresh tool
schemas with tools/list; tools use camelCase arguments and typed setting values.

AsyncSerial InputChannel0 /800baud defaults8N1 decodes handshake1BFF->1CFF and
all15reads1Cid00->1Bvalue, consistent with the protocolrepository. Values id00=01
(cutoff3.4V/cell), others00 (includingBEC6.0V). No settingwrites. OneextraFF occurs
nearpower-up before handshake. All data and decoded capture saved under ignored
`tmp/esc-card-baseline-2026-11-07/`; summary.json records values and timing.
Measured first-headerpulse medianbitperiods: ESC1250.75us,card1267.75us, both
within800bauddecoder range. No inference about voltage from digitalcapture.

Next investigation is receiver tap loading/wiring/pin ownership. Do not claim
receiverinputvalidated from a few startupbytes or bridgeTXcompleted counter.

## ER6G single-pin listen-only installed — 7 November

User explicitly requested ER6G flash and confirmed power-only/Wi-Fi with servo,
ESC signal and telemetry unplugged. Fresh current slotfix recovery backup saved
privately at `tmp/er6g-private-backup-2026-11-07-slotfix/`. Guarded new receiver
profile `er6g-esc-listen-wifi` uploaded successfully. Readback matches every
packaged image byte, version `4.1.0-er6g-esc-listen-r1`; all saved config/binding
and hardware retained. Listener ready, actual UART2 baud800, no init error,
unwired logic sample1 and counters0. Evidence in private deployment
`tmp/er6g-private-deploy-2026-11-07-listen-r1/verification.json`.

Receiver remains in Wi-Fi and R1 module paused. UART2 listener also runs in Wi-Fi;
no RF handover is needed for passive byte capture. Initial ESC test still pending.
Power off before reconnecting the normal ESC lead to physical output5. Motor
leads must remain disconnected. Power receiver before ESC to capture startup,
retain receiver power when reading diagnostics, and do not expect throttle or
neutralPWM from this installed bench firmware. No ESC data transmission or
setting writes are implemented. Previous BUILD-ONLY notes below are superseded.

## Alternative ER6G single-pin test candidate — 6 November

Bench testing paused while user charges packs. User approved preparing a first
ER6G listen-only image to explore using physical output5/GPIO2 for both throttle
and programming in a future integrated design. Candidate version
`4.1.0-er6g-esc-listen-r1` reserves output5 as input fromboot, routes UART2 receive
there at800baud8N1, and never attaches UARTTX or sends ESC commands. It does not
switch back to PWM and cannot provide throttle or neutral pulses. Other outputs
and primary ERS UART retain stock handling. This is BUILD ONLY, not installed.

`receiver/er6g/build.py --esc-listen-only` produces a separate generic artifact.
Existing stock upload guard rejects this mode. A future upload requires a fresh
current ER6G backup, preserved binding/WiFi packaging and reviewed dedicated
upload profile. No need to rewire or flash anything while packs are charging.
On the eventual isolated bench: upload with poweronly, poweroff to reconnect
normal ESC signal to output5, keep motorleadsdisconnected, independently power
receiver before ESC to capturestartup, then transition toWiFi WITHOUT reboot to
read `/config` field `esc-listener`. It reports readiness/initerror,actualbaud,
logic sample, total bytes/startup1bFF pairs/errors and newest64timestampedbytes.
A successful receive test would justify adding transmit, then controlledPWM/
UART transitions and physical output/failsafe verification. No such feature is
claimed by this first candidate.

Hardware build, host listener boundary tests and real prepared PWM-driver
reservation tests passed; unmodified stock driver fails the same no-drive test.
Artifacts/logs are recorded in the handoff. No hardware actions performed.

## First r6 diagnostic result — 6 November

Read reached ER4 (Busy then Timeout) and returned toER6G/100LQ/CUT. Retained
Wi-Fi evidence:1frame started/completed, noTXabort/timingfault, max lateness53us;
transmit trace1c0000. ZERO RXbytes whileactive/inactive, noownTXecho, no startup
pairs, noUARToverrun/error; timeoutstage1 with3echo bytespending. This is evidence
of a missing receive path, not confirmation of ESC protocol incompatibility or
successful electrical transmission. User asked to prepare an ESC-disconnected
output4signal->1kohm->output3signal loopback to distinguish RX/harness/bus issues.
No setting writes. Safe diagnostic snapshot retained under private r6deployment.
Both receivers rebooted toRF after saving counters; R1CUT andER6Grestored.

## Electrical diagnostics r6 — 6 November

User requested firmware diagnostics after two real ESC reads returned Timeout,
including a restart while ER4 stayed selected/powered. User confirmed motor leads
disconnected for those tests. RF routing and safe return to ER6G passed; no ESC
settings were obtained or changed. Missing-startup-handshake remains unproven.

The direct read-only r6 candidate adds allocation-free bounded counters and a
32-event circular trace. `/config` exposes `esc-electrical` alongside `esc-bridge`.
Trace entries are microseconds-since-boot, kind and hex byte:
`R` active-session receive, `i` discarded inactive receive, `T` frame bytes
scheduled for transmission, `C` completed frame length, `F` timing-fault bit
index, `A` deselection/session-abort bit index. Transmit completion is software
evidence, not proof that the ESC received it. Only newest32events are retained;
`traceTotal` counts all events. No raw bit-edge capture is claimed.

Counters distinguish RX active/inactive bytes, inactive startup pairs, parser
startup handshakes, matching/mismatching echoes, good/invalid replies, timeouts,
UART overrun/error observations, TX started/completed/aborted, timing faults and
maximum bit lateness. Last transaction stage/echo-pending/parameter/status survive
Hello keepalives and deselection. Digital pin samples are logic levels only, not
voltage measurements. Evidence survives an RF-to-Wi-Fi transition, but NOT reboot
or power loss; use Ranger Enable Rx WiFi after the test before reading `/config`.
Normal ER4 outputs stay disabled; GPIO3RX/GPIO9open-drainTX and timing limits
remain. ESC setting writes remain compiled OFF. Diagnostics format only on HTTP
read; the bit loop records into fixed-size storage without dynamic allocation.
Host tests exercise actual adapter timing, overrun/error counters, ring wrap,
missing-vs-matching echo timeout, mismatch, startup and invalid reply evidence.

Hardware build and host suite passed. User confirmed ER4 power-only/Wi-Fi for
diagnostic update. Guarded `er4-esc-direct-read-wifi` upload succeeded18.40s.
Fresh r5 private backup: `tmp/er4-private-backup-2026-11-06-r5/`.
Deployment/readback: `tmp/er4-private-deploy-2026-11-06-direct-read-r6/`.
Readback confirms r6 and every packaged image byte, all config/binding/hardware
preserved, esc-electrical present. Both receivers rebooted to RF. Existing guard
now expects r6 diagnostic version
and fresh r5 recovery snapshot, rejects setting-write capability and wrong mode.

## Direct read-only r5 installed — 6 November

User explicitly requested flashER4 and confirmed power-only. Guarded PlatformIO
profile er4-esc-direct-read-wifi uploaded successfully; source/image/identity/
recovery checks passed. Readback confirms4.1.0-er4-esc-direct-read-r5 and matches
every packaged image byte. Binding/config/hardware preserved. Private recovery
snapshot tmp/er4-private-backup-2026-11-06-r4/ and verification in
 tmp/er4-private-deploy-2026-11-06-direct-read-r5/. Host tests passed, including
profile mismatch/write-enabled rejection. ESC-setting writes remain disabled.
No actual ESC connection/read has been tested. This supersedes candidate-only
status below. R1 module remains paused; no new R1 firmware uploaded.

## Direct read-only interface candidate — 6 November

User explicitly accepts the assumption that ESC programming voltage levels are
safe for ER4 GPIO. Voltage translation is therefore omitted for this bench
candidate; that is a user-approved assumption, not a measurement result.

The direct r5 candidate uses physical output3/GPIO3 as UART RX and physical
output4/GPIO9 as open-drain TX. Connect ESC signal to output3 signal and, through
a1kohm series resistor, output4 signal; common ESC/ER4 ground. The resistor is
for current limiting, not voltage translation. No external transistor is used.
Firmware sets the TX latch HIGH before selecting OUTPUT_OPEN_DRAIN: HIGH releases
the bus and LOW pulls it low. Deselection and timing faults release the bus.
ESC signal must be disconnected from ER6G output5 during this first bench test;
a permanent installation still needs signal ownership/switching. Existing power
wiring is a separate matter; do not join independently powered BEC rails.

Build with receiver/er4/build.py --direct-read-only. Default remains RF-only.
The direct candidate advertises Interface but never Writes; ESC-setting writes
are rejected. Host tests exercise the actual adapter through GPIO/UART mocks:
released startup, all30read-request bits, echo handling, ESC reply, write
rejection, and timing-overrun/deselection release. Electrical timing and actual
ESC read compatibility still require bench testing. The installed r4 is RF-only;
this candidate has not been uploaded. The existing RF-only upload guard
intentionally refuses an interface-enabled candidate until its deployment path
is reviewed. Earlier transistor/level-protection proposal below is historical
and superseded for this direct bench candidate.

## Measured ESC protocol and electrical adapter

Authoritative independent repository:
https://github.com/cmlpreston/surpass-esc-programming-protocol
See its `docs/protocol.md` for all mappings, capture evidence and limits.

800 bit/s, 8-N-1, idle high, shared half-duplex DATA plus common ground.
ESC startup `1B FF`; programmer response `1C FF`. Read `1C id 00` -> `1B value`.
Write `1B id value` -> `1B 0A`, then a read verifies the value. No checksum
appears in captures. The meaning of `0A` remains inferred pending negative tests.

Proposed ER4 adapter (not yet an approved wiring instruction):

- Physical output 3 / GPIO3: UART RX through a suitable divider/level shifter.
- Physical output 4 / GPIO9: MOSFET gate; HIGH pulls DATA low, LOW releases it.
- No direct DATA-to-ESP connection, and never actively drive DATA high.
- Use the protocol repository's proposed 470 ohm–1 kohm pull-down-path series
  resistance only after measuring bus levels and verifying the drive topology.
- UART RX is 800 baud. TX is cooperative GPIO timing with a 150 us lateness
  guard; no blocking bit delays. It releases DATA and fails the operation on
  overrun. Capture timing under actual RF load before enabling writes.
- TX echoes are consumed explicitly. Turnaround includes a quiet interval.
- UART/PWM ownership is removed from normal ELRS drivers in this dedicated
  build. GPIO0/1 are not used for the proposed ESC adapter. Boot behaviour,
  GPIO9 suitability on the actual ER4 revision and common-ground/power wiring
  still require physical verification.

User confirmed on 5 November that programming uses the normal ESC throttle
signal wire. A permanent two-receiver connection therefore also needs signal
ownership: stock ER6G failsafe can continue driving PWM while the ER4 attempts
serial communication. Team Race alone does not provide electrical isolation;
do not simply parallel the two outputs. Options remain a signal switch or
having the dedicated receiver own both throttle PWM and programming.

Electrical signalling/open-drain behaviour is not fully characterized. Neither
programming entry/re-entry after boot nor coexistence with the normal throttle
lead is established. A permanent connection must not be treated as ready until
those tests pass. Do not connect or parallel a programming card blindly.

## Receiver bench address and recovery backup

User supplied **192.168.1.130**, expected to remain stable. Read-only HTTP checks
on 5 November confirmed `RadioMaster ER4 2.4GHz PWM RX`, output GPIOs
`[0, 1, 3, 9]`, target `ER4_ESP8285_2400_RX_SPORT`, version `sport-telemetry`.
This is the earlier custom S.Port receiver image, not stock firmware.

Firmware, `/config`, `/hardware.json` and `/options.json` were backed up in
ignored private directory `tmp/er4-private-backup-2026-11-05/`, with checksums.
The directory is mode 0700 and files 0600; configuration includes private
network/binding data and must not be committed or printed. Original firmware
header and SHA256 verified. The RF-only programmer image has not been uploaded.
Verify target identity again before any upload, even with the stable address.

## Reproducible builds, no upload

The receiver integration is pinned to official ELRS commit `a9d4a9cb`, matching
our recorded 4.1.0 baseline. `receiver/er4/prepare.py` requires a clean checkout,
checks patch anchors and prepares a NEW disposable source directory. It does
not modify the checkout, installed receivers, binding phrase or radio settings.
The R1 repo contains only our integration files, not a vendored ELRS fork.

From the R1 repository, with an existing clean official ELRS checkout and the
ELRS targets metadata checkout:

```sh
python3 receiver/er4/build.py \
  --upstream /path/to/ExpressLRS \
  --targets /path/to/targets \
  --destination /tmp/r1-er4-esc-bench-NEW
```

The builder uses the repository PlatformIO executable, the official RadioMaster
ER4 layout and configurator, and produces `artifacts/er4-esc-rf-only-bench.bin`
plus a checksum manifest. No upload occurs. Configure the binding/model match
separately; the artifact does not copy a binding phrase.

Default flags are ER4_ESC_INTERFACE=0 and ER4_ESC_WRITES=0. Both hardware adapter
and locked variants receive compile coverage. Enabling these flags is a later
bench development step after the relevant physical checks, not a menu toggle.
Keep a stock ER4 backup/recovery route before installing custom receiver code.
The only connected transmitter upload target remains the approved XIAO
landscape environment, through the existing guarded upload workflow.

## Validation and next bench sequence

Host tests cover wire framing, measured choice ordering, wrong/stale/replayed
responses, exactly-once writes, ACK/readback mismatch, UART timeout, lease /
selection loss, interface lock, timestamp wrap and ER6G return identity. Control
engine tests prove CH12 survives CUT and conflicting mix/trim/reversal, and
programming vetoes arming. Native UI tests use actual touch dispatch in both
orientations for commissioning, settings and finish/return paths.

Next physical steps: install RF-only ER4 firmware under a separate explicit
flash request, configure and measure neutral failsafe/CH12 selection, verify
only the selected receiver returns telemetry and the R1's identity handshake
works. Then passively characterize the ESC bus, capture the read-only adapter,
verify programming entry and read all settings. Only then test one benign write
with readback and persistence. No ESC write or receiver upload has been performed.

Latest validated RF-only image and manifest are retained locally at
`tmp/releases/er4-esc-rf-bench-2026-11-05/` (ignored build artifacts). Product,
output-pin metadata and checksum were checked after packaging. Host suite,
focused native UI audit in both orientations, broad UI/rotation/typography
regression, XIAO landscape build and actual ER4 bench/adapter/write-path compile
checks passed. These establish software/build coverage, not physical operation.

## ER4 installed — 5 November 2026

The RF-only image was uploaded to 192.168.1.130 through dedicated PlatformIO
profile `er4-esc-rf-bench-wifi` / ESP8285. Current receiver reports
`4.1.0-er4-esc-bench`, source `a9d4a9`, device `RM ER4 ESC`. Firmware executable
readback matched the tested image byte-for-byte; GPIO layout, retained binding
UID and Wi-Fi network were verified. ESC interface and writes remain disabled.
Neither R1 nor ER6G was flashed or reconfigured on this request.

The upload project calls `tools/upload_preflight.py`, which dispatches this
exact ER4 project to `tools/er4_upload_preflight.py` while retaining the existing
XIAO-only transmitter checks. Source/artifact/layout hashes, original recovery
backup and live identity are verified before upload. Official compressed
ESP8266 OTA is used, with no force request or unsafe override. The first-migration
guard currently expects the original S.Port target, so a subsequent upload
requires a fresh backup and explicit review of the new current identity.

`receiver/er4/deploy.py` prepares a private deployment image from the generic
build and saved ER4 options/UID. These images contain Wi-Fi/binding data and must
remain in the ignored private deployment folder, never in Git. The installed
record is `tmp/er4-private-deploy-2026-11-05-v2/verification.json`; original recovery
image remains in `tmp/er4-private-backup-2026-11-05/`.

## R1 installed — 5 November 2026

Current R1 programmer build uploaded via the normal guarded XIAO landscape
workflow, with full tests/UI audit passed. Boot #638 passed hardware/UI startup
checks and serial diagnostics confirmed CUT. Menu: Setup -> Vehicle -> ESC
settings. Snapshot `/tmp/r1-flash-i5mmne7o`, log
`/tmp/r1-er4-programmer-r1-flash.log`. The session was not started during flash
verification; pair/failsafe commissioning still needs the physical checks above.

## RF commissioning progress — 5 November 2026

ER6G Team Race has been configured/read back as AUX8/CH12,1/Low. R1 current
model throttle is CH2 and user confirmed ESC on physical ER6G output5 (mapped
CH2); output4 maps CH3. Normal mappings unchanged. Receiver acknowledged storing
current CUT failsafe positions. Physical output mode/neutral measurement remains
pending; no drive pack is connected for RF testing.

Initial trials returned to confirmed ER6G/CUT but aborted before Hello due to
a redundant configuration-CUT callback on Begin causing transient stale output
snapshots during UI rendering. The already-CUT path now avoids that callback,
while unsafe entry still immediately CUTs and requires another activation after
acknowledgement. Stale-control abort itself is unchanged. USB ESCPROGSTATUS reports
actual selector/CH5 values, freshness, state and pending status; handshake sends
and independent CRSF vetoes are logged without binding/network secrets.

## Temporary handset startup for this bench

Explicit user request: XIAO build flag JR_R1_TEMP_BENCH_STARTUP=1 skips the boot
PIN and auto-enables CRSF after module detection. It does not change the stored
PIN, Walk-exit authorization, manual Pause or deliberate arming; boot is CUT.
Recovery modes do not auto-enable. Remove/set0 the flag and reflash the same
current source to restore normal prompts. The default macro remains0.

RF trace on installed temporary boot #641 received a real ER4 Hello ACK with
flags1 (electrical interface locked), LQ100 and fresh CUT/CH12 high. A subsequent
keepalive was lost and the safety return confirmed ER6G. Sustained-session test
remains pending the bounded Hello retry update. Source audit also found that
normal CRSF receiver management must be blocked on the unselected ER4: both
receivers share addressEC and their ordinary parameter IDs differ. Avoid further
generic receiver writes with both powered until that isolation is implemented.

## Latest RF-only revision and result

ER4 r3 is installed and verified against the complete tested image, with only
erased FF padding beyond it in the firmware download. UID/model and GPIO layout
are retained. Generic PARAMETER_WRITE is always consumed by the bridge; ordinary
reads/discovery are consumed while unselected. Stale telemetry queues are cleared
while unselected and selection requests a downlink resync against the latest ACK.
ESC electrical access/writes remain OFF. Private r2 backup and r3 verification
are retained; the guarded upload expectation uses the r2 image and intentionally
refuses another upload against changed r3 firmware until a fresh backup/review.

Live r3 test still timed out, although diagnostics recorded all three Hello
requests accepted and replies queued, with no malformed frames. Return to ER6G
was confirmed in CUT. Requested physical ER4-only power isolation next to
distinguish two-receiver interaction from ER4 downlink transport. Sustained
programmer RF communication has not yet passed.

### 7 November: ER6G same-pin TX routing correction r3

The first r2 handshake attempt produced no physical TX waveform or echo. ESP-IDF
4.4.7 `gpio_set_direction` reassigns the output matrix to GPIO when enabling
output, so setting open-drain mode after `uart_set_pin` disconnected UART TX.
r3 routes U2TXD_OUT_IDX last; the host hardware mock now models this side effect
and rejects the original ordering. A minimum wire-duration guard prevents early
TX detach, and API requests are published into main-owned transaction state.

r3 flashed and image/settings verified; Wi-Fi auto entry is now 15 seconds.
Physical transmission remains to be verified by analyser capture 4 after user
reconnects the ESC without its card. Writes and output5 PWM remain disabled.

The r3 bench test now physically confirms TX: Saleae capture5 decodes the exact
800-baud `1C 00 00` parameter0 request; ER6G echoes all bytes with zero UART
errors. An earlier explicit `1C FF` also echoed. ESC did not reply to read0;
programming entry/session timing remains unresolved. Capture4 had a device
ReadTimeout and cannot verify handshake waveform. Capture6 is collecting a
fresh whole-system boot with the card removed. No setting writes were sent.

Working-card capture7 confirms all15 reads with r3 as passive tap, zero UART
errors and zero receiver TX. Two successful card+receiver boots show ~68.8ms
startup LOW followed by startup1BFF; no-card boot does not. This is a candidate
entry dependency, not proven as a deliberate command. Card bits are ~1268us
and byte starts ~19.03ms apart, versus our contiguous12.5ms at800baud. Next
test removes only card while preserving the powered programming session, then
tries one receiver read to distinguish entry from request timing.

### 7 November: ER6G same-pin reads validated for all15 parameters

After removing only the card without power cycling ESC/ER6G, receiver read0
succeeded with `1C 00 00 -> 1B 01`. Remaining14 reads also succeeded once each.
Saleae captures8/9 independently confirm all75 request/reply bytes, no framing
errors, and values identical to working-card capture7: cutoff raw01 (3.4V/cell),
BEC raw01 (7.4V), all others00. No writes or retries. Evidence in ignored
`tmp/er6g-r3-established-session-2026-11-07/`. Both captures stopped/saved.

Contiguous800-baud UART requests work, so the card's larger inter-byte gaps are
not necessary for these reads. Standalone programming entry is still unresolved;
~69ms startup LOW is a candidate to investigate, not a proven command. No
dynamic PWM/UART switching or normal-throttle restoration has been validated.

### 7 November: entry pulse alone did not re-enter programming

r4 explicit entry measured69.075ms on wire, but no ESC startup followed.
Capture11 did show `1B FF` at initial power-up, missed by the late receiver
listener. A delayed `1C FF` and read0 then echoed but got no ESC reply. Thus
startup timing needs isolation; absence of startup in an earlier no-card capture
is not universal. Next test keeps RX separately powered (ESC red/BEC isolated,
common signal/ground) and replies immediately to the ESC boot message, with
no further firmware change. Evidence `tmp/er6g-r4-entry-tests-2026-11-07/`.

### 7 November: card-free startup confirmed; single-BEC boot probe r6

After the loose signal wire was fixed, r4 with a prepowered receiver successfully
received `1B FF`, answered `1C FF`, and read cutoff raw01. A clean single-cycle
repeat and all15 reads also passed with Saleae validation (captures15–17). No
entry pulse or setting write was required. Earlier negative tests with uncertain
wiring are inconclusive. User requires final operation from ESC/BEC alone.

r6 bench candidate moves UART initialisation to the first statement of receiver
setup, before ELRS options/RF initialisation. A bounded3s boot window listens
passively; only received startup1BFF leads to one response after40ms, echo check,
then one read0 after1.25s. No startup means no TX. It records UART-ready time,
boot duration/result/value. Boot-only permission ends before ELRS setup continues.
Output5 PWM and setting writes remain disabled in this bench firmware. This
is a timing experiment; normal driving and intentional runtime programming-mode
selection are still separate work. Single-BEC hardware result pending.

### 7 November: r6 clean single-BEC capture and timing correction

Capture19 confirms ESC startup `1B FF`, but r6 sees no RX bytes even with UART
initialisation moved to first setup. `bootReadyUs` uses the ESP firmware timer;
it excludes ROM/bootloader time and must not be called milliseconds from physical
power-on. No physical readiness bound is established yet. Wired late69ms entry
and delayed handshake/read tests20/21 both failed, resolving loose-wire ambiguity
for those negative results. Evidence `tmp/er6g-r6-confirmed-wire-tests-2026-11-07/`.

r7 bench candidate instead sends one `1C FF` as soon as the app/UART is ready,
even if startup was missed, followed by one read0 after echo and1.25s delay.
This tests whether the reply can still catch the ESC window and provides a
physical UART-ready marker in the next single-BEC capture. No bootloader
changes, GPIO pulses, setting writes or retries. Outcome pending.

### 7 November: small startup stage installed with verified full-app recovery

Full receiver r8a and the264,816-byte startup stage now share the existing OTA
slots. Stock bootloader/partition table are unchanged. The stage waits for1BFF,
answers once, reads0, stores a sealed record, then selects the exact full r8a
peer and resets only the ESP32. Full r8a accepts handoff only on software reset,
with record/bank/hash checks; it selects the known small image for the next
boot and continues RF/Wi-Fi. No normal throttle PWM or ESC setting writes.

Power-only recovery was verified: no ESC startup -> stage returns to full r8a,
Wi-Fi/settings/map/full image intact; exact small-stage hash accepted and armed.
Actual card-free single-BEC entry and post-reset ESC communication still need
the next wired test. Artifacts/verification in private ignored
`tmp/er6g-private-deploy-2026-11-07-startup-r1/`. Future receiver updates must
account for both app slots; generic upload guards reject the installed pair.

### 7 November: validated card-free, single-BEC entry and handoff

The working pair is full receiver `4.1.0-er6g-esc-read-r8c` plus small
`er6g-startup-r3`. The small app starts its UART early, requires40ms of HIGH/RX
quiet (bounded500ms), sends `1C FF`, then reads0. It hands control back through
a receiver-only software restart, leaving ESC/BEC power intact. Full firmware
validates the exact stage hash and fresh software-reset handoff record.

Capture32 confirms `1B FF -> 1C FF -> 1C 00 00 -> 1B 01`, with ~39ms turnaround
after the ESC's final startup byte. The previous immediate-stage attempt began
~3.75ms before that byte finished and corrupted its first header; waiting for
line/RX quiet fixed the measured collision.

After returning to full r8c, all15 settings read successfully once each;
capture33 confirms every request/reply with no framing errors. Values match the
working card: cutoff3.4V/cell, BEC7.4V, others raw00. Receiver pack removed,
card removed, sole ESC/BEC supply, motor leads disconnected, no ESC writes.
Evidence: ignored `tmp/er6g-card-free-single-bec-PASS-2026-11-07/`.

This proves entry and retained programming across the receiver-only restart.
It remains dedicated read-only bench firmware: output5 PWM is reserved/disabled,
and normal drive/program mode selection, CUT arming and PWM restoration remain
to be integrated. Both app slots are part of the installed pair; stock bootloader
and partition table were not changed.

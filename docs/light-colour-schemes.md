# Light colour schemes

Display → Colour scheme now has Light and Dark tabs. The Light tab offers the
original palette, now named Sage / teal, plus Cloud / blue, Sand / bronze and
Mint / forest. Mint / forest replaces the rejected Rose / plum in the same saved
slot, so an existing Rose selection becomes Mint without changing other slots.
Sage, Cloud and Mint use white panels and settings rows. Sand uses warm ivory.
Tinted page backgrounds, clearer borders and darker small text give the light
schemes more definition. Selection fills use a tint from their palette.

The selected light scheme is saved independently in `light_scheme`. Existing
preferences without this key retain the original Light palette; invalid IDs
fall back to it. Existing dark indices remain unchanged. The assignable Night
mode action switches between the last chosen light and dark palettes. Scheme
selection does not alter RF, arm requests, models or telemetry settings.

There are four options per tab so every choice and the saved-selection marker
fit the landscape screen. Portrait layout is also covered by simulation. The
host test checks surface separation and text contrast at the display's RGB565
colour depth. The focused UI audit exercises real picker
callbacks, persistence, Night toggle, missing/corrupt preferences, tab and row
bounds, and screenshots in both orientations.

## Validation and installation — 19 November 2026

[GitHub validation](https://github.com/cmlpreston/jr-r1-native-crsf/actions/runs/44476675621)
passed the landscape firmware build, complete host suite (including new contrast
checks), ERS UI regression and focused colour-scheme UI audit. The latter checks
both orientations, all saved selections, Night toggling, missing/corrupt light
preferences, picker bounds, typography and unchanged arm/ERS state. It generated
16 picker/dashboard screenshots; all three new landscape dashboards and the
portrait picker were visually inspected.

Firmware cache hits: 388/392 (98.98%), four misses; build time 47.49 s. The whole
job with simulator/audits took 5m06s. RAM is 205220 bytes; flash is 2245021 bytes.
The user-authorized guarded upload installed
`b0130e5c31350dd082922e7ad7f2ab3b367ef4bf` from `codex/r1-v2`, environment
`seeed_xiao_esp32s3-pcb`, XIAO ESP32-S3 / Waveshare LCD(F), schematic (5),
480x320 landscape. Firmware SHA256:
`f9dd734376b17b85aa93a3394d6f476ac5c3aced421cf8039c738453f2c4dc22`.

Fresh main ancestry, clean source, matching USB serial `90:70:69:11:DE:70`,
embedded identity, artifact checksums and local framework boot image passed.
The existing `.pio/cloud-upload` workspace was reused, without local compilation
or a full test rerun. Upload took 41.73 s; application transfer 25.8 s. All flash
hashes verified. Read-only postboot at uptime 10.4 s shows CUT, both MCPs online
with zero errors, outputs 992/992 and no RF bench authorization or arm request.
Theme selection on the actual screen remains the user's next check; flashing
preserves the saved selection. Missing light preferences use the original
palette when light mode is active.

Logs, downloaded artifacts and status: `/tmp/r1-light-schemes-validated`;
upload: `/tmp/r1-light-schemes-upload.log`. Local PNG previews are
`/tmp/r1-light-landscape-1-dashboard.png`, `-2-dashboard.png`, `-3-dashboard.png`
and `/tmp/r1-light-portrait-3-picker.png`.

### Mint replacement and original-palette name — 19 November

The user rejected Rose / plum; slot 3 now contains Mint / forest. Slot 0 keeps
its colours and is named Sage / teal. [Revision validation](https://github.com/cmlpreston/jr-r1-native-crsf/actions/runs/44479239706)
passed the full host/contrast suite and light-scheme UI audit. Mint's dashboard
and the renamed picker were visually inspected. Cache hits were 390/392
(99.49%); firmware build took 43.32 s.

Under the user's existing theme flash instruction, guarded upload installed
`8a85aa06040a4d7335dbd429eebc2b742f50e7a5` using the same approved environment,
hardware and landscape profile after fresh main/identity/hash checks. Upload
took 39.98 s (25.8 s application transfer). Postboot at uptime 10.45 s reports
CUT, both MCPs online with zero errors, safe outputs 992/992 and no arm/RF bench
request. Logs, manifest/hash and postboot data: `/tmp/r1-mint-sage-validated`;
upload: `/tmp/r1-mint-sage-upload.log`.

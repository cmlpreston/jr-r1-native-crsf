# pi3 remote firmware bench

The existing fv-results Pi also hosts the XIAO development tools. Existing
fv-results files, Pi-hole and Caddy are left in place. At the user's request on
2026-09-21, the eight fv-results refresh timers were disabled and their services
stopped to free resources. The previous unit settings are recorded on the Pi at
`/home/pi/r1-dev/fv-units-before-pause.txt`; re-enable the relevant timers when
scheduled results updates are needed again.

## Access

```sh
ssh -p 10022 pi@gateway.purplenet.online
```

LAN alternatives: `pi@192.168.1.123` (Ethernet), `pi@192.168.1.111`
(Wi-Fi). The gateway is the verified remote path.

The Pi runs Debian 12 aarch64 with approximately 1 GB RAM. Use one build job.
The ESP32 platform is pinned to 7.1.3 on both the laptop and Pi. Hardware validation
is required before treating a newly built platform upgrade as verified on the handset.

- Project: `/home/pi/r1-dev/firmware`
- PlatformIO: `/home/pi/r1-dev/venv/bin/pio`
- Logs: `/home/pi/r1-dev/build.log`, `/home/pi/r1-dev/tests.log`
- Repository authentication: dedicated read-only deploy key, held only on Pi.
  The checkout's `core.sshCommand` selects it and a dedicated known-hosts file
  populated from GitHub's published host keys.

The initial checkout includes a snapshot of this laptop's tracked and untracked
non-ignored source files. These are working-tree changes, not all on origin/main.
A plain git pull alone does NOT deliver current development changes. Preserve
this working tree; transfer and review a fresh source snapshot for each update.
Never transfer macOS `.pio`, `.venv` or simulator build outputs to the Pi.

## Build and tests

Preferred workflow (2026-09-21): build and test on the laptop, then transfer the
verified firmware artifacts to the Pi for USB upload and serial monitoring.
Platform 7.1.3 and its dependencies are installed on both machines. The laptop's
clean build and host/browser tests passed; the Pi's firmware build and host tests
also passed. No firmware was flashed as part of this upgrade. Upgrade validation
logs on the Pi are `~/r1-dev/upgrade713.log` and `~/r1-dev/tests713.log`.

For artifact-only uploads, preserve the matching source snapshot, environment,
Git identity and checksums alongside the firmware, bootloader and partition
images. Continue using PlatformIO's upload target and upload preflight; do not
bypass these with a direct esptool command. Verify the transferred artifacts
before any upload. Use `pio run -e seeed_xiao_esp32s3-pcb-portrait -t nobuild
-t upload --upload-port PORT` on the Pi after copying `firmware.bin`,
`bootloader.bin` and `partitions.bin` into the matching `.pio/build/` environment
directory. Verify that the installed framework’s `boot_app0.bin` also matches
the local build machine. Do not copy macOS object files or SCons caches.
The `nobuild` target skips compilation while retaining the upload preflight.

Optional on-Pi build:

```sh
cd /home/pi/r1-dev/firmware
/home/pi/r1-dev/venv/bin/pio run -e seeed_xiao_esp32s3-pcb-portrait -j 1
make -C test run
```

The first PlatformIO build installs dependencies required by some host tests.
Browser JavaScript tests and native LCD simulator audits can run on the laptop.

## Connect and upload

Connect the XIAO USB-C data cable to a Pi USB port. Identify the device with:

```sh
ls -l /dev/serial/by-id/
/home/pi/r1-dev/venv/bin/pio device list
```

The `pi` user is already in `dialout`. Prefer the verified `/dev/serial/by-id/…`
path over an assumed ttyACM number. Confirm the hardware is the XIAO with
Waveshare LCD (F), schematic (5) wiring, before upload.

Follow AGENTS.md for every upload: fetch origin/main, inspect status, verify
ancestry, build/test, state the branch/environment/display and obtain upload
authorization. Stop any monitor holding the port first. Use PlatformIO's upload
target so `tools/upload_preflight.py` executes. Never use direct esptool or an
unsafe override. For example, after replacing PORT with the verified device:

```sh
git fetch origin main
git status --short --branch
git merge-base --is-ancestor origin/main HEAD
/home/pi/r1-dev/venv/bin/pio run -e seeed_xiao_esp32s3-pcb-portrait -j 1 -t upload --upload-port PORT
/home/pi/r1-dev/venv/bin/pio device monitor --port PORT --baud 115200
```

No firmware is flashed automatically on boot, git updates or source transfer.
The first remote upload still needs validation with the actual connected XIAO.

## Handset web access while away

On the laptop, keep this SSH tunnel running:

```sh
ssh -N -p 10022 -L 127.0.0.1:8781:192.168.1.251:80 pi@gateway.purplenet.online
```

Then open `http://127.0.0.1:8781/`. Use the handset's normal web login and PIN.
Update the destination IP if DHCP changes it. The tunnel needs no new public
web port and does not require changing Caddy or the fv-results site.


The All channels / speed dial upload on 2026-09-21 transferred binaries only.
The Pi source checkout therefore predates those widget changes. Do not rebuild
and upload that checkout as the latest firmware until the relevant source changes
have been synchronized and verified. The artifact manifest is
`~/r1-dev/r1-widget-binaries.json`; it includes binary and hardware configuration
checksums. The laptop retains the matching source and test logs.

The model image/body-colour update was also flashed using local binaries only
on 2026-09-21. Manifest: `~/r1-dev/r1-appearance-binaries.json`; upload log:
`~/r1-dev/upload-appearance.log`. The Pi source still predates these features.

The three-paint-colour and hardware-switch monitor update was flashed from local
binaries on 2026-09-21. Manifest: `~/r1-dev/r1-paint3-binaries.json`; upload log:
`~/r1-dev/upload-paint3.log`. Pi source remains older; do not rebuild it as latest.

On 2026-09-23 the full transmitter firmware was restored with
`JR_R1_INPUT_BENCH=0`. Manifest: `~/r1-dev/r1-full-restore.json`.
The first upload lost USB before verification; after reconnecting, the guarded
retry verified successfully (`~/r1-dev/upload-full-restore-retry.log`).

## One-command laptop build / copy / flash

From the project directory on the Mac:

```sh
./tools/flash_via_pi.py
```

Defaults to the currently installed landscape layout, gateway SSH on port
10022 and the existing Pi upload checkout. It fetches main, refuses stale or
unrelated branches, validates the XIAO/Waveshare pin allocation, builds locally,
runs native/browser tests and the matching SDL UI audit, then transfers only
firmware/bootloader/partitions plus configuration, upload preflight and a manifest.
The Pi checks all hashes, boot_app0 and Git identity, then runs PlatformIO’s
`nobuild` / `upload` targets. No new release directory or Pi build is performed.
An open serial monitor causes a clear stop; it is never killed automatically.
Running the script authorizes its flash; there is no extra confirmation prompt.

Local checks without an upload:

```sh
./tools/flash_via_pi.py --check
```

For an explicitly selected portrait layout:

```sh
./tools/flash_via_pi.py --environment seeed_xiao_esp32s3-pcb-portrait
```

The complete tested source snapshot and binary manifest remain in the printed
`/tmp/r1-flash-*` directory. The Pi directory contains upload metadata and
binaries; its application source is not updated by this script and must not
be rebuilt as current firmware. The Pi commit must match the laptop; the script
stops instead of changing either checkout. SSH key access and the existing
local/Pi PlatformIO installations are required. Use `--help` for LAN SSH or
USB-path overrides.

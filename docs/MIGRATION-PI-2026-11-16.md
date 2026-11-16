# Move R-1 EVO development to the existing Raspberry Pi

Destination: `pi@192.168.1.111`, Raspberry Pi 3 Model B, Debian 12, aarch64,
about 1 GB RAM. Codex CLI 0.169.0 was verified at `/home/pi/.local/bin/codex`.
Use one compilation job on this Pi. The models run through the signed-in
service; this migration does not install a local language model.

## What moves and what stays

| Item | Destination / treatment |
| --- | --- |
| Current project | `/home/pi/r1-dev/project`, branch `codex/r1-v2` |
| Private migration package | `/home/pi/r1-migration/2026-11-16/`, private permissions |
| Older Pi source | Leave `/home/pi/r1-dev/firmware` untouched; it is old and has local changes |
| Existing USB upload artifacts | Leave `/home/pi/r1-dev/usb-upload` untouched; it is not a development checkout |
| Existing PlatformIO installation | `/home/pi/r1-dev/venv/bin/pio`; may be reused for inspection, but create the project's own Linux `.venv` |
| Receiver private files / captures | Restore `workspace-tmp/` from the private archive to the new project's ignored `tmp/` |
| Historical receiver repository | All-refs `ExpressLRS-SPort.bundle`; preserve five unpublished commits; do not substitute it for current ER6G overlays |
| R1 models, calibration, PIN, settings, uploaded maps | Stay on the physical R1; Git and firmware images are not a backup of live NVS/filesystem settings |
| Mac Python environments / build caches | Recreate on Linux; do not transplant `.venv`, `.pio` or simulator executables |

The private package contains binding/network configuration in receiver images.
Keep it out of GitHub and public file sharing. Its SHA256SUMS verifies the source
bundles and archive. The installed R1 deployment archive excludes synthetic
simulator preferences: those must never be restored to the handset.

Receiver backups predate the 15 November Team Race change. They preserve known
firmware/recovery states, not a fresh copy of every current receiver setting.
The all-refs bundles preserve the quarantined stash; do not apply it automatically.

## Current hardware baseline

- R1 installed source `c405738`, XIAO ESP32-S3, Waveshare LCD(F), schematic(5),
  **480×320 landscape**, environment `seeed_xiao_esp32s3-pcb`.
- Installed firmware SHA256:
  `cd2b76deb289b9a7a93eca38ed2772728eb5be38d2f6f0c142844125abf77734`.
- This includes the taller dashboard, local clock and handset timezone page.
- ER6G: full r14 / startup r8 pair from this repository's stock ELRS 4.1.0
  overlays. ER4: direct-read r6. The historical S.Port repository is different.
- ER6G Team Race was changed to Disabled; RB01 motor poles confirmed at4.
- ERS-GPS updated and read back as v1.9. Blue sends position placeholders;
  mapping sends coordinates. Both send UTC. See the commissioning and binary
  analysis documents for measured rates and limits.
- The source still has the authorized temporary `JR_R1_TEMP_BENCH_STARTUP=1`
  baseline. Preserve it during migration; production startup policy is a separate
  change. Do not assume production PIN/module prompts from a bench build.

Moving computers does not require uploading any firmware. The R1 USB device
was not present during the migration inventory; connect it only when needed.

## Start Codex

The user installed the official standalone ARM64 CLI and completed device-code
sign-in during this migration. If sign-in is needed again:

```sh
~/.local/bin/codex login --device-auth
~/.local/bin/codex login status
```

Complete the displayed sign-in on your own browser. There is no need to copy
the Mac's authentication file, SSH private keys or entire Codex profile.
[Official authentication guidance](https://learn.chatgpt.com/docs/auth#login-on-headless-devices).

Start in the new project, not the old firmware directory:

```sh
cd /home/pi/r1-dev/project
~/.local/bin/codex
```

First message for the Pi's new chat:

> Continue R-1 EVO development on this Pi. First read AGENTS.md,
> docs/MIGRATION-PI-2026-11-16.md and docs/HANDOFF-2026-11-02.md.
> Verify the migration package and current branch, finish the Linux development
> dependencies, and run build/tests with one job. Do not use the old
> /home/pi/r1-dev/firmware or usb-upload as current source. Do not apply stashes
> or upload firmware until I request it. Report what passes on ARM64 and what
> still needs checking.

This fresh chat gets its project context from these documents. Browser/MCP
connections and Mac-specific tools are not implicitly recreated on the Pi.

## Verify the transferred package and source

```sh
cd /home/pi/r1-migration/2026-11-16
sha256sum -c SHA256SUMS
cd /home/pi/r1-dev/project
git bundle verify /home/pi/r1-migration/2026-11-16/jr-r1-native-crsf.bundle
git bundle verify /home/pi/r1-migration/2026-11-16/ExpressLRS-SPort.bundle
git status --short --branch
git fetch origin main
git merge-base --is-ancestor origin/main HEAD
```

The fresh checkout may use the Pi's existing dedicated read-only GitHub key.
Fetching is sufficient for the initial verification. For later pushes, sign
into GitHub separately on the Pi or deliberately provision a write-capable
project key; do not silently replace the existing read-only key or copy Mac keys.

Private archive layout: `workspace-tmp/` is the old main project's ignored
`tmp/`; `related/ExpressLRS-SPort/` holds its local-only hardware/private files;
`ephemeral/` preserves GPS tools, reports and extra captures; and
`installed-r1-c405738/` is the exact last tested R1 source/upload package.
The historical receiver bundle can be cloned into a separate directory later.

## Install development dependencies and rebuild

Required packages already present at inventory: Git, C/C++ build tools,
Python/venv, pkg-config and psmisc. Missing: Node, SDL2/OpenSSL development
files and ripgrep. tmux is useful for long builds over SSH. Install explicitly;
no OS upgrade is required for this project setup:

```sh
sudo apt-get update
sudo apt-get install --no-install-recommends nodejs libsdl2-dev libssl-dev ripgrep tmux
cd /home/pi/r1-dev/project
python3 -m venv .venv
.venv/bin/python -m pip install 'platformio==6.1.19'
```

The project core pin matches the Mac's verified PlatformIO Core6.1.19. The
existing shared Pi environment is Core6.2.0 and is left unchanged. The ESP32
platform is independently pinned in platformio.ini to espressif327.1.3.
Leave the default PlatformIO core location alone: some host tests inspect its
framework files.

First build the approved firmware environment to install framework/ArduinoJson
dependencies, then bootstrap and check the native simulator:

```sh
.venv/bin/pio run -e seeed_xiao_esp32s3-pcb -j 1
make -C simulator bootstrap
make -C test -j1 run
make -C simulator -j1
```

Run a UI audit with disposable synthetic preferences:

```sh
audit_state=$(mktemp -d /tmp/r1-ui-audit.XXXXXX)
SDL_VIDEODRIVER=dummy JR_R1_SIM_STATE="$audit_state" simulator/build/jr-r1-sim --ui-audit
```

Migration fixes make Linux OpenSSL discovery use pkg-config, remove the host
test's accidental reliance on a portrait ArduinoJson cache, and permit
`JOBS=1 sh simulator/build_web.sh` for optional WASM builds. Emscripten/iOS
tools are separate from the firmware/native-test prerequisites. Full ARM64
build/UI validation must pass before treating this Pi as a proven build host.

## Later uploads and optional tools

Follow AGENTS.md for every upload. Fetch main, inspect status and ancestry,
confirm hardware/pins/environment, build and run available tests, and state the
branch/environment/display. A direct Pi build should use one job; avoid the
Mac-to-Pi wrapper's fixed four-job simulator build on this 1 GB machine.
The direct PlatformIO upload target still runs the safety preflight. No upload
is authorized merely by following this migration guide.

Safari and Xcode/iOS work remain Mac-specific. Saleae now supplies a Linux
ARM64 build, but Logic GUI/capture performance and USB operation on this 1 GB
Pi3 are a separate check; saved captures and protocol tools are preserved.
[Official Saleae downloads](https://www.saleae.com/downloads).

Keep the Mac working copy and private package until the Pi's build/tests,
Git access and any needed hardware tools are verified. The backup contains
additional recovery/source history that a plain GitHub clone would miss.

# Hardware safety rules

This repository controls a live RC transmitter. Treat every firmware upload as
a safety-critical operation.

Before uploading to an ESP32-S3:

1. Run `git fetch origin main` and inspect `git status --short --branch`.
2. Confirm that the checked-out commit contains `origin/main`. Never upload a
   branch that is behind or unrelated to `origin/main` without first explaining
   the differences and obtaining explicit confirmation from the user.
3. Confirm the selected PlatformIO environment, board, display profile and pin
   map against the connected hardware. As directed by the user on 2026-09-16,
   use only the Seeed XIAO ESP32-S3 until the user explicitly says otherwise.
   As directed on 2026-10-26, use LANDSCAPE ONLY for transmitter uploads.
   The default and approved environment is `seeed_xiao_esp32s3-pcb` (480×320),
   with the Waveshare 3.5-inch RPi LCD (F) and final schematic (5) pin allocation
   in `docs/xiao-pcb.md`. Portrait upload targets require a new explicit user
   instruction; retaining portrait simulator/build coverage is permitted.
   DevKitC, BTT and diagnostic environments are legacy and require explicit
   user authorization before uploading.
4. Build and run the available tests before upload.
5. State which branch, environment and display profile will be uploaded.

Never resolve a firmware problem by checking out or flashing an older branch
without the user's explicit approval. Never use the BTT or display-diagnostic
PlatformIO environments on the installed Waveshare screen without explicit
confirmation.

Do not apply an old stash, cherry-pick an older hardware experiment, or restore
files from another branch until its diff and hardware assumptions have been
reviewed with the user. In particular, `stash@{0}` may contain obsolete
pre-Waveshare work and must remain quarantined unless explicitly reviewed.

Use the PlatformIO upload target so `tools/upload_preflight.py` runs. Do not
invoke `esptool.py` directly.

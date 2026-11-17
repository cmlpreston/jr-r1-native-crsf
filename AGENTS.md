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

## Default build and upload workflow

As directed by the user on 2026-11-17, use GitHub Actions for firmware builds
and the complete host test suite, with ccache and one compilation job. For an
authorized R1 flash, download only the successful run's small
`r1-landscape-<commit>` artifact; the separate source archive is optional.
Verify the exact tested commit, fresh `origin/main` ancestry, artifact hashes
and connected hardware before the guarded PlatformIO upload.

Reuse `.pio/cloud-upload` and its installed dependencies. Do not recreate the
workspace, compile firmware or rerun the full host suite on the Pi3 for an
already tested cloud artifact. Keep Pi work to lightweight integrity/safety
checks, the guarded `nobuild` upload and read-only postboot status. Simulator/UI
checks remain change-dependent. All hardware safety rules above still apply;
a build/download alone does not authorize flashing. See
`docs/github-compilation.md` for the validated procedure and timings.

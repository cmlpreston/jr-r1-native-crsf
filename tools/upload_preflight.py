"""Refuse an unsafe JR R1 firmware upload before PlatformIO invokes esptool."""

Import("env")

import os
import subprocess


def git(project_dir, *arguments):
    return subprocess.run(
        ["git", *arguments], cwd=project_dir, text=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def fail(message):
    print("\nJR R1 UPLOAD PREFLIGHT: REFUSED")
    print(message)
    print("No firmware has been written.\n")
    Exit(1)


def git_value(project_dir, fallback, *arguments):
    result = git(project_dir, *arguments)
    value = result.stdout.strip() if result.returncode == 0 else ""
    return value or fallback


project_dir = env.subst("$PROJECT_DIR")
branch_identity = git_value(project_dir, "detached", "branch", "--show-current")
commit_identity = git_value(project_dir, "unknown", "rev-parse", "--short=8", "HEAD")
dirty_result = git(project_dir, "status", "--porcelain")
git_state = "dirty" if dirty_result.returncode != 0 or dirty_result.stdout.strip() else "clean"

# Embed the identity of the source state that is actually being compiled.
env.Append(CPPDEFINES=[
    ("JR_R1_GIT_BRANCH", '\\"%s\\"' % branch_identity),
    ("JR_R1_GIT_COMMIT", '\\"%s\\"' % commit_identity),
    ("JR_R1_GIT_STATE", '\\"%s\\"' % git_state),
    ("JR_R1_BUILD_ENV", '\\"%s\\"' % env.subst("$PIOENV")),
])


targets = set(COMMAND_LINE_TARGETS)
er4_upload = "upload" in targets and env.subst("$PIOENV") in ("er4-esc-rf-bench-wifi", "er4-esc-direct-read-wifi")
if er4_upload:
    # User authorized a separate ER4 programmer on 2026-11-05. This exact
    # project/profile has its own artifact and live-device checks; no unsafe
    # override and no relaxation of the XIAO/display checks below.
    import pathlib, runpy
    root = pathlib.Path(project_dir).resolve().parents[2]
    if os.environ.get("JR_R1_ALLOW_UNSAFE_UPLOAD") == "1":
        fail("The ER4 upload never accepts the unsafe override")
    fetched = git(str(root), "fetch", "--quiet", "origin", "main")
    if fetched.returncode != 0:
        fail("Could not refresh origin/main for ER4 upload")
    if git(str(root), "merge-base", "--is-ancestor", "origin/main", "HEAD").returncode != 0:
        fail("Current R1 branch does not contain origin/main")
    try:
        runpy.run_path(str(root / "tools/er4_upload_preflight.py"))["validate"](
            root, project_dir, env.subst("$BOARD"), os.environ["JR_R1_ER4_ARTIFACT_DIR"],
            direct_read=env.subst("$PIOENV") == "er4-esc-direct-read-wifi")
    except (OSError, ValueError, KeyError) as error:
        fail(str(error))
    env.Replace(LDSCRIPT_PATH=str(pathlib.Path(os.environ["JR_R1_ER4_ARTIFACT_DIR"]) / "elrs.flash.1m.ld"))
    print("ER4 UPLOAD PREFLIGHT: PASSED; ESP8285 / CH12 / writes OFF; interface=" +
          ("direct read-only" if env.subst("$PIOENV") == "er4-esc-direct-read-wifi" else "OFF"))

er6g_upload = "upload" in targets and env.subst("$PIOENV") in ("er6g-stock-teamrace-wifi", "er6g-esc-listen-wifi", "er6g-esc-read-wifi", "er6g-esc-startup-wifi", "er6g-esc-mode-wifi", "er6g-esc-mode-startup-wifi", "er6g-esc-single-wifi", "er6g-esc-restore-wifi", "er6g-esc-restore-startup-wifi", "er6g-esc-write-wifi", "er6g-esc-write-startup-wifi")
if er6g_upload:
    # Prepared receiver-only candidate. Invoke only after an explicit identified
    # ER6G upload request and physical wiring isolation; no XIAO guard relaxation.
    import pathlib, runpy
    root=pathlib.Path(project_dir).resolve().parents[2]
    if os.environ.get("JR_R1_ALLOW_UNSAFE_UPLOAD") == "1":fail("ER6G never accepts unsafe override")
    fetched=git(str(root),"fetch","--quiet","origin","main")
    if fetched.returncode!=0 or git(str(root),"merge-base","--is-ancestor","origin/main","HEAD").returncode!=0:fail("Refresh origin/main and review branch ancestry before ER6G upload")
    try:
        if env.subst("$PIOENV") in ("er6g-esc-write-wifi","er6g-esc-write-startup-wifi"):
            runpy.run_path(str(root/"tools/er6g_write_upload_preflight.py"))["validate"](root,project_dir,env.subst("$BOARD"),os.environ["JR_R1_ER6G_ARTIFACT_DIR"],startup=env.subst("$PIOENV")=="er6g-esc-write-startup-wifi")
        elif env.subst("$PIOENV") in ("er6g-esc-restore-wifi","er6g-esc-restore-startup-wifi"):
            runpy.run_path(str(root/"tools/er6g_restore_upload_preflight.py"))["validate"](root,project_dir,env.subst("$BOARD"),os.environ["JR_R1_ER6G_ARTIFACT_DIR"],startup=env.subst("$PIOENV")=="er6g-esc-restore-startup-wifi")
        elif env.subst("$PIOENV")=="er6g-esc-single-wifi":
            runpy.run_path(str(root/"tools/er6g_single_upload_preflight.py"))["validate"](root,project_dir,env.subst("$BOARD"),os.environ["JR_R1_ER6G_ARTIFACT_DIR"])
        elif env.subst("$PIOENV") in ("er6g-esc-mode-wifi","er6g-esc-mode-startup-wifi"):
            runpy.run_path(str(root/"tools/er6g_mode_upload_preflight.py"))["validate"](root,project_dir,env.subst("$BOARD"),os.environ["JR_R1_ER6G_ARTIFACT_DIR"],startup=env.subst("$PIOENV")=="er6g-esc-mode-startup-wifi")
        elif env.subst("$PIOENV")=="er6g-esc-startup-wifi":
            runpy.run_path(str(root/"tools/er6g_startup_upload_preflight.py"))["validate"](root,project_dir,env.subst("$BOARD"),os.environ["JR_R1_ER6G_ARTIFACT_DIR"])
        else:
            runpy.run_path(str(root/"tools/er6g_upload_preflight.py"))["validate"](root,project_dir,env.subst("$BOARD"),os.environ["JR_R1_ER6G_ARTIFACT_DIR"],listen_only=env.subst("$PIOENV")=="er6g-esc-listen-wifi",read_only=env.subst("$PIOENV")=="er6g-esc-read-wifi")
    except (OSError,ValueError,KeyError) as error:fail(str(error))
    print("ER6G PREFLIGHT: PASSED; ESP32 / ISM2G4 / GPIO map and recovery verified")

if "upload" in targets and not er4_upload and not er6g_upload:
    pio_environment = env.subst("$PIOENV")
    board = env.subst("$BOARD")

    if os.environ.get("JR_R1_ALLOW_UNSAFE_UPLOAD") == "1":
        print("JR R1 UPLOAD PREFLIGHT: explicit unsafe override is active")
    else:
        approved_waveshare_environments = {
            "seeed_xiao_esp32s3-pcb",
        }
        if pio_environment not in approved_waveshare_environments:
            fail(
                "The selected environment is '%s'. Active hardware is XIAO ESP32-S3 "
                "with schematic (5) wiring, landscape only per the 2026-10-26 user instruction. "
                "Other profiles require explicit user confirmation."
                % pio_environment)
        if board != "seeed_xiao_esp32s3":
            fail("Unexpected board target: %s" % board)

        artifact_manifest = os.environ.get("JR_R1_ARTIFACT_MANIFEST")
        if artifact_manifest:
            import runpy
            verify = runpy.run_path(os.path.join(project_dir, "tools", "artifact_preflight.py"))["verify"]
            def check_artifacts(*args, **kwargs):
                try:
                    return verify(project_dir, pio_environment, board, targets, artifact_manifest)
                except (OSError, ValueError, KeyError) as error:
                    fail(str(error))
            manifest = check_artifacts()
            upload_images = runpy.run_path(os.path.join(project_dir, "tools", "artifact_preflight.py"))["upload_images"]
            env.Replace(FLASH_EXTRA_IMAGES=upload_images(project_dir, pio_environment))
            def pre_upload_artifacts(*args, **kwargs):
                check_artifacts()
                return 0
            env.AddPreAction("upload", pre_upload_artifacts)
            print("JR R1 ARTIFACT UPLOAD PREFLIGHT: PASSED")
            print("  Tested source: %s / %s" % (manifest["branch"], manifest["commit"]))
            print("  Pi: verified artifacts only; no source checkout or build")
        else:
            fetched = git(project_dir, "fetch", "--quiet", "origin", "main")
            if fetched.returncode != 0:
                fail("Could not refresh origin/main: %s" % fetched.stderr.strip())

            branch = git(project_dir, "branch", "--show-current")
            branch_name = branch.stdout.strip() if branch.returncode == 0 else ""
            if not branch_name:
                fail("Detached HEAD; branch identity cannot be verified.")

            ancestry = git(
                project_dir, "merge-base", "--is-ancestor", "origin/main", "HEAD")
            if ancestry.returncode != 0:
                fail(
                    "Branch '%s' does not contain the current origin/main. This may "
                    "be a stale hardware profile." % branch_name)

            profile_path = os.path.join(project_dir, "include", "display_profile.h")
            with open(profile_path, "r", encoding="utf-8") as profile_file:
                profile = profile_file.read()
            expected_default = (
                "#define DISPLAY_PROFILE DISPLAY_PROFILE_WAVESHARE_RPI_35F")
            if expected_default not in profile:
                fail("The default display profile is not the Waveshare RPi LCD (F).")

            dirty = git(project_dir, "status", "--short")
            dirty_text = dirty.stdout.strip() if dirty.returncode == 0 else "unknown"
            print("\nJR R1 UPLOAD PREFLIGHT: PASSED")
            print("  branch : %s" % branch_name)
            print("  board  : %s" % board)
            print("  env    : %s" % pio_environment)
            print("  display: Waveshare 3.5-inch RPi LCD (F)")
            if dirty_text:
                print("  note   : uploading tested working-tree changes")
            print()

    # Wi-Fi uses the same target, ancestry and hardware guards as USB uploads.
    if os.environ.get("JR_R1_OTA_URL"):
        import sys
        import shlex
        command = shlex.quote(sys.executable) + " " + shlex.quote(
            os.path.join(project_dir, "tools", "upload_wifi.py")) + ' "$SOURCE"'
        # The ESP32 platform builder runs after this pre-script. Its serial
        # branch overwrites UPLOADCMD unless the protocol is explicitly custom.
        # Selecting custom also prevents serial-port detection/reset actions.
        env.Replace(UPLOAD_PROTOCOL="custom", UPLOADCMD=command)

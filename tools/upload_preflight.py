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


targets = set(COMMAND_LINE_TARGETS)
if "upload" in targets:
    project_dir = env.subst("$PROJECT_DIR")
    pio_environment = env.subst("$PIOENV")
    board = env.subst("$BOARD")

    if os.environ.get("JR_R1_ALLOW_UNSAFE_UPLOAD") == "1":
        print("JR R1 UPLOAD PREFLIGHT: explicit unsafe override is active")
    else:
        if pio_environment != "esp32-s3-devkitc-1":
            fail(
                "The selected environment is '%s', not the approved Waveshare "
                "hardware environment 'esp32-s3-devkitc-1'. Explicit user "
                "confirmation is required." % pio_environment)
        if board != "esp32-s3-devkitc-1-n16r8":
            fail("Unexpected board target: %s" % board)

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

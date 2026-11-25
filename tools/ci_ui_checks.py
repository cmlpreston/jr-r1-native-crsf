#!/usr/bin/env python3
"""Select native simulator checks from the changed files in a GitHub build."""
import argparse
from fnmatch import fnmatchcase
import os
import subprocess


FOCUSED = {
    'history': (
        'include/sensor_history.h',
        'include/ui_telemetry_history.h',
        'src/transmitter_ui_sensor_history.inc',
        'simulator/ui_sensor_history_audit.inc',
    ),
    'ers': (
        'include/ers_reply_queue.h',
        'include/ers_sensor_config.h',
        'include/crsf_telemetry.h',
        'src/crsf_telemetry.cpp',
        'src/transmitter_ui_ers.inc',
        'src/setup_portal_ers.inc',
    ),
    'colours': (
        'include/ui_colour_schemes.h',
        'simulator/ui_colour_schemes_audit.inc',
    ),
    'records': (
        'include/run_record*.h',
        'src/run_record_store.cpp',
        'src/transmitter_ui_run_records.inc',
        'simulator/ui_run_records_audit.inc',
    ),
}

# Components with their own audit take the focused route. Changes to the UI
# foundation or shared callbacks run all those audits plus the full UI audit.
SHARED = (
    '.github/workflows/firmware.yml',
    'tools/ci_ui_checks.py',
    'platformio.ini',
    'include/transmitter_ui.h',
    'include/ui_*.h',
    'include/dashboard_widgets.h',
    'include/display_profile.h',
    'include/lv_conf.h',
    'include/setup_portal*.h',
    'src/setup_portal.cpp',
    'src/transmitter_ui*',
    'src/fonts/*',
    'src/r1_dm_*.cpp',
    'src/r1_manrope_*.cpp',
    'src/r1_symbol_fonts.cpp',
    'simulator/Makefile',
    'simulator/sources.mk',
    'simulator/*.cpp',
    'simulator/*.inc',
    'simulator/include/*.h',
)
CHECKS = ('shared', *FOCUSED)


def select_checks(paths):
    if paths is None:
        # No known baseline, e.g. a manual dispatch or a first push.
        return set(CHECKS)

    selected = set()
    for path in paths:
        focused = {name for name, patterns in FOCUSED.items()
                   if any(fnmatchcase(path, pattern) for pattern in patterns)}
        if focused:
            selected.update(focused)
        elif any(fnmatchcase(path, pattern) for pattern in SHARED):
            return set(CHECKS)
    return selected


def changed_paths(before, head):
    if not before or subprocess.run(
        ['git', 'cat-file', '-e', f'{before}^{{commit}}'],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    ).returncode:
        return None
    # Disable rename detection so both ends of a rename are checked. NUL
    # delimiters keep path names with spaces, tabs or newlines intact.
    names = subprocess.check_output(
        ['git', 'diff', '--name-only', '--no-renames', '-z', before, head, '--'])
    return [os.fsdecode(name) for name in names.split(b'\0') if name]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before', default='')
    parser.add_argument('--head', required=True)
    args = parser.parse_args()
    selected = select_checks(changed_paths(args.before, args.head))
    print(f'simulator={str(bool(selected)).lower()}')
    for name in CHECKS:
        print(f'{name}={str(name in selected).lower()}')


if __name__ == '__main__':
    main()

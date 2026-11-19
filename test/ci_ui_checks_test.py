#!/usr/bin/env python3
"""Verify that hosted UI check selection covers shared and focused changes."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from ci_ui_checks import select_checks


class UiSelectionTest(unittest.TestCase):
    def test_non_ui_change_does_not_build_simulator(self):
        self.assertEqual(select_checks(['src/main_input_pipeline.inc', 'docs/run-records.md']), set())

    def test_focused_changes_select_only_relevant_audits(self):
        cases = {
            'src/run_record_store.cpp': 'records',
            'simulator/ui_run_records_audit.inc': 'records',
            'include/run_records.h': 'records',
            'src/transmitter_ui_sensor_history.inc': 'history',
            'include/ers_reply_queue.h': 'ers',
            'include/ui_colour_schemes.h': 'colours',
        }
        for path, expected in cases.items():
            with self.subTest(path=path):
                self.assertEqual(select_checks([path]), {expected})
        self.assertEqual(select_checks(['include/run_record_store.h', 'include/ui_colour_schemes.h']),
                         {'records', 'colours'})

    def test_shared_dependencies_select_broad_and_focused_audits(self):
        cases = [
            '.github/workflows/firmware.yml',
            'src/transmitter_ui_design.inc',
            'src/transmitter_ui_lvgl.cpp',
            'include/transmitter_ui.h',
            'include/ui_typography.h',
            'include/dashboard_widgets.h',
            'simulator/ui_audit.inc',
        ]
        expected = {'shared', 'records', 'history', 'ers', 'colours'}
        for path in cases:
            with self.subTest(path=path):
                self.assertEqual(select_checks([path]), expected)
        self.assertEqual(select_checks(None), expected)


if __name__ == '__main__':
    unittest.main()

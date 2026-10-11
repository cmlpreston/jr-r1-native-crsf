import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('monitor', Path(__file__).resolve().parents[1] / 'tools/input_monitor.py')
monitor = importlib.util.module_from_spec(spec)
spec.loader.exec_module(monitor)


def messages():
    yield '@INPUT_BEGIN\t1\t12345\tCUT\t1880\t1879\t-100\t2000\t400\t1\t1737\t1'
    yield '@INPUT_MCP\t1\t1\t32543\t0'
    yield '@INPUT_MCP\t2\t0\t65535\t4'
    yield '@INPUT_RF\t1\t0\t992\t992\t1\t100\t5000'
    for i in range(19):
        kind = 0 if i < 2 else 3
        yield f'@INPUT_ROW\t{i}\tControl {i}\tK2 PA0/PA2\t{kind}\t1\t{int(i == 4)}\t1\t0\t1\t0\tNEG_PRESS\t12000'
    yield '@INPUT_END\t19'


class MonitorTest(unittest.TestCase):
    def test_noise_and_readonly_values(self):
        parser = monitor.SnapshotParser()
        for line in messages():
            self.assertIsNone(parser.feed('TOUCH down page=dashboard'))
            result = parser.feed(line)
        self.assertIsNotNone(result)
        text = '\n'.join(monitor.render(result))
        for expected in ['CUT', '-10.0%', '+40.0%', 'K2 OFFLINE', 'FAULT', 'NEGATIVE', '1737 mV']:
            self.assertIn(expected, text)
        self.assertIn('STALE', '\n'.join(monitor.render(result, True)))
        rf = '\n'.join(monitor.render(result, rf_test=True, reply='Outputs CUT'))
        self.assertIn('Space/C: CUT', rf)
        self.assertIn('sent THR=992', rf)
        self.assertIn('CRSF frames: 5000', rf)
        result['header'][1] = 'HALTED_SAFE'
        halted = '\n'.join(monitor.render(result))
        self.assertIn('Calibration not applied', halted)
        self.assertNotIn('-10.0%', halted)

    def test_incomplete_snapshot_not_published(self):
        parser = monitor.SnapshotParser()
        for line in messages():
            if line.startswith('@INPUT_ROW\t4\t'):
                continue
            self.assertIsNone(parser.feed(line))

    def test_malformed_frame_recovers(self):
        parser = monitor.SnapshotParser()
        parser.feed(next(messages()))
        parser.feed('@INPUT_ROW\tbad')
        self.assertIsNone(parser.feed('@INPUT_END\t19'))
        for line in messages():
            result = parser.feed(line)
        self.assertIsNotNone(result)

    def test_terminal_controls_sanitized(self):
        self.assertNotIn('\x1b', monitor.safe('\x1b[2J'))
        self.assertEqual(monitor.bar(-9999).index('#'), 1)
        self.assertEqual(monitor.bar(9999).index('#'), 19)


if __name__ == '__main__':
    unittest.main()

import importlib.util
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('saleae_capture', Path(__file__).resolve().parents[1] / 'tools/saleae_capture.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class CaptureTests(unittest.TestCase):
    def test_device_selection_rejects_simulation_and_ambiguity(self):
        sim = SimpleNamespace(device_id='demo', is_simulation=True)
        a = SimpleNamespace(device_id='a', is_simulation=False)
        b = SimpleNamespace(device_id='b', is_simulation=False)
        with self.assertRaises(ValueError): module.select_device([sim])
        with self.assertRaises(ValueError): module.select_device([a, b])
        self.assertIs(module.select_device([a, b, sim], 'b'), b)

    def test_channels(self):
        self.assertEqual(module.channels('0,1,4'), [0,1,4])
        for value in ('0,0', '-1', '16', 'a', ''):
            with self.assertRaises(Exception): module.channels(value)

    def test_summary_counts_edges_not_samples(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / 'digital.csv'
            path.write_text('Time [s],Channel 0,Channel 1\n0,0,1\n0.1,1,1\n0.2,1,0\n0.3,1,0\n')
            result = module.summarize_csv(path)
            self.assertEqual(result['transitions'], {'Channel 0':1, 'Channel 1':1})
            self.assertEqual(result['rows'],4)

    def test_capture_saved_before_export_failure(self):
        calls=[]
        class FakeCapture:
            def save_capture(self, path): calls.append('saved')
            def export_raw_data_csv(self, path):
                calls.append('export')
                raise RuntimeError('test export failure')
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaises(RuntimeError): module.export_capture(FakeCapture(), Path(d), 'csv', [])
        self.assertEqual(calls, ['saved','export'])

    def test_reject_undersampling_before_connecting(self):
        with self.assertRaises(SystemExit) as result:
            module.main(['capture','--signal-hz','40000000','--rate','24000000'])
        self.assertEqual(result.exception.code,2)


if __name__ == '__main__': unittest.main()

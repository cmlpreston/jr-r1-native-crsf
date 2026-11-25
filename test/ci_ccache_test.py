#!/usr/bin/env python3
"""Check that the CI launcher wraps compilation without changing toolchains."""
from pathlib import Path
import runpy
import unittest


class Environment(dict):
    def WhereIs(self, name):
        assert name == 'ccache'
        return self.launcher

    def Replace(self, **values):
        self.update(values)


class CiCcacheTest(unittest.TestCase):
    def run_script(self, env):
        runpy.run_path(str(Path(__file__).resolve().parents[1] / 'tools/ci_ccache.py'),
                       init_globals={'Import': lambda name: None, 'env': env})

    def test_wraps_compile_commands_only(self):
        env = Environment(CC='target-gcc', CXX='target-g++', LINKCOM='$CXX objects',
                          ASCOM='$AS source', CCCOM='$CC -c source', CXXCOM='$CXX -c source')
        env.launcher = '/usr/bin/ccache'
        before = env.copy()
        self.run_script(env)
        for key in ('CC', 'CXX', 'LINKCOM', 'ASCOM'):
            self.assertEqual(env[key], before[key])
        for key in ('CCCOM', 'CXXCOM'):
            self.assertEqual(env[key], '"$R1_CCACHE" ' + before[key])

    def test_missing_launcher_fails_explicitly(self):
        env = Environment()
        env.launcher = None
        with self.assertRaisesRegex(RuntimeError, 'not installed'):
            self.run_script(env)


if __name__ == '__main__':
    unittest.main()

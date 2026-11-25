"""This project builds only. Any reviewed upload uses the separate guarded uploader."""
Import('env')
from SCons.Script import COMMAND_LINE_TARGETS
if any('upload' in str(t) for t in COMMAND_LINE_TARGETS):
    raise RuntimeError('ER6G startup project is BUILD ONLY; no serial/force uploads')

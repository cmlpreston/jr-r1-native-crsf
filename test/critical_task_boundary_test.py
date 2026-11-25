"""Structural guard for hardware-only task boundaries omitted by SDL builds."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root/'src/main.cpp').read_text()
control=s[s.index('void controlTask(void *)'):s.index('void crsfTask(void *)')]
assert 'Serial.' not in control and 'printf(' not in control
assert control.index('publishChannelOutputs(')<control.index('recordDiagnostic(')
assert 'physicalInputSampleFresh(' in control and 'immediatePhysicalCut()' in control
assert '!samplerFaultLatched||configuration.armRequested' in control
rf=s[s.index('void crsfTask(void *)'):s.index('// Read-only snapshots')]
assert 'Serial.' not in rf and 'printf(' not in rf
management=s[s.index('void sendManagementRequest('):s.index('void readReturnedTelemetry()')]
assert 'Serial.' not in management and 'printf(' not in management
pipeline=(root/'src/main_input_pipeline.inc').read_text()
sampler=pipeline[pipeline.index('void inputTask(void *)'):pipeline.index('void resetQueuedKeypadState')]
assert 'Serial.' not in sampler and 'transmitterUi.' not in sampler
assert 'vTaskDelayUntil' in sampler and 'inputLastSampleMs.store' in sampler
assert 'inputDispatchCurrent(' in pipeline
assert 'pendingPhysicalCut.exchange' in pipeline
assert 'requestPhysicalInputCut' in pipeline
assert 'physical.ready&&physicalInputSampleFresh' in s
print('Critical tasks: no formatting/USB/LVGL in producers, publish-before-diagnostics, independent heartbeat and queued-cut boundaries PASS')

#!/usr/bin/env python3
"""Prepare a disposable ELRS source checkout. Never uploads or edits upstream."""
import argparse, pathlib, shutil, subprocess, runpy
p=argparse.ArgumentParser();p.add_argument('--upstream',type=pathlib.Path,required=True);p.add_argument('--destination',type=pathlib.Path,required=True);p.add_argument('--direct-read-only',action='store_true');a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[2]
expected='a9d4a9cb'
commit=subprocess.check_output(['git','-C',str(a.upstream),'rev-parse','HEAD'],text=True).strip()
if not commit.startswith(expected):raise SystemExit('Review integration against required ELRS commit '+expected)
if subprocess.check_output(['git','-C',str(a.upstream),'status','--porcelain'],text=True).strip():raise SystemExit('Upstream must be clean')
if a.destination.exists():raise SystemExit('Destination already exists; choose a new disposable directory')
shutil.copytree(a.upstream,a.destination,ignore=shutil.ignore_patterns('.git','.pio','node_modules'))
src=a.destination/'src'
version='4.1.0-er4-esc-direct-read-r6' if a.direct_read_only else '4.1.0-er4-esc-bench-r6'
(src/'VERSION').write_text(version+' '+commit+'\n')
lib=src/'lib'/'Er4EscBridge';lib.mkdir()
for name in ('Er4EscBridge.cpp','Er4EscBridge.h'):shutil.copy2(root/'receiver'/'er4'/name,lib/name)
for name in ('esc_programmer_server.h','esc_programmer_wire.h','surpass_protocol.h'):shutil.copy2(root/'include'/name,lib/name)
def replace(file,old,new):
 path=src/file;s=path.read_text()
 if s.count(old)!=1:raise SystemExit('Integration anchor changed: '+str(file)+' '+old[:40])
 path.write_text(s.replace(old,new,1))
runpy.run_path(str(root/'receiver/elrs/teamrace_filter.py'))['apply'](src/'src/rx_main.cpp')
replace(pathlib.Path('src/rx_main.cpp'),'#include "rx-serial/SerialIO.h"','#include "rx-serial/SerialIO.h"\n#include "Er4EscBridge.h"')
replace(pathlib.Path('src/rx_main.cpp'),'  {&ServoOut_device, 1},','  // Dedicated ER4 programmer: PWM drivers intentionally omitted.')
# Disable serial ownership regardless of persisted PWM or UART settings.
replace(pathlib.Path('src/rx_main.cpp'),'static void setupSerial()\n{','static void setupSerial()\n{\n    serialIO=nullptr;return; // Dedicated programmer owns UART RX.\n')
replace(pathlib.Path('src/rx_main.cpp'),'    devicesUpdate(now);','    er4EscLoop();\n    devicesUpdate(now);')
replace(pathlib.Path('src/rx_main.cpp'),' || !teamraceHasModelMatch)',' || !er4EscSelected())')
replace(pathlib.Path('src/rx_main.cpp'),'    DataDlSender.ConfirmCurrentPayload(telemetryConfirmValue);',
        '    er4EscRecordAck(telemetryConfirmValue);\n    DataDlSender.ConfirmCurrentPayload(telemetryConfirmValue);')
replace(pathlib.Path('lib/rx-crsf/RXEndpoint.cpp'),'#include "RXEndpoint.h"','#include "RXEndpoint.h"\n#include "Er4EscBridge.h"')
replace(pathlib.Path('lib/rx-crsf/RXEndpoint.cpp'),'    if (handleRxTxMessage(message))','    if (er4EscHandle(message)) return;\n\n    if (handleRxTxMessage(message))')
# Queue ownership and the downlink ACK switch along with the active receiver.
replace(pathlib.Path('lib/rx-crsf/RXOTAConnector.h'),
        '    uint8_t GetFifoFullPct() const',
        '    void discardQueued() { messagePayloads.flush(); prioritizedCount=0; }\n    uint8_t GetFifoFullPct() const')
replace(pathlib.Path('lib/StubbornSender/stubborn_sender.h'),
        '    void ResetState();',
        '    void ResetState();\n    void RequestResync(bool observedConfirm) { ResetState(); telemetryConfirmExpectedValue=!observedConfirm; senderState=RESYNC; }')
replace(pathlib.Path('lib/WIFI/devWIFI.cpp'),'#include "device.h"',
        '#include "device.h"\n#include "Er4EscBridge.h"')
replace(pathlib.Path('lib/WIFI/devWIFI.cpp'),'    json["options"] = options;',
        '    json["options"] = options;\n    json["esc-bridge"] = er4EscDiagnostics();\n    json["esc-electrical"] = er4EscElectricalDiagnostics();')
replace(pathlib.Path('python/elrs_helpers.py'), '            sha = data.split()[1].strip()', '            sha = data.split()[1].strip()\n            ver = data.split()[0].strip()')
# Avoid the ESP32-only analog/servo peripherals: this build is ESP8285 only.
with (src/'platformio.ini').open('a') as f:f.write('''\n[env:r1_er4_esc_ESP8285_RX_bench]\nextends = env:Unified_ESP8285_2400_RX_via_UART\nbuild_flags =\n    ${env:Unified_ESP8285_2400_RX_via_UART.build_flags}\n    -D ER4_ESC_INTERFACE=%d\n    -D ER4_ESC_WRITES=0\n''' % int(a.direct_read_only))
print(src)
print('Build only: pio run -e r1_er4_esc_ESP8285_RX_bench. Hardware layout/options must be appended for RadioMaster ER4 before any upload.')

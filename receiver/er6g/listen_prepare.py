"""Install receive-only output-5 bench hooks into a disposable pinned source tree."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def apply(src):
    src=Path(src)
    def replace(name,old,new,count=1):
        path=src/name;s=path.read_text()
        if s.count(old)!=count:raise ValueError('Listen-only integration anchor changed: '+name)
        path.write_text(s.replace(old,new))
    import shutil
    lib=src/'lib/Er6gEscListen';lib.mkdir()
    for name in ('Er6gEscListen.h','Er6gEscListen.cpp'):
        shutil.copy2(ROOT/'receiver/er6g/listener'/name,lib/name)
    replace('src/rx_main.cpp','#include "rx-serial/SerialIO.h"','#include "rx-serial/SerialIO.h"\n#include "Er6gEscListen.h"')
    replace('src/rx_main.cpp','    devicesUpdate(now);','    er6gEscListenLoop();\n    devicesUpdate(now);')
    replace('src/rx_main.cpp','static void setupSerial()\n{','static void setupSerial()\n{\n    if(er6gEscListenOwnsPin(GPIO_PIN_RCSIGNAL_RX)||er6gEscListenOwnsPin(GPIO_PIN_RCSIGNAL_TX)){serialIO=nullptr;return;}\n')
    replace('src/rx_main.cpp','    switch(config.GetSerial1Protocol())','    if(er6gEscListenOwnsPin(serial1RXpin)||er6gEscListenOwnsPin(serial1TXpin))return;\n    switch(config.GetSerial1Protocol())')
    name='lib/ServoOutput/devServoOutput.cpp'
    replace(name,'#include "PWM.h"','#include "PWM.h"\n#include "Er6gEscListen.h"')
    replace(name,'static void servoWrite(uint8_t ch, uint16_t us)\n{','static void servoWrite(uint8_t ch, uint16_t us)\n{\n    if(er6gEscListenOwnsPin(GPIO_PIN_PWM_OUTPUTS[ch]))return;\n')
    replace(name,'        int8_t pin = GPIO_PIN_PWM_OUTPUTS[ch];','        int8_t pin = GPIO_PIN_PWM_OUTPUTS[ch];\n        if(er6gEscListenOwnsPin(pin)){servoPins[ch]=UNDEF_PIN;continue;}')
    replace(name,'            const rx_config_pwm_t *chConfig = config.GetPwmChannel(ch);','            if(er6gEscListenOwnsPin(GPIO_PIN_PWM_OUTPUTS[ch]))continue;\n            const rx_config_pwm_t *chConfig = config.GetPwmChannel(ch);')
    replace('lib/WIFI/devWIFI.cpp','#include "device.h"','#include "device.h"\n#include "Er6gEscListen.h"')
    replace('lib/WIFI/devWIFI.cpp','    json["options"] = options;','    json["options"] = options;\n    json["esc-listener"] = er6gEscListenDiagnostics();')

def apply_read(src):
    apply(src)
    import shutil
    src=Path(src);lib=src/'lib/Er6gEscListen'
    shutil.copy2(ROOT/'receiver/er6g/reader/Er6gEscRead.cpp',lib/'Er6gEscListen.cpp')
    shutil.copy2(ROOT/'receiver/er6g/reader/Er6gEscRead.h',lib/'Er6gEscRead.h')
    shutil.copy2(ROOT/'include/surpass_protocol.h',lib/'surpass_protocol.h')
    for name in ('Er6gEscStartupPeer.h','Er6gEscStartupPeer.cpp'):
        shutil.copy2(ROOT/'receiver/er6g/reader'/name,lib/name)
    shutil.copy2(ROOT/'include/er6g_esc_startup.h',lib/'er6g_esc_startup.h')
    main=src/'src/rx_main.cpp';m=main.read_text()
    assert m.count('void setup()\n{')==1
    m=m.replace('#include "Er6gEscListen.h"','#include "Er6gEscListen.h"\n#include "Er6gEscRead.h"',1)
    m=m.replace('void setup()\n{','void setup()\n{\n    er6gEscReadBootProbe();',1)
    main.write_text(m+'\nbool er6gEscReadWifiAllowed(){return connectionState==wifiUpdate;}\n')
    path=src/'lib/WIFI/devWIFI.cpp';s=path.read_text()
    s=s.replace('#include "Er6gEscListen.h"','#include "Er6gEscListen.h"\n#include "Er6gEscRead.h"',1)
    anchor='  server.on("/config", HTTP_GET, GetConfiguration);'
    assert s.count(anchor)==1
    route=r'''  // Explicit bench-only POST; no raw bytes or setting-write operation exists.
  server.on("/esc-read-test", HTTP_POST, [](AsyncWebServerRequest *request) {
    if(request->header("X-R1-ESC-Bench")!="motor-disconnected-card-removed") {
      request->send(403,"text/plain","Confirm motor disconnected and programming card removed");return;
    }
    if(!request->hasParam("operation",true)){request->send(400,"text/plain","Missing operation");return;}
    const String op=request->getParam("operation",true)->value();uint8_t param=0;
    if(op=="release-startup"){
      const bool ok=er6gEscReadReleaseEntry(true);
      request->send(ok?202:409,"text/plain",ok?"Release queued":"No startup hold armed");return;
    }
    if(op!="handshake"&&op!="read"&&op!="entry"&&op!="arm-startup"){request->send(400,"text/plain","Only startup hold/release, entry, handshake or read allowed");return;}
    if(op=="read"){
      if(!request->hasParam("parameter",true)){request->send(400,"text/plain","Missing parameter");return;}
      const String text=request->getParam("parameter",true)->value();
      if(text.length()==0||text.length()>2){request->send(400,"text/plain","Parameter must be0..14");return;}
      for(unsigned i=0;i<text.length();++i)if(text[i]<'0'||text[i]>'9'){request->send(400,"text/plain","Invalid parameter");return;}
      const int value=text.toInt();if(value>14){request->send(400,"text/plain","Parameter must be0..14");return;}param=value;
    }
    const bool ok=er6gEscReadStart(op=="arm-startup"?3:op=="entry"?2:op=="read"?1:0,param,true);
    request->send(ok?202:409,"text/plain",ok?"Queued once; inspect esc-listener diagnostics":"Busy, unavailable, or not in Wi-Fi bench mode");
  });
'''
    path.write_text(s.replace(anchor,route+anchor,1))

def apply_mode(src,single_stage=False,write_enabled=False):
    apply_read(src)
    import shutil
    src=Path(src);lib=src/'lib/Er6gEscListen'
    for name in ('Er6gEscMode.h','Er6gEscMode.cpp'):
        shutil.copy2(ROOT/'receiver/er6g/reader'/name,lib/name)
    shutil.copy2(ROOT/'include/esc_programmer_wire.h',lib/'esc_programmer_wire.h')
    if single_stage:
        shutil.copy2(ROOT/'receiver/er6g/reader/Er6gEscSingleStartup.cpp',lib/'Er6gEscStartupPeer.cpp')
    main=src/'src/rx_main.cpp';s=main.read_text()
    s=s.replace('#include "Er6gEscRead.h"','#include "Er6gEscRead.h"\n#include "Er6gEscMode.h"',1)
    s=s.replace('    er6gEscListenLoop();','    er6gEscListenLoop();\n    er6gEscModeLoop();',1)
    main.write_text(s)
    p=src/'lib/rx-crsf/RXEndpoint.cpp';s=p.read_text()
    assert s.count('    if (handleRxTxMessage(message))')==1
    s=s.replace('#include "RXEndpoint.h"','#include "RXEndpoint.h"\n#include "Er6gEscMode.h"',1)
    s=s.replace('    if (handleRxTxMessage(message))','    if (er6gEscModeHandle(message)) return;\n    if (handleRxTxMessage(message))',1)
    p.write_text(s)
    # One receiver owns driving AND programming. Remove only the obsolete
    # Team Race model-selection gate; retain model-match and RF failsafes.
    main=src/'src/rx_main.cpp';s=main.read_text().replace('teamraceHasModelMatch','true');main.write_text(s)
    import runpy
    runpy.run_path(str(ROOT/'receiver/er6g/drive_pwm_prepare.py'))['apply'](src/'lib/ServoOutput/devServoOutput.cpp')
    with (src/'platformio.ini').open('a') as f:
        f.write('\n[env:Unified_ESP32_2400_RX_via_R1_ESC_MODE]\nextends = env:Unified_ESP32_2400_RX_via_UART\nbuild_flags =\n    ${env:Unified_ESP32_2400_RX_via_UART.build_flags}\n    -D ER6G_ESC_DRIVE_MODE=1\n'+('    -D ER6G_ESC_SINGLE_STAGE=1\n' if single_stage else '')+('    -D ER6G_ESC_WRITES=1\n' if write_enabled else ''))

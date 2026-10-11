// Included inside main.cpp's namespace. No radio, UI or control tasks are started.
#if JR_R1_INPUT_BENCH
#ifndef JR_R1_XIAO_PCB
#error "Input bench requires XIAO schematic (5) pin map"
#endif
uint32_t benchPressed=0,benchReleased=0,benchPending=0;
uint32_t benchNextPoll=0,benchNextStatus=0,benchNextProbe=0,benchNextAnalog=0;
bool benchAnalog=false;
char benchCommand[24]={};size_t benchCommandLength=0;
void benchStatus(){
    Serial.printf("BENCH: K1=%s K2=%s | CRSF OFF | LCD OFF | tests since boot\n",pcbKeypad.online()?"PASS":"OFFLINE",pcbControls.online()?"PASS":"OFFLINE");
    for(unsigned i=static_cast<unsigned>(PhysicalControlId::KeyMode);i<kPhysicalControlCount;++i){
        const uint32_t bit=1UL<<i;
        Serial.printf("  %-15s %-10s %s\n",physicalControlDescriptor(i).name,r1InputEndpointLabel(static_cast<PhysicalControlId>(i)),(benchReleased&bit)?"PRESS+RELEASE OK":(benchPressed&bit)?"PRESSED; release to complete":"NOT TESTED");
    }
}
void benchBegin(){
    pinMode(TFT_BACKLIGHT_PIN,OUTPUT);digitalWrite(TFT_BACKLIGHT_PIN,LOW);
    Serial.println("\nR-1 PCB INPUT BENCH: no LCD required; CRSF transmission disabled");
    Serial.println("Press/release each key. Commands: STATUS, INPUTS, CLEAR, ANALOG, HELP");
    Wire.begin(TOUCH_SDA_PIN,TOUCH_SCL_PIN);Wire.setClock(100000);
    pcbIoBegin([](uint8_t attempt,bool a,bool b){Serial.printf("BENCH probe %u K1=%s K2=%s\n",attempt,a?"PASS":"FAIL",b?"PASS":"FAIL");});
    physicalInputs.begin(kR1InputMap,kR1InputMapCount);
    analogReadResolution(12);
    benchStatus();benchNextStatus=millis()+10000;
}
void benchLoop(){
    const uint32_t now=millis();
    while(Serial.available()){
        const char c=Serial.read();
        if(c=='\r'||c=='\n'){
            if(!benchCommandLength)continue;
            benchCommand[benchCommandLength]=0;
            if(!strcmp(benchCommand,"CLEAR")){benchPressed=benchReleased=benchPending=0;Serial.println("BENCH checklist cleared");}
            else if(!strcmp(benchCommand,"ANALOG")){benchAnalog=!benchAnalog;Serial.printf("BENCH ADC samples %s (unconnected ADCs float)\n",benchAnalog?"ON":"OFF");}
            else if(!strcmp(benchCommand,"INPUTS")){
                uint16_t a=0xffff,b=0xffff;bool ka=pcbKeypad.read(a),kb=pcbControls.read(b);
                Serial.printf("RAW K1=%s 0x%04X K2=%s 0x%04X (inputs active LOW)\n",ka?"OK":"FAIL",a,kb?"OK":"FAIL",b);
            }else if(strcmp(benchCommand,"STATUS"))Serial.println("Commands: STATUS, INPUTS, CLEAR, ANALOG (toggle)");
            benchCommandLength=0;benchStatus();
        }else if(benchCommandLength<sizeof(benchCommand)-1)benchCommand[benchCommandLength++]=(c>='a'&&c<='z')?c-'a'+'A':c;
    }
    if(int32_t(now-benchNextProbe)>=0){
        benchNextProbe=now+1000;
        if(!pcbKeypad.online()){bool ok=pcbKeypad.configure(0x7f7f,0x7f1f,0);Serial.printf("BENCH K1 retry %s\n",ok?"PASS":"FAIL");}
        if(!pcbControls.online()){bool ok=pcbControls.configure(0x767f,0x761f,0x8000);Serial.printf("BENCH K2 retry %s\n",ok?"PASS":"FAIL");}
    }
    if(int32_t(now-benchNextPoll)>=0){
        benchNextPoll=now+5;
        McpInputSnapshot inputs[]={{0x20,0xffff,false},{0x21,0xffff,false}};
        inputs[0].available=pcbKeypad.online()&&pcbKeypad.read(inputs[0].levels);
        inputs[1].available=pcbControls.online()&&pcbControls.read(inputs[1].levels);
        physicalInputEvents.clear();physicalInputs.update(inputs,2,0,now,physicalInputEvents);
        for(unsigned i=0;i<kPhysicalControlCount;++i)if(physicalInputs.state(static_cast<PhysicalControlId>(i)).faulted)benchPending&=~(1UL<<i);
        for(size_t j=0;j<physicalInputEvents.size();++j){
            const auto &e=physicalInputEvents[j];const unsigned i=static_cast<unsigned>(e.input);const uint32_t bit=1UL<<i;
            if(physicalInputs.state(e.input).faulted)continue;
            if(e.event==InputEvent::Pressed){benchPressed|=bit;benchPending|=bit;}
            if(e.event==InputEvent::Released&&(benchPending&bit)){benchReleased|=bit;benchPending&=~bit;}
            if(e.event==InputEvent::InvalidState)benchPending&=~bit;
            Serial.printf("BENCH %lums %-15s %-10s %-12s value=%d\n",static_cast<unsigned long>(now),physicalControlDescriptor(i).name,r1InputEndpointLabel(e.input),inputEventName(e.event),e.value);
        }
        if(physicalInputEvents.overflowed())Serial.println("BENCH event overflow: test one control at a time");
    }
    if(benchAnalog&&int32_t(now-benchNextAnalog)>=0){benchNextAnalog=now+500;Serial.printf("ADC throttle=%u steering=%u battery=%u\n",analogRead(THROTTLE_ADC_PIN),analogRead(STEERING_ADC_PIN),analogRead(TX_BATTERY_ADC_PIN));}
    if(int32_t(now-benchNextStatus)>=0){benchNextStatus=now+10000;benchStatus();}
    delay(1);
}
#endif

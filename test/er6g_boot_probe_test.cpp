#define ER6G_READ_TEST 1
#include "rxtx_intf.h"
bool er6gEscReadWifiAllowed(){return connectionState==wifiUpdate;}
#include "../receiver/er6g/reader/Er6gEscRead.cpp"
#include <cstring>
unsigned injected=0;int scenario=0;
void driveBench(){
    if(injected==0&&transmitted.size()==2&&listenerUs>=60000){
        uartInput=scenario==2?std::deque<uint8_t>{0}:transmitted;txDone=true;injected=1;
    }
    if(injected==1&&transmitted.size()==5){uartInput={0x1c,0,0};txDone=true;injected=2;}
    else if(injected==2&&scenario!=1){uartInput={0x1b,uint8_t(scenario==3?0xff:1)};injected=3;}
}
void resetProbe(int which){
    scenario=which;bootRan=false;bootStage=0;bootValue=255;listenerUs=20000;
    injected=0;transmitted.clear();uartInput.clear();txDone=false;delayHook=driveBench;
}
int main(){
    resetProbe(0);er6gEscReadBootProbe();
    assert(ready&&bootReadyUs<50000&&!bootProbe&&!busy.load());
    assert(std::strstr(bootResult,"PASS")&&bootValue==1&&transmitted.size()==5);
    assert(entryPulses==0&&outputLatch==1&&gpioMode==GPIO_MODE_INPUT);
    const auto count=txStarted;er6gEscReadBootProbe();assert(txStarted==count); // once per boot
    assert(!er6gEscReadStart(1,0,true)); // early boot permission does not leak into RF
    resetProbe(1);er6gEscReadBootProbe();
    assert(std::strstr(bootResult,"timed out")&&transmitted.size()==5&&bootDurationUs>=3000000);
    resetProbe(2);er6gEscReadBootProbe();
    assert(std::strstr(bootResult,"response failed")&&transmitted.size()==2&&!busy.load());
    resetProbe(3);er6gEscReadBootProbe();
    assert(std::strstr(bootResult,"Boot read failed")&&bootValue==255&&!busy.load());
    assert(!bootProbe&&outputLatch==1&&gpioMode==GPIO_MODE_INPUT&&entryPulses==0);
    puts("ER6G early boot: one immediate response/read0, no-reply/failure bounds, no pulses/writes/retries or leaked RF permission PASS");
}

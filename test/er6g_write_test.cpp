#define ER6G_ESC_DRIVE_MODE 1
#define ER6G_ESC_WRITES 1
#define ER6G_READ_TEST 1
#include "rxtx_intf.h"
#include "../receiver/er6g/reader/Er6gEscRead.cpp"
#include <cstdio>
bool rfSafe=true,wifiSafe=false;
bool er6gStartupProgramming(){return true;}
void er6gStartupModeBegin(){}
bool er6gStartupResume(Er6gStartup::Record &){return false;}
void er6gStartupArmNextBoot(){}
bool er6gStartupCancel(){return true;}
const char *er6gStartupDiagnostics(){return "test";}
bool er6gEscReadRfAllowed(){return rfSafe;}
bool er6gEscReadWifiAllowed(){return wifiSafe;}
void tick(uint32_t us){listenerUs=us;er6gEscListenLoop();}
void beginWrite(uint32_t base){
 transmitted.clear();txDone=false;assert(er6gEscWriteStart(8,1,true));
 assert(!er6gEscWriteStart(8,2,true));tick(base);
 assert((transmitted==std::deque<uint8_t>{0x1b,8,1}));
 txDone=true;uartInput=transmitted;tick(base+45000);assert(!outputEnabled&&echoAt==3);
}
void ackAndRead(uint32_t base){
 uartInput={0x1b,0x0a};tick(base+80000);assert(transmitted.size()==3);
 tick(base+179000);assert(transmitted.size()==3);
 tick(base+182000);assert((transmitted==std::deque<uint8_t>{0x1b,8,1,0x1c,8,0}));
 uartInput={0x1c,8,0};tick(base+227000);assert(!outputEnabled);
}
int main(){
 tick(10000);bootValue=1;bootResult="PASS; verified entry";
 assert(!er6gEscReadStart(4,8,true)); // Wi-Fi operation API cannot issue writes.
 assert(!er6gEscWriteStart(8,1,false)&&!er6gEscWriteStart(15,0,true)&&!er6gEscWriteStart(8,5,true));
 rfSafe=false;assert(!er6gEscWriteStart(8,1,true));rfSafe=true;
 bootValue=255;assert(!er6gEscWriteStart(8,1,true));bootValue=1;
 beginWrite(100000);ackAndRead(100000);uartInput={0x1b,1};tick(360000);
 uint8_t value=255;assert(er6gEscWriteValue(value)&&value==1&&!busy.load());
 beginWrite(3100000);uartInput={0x1b,0};tick(3180000);
 assert(!busy.load()&&!er6gEscWriteValue(value)&&transmitted.size()==3);
 beginWrite(6100000);ackAndRead(6100000);uartInput={0x1b,0};tick(6360000);
 assert(!busy.load()&&!er6gEscWriteValue(value)&&value==0);
 beginWrite(9100000);tick(11000000);assert(!busy.load()&&transmitted.size()==3);
 tick(12000000);assert(transmitted.size()==3); // Never retry an uncertain write.
 beginWrite(13100000);ackAndRead(13100000);rfSafe=false;wifiSafe=true;tick(13330000);
 assert(!busy.load()&&!outputEnabled&&!er6gEscWriteValue(value));
 puts("ER6G write: verified RF/CUT session only, one frame, ACK then delayed readback, matching value required, failures/timeouts/link loss release and never retry PASS");
}

#define ER6G_READ_TEST 1
#include "rxtx_intf.h"
bool er6gEscReadWifiAllowed(){return connectionState==wifiUpdate;}
#include "../receiver/er6g/reader/Er6gEscRead.cpp"
#include <cstring>
void tick(uint32_t us){listenerUs=us;dispatchTimer();er6gEscListenLoop();}
int main(){
 release();assert(!outputEnabled&&outputSignal==256); // ROM matrix routing must not re-enable RX-only pin.
 tick(10000);assert(ready&&transmitted.empty()&&gpioMode==GPIO_MODE_INPUT&&entryPulses==0&&!mockTimer.running);
 assert(!er6gEscReadStart(0,0,true));connectionState=wifiUpdate;
 assert(!er6gEscReadStart(0,0,false));assert(!er6gEscReadStart(4,0,true));
 assert(!er6gEscReadStart(1,15,true)); // no write/raw-byte operation exists
 for(unsigned p=0;p<15;++p){
  const unsigned base=100000+p*300000;transmitted.clear();txDone=false;
  assert(er6gEscReadStart(1,p,true));assert(!er6gEscReadStart(1,p,true));
  tick(base);assert((transmitted==std::deque<uint8_t>{0x1c,uint8_t(p),0}));
  txDone=true;tick(base+1000);assert(gpioMode==GPIO_MODE_INPUT_OUTPUT_OD&&sending);
  uartInput=transmitted;tick(base+45000);assert(gpioMode==GPIO_MODE_INPUT&&!outputEnabled&&echoAt==3);
  uartInput={0x1b,0};tick(base+75000);assert(!busy.load()&&lastValue==0);
 }
 assert(txStarted==15&&txCompleted==15&&echoMatched==45&&timeouts==0);
 transmitted.clear();txDone=false;assert(er6gEscReadStart(0,0,true));tick(5000000);
 assert((transmitted==std::deque<uint8_t>{0x1c,0xff}));uartInput=transmitted;txDone=true;tick(5050000);
 assert(!busy.load()&&strstr(status,"acceptance not confirmed"));
 // A busy/low bus produces no frame and times out without retry.
 transmitted.clear();padLevel=0;assert(er6gEscReadStart(1,0,true));tick(5100000);assert(transmitted.empty());
 tick(7100000);assert(!busy.load()&&timeouts==1&&transmitted.empty());padLevel=1;
 // Missing echo is reported; timeouts cannot retransmit automatically.
 assert(er6gEscReadStart(1,0,true));tick(7200000);const auto sent=transmitted.size();txDone=true;
 tick(9200000);assert(!busy.load()&&timeouts==2);tick(9300000);assert(transmitted.size()==sent);
 // Leaving Wi-Fi releases the pin immediately, including an in-flight request.
 assert(er6gEscReadStart(1,0,true));tick(9400000);connectionState=connected;tick(9401000);
 assert(!busy.load()&&gpioMode==GPIO_MODE_INPUT);connectionState=wifiUpdate;
 assert(er6gEscReadStart(1,0,true));tick(9500000);uartInput={0xff};tick(9505000);
 assert(!busy.load()&&echoMismatch==1&&gpioMode==GPIO_MODE_INPUT);
 shortTx=true;assert(er6gEscReadStart(1,0,true));tick(9600000);assert(!busy.load()&&strstr(status,"Short TX"));
 // Entry is explicit, single-shot and never emits serial data without ESC startup.
 shortTx=false;transmitted.clear();txDone=false;
 assert(er6gEscReadStart(2,0,true));tick(10000000);
 assert(entryState==1&&outputLatch==0&&mockTimer.running&&transmitted.empty());
 uartEvents.push_back({UART_FRAME_ERR});uartInput={0};tick(10068000);
 assert(outputLatch==0&&entryState==1);tick(10069010);
 assert(outputLatch==1&&gpioMode==GPIO_MODE_INPUT&&entryState==2&&uartEvents.empty());
 uartInput={0x1b};tick(10180000);assert(transmitted.empty());
 uartInput={0xff};tick(10205000);assert(entryState==3&&transmitted.empty());
 tick(10244000);assert(transmitted.empty());tick(10246000);
 assert((transmitted==std::deque<uint8_t>{0x1c,0xff}));
 txDone=true;uartInput=transmitted;tick(10276000);
 assert(!busy.load()&&entryStartups==1&&strstr(status,"verify with a read"));
 // No startup: no handshake, no retries, pin remains released at timeout.
 transmitted.clear();assert(er6gEscReadStart(2,0,true));tick(11000000);tick(11069010);tick(12900000);
 assert(!busy.load()&&transmitted.empty()&&outputLatch==1&&!mockTimer.running);
 // Timer releases even without a main-loop call. Excessive pulse latency aborts.
 assert(er6gEscReadStart(2,0,true));tick(13000000);listenerUs=13080000;dispatchTimer();
 assert(outputLatch==1);tick(13080001);assert(!busy.load()&&strstr(status,"overrun"));
 // Mode exit during LOW stops the timer and releases immediately.
 assert(er6gEscReadStart(2,0,true));tick(14000000);connectionState=connected;tick(14010000);
 assert(!busy.load()&&!mockTimer.running&&outputLatch==1&&gpioMode==GPIO_MODE_INPUT);
 connectionState=wifiUpdate;failTimerStart=true;assert(er6gEscReadStart(2,0,true));tick(15000000);
 assert(!busy.load()&&outputLatch==1&&strstr(status,"timer failed"));
 failTimerStart=false;
 assert(er6gEscReadStart(2,0,true));tick(16000000);listenerUs=16080000;er6gEscListenLoop();
 assert(!busy.load()&&!mockTimer.running&&outputLatch==1&&strstr(status,"timer late"));
 // Startup hold is explicit, single-flight, bounded, and requires explicit release.
 transmitted.clear();txDone=false;assert(!er6gEscReadReleaseEntry(true));
 assert(er6gEscReadStart(3,0,true));tick(17000000);
 assert(holdReady.load()&&outputLatch==0&&mockTimer.deadline>=196999000);
 assert(!er6gEscReadReleaseEntry(false));tick(18000000);assert(outputLatch==0&&transmitted.empty());
 assert(er6gEscReadReleaseEntry(true));tick(18010000);
 assert(!holdReady.load()&&outputLatch==1&&entryState==2&&!mockTimer.running);
 uartInput={0x1b,0xff};tick(18100000);tick(18141000);
 assert((transmitted==std::deque<uint8_t>{0x1c,0xff}));txDone=true;uartInput=transmitted;tick(18171000);
 assert(!busy.load()&&strstr(status,"verify with a read"));
 transmitted.clear();assert(er6gEscReadStart(3,0,true));tick(20000000);tick(200000010);
 assert(outputLatch==1&&!holdReady.load()&&entryState==2&&transmitted.empty());
 tick(202000000);assert(!busy.load()&&transmitted.empty());
 assert(er6gEscReadStart(3,0,true));tick(203000000);connectionState=connected;tick(203001000);
 assert(!holdReady.load()&&outputLatch==1&&!mockTimer.running&&!er6gEscReadReleaseEntry(true));
 connectionState=wifiUpdate;
 assert(strstr(er6gEscListenDiagnostics(),"writes=disabled pwm=disabled"));
 puts("ER6G reader: explicit Wi-Fi-only gates, open-drain same-pin TX, all15read frames/echo/replies, handshake, explicit69ms entry/conditional response, timer failure/overrun/abort release, no writes/retries PASS");
}

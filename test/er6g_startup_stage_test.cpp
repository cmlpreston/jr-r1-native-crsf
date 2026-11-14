#define ER6G_READ_TEST 1
#include "../receiver/er6g/startup/src/main.cpp"
#include <iostream>
bool supplyStartup=true,injected=false;uint32_t startupAt=100000;
void supplyEsc(){if(supplyStartup&&!injected&&listenerUs>=startupAt){uartInput={0x1b,0xff};injected=true;}}
void reset(){
 savedTicket=Er6gStartup::ProgramTicket;savedLock=0;result={};listenerUs=20000;uartInput.clear();transmitted.clear();savedRecord.clear();selects=0;
 failEcho=failUart=failStore=badHash=tooSmall=failSelect=noReply=false;replyValue=1;
 supplyStartup=true;injected=false;startupAt=100000;padLevel=1;delayHook=supplyEsc;
 peerImage="\xBE\xEF\xCA\xFE" "UNIFIED_ESP32_2400_RX full-app 4.1.0-er6g-esc-mode-r14";
}
bool run(){try{setup();}catch(const std::runtime_error&e){assert(!strcmp(e.what(),"mock restart"));return true;}return false;}
Er6gStartup::Record record(){Er6gStartup::Record r;assert(savedRecord.size()==sizeof(r));memcpy(&r,savedRecord.data(),sizeof(r));assert(r.valid());return r;}
int main(){
 release();assert(!outputEnabled&&outputSignal==256); // Also protect the small stage after TX.
 reset();assert(run()&&selects==1);assert(record().result==Er6gStartup::ReadVerified&&record().value==1&&transmitted.size()==7);
 assert(gpioMode==1&&!outputEnabled&&outputLatch==1&&record().readProgress==5&&record().rxCount>=7&&(record().rxTail&0xffff)==0x1b01);
 reset();startupAt=40000;assert(run()&&record().result==Er6gStartup::ReadVerified&&transmitted.size()==5);
 reset();padLevel=0;assert(run()&&record().result==Er6gStartup::HandshakeFailed&&transmitted.empty());
 reset();supplyStartup=false;assert(run()&&transmitted.size()==5&&record().result==Er6gStartup::ReadVerified);
 reset();supplyStartup=false;noReply=true;assert(run()&&transmitted.size()==5&&record().result==Er6gStartup::ReadFailed&&record().readProgress==3);
 reset();failUart=true;assert(run()&&transmitted.empty()&&record().result==Er6gStartup::UartFailed);
 reset();failEcho=true;assert(run()&&transmitted.size()==2&&record().result==Er6gStartup::HandshakeFailed);
 reset();replyValue=255;assert(run()&&record().result==Er6gStartup::ReadFailed);
 reset();failStore=true;assert(run()&&selects==1&&savedRecord.empty()&&transmitted.empty()); // recovery never depends on NVS write
 for(int scenario=0;scenario<4;++scenario){
  reset();if(scenario==0)peerImage="wrong target";if(scenario==1)tooSmall=true;if(scenario==2)badHash=true;if(scenario==3)peerImage="UNIFIED_ESP32_2400_RX wrong version";
  assert(!run()&&selects==0&&savedRecord.empty());
 }
 reset();savedTicket=0;assert(run()&&transmitted.empty()&&record().result==Er6gStartup::DriveBypass);
 reset();savedTicket=123;assert(run()&&transmitted.empty());
 reset();failSelect=true;assert(!run()&&selects==1);
 auto r=record();r.value^=1;assert(!r.valid());
 std::cout<<"ER6G startup stage: 40ms quiet guard/blocked bus/missed-or-later-startup/read, UART/echo/read/store-failure recovery, wrong-peer/size/hash rejection, sealed record, no setting writes PASS\n";
}

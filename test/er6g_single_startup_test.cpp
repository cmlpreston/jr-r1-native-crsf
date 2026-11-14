#define ER6G_ESC_DRIVE_MODE 1
#define ER6G_ESC_SINGLE_STAGE 1
#define ER6G_READ_TEST 1
#include <Arduino.h>
#include "rxtx_intf.h"
#include "../receiver/er6g/reader/Er6gEscSingleStartup.cpp"
#include "../receiver/er6g/reader/Er6gEscRead.cpp"
#include <cstdio>
bool er6gEscReadWifiAllowed(){return false;}
bool er6gEscReadRfAllowed(){return false;}
unsigned echoed=0;bool replied=false;
void escReply(){
 while(echoed<transmitted.size())uartInput.push_back(transmitted[echoed++]);
 // The external ESC reply only reaches the UART after pad output is disabled.
 if(transmitted.size()==5&&!outputEnabled&&!replied){
  uartInput.push_back(0x1b);uartInput.push_back(1);replied=true;
 }
}
int main(){
 resetReason=ESP_RST_POWERON;
 er6gEscReadBootProbe();
 assert(!er6gStartupProgramming()&&!ready&&!attempted&&!outputEnabled&&transmitted.empty());
 // Prepare and cancel touch only NVS: no second-app selection or GPIO activity.
 assert(er6gStartupPrepare()&&er6gStartupPending());
 assert(er6gStartupCancel()&&!er6gStartupPending()&&savedTicket==0);
 failStore=true;assert(!er6gStartupPrepare());failStore=false;
 assert(er6gStartupPrepare());
 bootRan=false;txDone=true;delayHook=escReply;
 er6gEscReadBootProbe();
 assert(savedTicket==0&&savedLock==Er6gStartup::ProgramTicket);
 assert(er6gStartupProgramming()&&er6gEscReadSessionReady()&&replied&&!outputEnabled);
 assert((transmitted==std::deque<uint8_t>{0x1c,0xff,0x1c,0,0}));
 assert(!er6gStartupPrepare());
 assert(er6gStartupCancel()&&er6gStartupProgramming());
 // A software reset keeps PWM blocked but cannot repeat the handshake or use
 // a stale successful result. In hardware globals start afresh on each reset.
 resetReason=ESP_RST_SW;bootRan=false;bootValue=255;const auto count=transmitted.size();
 er6gEscReadBootProbe();
 assert(er6gStartupProgramming()&&!er6gEscReadSessionReady()&&transmitted.size()==count);
 resetReason=ESP_RST_POWERON;bootRan=false;
 er6gEscReadBootProbe();
 assert(!er6gStartupProgramming()&&!outputEnabled&&savedLock==0&&transmitted.size()==count);
 assert(strstr(er6gStartupDiagnostics(),"startupStrategy=single-app"));
 puts("Single-app startup: Drive no UART, one-shot NVS request, corrected RX release/read, no handoff/OTA selection, reset lock/no re-entry, cold Drive recovery PASS");
}

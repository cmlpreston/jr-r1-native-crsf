#define ER6G_ESC_DRIVE_MODE 1
#define ER6G_READ_TEST 1
#include "../receiver/er6g/reader/Er6gEscRead.cpp"
#include <cstdio>
bool programMode=false;unsigned cancelled=0;
void er6gStartupModeBegin(){}
bool er6gStartupProgramming(){return programMode;}
bool er6gStartupResume(Er6gStartup::Record &r){r.result=Er6gStartup::ReadVerified;r.value=1;return true;}
void er6gStartupArmNextBoot(){assert(false);} // mode path must never re-arm automatically
bool er6gStartupCancel(){++cancelled;return true;}
const char *er6gStartupDiagnostics(){return "test";}
bool er6gEscReadRfAllowed(){return programMode;}
bool er6gEscReadWifiAllowed(){return false;}
int main(){
 outputLatch=7;gpioMode=9;er6gEscReadBootProbe();er6gEscListenLoop();
 assert(!attempted&&!ready&&!er6gEscListenOwnsPin(2)&&outputLatch==1&&gpioMode==GPIO_MODE_INPUT&&transmitted.empty());
 assert(!er6gEscReadStart(1,0,true));
 programMode=true;bootRan=false;er6gEscReadBootProbe();
 assert(er6gEscListenOwnsPin(2)&&er6gEscReadSessionReady()&&cancelled==1&&transmitted.empty());
 er6gEscListenLoop();assert(ready&&attempted&&gpioMode==GPIO_MODE_INPUT);
 assert(er6gEscReadStart(1,0,true)&&er6gEscReadBusy());
 er6gEscReadAbort();assert(!er6gEscReadBusy()&&er6gEscListenOwnsPin(2)&&transmitted.empty());
 puts("Actual mode reader: normal boot releases GPIO without UART; programming boot retains pin ownership, no repeated handshake, abort never restores PWM PASS");
}

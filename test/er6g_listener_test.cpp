// Execute the canonical listener against UART/GPIO boundaries. No TX API exists.
#include "../receiver/er6g/listener/Er6gEscListen.cpp"
#include <cstring>
int main(){
 assert(er6gEscListenOwnsPin(2)&&!er6gEscListenOwnsPin(1)&&!er6gEscListenOwnsPin(3));
 er6gEscListenLoop();er6gEscListenLoop();
 assert(configs==1&&installs==1&&pinRoutes==1&&inputSet==1&&ready);
 uartInput={0x1b};er6gEscListenLoop();assert(byteCount==1&&startups==0);
 uartInput={0xff};er6gEscListenLoop();assert(byteCount==2&&startups==1);
 uartEvents={{UART_FIFO_OVF},{UART_FRAME_ERR},{UART_PARITY_ERR}};er6gEscListenLoop();
 assert(overflows==1&&frameErrors==1&&parityErrors==1);
 for(unsigned i=0;i<80;++i)uartInput.push_back(i);
 er6gEscListenLoop();assert(uartInput.size()==64); // bounded read, no blocking
 for(unsigned i=0;i<4;++i)er6gEscListenLoop();assert(byteCount==82&&uartInput.empty());
 const char *text=er6gEscListenDiagnostics();
 assert(strstr(text,"tx=disabled pwm=disabled ready=1")&&strstr(text,"bytes=82 startups=1"));
 const char *trace=strstr(text," trace=");assert(trace);unsigned commas=0;
 for(const char *p=trace;*p;++p)if(*p==',')++commas;assert(commas==63);
 // Initialization failure leaves only input configuration and is visible, not retried.
 attempted=ready=false;failInstall=true;er6gEscListenLoop();assert(!ready);
 const unsigned before=installs;er6gEscListenLoop();assert(installs==before);
 assert(strstr(er6gEscListenDiagnostics(),"ready=0 initError=-1"));
 puts("ER6G listener: RX-only routing, split startup detection, bounded capture/ring, UART errors and init-failure visibility PASS");
}

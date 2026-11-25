// Exercise actual GPIO/UART adapter, including open-drain polarity and aborts.
#define ER4_ESC_INTERFACE 1
#define ER4_ESC_WRITES 0
#include "../receiver/er4/Er4EscBridge.cpp"
#include <cstring>
static void clockAt(uint32_t us){mockUs=us;mockNow=us/1000;}
static void request(EscWire::Op op,uint16_t sequence){
 EscWire::Message m;m.session=77;m.sequence=sequence;m.op=op;m.parameter=0;m.value=0;
 uint8_t f[19];assert(EscWire::build(m,false,f,sizeof(f))==19);
 assert(er4EscHandle(reinterpret_cast<const crsf_header_t*>(f)));
}
static EscWire::Message reply(){
 auto &f=crsfRouter.replies.back();EscWire::Message m;
 assert(EscWire::parse(f[2],f.data()+3,17,true,m));return m;
}
int main(){
 Serial.input={0x1b,0xff};Serial.overrun=true;Serial.rxError=true;
 clockAt(10000);er4EscLoop();
 assert(rxInactive==2&&startupInactive==1&&rxOverruns==1&&rxErrors==1);
 assert(txStarted==0); // Passive observation cannot handshake or drive.

 assert(pinWrites.front().pin==9&&pinWrites.front().value==HIGH);
 assert(pinModes.size()==1&&pinModes[0].pin==9&&pinModes[0].value==OUTPUT_OPEN_DRAIN);
 assert(Serial.baud==800); // UART receives on GPIO3; no normal PWM/UART TX.
 connectionState=connected;connectionHasModelMatch=true;ChannelData[11]=1811;
 request(EscWire::Op::Hello,1);er4EscLoop();
 assert(reply().flags==(EscWire::SelectorAvailable|EscWire::Interface));
 request(EscWire::Op::Write,2);er4EscLoop();assert(reply().status==EscWire::Status::Rejected);
 request(EscWire::Op::Read,3);
 const uint8_t expected[]={0x1c,0,0};
 for(unsigned bit=0;bit<30;++bit){
  clockAt(15000+bit*1250);const size_t before=pinWrites.size();er4EscLoop();
  const unsigned pos=bit%10;const bool high=pos==9||(pos>0&&((expected[bit/10]>>(pos-1))&1));
  assert(pinWrites[before].value==(high?HIGH:LOW));
 }
 assert(!txActive&&pinWrites.back().value==HIGH);
 Serial.input={0x1c,0,0,0x1b,2};clockAt(53000);er4EscLoop();
 assert(reply().status==EscWire::Status::Ok&&reply().value==2);
 assert(txStarted==1&&txCompleted==1&&rxActive==5);
 assert(bridge.diagnostics().echoMatched==3&&bridge.diagnostics().readsOk==1);
 assert(bridge.diagnostics().lastEchoPending==0);

 request(EscWire::Op::Read,4);clockAt(57000);er4EscLoop();assert(txActive);
 clockAt(59000);er4EscLoop();assert(!txActive&&pinWrites.back().value==HIGH);
 assert(reply().status==EscWire::Status::Unverified); // Timing overrun releases bus.
 request(EscWire::Op::Read,5);clockAt(63000);er4EscLoop();assert(txActive);
 ChannelData[11]=172;er4EscLoop();assert(!txActive&&!bridge.active()&&pinWrites.back().value==HIGH);
 assert(txStarted==3&&txCompleted==1&&txAborted==2&&timingFaults==1);
 assert(maxLateUs==750&&lastAbortBit==1);
 // A missing echo must remain distinguishable from an echoed request with no reply.
 ChannelData[11]=1811;request(EscWire::Op::Hello,6);er4EscLoop();
 request(EscWire::Op::Read,7);clockAt(70000);er4EscLoop();
 clockAt(1870000);er4EscLoop();
 assert(bridge.diagnostics().timeouts==1&&bridge.diagnostics().lastEchoPending==3);
 request(EscWire::Op::Hello,8);er4EscLoop();
 assert(bridge.diagnostics().lastStatus==uint8_t(EscWire::Status::Timeout));
 // Exercise bounded ring wrap: retain newest32 events, count all observed bytes.
 ChannelData[11]=172;Serial.input.assign(40,0xaa);
 for(unsigned i=0;i<5;++i)er4EscLoop();
 assert(Serial.input.empty()&&rxInactive==42&&traceTotal>32);
 const char *text=er4EscElectricalDiagnostics();
 assert(strstr(text,"interface=1 writes=0 baud=800 rxPin=3 txPin=9"));
 assert(strstr(text,"echoMatched=3")&&strstr(text,"lastEchoPending=3"));
 assert(strstr(text,"startupInactive=1")&&strstr(text,"timeouts=1"));
 const char *trace=strstr(text," trace=");assert(trace);
 unsigned separators=0;for(const char *p=trace;*p;++p)if(*p==',')++separators;
 assert(separators==31); // Bounded, chronological event history.
 puts("ER4 direct read: released startup, open-drain framing, echoed read reply, write rejection, timing/selection release PASS");
}

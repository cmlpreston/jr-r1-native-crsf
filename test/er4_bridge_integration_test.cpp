// Executes the real receiver integration with RF/router/platform boundaries mocked.
#include "../receiver/er4/Er4EscBridge.cpp"
#include <cstring>
static bool handle(EscWire::Message m){uint8_t f[19];assert(EscWire::build(m,false,f,sizeof(f))==19);return er4EscHandle(reinterpret_cast<const crsf_header_t*>(f));}
int main(){
 connectionState=connected;connectionHasModelMatch=true;ChannelData[11]=65535;
 assert(!er4EscSelected()); // An unset channel must never select the bridge.
 ChannelData[11]=172;
 uint8_t ordinary[]={0xc8,6,0x2d,0xec,0xea,9,6,0};
 assert(er4EscHandle(reinterpret_cast<const crsf_header_t*>(ordinary)));
 ordinary[2]=0x28;assert(er4EscHandle(reinterpret_cast<const crsf_header_t*>(ordinary)));
 EscWire::Message m;m.session=77;m.sequence=1;assert(handle(m));er4EscLoop();assert(crsfRouter.replies.empty());
 er4EscRecordAck(true);ChannelData[11]=1811;er4EscLoop();assert(er4EscSelected()&&DataDlSender.resyncs==1&&DataDlSender.observed);
 er4EscLoop();assert(DataDlSender.resyncs==1); // One resync per selection edge.
 ordinary[2]=0x2d;assert(er4EscHandle(reinterpret_cast<const crsf_header_t*>(ordinary)));
 ordinary[2]=0x2c;assert(!er4EscHandle(reinterpret_cast<const crsf_header_t*>(ordinary))); // Selected reads allowed.
 mockNow=100;assert(handle(m));er4EscLoop();assert(crsfRouter.replies.size()==1);
 auto &f=crsfRouter.replies.back();EscWire::Message r;assert(EscWire::parse(f[2],f.data()+3,17,true,r));
 assert(r.status==EscWire::Status::Ok&&r.flags==EscWire::SelectorAvailable&&r.sequence==1);
 mockNow=200;assert(handle(m));er4EscLoop();assert(crsfRouter.replies.size()==2); // Cached Hello replay.
 ChannelData[11]=172;otaConnector.queued=1;er4EscLoop();assert(!bridge.active()&&!DataDlSender.active&&otaConnector.queued==0);
 er4EscRecordAck(false);ChannelData[11]=1811;er4EscLoop();assert(DataDlSender.resyncs==2&&!DataDlSender.observed);
 m.session=88;m.sequence=2;mockNow=300;assert(handle(m));er4EscLoop();assert(crsfRouter.replies.size()==3);
 isArmed=true;er4EscLoop();assert(!er4EscSelected()&&!bridge.active());
 isArmed=false;OtaIsFullRes=true;assert(!er4EscSelected());OtaIsFullRes=false;
 connectionHasModelMatch=false;assert(!er4EscSelected());connectionHasModelMatch=true;connectionState=disconnected;assert(!er4EscSelected());
 uint8_t bad[19];assert(EscWire::build(m,false,bad,19)==19);bad[7]^=1;
 assert(er4EscHandle(reinterpret_cast<const crsf_header_t*>(bad)));
 assert(irqDepth==0&&strstr(er4EscDiagnostics(),"accepted=3")&&strstr(er4EscDiagnostics(),"malformed=1"));
 puts("ER4 integration: shared-address isolation, selector validity, resync/flush edges, cached RF-only Hello and arm/mode/link gates PASS");
}

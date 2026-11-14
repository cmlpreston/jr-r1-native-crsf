#include "esc_programmer.h"
#include "esc_programmer_server.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static constexpr uint8_t Caps=EscWire::SelectorAvailable|EscWire::Interface|EscWire::Writes;
static void deliver(EscProgrammer &client,const EscWire::Message &m,uint32_t now){uint8_t f[21];assert(EscWire::build(m,true,f,sizeof(f))==21);client.receive(f[2],f+3,17,now);}
static EscWire::Message poll(EscProgrammer &c,uint32_t now){EscWire::Message m;bool probe;assert(c.poll(now,m,probe)&&!probe);return m;}
static void echo(EscProgrammerServer &s,uint32_t now){uint8_t b[3];auto n=s.takeTx(b);assert(n);for(size_t i=0;i<n;++i)s.receive(b[i],now);}
static void reply(EscProgrammerServer &s,EscProgrammer &c,uint32_t now){EscWire::Message r;assert(s.reply(r));deliver(c,r,now);}
static void testFrames(){
 for(uint8_t id=0;id<15;++id)for(uint8_t v=0;v<Surpass::choices(id);++v){uint8_t b[3];assert(Surpass::request(true,id,v,b)==3);assert(b[0]==0x1b&&b[1]==id&&b[2]==v);assert(strcmp(Surpass::choice(id,v),"--"));}
 assert(!Surpass::valid(15,0)&&!Surpass::valid(0,9));assert(!strcmp(Surpass::choice(0,5),"3.2 V/cell"));assert(!strcmp(Surpass::choice(1,1),"Level 1"));
 EscWire::Message m;m.session=0x12345678;m.sequence=0x9876;m.parameter=14;m.value=2;m.op=EscWire::Op::Write;
 uint8_t b[21];assert(EscWire::build(m,false,b,sizeof(b))==19);EscWire::Message decoded;
 assert(EscWire::parse(b[2],b+3,15,false,decoded)&&decoded.session==m.session&&decoded.sequence==m.sequence&&decoded.value==2);
 assert(!EscWire::parse(b[2],b+3,14,false,decoded));b[7]^=1;assert(!EscWire::parse(b[2],b+3,15,false,decoded));
}
static void testSession(){
 EscProgrammer c;assert(!c.begin(0,1,false,true));assert(!c.begin(0,1,true,false));assert(c.begin(0,123,true,true));
 assert(c.blocksArm()&&c.selected());EscWire::Message m;bool probe;assert(!c.poll(749,m,probe));m=poll(c,750);
 EscProgrammerServer s;s.update(750,true,Caps);assert(s.handle(m,750));reply(s,c,751);assert(c.ready());
 assert(c.read(800));
 for(unsigned id=0;id<15;++id){auto request=poll(c,800+id*50);assert(request.op==EscWire::Op::Read&&request.parameter==id);
  assert(s.handle(request,800+id*50));reply(s,c,801+id*50);assert(!c.ready());echo(s,802+id*50);
  s.receive(0x1b,803+id*50);s.receive(0,803+id*50);reply(s,c,803+id*50);
 }
 assert(c.ready()&&c.known(14));assert(c.write(14,2,1600));m=poll(c,1600);assert(s.handle(m,1600));reply(s,c,1601);echo(s,1602);
 s.receive(0x1b,1603);s.receive(0x0a,1603);assert(!c.ready());echo(s,1604);s.receive(0x1b,1605);s.receive(2,1605);reply(s,c,1605);assert(c.ready()&&c.value(14)==2);
 // Lost acknowledgement/replayed request must not repeat the ESC write.
 assert(s.handle(m,1606));uint8_t tx[3];assert(!s.takeTx(tx));reply(s,c,1607);assert(c.ready());
 // Same sequence with changed content and old sequences are rejected.
 m.value=1;assert(!s.handle(m,1608));--m.sequence;assert(!s.handle(m,1608));
 c.finish(1700);assert(!c.selected()&&c.blocksArm());assert(c.poll(2450,m,probe)&&probe);
 uint8_t er4[]={0xea,0xec,'E','R','4',0};c.receive(0x29,er4,sizeof(er4),2451);assert(c.blocksArm());
 uint8_t er6[]={0xea,0xec,'R','M',' ','E','R','6','-','G',0};c.receive(0x29,er6,sizeof(er6),2452);assert(!c.blocksArm());
}
static void testFailures(){
 EscProgrammer c;c.begin(0xfffffff0,11,true,true);auto m=poll(c,734);auto stale=m;stale.session=10;stale.flags=Caps;deliver(c,stale,735);assert(!c.ready());
 c.update(3234,true,true);assert(!c.selected()&&c.blocksArm());c.update(9234,true,true);assert(c.state()==EscProgrammer::State::Fault);assert(!c.begin(9234,12,true,true));
 c.finish(10000);EscWire::Message probeMessage;bool probe;assert(c.poll(10750,probeMessage,probe)&&probe);
 EscProgrammerServer s;s.update(0,false,Caps);m.session=1;m.sequence=1;assert(!s.handle(m,0));s.update(0,true,Caps);assert(s.handle(m,0));
 m.sequence=2;m.op=EscWire::Op::Write;m.parameter=14;m.value=3;assert(s.handle(m,1));uint8_t b[3];assert(!s.takeTx(b));
 EscWire::Message r;assert(s.reply(r)&&r.status==EscWire::Status::Rejected);
 m.sequence=3;m.value=2;assert(s.handle(m,2));echo(s,3);s.receive(0x1b,4);s.receive(0x0a,4);echo(s,5);s.receive(0x1b,6);s.receive(1,6);assert(s.reply(r)&&r.status==EscWire::Status::Unverified);
 m.sequence=4;assert(s.handle(m,7));echo(s,8);s.update(1807,true,Caps);assert(s.reply(r)&&r.status==EscWire::Status::Timeout);assert(!s.takeTx(b));
 s.update(2000,false,Caps);assert(!s.active()&&!s.handle(m,2000));
 // Timeout before the UART drains must discard queued bytes, including writes.
 s.update(2100,true,Caps);m.op=EscWire::Op::Hello;m.session=2;m.sequence=1;assert(s.handle(m,2100));
 m.op=EscWire::Op::Write;m.parameter=14;m.value=1;m.sequence=2;assert(s.handle(m,2101));s.update(3901,true,Caps);assert(!s.takeTx(b));
 s.update(6000,true,Caps);assert(!s.active());
 EscProgrammer unsafe;assert(unsafe.begin(0,9,true,true));unsafe.update(1,false,true);assert(!unsafe.selected()&&unsafe.blocksArm());
 EscProgrammer lost;assert(lost.begin(0,10,true,true));lost.update(1,true,false);assert(!lost.selected()&&lost.blocksArm());
 // Default bench firmware reports identity but cannot touch the ESC bus.
 EscProgrammerServer locked;s=EscProgrammerServer{};m.op=EscWire::Op::Hello;m.sequence=1;locked.update(0,true,EscWire::SelectorAvailable);assert(locked.handle(m,0));
 m.op=EscWire::Op::Read;m.sequence=2;assert(locked.handle(m,1));assert(locked.reply(r)&&r.status==EscWire::Status::Rejected);assert(!locked.takeTx(b));
}
static void testHelloRetry(){
 EscProgrammer c;assert(c.begin(0,77,true,true));auto first=poll(c,750);
 EscProgrammerServer server;server.update(750,true,EscWire::SelectorAvailable);assert(server.handle(first,750));
 EscWire::Message lost;assert(server.reply(lost)); // RF drops the first reply.
 EscWire::Message again;bool probe;
 assert(!c.poll(1449,again,probe));assert(c.poll(1450,again,probe)&&!probe);
 uint8_t a[19],b[19];assert(EscWire::build(first,false,a,19)==19&&EscWire::build(again,false,b,19)==19);
 assert(memcmp(a,b,19)==0); // Exact duplicate, no new sequence or transaction.
 assert(server.handle(again,1450));uint8_t bus[3];assert(!server.takeTx(bus));reply(server,c,1451);assert(c.ready());
 auto keepalive=poll(c,2451);assert(keepalive.sequence!=first.sequence);
 assert(c.poll(3151,again,probe)&&again.sequence==keepalive.sequence);
 assert(c.poll(3851,again,probe)&&again.sequence==keepalive.sequence);
 assert(!c.poll(4551,again,probe)); // Bounded even if every reply is lost.
 c.update(4951,true,true);assert(!c.selected()&&c.blocksArm()); // Original deadline.
 // Read and Write retain single-shot transport, even at the retry intervals.
 EscProgrammer w;assert(w.begin(0,88,true,true));auto r=poll(w,750);r.flags=Caps;deliver(w,r,751);
 assert(w.read(800));
 for(unsigned id=0;id<15;++id){r=poll(w,800+id*35);r.flags=Caps;r.value=0;deliver(w,r,801+id*35);}
 assert(w.write(14,1,1400));r=poll(w,1400);assert(r.op==EscWire::Op::Write);
 assert(!w.poll(2100,again,probe)&&!w.poll(2800,again,probe)&&!w.poll(3500,again,probe));
 w.update(3900,true,true);assert(!w.selected());
}
static void testElectricalDiagnostics(){
 EscProgrammerServer s;s.update(0,true,Caps);EscWire::Message m;m.session=12;m.sequence=1;
 assert(s.handle(m,0));m.op=EscWire::Op::Read;m.sequence=2;assert(s.handle(m,1));
 echo(s,2);s.update(1801,true,Caps);
 assert(s.diagnostics().timeouts==1&&s.diagnostics().echoMatched==3);
 assert(s.diagnostics().lastStage==1&&s.diagnostics().lastEchoPending==0);
 // Terminal evidence persists through keepalives and deselection.
 m.op=EscWire::Op::Hello;m.sequence=3;assert(s.handle(m,1802));
 assert(s.diagnostics().lastStatus==uint8_t(EscWire::Status::Timeout));
 m.op=EscWire::Op::Read;m.sequence=4;assert(s.handle(m,1803));
 uint8_t b[3];assert(s.takeTx(b)==3);s.receive(0xff,1804);
 assert(s.diagnostics().echoMismatch==1&&s.diagnostics().busFaults==1);
 assert(s.diagnostics().lastEchoPending==3);
 s.receive(0x1b,1805);s.receive(0xff,1805);assert(s.diagnostics().startups==1);
 assert(s.takeTx(b)==2&&b[0]==0x1c&&b[1]==0xff);
 s.receive(0x1c,1806);s.receive(0xff,1806);
 m.sequence=5;assert(s.handle(m,1807));echo(s,1808);
 s.receive(0x1b,1809);s.receive(0xfe,1809);assert(s.diagnostics().invalidReplies==1);
 s.update(1810,false,Caps);assert(s.diagnostics().echoMatched==8&&s.diagnostics().timeouts==1);
}
int main(){testElectricalDiagnostics();testFrames();testSession();testFailures();testHelloRetry();puts("ESC programmer: framing, map, handover, replay guards, exactly-once writes/readback, timeout, CUT/link and bench lock PASS");}

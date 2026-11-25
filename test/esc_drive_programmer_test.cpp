#include "esc_drive_programmer.h"
#include "esc_rf_policy.h"
#include <cassert>
#include <cstdio>
#include <cstring>
int main(){
 EscDriveProgrammer c;EscWire::Message q;bool probe;
 auto response=[&](uint32_t now,uint8_t flags,EscWire::Status status=EscWire::Status::Ok){uint8_t f[21];q.flags=flags;q.status=status;assert(EscWire::build(q,true,f,sizeof(f)));c.receive(f[2],f+3,17,now);};
 // Exercise the actual final RF policy with independent safety arbitration.
 const auto cut=arbitrateOutputSafety(OutputSafetyStatus::Cut,true);
 const auto armed=arbitrateOutputSafety(OutputSafetyStatus::Armed,true);
 const auto stale=arbitrateOutputSafety(OutputSafetyStatus::Cut,false);
 assert(escRfRequestAllowed(false,cut)&&!escRfRequestAllowed(true,cut));
 assert(!escRfRequestAllowed(false,armed)&&!escRfRequestAllowed(true,stale));
 constexpr uint8_t drive=EscWire::ModeSelect|EscWire::Drive;
 constexpr uint8_t program=EscWire::ModeSelect|EscWire::Programming|EscWire::Interface;
 assert(!c.begin(0,42,false,true));assert(c.begin(0,42,true,true)&&c.blocksArm()&&!c.usesSelector());
 assert(c.poll(0,q,probe)&&q.op==EscWire::Op::Hello&&!probe);response(1,drive);
 assert(c.poll(1,q,probe)&&q.op==EscWire::Op::Prepare);response(2,drive|EscWire::Pending);
 assert(!c.ready()&&c.blocksArm());c.update(1000,true,false);assert(c.blocksArm());
 assert(c.poll(1500,q,probe)&&q.op==EscWire::Op::Hello);response(1501,program);assert(c.ready());
 assert(!c.writable()&&!c.write(0,1,1502));assert(c.read(1600));
 for(unsigned i=0;i<15;++i){assert(c.poll(1600+i*100,q,probe)&&q.op==EscWire::Op::Read&&q.parameter==i);q.value=0;response(1601+i*100,program);}
 assert(c.poll(3600,q,probe)&&q.op==EscWire::Op::Hello);response(3601,program);
 assert(!strcmp(c.status(),"15 settings read; writes disabled"));
 assert(c.ready()&&c.known(14));c.finish(4000);assert(c.blocksArm());
 assert(c.poll(4000,q,probe)&&q.op==EscWire::Op::Close);response(4001,program);assert(c.blocksArm());
 assert(c.poll(4600,q,probe)&&q.op==EscWire::Op::Hello);response(4601,drive);assert(!c.blocksArm());
 assert(c.begin(5000,43,true,true));assert(c.poll(5000,q,probe));response(5001,program);assert(c.read(5100));assert(c.poll(5100,q,probe));
 c.update(5200,true,false);assert(c.state()==EscDriveProgrammer::State::Fault&&c.blocksArm());assert(!c.poll(9000,q,probe));
 c.finish(10000);assert(c.poll(10000,q,probe)&&q.op==EscWire::Op::Close);c.update(12500,true,true);
 assert(c.poll(13000,q,probe)&&q.op==EscWire::Op::Hello);response(13001,program);assert(c.blocksArm());
 // A latched CUT fault must not repeatedly rebuild its UI on every update.
 assert(c.poll(13600,q,probe));response(13601,drive);assert(!c.blocksArm());
 assert(c.begin(14000,44,true,true));c.update(14001,false,true);assert(c.state()==EscDriveProgrammer::State::Fault);
 const auto faultRevision=c.revision();c.update(14002,false,true);
 assert(c.revision()==faultRevision&&c.blocksArm());
 // Drive recovery waits for fresh CUT rather than faulting again. It may not
 // send Close or clear the interlock until both CUT and receiver are confirmed.
 c.finish(15000);c.update(15001,false,true);
 assert(c.state()==EscDriveProgrammer::State::Returning&&c.blocksArm());
 assert(!c.poll(16000,q,probe));const auto waitingRevision=c.revision();
 c.update(16001,false,true);assert(c.revision()==waitingRevision);
 c.update(17000,true,true);assert(c.poll(17000,q,probe)&&q.op==EscWire::Op::Close);
 response(17001,program);assert(c.blocksArm());
 assert(c.poll(17600,q,probe)&&q.op==EscWire::Op::Hello);
 response(17601,drive);assert(!c.blocksArm());
 // Write capability is opt-in, requires a known value, and never retries.
 c=EscDriveProgrammer{};assert(c.begin(20000,45,true,true));
 assert(c.poll(20000,q,probe));response(20001,program|EscWire::Writes);
 assert(c.writable()&&!c.write(8,1,20002));assert(c.read(20100));
 for(unsigned i=0;i<15;++i){assert(c.poll(20100+i*100,q,probe));q.value=0;response(20101+i*100,program|EscWire::Writes);}
 assert(!c.write(8,0,22000)&&!c.write(8,5,22000));
 assert(c.write(8,1,22000)&&!c.known(8));
 assert(c.poll(22000,q,probe)&&q.op==EscWire::Op::Write&&q.parameter==8&&q.value==1);
 assert(!c.poll(22001,q,probe));response(22002,program|EscWire::Writes,EscWire::Status::Busy);
 assert(!c.poll(22500,q,probe));response(22501,program|EscWire::Writes);
 assert(c.ready()&&c.known(8)&&c.value(8)==1&&!strcmp(c.status(),"Setting saved; readback verified"));
 assert(c.poll(23100,q,probe));response(23101,program|EscWire::Writes);
 assert(!strcmp(c.status(),"Setting saved; readback verified"));
 assert(c.write(8,2,24000)&&c.poll(24000,q,probe));
 c.update(28500,true,true);assert(c.state()==EscDriveProgrammer::State::Fault&&c.blocksArm()&&!c.known(8));
 assert(!c.poll(30000,q,probe));
 puts("Single ER6G client: CUT-only entry, prepare/power-cycle/read/Drive confirmation, no selector/writes, interruption remains locked PASS");
}

#pragma once
#include "esc_programmer_wire.h"
#include "surpass_protocol.h"
// Single ER6G mode selection. A power cycle is an explicit part of both
// transitions; loss of RF never clears the handset's CUT interlock.
class EscDriveProgrammer {
public:
 enum class State {Idle,Selecting,AwaitProgramming,Ready,Reading,Returning,Fault,Writing};
 bool begin(uint32_t now,uint32_t nonce,bool cut,bool provisioned){
  (void)provisioned;if(state_!=State::Idle||!cut||!nonce)return false;
  session_=nonce;state_=State::Selecting;waiting_=false;known_=0;flags_=0;due_=now;
  prepared_=closed_=waitingForCut_=writeVerified_=false;status_="Checking ER6G mode; CUT";++revision_;return true;
 }
 bool selected()const{return state_!=State::Idle&&state_!=State::Returning&&state_!=State::Fault;}
 bool blocksArm()const{return state_!=State::Idle;}
 bool ready()const{return state_==State::Ready;}
 bool writable()const{return ready()&&(flags_&EscWire::Writes)&&(flags_&EscWire::Interface);}
 bool usesSelector()const{return false;}
 State state()const{return state_;}
 const char *status()const{return status_;}
 uint32_t revision()const{return revision_;}
 uint8_t flags()const{return flags_;}
 bool known(uint8_t p)const{return p<15&&(known_&(1u<<p));}
 uint8_t value(uint8_t p)const{return p<15?values_[p]:0;}
 void blocked(const char *s){status_=s;++revision_;}
 void finish(uint32_t now,bool failure=false){
  (void)failure;if(state_==State::Idle)return;
  state_=State::Returning;waiting_=false;closed_=waitingForCut_=false;due_=now;
  status_="Select Drive, then cycle ESC power; stay CUT";++revision_;
 }
 bool read(uint32_t now){
  if(!ready()||!(flags_&EscWire::Interface))return false;
  state_=State::Reading;next_=0;known_=0;waiting_=writeVerified_=false;due_=now;
  status_="Reading ESC settings";++revision_;return true;
 }
 bool write(uint8_t parameter,uint8_t value,uint32_t now){
  if(!writable()||!known(parameter)||!Surpass::valid(parameter,value)||values_[parameter]==value)return false;
  state_=State::Writing;next_=parameter;desired_=value;known_&=~(1u<<parameter);
  waiting_=writeVerified_=false;due_=now;
  status_="Writing once; waiting for verified readback";++revision_;return true;
 }
 void update(uint32_t now,bool cut,bool link){
  if(!blocksArm()||state_==State::Fault)return;
  if(!cut){
   if(state_!=State::Returning){fault("CUT required; select Drive to recover");return;}
   // Drive recovery must survive the same stale/safety transition that caused
   // the fault. Retain the interlock; send nothing until fresh CUT is confirmed.
   waiting_=false;due_=now+500;
   if(!waitingForCut_){waitingForCut_=true;status_="Waiting for CUT; Drive recovery pending";++revision_;}
   return;
  }
  if(waitingForCut_){waitingForCut_=false;due_=now;status_="Confirming Drive; outputs remain CUT";++revision_;}
  if(!link){if(state_==State::Reading||state_==State::Writing){fault("Transfer interrupted; select Drive to recover");return;}waiting_=false;due_=now+500;return;}
  if(waiting_&&uint32_t(now-sent_)>=(state_==State::Writing?4500u:2500u)){
   waiting_=false;due_=now+500;
   if(state_==State::Reading||state_==State::Writing){fault("Transfer unverified; select Drive to recover");return;}
   // Mode command outcome unknown: rediscover instead of repeating a command.
   if(pending_.op==EscWire::Op::Prepare)state_=State::AwaitProgramming;
   if(pending_.op==EscWire::Op::Close)closed_=true;
   status_=state_==State::Returning?"Cycle ESC power to Drive; waiting for ER6G":"Cycle ESC power for programming; waiting for ER6G";++revision_;
  }
 }
 bool poll(uint32_t now,EscWire::Message &m,bool &probe){
  probe=false;if(!blocksArm()||state_==State::Fault||waitingForCut_||waiting_||int32_t(now-due_)<0)return false;
  m={};m.session=session_;m.sequence=++sequence_;
  m.op=state_==State::Reading?EscWire::Op::Read:state_==State::Writing?EscWire::Op::Write:
       state_==State::Selecting&&prepared_?EscWire::Op::Prepare:
       state_==State::Returning&&!closed_?EscWire::Op::Close:EscWire::Op::Hello;
  m.parameter=next_;m.value=desired_;pending_=m;waiting_=true;sent_=now;return true;
 }
 void receive(uint8_t type,const uint8_t *p,size_t n,uint32_t now){
  EscWire::Message m;
  if(!waiting_||!EscWire::parse(type,p,n,true,m)||m.session!=session_||m.sequence!=pending_.sequence||m.op!=pending_.op||m.parameter!=pending_.parameter)return;
  if(m.status==EscWire::Status::Busy)return;
  waiting_=false;flags_=m.flags;due_=now+500;
  if(m.status!=EscWire::Status::Ok||!(flags_&EscWire::ModeSelect)){
   fault(pending_.op==EscWire::Op::Write?"Write unverified; select Drive; keep CUT":"ER6G mode unavailable; keep CUT");return;
  }
  if(state_==State::Returning){
   if(m.op==EscWire::Op::Close)closed_=true;
   // A Close ACK in programming mode cannot prove that PWM was restored.
   if(closed_&&(flags_&EscWire::Drive)&&!(flags_&EscWire::Pending)){
    state_=State::Idle;status_="Drive confirmed; outputs remain CUT";
   }else status_="Cycle ESC power to Drive; stay CUT";
  }else if(m.op==EscWire::Op::Write){
   if(!Surpass::valid(next_,m.value)||m.value!=desired_){fault("Write readback mismatch; select Drive to recover");return;}
   values_[next_]=m.value;known_|=1u<<next_;state_=State::Ready;writeVerified_=true;
   status_="Setting saved; readback verified";
  }else if(m.op==EscWire::Op::Read){
   if(!Surpass::valid(next_,m.value)){fault("Invalid ESC reply; keep CUT");return;}
   values_[next_]=m.value;known_|=1u<<next_;
   if(++next_==15){state_=State::Ready;status_=(flags_&EscWire::Writes)?"15 settings read; select one to edit":"15 settings read; writes disabled";}
   else due_=now+30;
  }else if(flags_&EscWire::Programming){
   if(flags_&EscWire::Interface){
    state_=State::Ready;
    // Periodic Hello replies must not hide a completed settings transfer.
    status_=writeVerified_?"Setting saved; readback verified":known_==0x7fff?((flags_&EscWire::Writes)?"15 settings read; select one to edit":"15 settings read; writes disabled"):"Programming ready; Read ESC";
   }
   else fault("ESC entry failed; select Drive and cycle power");
  }else if(flags_&EscWire::Pending){
   state_=State::AwaitProgramming;status_="Cycle ESC power for programming; stay CUT";
  }else if(state_==State::Selecting){prepared_=true;due_=now;status_="Preparing one programming boot; CUT";}
  else if(state_==State::AwaitProgramming){fault("Programming not entered; select Drive to retry");}
  ++revision_;
 }
private:
 void fault(const char*s){state_=State::Fault;waiting_=false;status_=s;++revision_;}
 State state_=State::Idle;uint32_t session_=0,due_=0,sent_=0,revision_=0;
 uint16_t sequence_=0,known_=0;uint8_t values_[15]={},flags_=0,next_=0,desired_=0;
 bool waiting_=false,prepared_=false,closed_=false,waitingForCut_=false,writeVerified_=false;EscWire::Message pending_;
 const char *status_="Drive / ESC programming; single ER6G";
};

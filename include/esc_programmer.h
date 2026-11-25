#pragma once
#include "esc_programmer_wire.h"
#include "surpass_protocol.h"
#include <string.h>

class EscProgrammer {
public:
    enum class State { Idle,Selecting,Ready,Reading,Writing,Returning,Fault };
    bool begin(uint32_t now,uint32_t nonce,bool cut,bool provisioned) {
        if(state_!=State::Idle||!cut||!provisioned||!nonce)return false;
        session_=nonce;state_=State::Selecting;since_=due_=now;waiting_=false;
        known_=0;flags_=0;error_=false;status_="Selecting ER4; waiting for CH12";++revision_;return true;
    }
    void blocked(const char *message){if(state_==State::Idle){status_=message;++revision_;}}
    bool selected()const{return state_==State::Selecting||state_==State::Ready||state_==State::Reading||state_==State::Writing;}
    bool blocksArm()const{return state_!=State::Idle;}
    bool ready()const{return state_==State::Ready;}
    bool writable()const{return ready()&&(flags_&EscWire::Writes)&&(flags_&EscWire::Interface);}
    State state()const{return state_;}
    const char *status()const{return status_;}
    uint32_t revision()const{return revision_;}
    uint8_t flags()const{return flags_;}
    bool known(uint8_t p)const{return p<15&&(known_&(1u<<p));}
    uint8_t value(uint8_t p)const{return p<15?values_[p]:0;}
    void finish(uint32_t now,bool failure=false) {
        if(state_==State::Idle)return;
        state_=State::Returning;since_=now;due_=now+750;waiting_=probeSent_=false;error_|=failure;
        status_="Restoring ER6G; remain CUT";++revision_;
    }
    bool read(uint32_t now) {
        if(!ready()||!(flags_&EscWire::Interface))return false;
        state_=State::Reading;next_=0;known_=0;waiting_=false;since_=due_=now;
        status_="Reading ESC settings";++revision_;return true;
    }
    bool write(uint8_t p,uint8_t v,uint32_t now) {
        if(!writable()||!known(p)||!Surpass::valid(p,v))return false;
        state_=State::Writing;next_=p;desired_=v;waiting_=false;since_=due_=now;
        status_="Writing once; waiting for readback";++revision_;return true;
    }
    void update(uint32_t now,bool cut,bool linkEnabled) {
        if(selected()&&(!cut||!linkEnabled)){finish(now,true);return;}
        if(waiting_&&uint32_t(now-sent_)>=2500){finish(now,true);return;}
        if(state_==State::Selecting&&uint32_t(now-since_)>=5000){finish(now,true);return;}
        if(state_==State::Returning&&uint32_t(now-since_)>=6000){state_=State::Fault;waiting_=false;status_="ER6G not confirmed; retry Finish";++revision_;}
    }
    bool poll(uint32_t now,EscWire::Message &m,bool &probe) {
        probe=false;
        if(waiting_){
            // RF handover may need ELRS data transport to resynchronize. Hello
            // is side-effect-free: resend the identical session/sequence at
            // most twice, without extending the original reply deadline.
            // Read/Write are never retransmitted here.
            if(pending_.op!=EscWire::Op::Hello || helloRetries_>=2 ||
               uint32_t(now-sent_)>=2500 || uint32_t(now-lastHelloSent_)<700)return false;
            m=pending_;++helloRetries_;lastHelloSent_=now;return true;
        }
        if(int32_t(now-due_)<0)return false;
        if(state_==State::Returning){probe=true;probeSent_=true;due_=now+750;return true;}
        if(!selected())return false;
        if(state_==State::Selecting&&uint32_t(now-since_)<750)return false;
        m=EscWire::Message{};m.session=session_;m.sequence=++sequence_;
        m.op=state_==State::Reading?EscWire::Op::Read:state_==State::Writing?EscWire::Op::Write:EscWire::Op::Hello;
        m.parameter=next_;m.value=desired_;pending_=m;waiting_=true;sent_=lastHelloSent_=now;helloRetries_=0;return true;
    }
    void receive(uint8_t type,const uint8_t *p,size_t n,uint32_t now) {
        if(state_==State::Returning&&probeSent_&&type==0x29&&n>=4&&p[0]==0xea&&p[1]==0xec){
            // Distinguish the stock driving receiver from ER4 link statistics.
            char name[48];size_t i=0;while(2+i<n&&i<sizeof(name)-1&&p[2+i]){name[i]=p[2+i];++i;}name[i]=0;
            if(2+i<n&&p[2+i]==0&&strstr(name,"ER6")){state_=State::Idle;status_=error_?"Stopped; ER6G restored; CUT":"ER6G restored; CUT";waiting_=false;++revision_;}return;
        }
        EscWire::Message m;
        if(!waiting_||!EscWire::parse(type,p,n,true,m)||m.session!=session_||m.sequence!=pending_.sequence||m.op!=pending_.op||m.parameter!=pending_.parameter)return;
        if(m.status==EscWire::Status::Busy)return;
        waiting_=false;
        if(m.status!=EscWire::Status::Ok||!(m.flags&EscWire::SelectorAvailable)){finish(now,true);return;}
        flags_=m.flags;
        if(m.op==EscWire::Op::Read||m.op==EscWire::Op::Write){
            if(!Surpass::valid(m.parameter,m.value)||(m.op==EscWire::Op::Write&&m.value!=desired_)){finish(now,true);return;}
            values_[m.parameter]=m.value;known_|=1u<<m.parameter;
            if(state_==State::Reading&&++next_<15){due_=now+30;++revision_;return;}
        }
        state_=State::Ready;due_=now+1000;
        status_=!(flags_&EscWire::Interface)?"ER4 linked; electrical interface locked":m.op==EscWire::Op::Write?"ESC readback verified":known_==0x7fff?"ESC settings read":"ER4 linked; Read ESC to begin";
        ++revision_;
    }
private:
    State state_=State::Idle;uint32_t session_=0,since_=0,sent_=0,due_=0,revision_=0,lastHelloSent_=0;
    uint16_t sequence_=0,known_=0;uint8_t values_[15]={},flags_=0,next_=0,desired_=0,helloRetries_=0;
    bool waiting_=false,probeSent_=false,error_=false;EscWire::Message pending_;
    const char *status_="CH12 handover not commissioned";
};

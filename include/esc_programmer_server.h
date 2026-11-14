#pragma once
#include "esc_programmer_wire.h"
#include "surpass_protocol.h"

// Allocation-free receiver transaction engine. UART adapter must return the
// shared-wire TX echo through receive(), and release its pull-down on abort.
class EscProgrammerServer {
public:
    struct Diagnostics {
        uint32_t timeouts=0,busFaults=0,echoMatched=0,echoMismatch=0,startups=0,readsOk=0,invalidReplies=0;
        uint8_t lastStatus=0,lastStage=0,lastEchoPending=0,lastParameter=255;
    };
    const Diagnostics &diagnostics()const{return diagnostics_;}
    uint8_t pendingEcho()const{return echoCount_;}
    uint8_t stage()const{return stage_;}
    void update(uint32_t now,bool selected,uint8_t capabilities) {
        selected_=selected;flags_=capabilities;
        if(!selected || (session_ && uint32_t(now-lastRequest_)>=3500)) {
            session_=0;stage_=0;txLength_=echoCount_=0;replyReady_=false;head_=false;return;
        }
        if(stage_&&uint32_t(now-started_)>=1800)complete(EscWire::Status::Timeout,0);
    }
    bool handle(const EscWire::Message &m,uint32_t now) {
        if(!selected_||!(flags_&EscWire::SelectorAvailable))return false;
        if(m.op==EscWire::Op::Hello){
            if(stage_&&m.session!=session_)return false;
            if(!session_){session_=m.session;lastSequence_=uint16_t(m.sequence-1);}
        }
        if(!session_||m.session!=session_)return false;
        if(m.sequence==lastSequence_){
            if(m.op==last_.op&&m.parameter==last_.parameter&&m.value==last_.value){replyReady_=true;return true;}
            return false;
        }
        if(int16_t(m.sequence-lastSequence_)<=0||stage_)return false;
        lastSequence_=m.sequence;lastRequest_=now;last_=m;response_=m;response_.flags=flags_;head_=false;
        if(m.op==EscWire::Op::Hello){complete(EscWire::Status::Ok,0);return true;}
        if(m.op==EscWire::Op::Close){complete(EscWire::Status::Ok,0);session_=0;return true;}
        if(m.op==EscWire::Op::Prepare){complete(EscWire::Status::Rejected,0);return true;}
        if(!(flags_&EscWire::Interface)||m.parameter>=15||
           (m.op==EscWire::Op::Write&&(!(flags_&EscWire::Writes)||!Surpass::valid(m.parameter,m.value)))){
            complete(EscWire::Status::Rejected,0);return true;
        }
        response_.status=EscWire::Status::Busy;replyReady_=true;
        stage_=m.op==EscWire::Op::Write?2:1;started_=now;
        uint8_t bytes[3];Surpass::request(stage_==2,m.parameter,m.value,bytes);queue(bytes,3);return true;
    }
    bool reply(EscWire::Message &m){if(!replyReady_)return false;m=response_;replyReady_=false;return true;}
    size_t takeTx(uint8_t out[3]){const size_t n=txLength_;for(size_t i=0;i<n;++i)out[i]=tx_[i];txLength_=0;return n;}
    bool active()const{return selected_&&session_;}
    void busFault(){++diagnostics_.busFaults;if(stage_)complete(EscWire::Status::Unverified,0);txLength_=echoCount_=0;head_=false;}
    void receive(uint8_t b,uint32_t now) {
        if(!active())return;
        if(echoCount_){
            if(b!=echo_[echoAt_++]){++diagnostics_.echoMismatch;busFault();return;}
            ++diagnostics_.echoMatched;--echoCount_;return;
        }
        if(!head_){head_=b==0x1b;return;}head_=false;
        if(b==0xff){
            ++diagnostics_.startups;
            if(stage_==2||stage_==3){complete(EscWire::Status::Unverified,0);return;}
            // Startup handshake only. A read interrupted by startup must be
            // explicitly retried; a write is never automatically repeated.
            if(stage_)complete(EscWire::Status::Timeout,0);
            const uint8_t handshake[]={0x1c,0xff};queue(handshake,2);return;
        }
        if(stage_==2){
            if(b!=0x0a){complete(EscWire::Status::Unverified,0);return;}
            uint8_t read[3];Surpass::request(false,last_.parameter,0,read);
            stage_=3;started_=now;queue(read,3);return;
        }
        if(stage_==1||stage_==3){
            const bool valid=Surpass::valid(last_.parameter,b)&&(stage_!=3||b==last_.value);
            if(valid)++diagnostics_.readsOk;else ++diagnostics_.invalidReplies;
            complete(valid?EscWire::Status::Ok:EscWire::Status::Unverified,b);
        }
    }
private:
    void queue(const uint8_t *p,size_t n){txLength_=echoCount_=n;echoAt_=0;for(size_t i=0;i<n;++i)tx_[i]=echo_[i]=p[i];}
    void complete(EscWire::Status s,uint8_t value){
        if(response_.op==EscWire::Op::Read||response_.op==EscWire::Op::Write){
            diagnostics_.lastStatus=uint8_t(s);diagnostics_.lastStage=stage_;
            diagnostics_.lastEchoPending=echoCount_;diagnostics_.lastParameter=response_.parameter;
        }
        if(s==EscWire::Status::Timeout)++diagnostics_.timeouts;
        response_.status=s;response_.value=value;response_.flags=flags_;stage_=0;txLength_=echoCount_=0;head_=false;replyReady_=true;}
    Diagnostics diagnostics_;
    EscWire::Message last_,response_;uint32_t session_=0,lastRequest_=0,started_=0;
    uint16_t lastSequence_=0;uint8_t stage_=0,flags_=0,tx_[3]={},echo_[3]={},txLength_=0,echoCount_=0,echoAt_=0;
    bool selected_=false,replyReady_=false,head_=false;
};

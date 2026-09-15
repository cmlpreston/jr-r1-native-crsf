#include "crsf_tx_config.h"
#include "crsf_channel_output.h"
#include <cstring>
#include <cstdio>

size_t buildTxParameterFrame(const TxParameterRequest &r, uint8_t *out, size_t cap)
{
    if (r.type != 0x28 && r.type != 0x2c && r.type != 0x2d) return 0;
    const size_t n = r.type == 0x28 ? 6 : 8;
    if (!out || cap < n || (r.type != 0x28 && !r.id)) return 0;
    out[0] = 0xc8; out[1] = n - 2; out[2] = r.type;
    out[3] = 0xee; out[4] = 0xea;
    if (n == 8) { out[5] = r.id; out[6] = r.value; }
    out[n-1] = crc8DvbS2(out+2, n-3);
    return n;
}
namespace {
bool stringAt(const uint8_t *d, size_t n, size_t &at, char *out, size_t cap)
{
    const size_t start = at;
    while (at < n && d[at]) ++at;
    if (at == n || at-start >= cap) return false;
    memcpy(out, d+start, at-start); out[at-start] = 0; ++at;
    return true;
}
bool due(uint32_t now, uint32_t at) { return static_cast<int32_t>(now-at) >= 0; }
}
bool TxParameter::option(uint8_t index, char *out, size_t cap) const
{
    if (!cap) return false;
    out[0] = 0;
    const char *p = options;
    for (unsigned i = 0; i < index; ++i) {
        p = strchr(p, ';'); if (!p) return false; ++p;
    }
    const char *end = strchr(p, ';');
    size_t n = end ? static_cast<size_t>(end-p) : strlen(p);
    if (!n || n >= cap) return false;
    memcpy(out, p, n); out[n] = 0; return true;
}
void CrsfTxConfig::status(const char *s) { snprintf(status_, sizeof(status_), "%s", s); ++revision_; }
void CrsfTxConfig::fail(const char *s) { state_=State::Failed; waiting_=false; commandId_=0; status(s); }
void CrsfTxConfig::stop() { state_=State::Off; waiting_=false; commandId_=0; }
void CrsfTxConfig::start(uint32_t now)
{
    for (auto &p : parameters_) p = TxParameter{};
    count_=0; verifyId_=commandId_=0; used_=0; waiting_=heartbeat_=false;
    attempts_=0; due_=now; state_=State::Discover; device_[0]=0;
    status("Discovering TX module; outputs CUT");
}
bool CrsfTxConfig::ready() const { return state_ == State::Ready && !waiting_; }
bool CrsfTxConfig::confirming() const
{
    return state_==State::Command && commandId_ && parameters_[commandId_-1].value==3 && !waiting_;
}
const TxParameter *CrsfTxConfig::parameter(uint8_t id) const
{
    return id && id<=count_ && parameters_[id-1].valid ? &parameters_[id-1] : nullptr;
}
void CrsfTxConfig::read(uint8_t id, uint32_t now)
{
    id_=id; chunk_=remaining_=0; used_=0; waiting_=false; attempts_=0;
    due_=now; state_=State::Read;
}
bool CrsfTxConfig::poll(uint32_t now, TxParameterRequest &r)
{
    if (state_==State::Off || state_==State::Failed || !due(now,due_)) return false;
    if (state_==State::Ready) {
        if (!waiting_ && now-lastReply_ < 2000) return false;
        heartbeat_=true;
        if (attempts_>=3) { fail("TX disconnected; refresh to reconnect"); return false; }
        r={0x28,0,0}; waiting_=true; ++attempts_; due_=now+600; return true;
    }
    if (state_==State::Write) {
        // Exactly one write, followed by delayed readback. Never repeat a write.
        r={0x2d,verifyId_,desired_}; read(verifyId_,now+250); return true;
    }
    if (state_==State::Command) {
        if (confirming() && commandAction_==0) return false;
        if (attempts_>=20) { fail("Command response lost; refresh to reconnect"); return false; }
        // ExpressLRS advances command-response chunks through lcsQuery,
        // not PARAMETER_READ (which also resets commands on chunk zero).
        if (chunk_) r={0x2d,commandId_,6};
        else {
            used_=0;
            r={0x2d,commandId_,commandAction_ ? commandAction_ : uint8_t(6)};
        }
        commandAction_=0; waiting_=true; ++attempts_;
        const auto &p=parameters_[commandId_-1];
        due_=now+(p.timeout*10U < 200 ? 200 : p.timeout*10U); return true;
    }
    if (attempts_>=3) { fail(verifyId_ ? "Write not verified; refresh and check TX" : "TX response timed out; refresh to retry"); return false; }
    r = state_==State::Discover ? TxParameterRequest{0x28,0,0} : TxParameterRequest{0x2c,id_,chunk_};
    waiting_=true; ++attempts_; due_=now+600; return true;
}
bool CrsfTxConfig::decode(TxParameter &p)
{
    if (used_<3) return false;
    p=TxParameter{}; p.parent=data_[0]; p.type=data_[1]&0x7f; p.hidden=(data_[1]&0x80)!=0;
    if (p.parent>count_ || p.parent==id_) return false;
    size_t at=2;
    if (!stringAt(data_,used_,at,p.name,sizeof(p.name))) return false;
    if (p.type==9) {
        if (!stringAt(data_,used_,at,p.options,sizeof(p.options)) || at+4>used_) return false;
        p.value=data_[at++]; p.minimum=data_[at++]; p.maximum=data_[at++]; ++at;
        if (!stringAt(data_,used_,at,p.info,sizeof(p.info))) return false;
        char optionText[384];
        if (p.minimum>p.maximum || p.value<p.minimum || p.value>p.maximum || !p.option(p.value,optionText,sizeof(optionText))) return false;
    } else if (p.type==13) {
        if (at+2>used_) return false;
        p.value=data_[at++]; p.timeout=data_[at++];
        if (p.value>6 || !stringAt(data_,used_,at,p.info,sizeof(p.info))) return false;
    } else if (p.type==10 || p.type==12) {
        if (!stringAt(data_,used_,at,p.info,sizeof(p.info))) return false;
    } else if (p.type!=11) {
        snprintf(p.info,sizeof(p.info),"Unsupported parameter type %u",p.type);
    }
    p.valid=true; return true;
}
void CrsfTxConfig::receive(uint8_t type,const uint8_t *p,size_t n,uint32_t now)
{
    if (!p || n<2 || p[0]!=0xea || p[1]!=0xee || state_==State::Off || state_==State::Failed) return;
    if (type==0x29 && waiting_ && (state_==State::Discover || heartbeat_)) {
        size_t at=2; char name[48];
        if (!stringAt(p,n,at,name,sizeof(name)) || at+14>n) return;
        if (memcmp(p+at,"ELRS",4)!=0) { fail("Device is not an ExpressLRS TX module"); return; }
        const uint8_t count=p[at+12];
        if (!count || count>kMaxParameters) { fail("Unsupported TX parameter count"); return; }
        lastReply_=now; attempts_=0; waiting_=false;
        if (heartbeat_ && count==count_ && strcmp(name,device_)==0) { heartbeat_=false; return; }
        heartbeat_=false; count_=count; snprintf(device_,sizeof(device_),"%s",name);
        read(1,now); status("Reading TX settings; outputs CUT"); return;
    }
    const bool cmd=state_==State::Command;
    if (type!=0x2b || n<5 || !waiting_ || (!cmd && state_!=State::Read) || p[2]!=(cmd ? commandId_ : id_)) return;
    if (cmd) id_=commandId_;
    if (chunk_ && p[3]!=remaining_) return; // duplicate or reordered chunk
    if (used_+n-4>sizeof(data_)) { fail("TX parameter too large"); return; }
    memcpy(data_+used_,p+4,n-4); used_+=n-4;
    lastReply_=now; waiting_=false; attempts_=0;
    if (p[3]) {
        if (chunk_>=15) { fail("Too many TX parameter chunks"); return; }
        ++chunk_; remaining_=p[3]-1; due_=now+20; return;
    }
    TxParameter decoded;
    if (!decode(decoded)) { fail("Invalid TX parameter; no changes sent"); return; }
    parameters_[id_-1]=decoded; ++revision_;
    if (cmd) {
        chunk_=0; used_=0;
        status(decoded.info[0] ? decoded.info : "TX command running");
        if (!decoded.value) { commandId_=0; read(1,now+250); status("Command complete; refreshing settings"); }
        else due_=now+(decoded.timeout*10U < 200 ? 200 : decoded.timeout*10U);
        return;
    }
    if (verifyId_) {
        if (decoded.type!=9 || decoded.value!=desired_) { verifyId_=0; fail("TX did not accept value; refresh to retry"); return; }
        verifyId_=0; read(1,now+100); status("Value verified; refreshing related settings"); return;
    }
    if (id_<count_) read(id_+1,now+20);
    else { state_=State::Ready; due_=now+2000; status("TX settings ready; outputs CUT"); }
}
bool CrsfTxConfig::apply(uint8_t id,uint8_t value,uint32_t now)
{
    auto p=parameter(id); char label[384];
    if (!ready() || now-lastReply_>4000 || !p || p->hidden || p->type!=9 || value<p->minimum || value>p->maximum || !p->option(value,label,sizeof(label))) return false;
    verifyId_=id; desired_=value; state_=State::Write; due_=now;
    status("Applying TX setting; awaiting readback"); return true;
}
bool CrsfTxConfig::command(uint8_t id,uint8_t action,uint32_t now)
{
    auto p=parameter(id);
    if (!p || p->hidden || p->type!=13) return false;
    if (action==1) { if (!ready() || now-lastReply_>4000 || p->value!=0) return false; }
    else if (action==4) { if (!confirming() || id!=commandId_) return false; }
    else if (action==5) { if (state_!=State::Command || id!=commandId_) return false; }
    else return false;
    commandId_=id; commandAction_=action; state_=State::Command; chunk_=0; used_=0;
    waiting_=false; attempts_=0; due_=now;
    status(action==5 ? "Cancelling command" : "Waiting for TX command response"); return true;
}

#pragma once
#include <stdint.h>
#include <stddef.h>
namespace EscWire {
constexpr uint8_t SelectorChannel=12, Parameter=0xf0;
enum class Op:uint8_t { Hello=1,Read=2,Write=3,Close=4,Prepare=5 };
enum class Status:uint8_t { Ok=0,Busy=1,Rejected=2,Timeout=3,Unverified=4 };
enum Capability:uint8_t { SelectorAvailable=1, Interface=2, Writes=4, ModeSelect=8, Programming=16, Pending=32, Drive=64 };
struct Message {
    Op op=Op::Hello;uint16_t sequence=0;uint32_t session=0;
    uint8_t parameter=0,value=0;Status status=Status::Ok;uint8_t flags=0;
};
inline uint8_t crc(const uint8_t *p,size_t n){uint8_t c=0;while(n--){c^=*p++;for(int b=0;b<8;++b)c=c&128?uint8_t((c<<1)^0xd5):uint8_t(c<<1);}return c;}
// Reserved PARAMETER_WRITE index + magic/version, not an MSP or ERS envelope.
// Request payload = dest,src,F0,'S','P',1,op,seqLE16,sessionLE32,id,value.
// Response PARAMETER_SETTINGS_ENTRY appends status/capabilities.
inline size_t build(const Message &m,bool reply,uint8_t *out,size_t cap) {
    const size_t n=reply?21:19;if(!out||cap<n||!m.session)return 0;
    out[0]=0xc8;out[1]=n-2;out[2]=reply?0x2b:0x2d;out[3]=reply?0xea:0xec;out[4]=reply?0xec:0xea;
    out[5]=Parameter;out[6]='S';out[7]='P';out[8]=1;out[9]=uint8_t(m.op);
    out[10]=m.sequence;out[11]=m.sequence>>8;
    for(unsigned i=0;i<4;++i)out[12+i]=m.session>>(i*8);
    out[16]=m.parameter;out[17]=m.value;
    if(reply){out[18]=uint8_t(m.status);out[19]=m.flags;}
    out[n-1]=crc(out+2,n-3);return n;
}
inline bool parse(uint8_t type,const uint8_t *p,size_t n,bool reply,Message &m) {
    if(!p||type!=(reply?0x2b:0x2d)||n!=(reply?17u:15u)||p[0]!=(reply?0xea:0xec)||p[1]!=(reply?0xec:0xea)||p[2]!=Parameter||p[3]!='S'||p[4]!='P'||p[5]!=1||p[6]<1||p[6]>5)return false;
    m.op=Op(p[6]);m.sequence=uint16_t(p[7])|(uint16_t(p[8])<<8);m.session=0;
    for(unsigned i=0;i<4;++i)m.session|=uint32_t(p[9+i])<<(i*8);
    m.parameter=p[13];m.value=p[14];m.status=Status::Ok;m.flags=0;
    if(reply){if(p[15]>4)return false;m.status=Status(p[15]);m.flags=p[16];}
    return m.session!=0;
}
}

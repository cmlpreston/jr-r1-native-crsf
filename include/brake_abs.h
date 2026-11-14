#pragma once
#include <stdint.h>
#include <stddef.h>

// Pulsed braking, not wheel-lock detection. Only for an ESC verified in
// Forward/Brake mode; its shared brake/reverse command cannot identify direction.
struct BrakeAbsSettings {
    bool enabled=false, forwardBrakeVerified=false;
    uint8_t activationPercent=30, releasePercent=30, frequencyHz=5;
    uint16_t delayMs=200;
};
inline bool validBrakeAbs(const BrakeAbsSettings &s) {
    return s.activationPercent>=10&&s.activationPercent<=90&&
        s.releasePercent>=10&&s.releasePercent<=90&&s.frequencyHz>=2&&s.frequencyHz<=10&&s.delayMs<=1000&&
        (!s.enabled||s.forwardBrakeVerified);
}
inline bool equalBrakeAbs(const BrakeAbsSettings &a,const BrakeAbsSettings &b) {
    return a.enabled==b.enabled&&a.forwardBrakeVerified==b.forwardBrakeVerified&&
        a.activationPercent==b.activationPercent&&a.releasePercent==b.releasePercent&&
        a.frequencyHz==b.frequencyHz&&a.delayMs==b.delayMs;
}
// Stable per-model record; checksum, strict flags and transactional decoding.
constexpr size_t kBrakeAbsRecordSize=12;
inline bool encodeBrakeAbs(const BrakeAbsSettings &s,uint8_t *p,size_t n) {
    if(!p||n!=kBrakeAbsRecordSize||!validBrakeAbs(s))return false;
    p[0]='A';p[1]='B';p[2]=1;p[3]=s.enabled|(s.forwardBrakeVerified<<1);
    p[4]=s.activationPercent;p[5]=s.releasePercent;p[6]=s.frequencyHz;p[7]=0;
    p[8]=s.delayMs;p[9]=s.delayMs>>8;uint16_t h=0x4a52;
    for(unsigned i=0;i<10;++i)h=uint16_t((h*257)^p[i]);
    p[10]=h;p[11]=h>>8;return true;
}
inline bool decodeBrakeAbs(const uint8_t *p,size_t n,BrakeAbsSettings &out) {
    if(!p||n!=kBrakeAbsRecordSize||p[0]!='A'||p[1]!='B'||p[2]!=1||p[3]>3||p[7])return false;
    uint16_t h=0x4a52;for(unsigned i=0;i<10;++i)h=uint16_t((h*257)^p[i]);
    if(h!=(uint16_t(p[10])|(uint16_t(p[11])<<8)))return false;
    BrakeAbsSettings s;s.enabled=p[3]&1;s.forwardBrakeVerified=p[3]&2;
    s.activationPercent=p[4];s.releasePercent=p[5];s.frequencyHz=p[6];s.delayMs=p[8]|uint16_t(p[9])<<8;
    if(!validBrakeAbs(s))return false;
    out=s;return true;
}
class BrakeAbsRuntime {
public:
    void reset(){timing_=false;}
    int16_t apply(const BrakeAbsSettings &s,int16_t physicalThrottle,int16_t command,
                  int16_t neutral,bool reversed,uint32_t now,bool live) {
        // A trim must not make the release half generate opposite-side drive.
        // Use the brake-side portion of safe neutral, bounded by signal centre.
        if((!reversed&&neutral>0)||(reversed&&neutral<0))neutral=0;
        const int32_t delta=int32_t(command)-neutral;
        // The physical brake side activates ABS, but a mix must not turn that
        // into a forward-side command. Shape/slew/trim precede this stage.
        if(!live||!s.enabled||!validBrakeAbs(s)||physicalThrottle>-int(s.activationPercent)*10||
           (!reversed&&delta>=0)||(reversed&&delta<=0)) {reset();return command;}
        if(!timing_){timing_=true;start_=now;}
        const uint32_t elapsed=now-start_;
        if(elapsed<s.delayMs)return command;
        // Rational phase avoids integer-period drift; 50% strong / 50% reduced.
        const uint32_t phase=(uint64_t(elapsed-s.delayMs)*s.frequencyHz)%1000;
        if(phase<500)return command;
        // Reduce toward configured neutral, never cross it or deepen braking.
        return int16_t(neutral+delta*(100-s.releasePercent)/100);
    }
private:
    bool timing_=false;uint32_t start_=0;
};

#pragma once
#include <stdint.h>
#include "control_assignment.h"

// Sources 0..18: physical; 19..34: logical CH1..16; 35..36: configured steering/throttle.
constexpr uint8_t kMixChannelSource = 19, kMixConfiguredSource = 35, kMixSourceCount = 37;
enum class MixCondition : uint8_t { Always, SwitchLow, SwitchHigh, Walk, Normal };
struct ChannelMixRuleV3 {
    uint8_t enabled = 0;
    uint8_t source = 0;
    int8_t weight = 100;
    uint8_t replace = 0;
    uint8_t condition = 0;
    uint8_t control = 3;
    int8_t negativeWeight = 100;
    uint8_t splitWeight = 0;
    uint8_t followTrim = 0;
};
struct ChannelMixSettingsV2 {
    ChannelMixRuleV3 rules[2];
    uint8_t includeBase = 1;
    uint8_t rateEnabled = 0;
    uint8_t rateControl = 3;
    uint8_t lowNegative = 50, lowPositive = 50;
    uint8_t highNegative = 100, highPositive = 100;
    uint8_t reserved = 0;
    uint16_t transitionMs = 0;
};
static_assert(sizeof(ChannelMixSettingsV2)==28,"Version 2 mixer layout");
struct ChannelMixSettingsV3 : ChannelMixSettingsV2 {
    int8_t constantPercent=0;
    int8_t outputMinimum=-100,outputMaximum=100;
    uint8_t cutOverride=0;
    int8_t cutPercent=0;
    uint8_t reservedV3=0;
};
static_assert(sizeof(ChannelMixSettingsV3)==34,"Version 3 mixer layout");
// Threshold rules contribute 0 or +100%, before destination response/limits.
enum class MixSourceMode : uint8_t { Proportional, Above, Below };
struct ChannelMixRule : ChannelMixRuleV3 {
    uint8_t sourceMode=0;
    int8_t thresholdPercent=0;
    uint8_t hysteresisPercent=5;
};
struct ChannelMixSettings {
    ChannelMixRule rules[2];
    uint8_t includeBase=1,rateEnabled=0,rateControl=3;
    uint8_t lowNegative=50,lowPositive=50,highNegative=100,highPositive=100;
    uint8_t reserved=0;
    uint16_t transitionMs=0;
    int8_t constantPercent=0,outputMinimum=-100,outputMaximum=100;
    uint8_t cutOverride=0;
    int8_t cutPercent=0;
    uint8_t reservedV3=0;
};
static_assert(sizeof(ChannelMixSettings)==40,"Version 4 mixer layout");
inline ChannelMixSettings migrateChannelMix(const ChannelMixSettingsV2 &old) {
    ChannelMixSettings m;
    for(int n=0;n<2;++n)static_cast<ChannelMixRuleV3 &>(m.rules[n])=old.rules[n];
    m.includeBase=old.includeBase;m.rateEnabled=old.rateEnabled;m.rateControl=old.rateControl;
    m.lowNegative=old.lowNegative;m.lowPositive=old.lowPositive;
    m.highNegative=old.highNegative;m.highPositive=old.highPositive;m.transitionMs=old.transitionMs;
    return m;
}
inline ChannelMixSettings migrateChannelMix(const ChannelMixSettingsV3 &old) {
    auto m=migrateChannelMix(static_cast<const ChannelMixSettingsV2 &>(old));
    m.constantPercent=old.constantPercent;m.outputMinimum=old.outputMinimum;m.outputMaximum=old.outputMaximum;
    m.cutOverride=old.cutOverride;m.cutPercent=old.cutPercent;return m;
}
// Exact legacy layout, used only when reading the v1 blob.
struct LegacyMixRule { uint8_t enabled,source; int8_t weight; uint8_t replace,condition,control; };
struct LegacyMixSettings { LegacyMixRule rules[2]; uint8_t rateEnabled,rateControl,lowNegative,lowPositive,highNegative,highPositive; uint16_t transitionMs; };
static_assert(sizeof(LegacyMixSettings)==20,"Legacy mixer layout");
inline ChannelMixSettings migrateChannelMix(const LegacyMixSettings &old) {
    ChannelMixSettings m;
    for(int n=0;n<2;++n) { auto &r=m.rules[n];const auto &v=old.rules[n];
        r.enabled=v.enabled;r.source=v.source;r.weight=v.weight;r.negativeWeight=v.weight;
        r.replace=v.replace;r.condition=v.condition;r.control=v.control;
    }
    m.rateEnabled=old.rateEnabled;m.rateControl=old.rateControl;m.lowNegative=old.lowNegative;
    m.lowPositive=old.lowPositive;m.highNegative=old.highNegative;m.highPositive=old.highPositive;m.transitionMs=old.transitionMs;
    return m;
}
inline bool mixSwitch(uint8_t id) { return id==3||id==9||id==10; }
inline bool validChannelMix(const ChannelMixSettings &m) {
    for(const auto &r:m.rules)
        if(r.enabled>1||r.source>=kMixSourceCount||
           r.weight < -100||r.weight>100||r.negativeWeight < -100||r.negativeWeight>100||r.splitWeight>1||r.followTrim>1||r.replace>1||r.condition>4||!mixSwitch(r.control)||r.sourceMode>2||r.thresholdPercent < -100||r.thresholdPercent>100||r.hysteresisPercent>100||
           (r.sourceMode==1&&int(r.thresholdPercent)-r.hysteresisPercent < -100)||
           (r.sourceMode==2&&int(r.thresholdPercent)+r.hysteresisPercent>100))return false;
    return m.constantPercent>=-100&&m.constantPercent<=100&&m.outputMinimum>=-100&&m.outputMaximum<=100&&
           m.outputMinimum<=m.outputMaximum&&m.cutOverride<=1&&m.cutPercent>=-100&&m.cutPercent<=100&&m.includeBase<=1&&m.rateEnabled<=1&&mixSwitch(m.rateControl)&&m.lowNegative<=100&&m.lowPositive<=100&&
           m.highNegative<=100&&m.highPositive<=100&&m.transitionMs<=2000;
}
struct MixInputSnapshot {
    int16_t values[kMixSourceCount] = {};
    int16_t trims[kMixSourceCount] = {};
    uint64_t valid = 0;
};
inline bool mixInputValid(const MixInputSnapshot &i,uint8_t id) {
    return id<kMixSourceCount&&(i.valid&(uint64_t{1}<<id));
}
inline bool mixConditionActive(const ChannelMixRule &r,const MixInputSnapshot &i,bool walk) {
    switch(static_cast<MixCondition>(r.condition)) {
        case MixCondition::Always:return true;
        case MixCondition::SwitchLow:return i.values[r.control]<=0;
        case MixCondition::SwitchHigh:return i.values[r.control]>0;
        case MixCondition::Walk:return walk;
        case MixCondition::Normal:return !walk;
    }
    return false;
}
inline bool channelMixInputsValid(const ChannelMixSettings &m,const MixInputSnapshot &i,bool walk) {
    if(!validChannelMix(m))return false;
    if(m.rateEnabled&&!mixInputValid(i,m.rateControl))return false;
    for(const auto &r:m.rules)if(r.enabled) {
        if((r.condition==1||r.condition==2)&&!mixInputValid(i,r.control))return false;
        if(mixConditionActive(r,i,walk)&&!mixInputValid(i,r.source))return false;
    }
    return true;
}
inline bool channelHasMix(const ChannelMixSettings &m) {
    return m.constantPercent||m.outputMinimum!=-100||m.outputMaximum!=100||m.cutOverride||!m.includeBase||m.rateEnabled||m.rules[0].enabled||m.rules[1].enabled;
}
// Strict trigger comparison, inclusive release comparison; zero hysteresis is well-defined.
inline bool thresholdMixState(const ChannelMixRule &r,int32_t source,bool active) {
    const int threshold=int(r.thresholdPercent)*10,hysteresis=int(r.hysteresisPercent)*10;
    if(r.sourceMode==1)return active?source>threshold-hysteresis:source>threshold;
    return active?source<threshold+hysteresis:source<threshold;
}
class ChannelMixRuntime {
    bool thresholdActive_[16][2]={};
    int16_t rates_[16][2] = {};
    uint32_t last_[16] = {};
    bool ready_[16] = {};
public:
    void reset() { for(auto &r:ready_)r=false;for(auto &channel:thresholdActive_)for(auto &active:channel)active=false; }
    // Called before response, endpoints and trims. Invalid inputs are handled
    // by final output safety; a bad source must never become a live zero sample.
    int16_t apply(uint8_t channel,const ChannelMixSettings &m,const MixInputSnapshot &i,
                  bool walk,int16_t base,uint32_t now,bool live,int16_t *rateOut=nullptr,int16_t *contributions=nullptr) {
        if(channel<1||channel>16)return base;
        const unsigned index=channel-1;
        if(!channelMixInputsValid(m,i,walk)) {
            for(auto &active:thresholdActive_[index])active=false;
            ready_[index]=false;return base;
        }
        if(!live)for(auto &active:thresholdActive_[index])active=false;
        int32_t value=(m.includeBase?base:0)+int32_t(m.constantPercent)*10;
        for(int n=0;n<2;++n) {
            const auto &r=m.rules[n];auto &active=thresholdActive_[index][n];
            if(contributions)contributions[n]=0;
            if(!r.enabled||!mixConditionActive(r,i,walk)){active=false;continue;}
            const int32_t source=int32_t(i.values[r.source])+(r.followTrim?i.trims[r.source]:0);
            const int weight=(source<0&&r.splitWeight)?r.negativeWeight:r.weight;
            int32_t contribution=source*weight/100;
            if(r.sourceMode){active=thresholdMixState(r,source,active);contribution=active?1000:0;}
            else active=false;
            if(!live)active=false; // CUT/preview may calculate a demand but retain no latch history.
            if(contributions)contributions[n]=contribution;
            value=(r.replace?0:value)+contribution;
        }
        value=value<-1000?-1000:value>1000?1000:value;
        const bool high=m.rateEnabled&&i.values[m.rateControl]>0;
        const int target[2]={10*(m.rateEnabled?(high?m.highNegative:m.lowNegative):100),
                             10*(m.rateEnabled?(high?m.highPositive:m.lowPositive):100)};
        uint32_t elapsed=now-last_[index];if(elapsed>100)elapsed=100;
        for(int side=0;side<2;++side) {
            if(!ready_[index]||!live||!m.transitionMs)rates_[index][side]=target[side];
            else {
                int step=(1000*elapsed+m.transitionMs-1)/m.transitionMs;
                int delta=target[side]-rates_[index][side];
                rates_[index][side]+=delta>step?step:delta < -step?-step:delta;
            }
        }
        last_[index]=now;ready_[index]=true;
        if(rateOut){*rateOut=rates_[index][value>0?1:0];return value;}
        return value*rates_[index][value>0?1:0]/1000;
    }
};

// Validate the entire potential graph, including currently inactive switch branches.
inline bool visitMixGraph(const ChannelMixSettings *all,uint8_t ch,uint16_t &visiting,uint16_t &done) {
    const uint16_t bit=uint16_t(1U<<ch);
    if(visiting&bit)return false;
    if(done&bit)return true;
    if(!validChannelMix(all[ch]))return false;
    visiting|=bit;
    for(const auto &r:all[ch].rules)if(r.enabled&&r.source>=kMixChannelSource&&r.source<kMixConfiguredSource)
        if(!visitMixGraph(all,r.source-kMixChannelSource,visiting,done))return false;
    visiting&=~bit;done|=bit;return true;
}
inline bool validChannelMixGraph(const ChannelMixSettings *all) {
    uint16_t visiting=0,done=0;
    for(uint8_t ch=0;ch<16;++ch)if(!visitMixGraph(all,ch,visiting,done))return false;
    return true;
}

// Call only after graph validation. Threshold latches are auxiliary-only,
// including indirect dependencies, so drive-role neutral checks remain stateless.
inline bool channelMixUsesThreshold(const ChannelMixSettings *all,uint8_t index,uint16_t &visited) {
    const uint16_t bit=uint16_t(1U<<index);
    if(visited&bit)return false;
    visited|=bit;
    for(const auto &r:all[index].rules)if(r.enabled) {
        if(r.sourceMode)return true;
        if(r.source>=kMixChannelSource&&r.source<kMixConfiguredSource&&
           channelMixUsesThreshold(all,r.source-kMixChannelSource,visited))return true;
    }
    return false;
}

inline bool channelMixUsesThreshold(const ChannelMixSettings *all,uint8_t index) {
    uint16_t visited=0;return channelMixUsesThreshold(all,index,visited);
}

inline int16_t limitMixedOutput(const ChannelMixSettings &m,int16_t value) {
    const int16_t low=int16_t(m.outputMinimum)*10,high=int16_t(m.outputMaximum)*10;
    return value<low?low:value>high?high:value;
}

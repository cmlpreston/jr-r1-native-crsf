#pragma once
#include <stdint.h>
#include <stddef.h>

// UI-owned calibrated role inputs, before expo, mixing, mechanical trim or CUT.
// Keep actual observation times; do not synthesize samples after a blocked UI.
class ControlHistory {
public:
    static constexpr uint32_t WindowMs=10000,PeriodMs=100,FreshnessMs=100;
    static constexpr size_t Capacity=101;
    struct Sample {
        uint32_t stampMs=0;
        int16_t steering=0,throttle=0;
        bool steeringValid=false,throttleValid=false,breakBefore=true;
    };
    void reset(){head_=count_=0;sourceSeen_=stored_=forceBreak_=false;nextDue_=lastSource_=lastStored_=0;}
    size_t count()const{return count_;}
    const Sample &sample(size_t index)const{return samples_[(head_+index)%Capacity];}
    void update(int16_t steering,int16_t throttle,uint32_t sampleStamp,
                bool steeringValid,bool throttleValid,uint32_t now){
        while(count_&&uint32_t(now-sample(0).stampMs)>WindowMs){head_=(head_+1)%Capacity;--count_;}
        const bool fresh=uint32_t(now-sampleStamp)<=FreshnessMs;
        uint32_t stamp=sampleStamp;
        if(!fresh){steeringValid=throttleValid=false;stamp=now;}
        else{
            const uint32_t elapsed=sampleStamp-lastSource_;
            if(sourceSeen_&&!elapsed){
                if(steeringValid&&throttleValid)return;
                stamp=now; // A newly invalid role is a gap, not a new raw sample.
                steeringValid=throttleValid=false;
            }
            if(sourceSeen_&&elapsed>=0x80000000u){forceBreak_=true;return;}
            lastSource_=sampleStamp;sourceSeen_=true;
        }
        if(stored_&&uint32_t(stamp-nextDue_)>=0x80000000u)return;
        Sample point;
        point.stampMs=stamp;
        point.steeringValid=steeringValid&&steering>=-1000&&steering<=1000;
        point.throttleValid=throttleValid&&throttle>=-1000&&throttle<=1000;
        point.steering=point.steeringValid?steering:0;
        point.throttle=point.throttleValid?throttle:0;
        point.breakBefore=forceBreak_||!stored_||uint32_t(stamp-lastStored_)>2*PeriodMs;
        if(count_==Capacity){head_=(head_+1)%Capacity;--count_;}
        samples_[(head_+count_)%Capacity]=point;++count_;
        if(!stored_)nextDue_=stamp+PeriodMs;
        else nextDue_+=(uint32_t(stamp-nextDue_)/PeriodMs+1)*PeriodMs;
        lastStored_=stamp;stored_=true;forceBreak_=false;
    }
private:
    Sample samples_[Capacity];
    size_t head_=0,count_=0;
    uint32_t nextDue_=0,lastSource_=0,lastStored_=0;
    bool sourceSeen_=false,stored_=false,forceBreak_=false;
};

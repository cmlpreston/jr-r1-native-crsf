#pragma once
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

// UI observations on a bounded one-second grid. Never invent samples when
// the UI stalls, or turn cached packets from another model into fresh data.
class SensorHistory {
public:
    static constexpr uint32_t WindowMs=60000,PeriodMs=1000;
    static constexpr size_t Capacity=61;
    struct Sample {uint32_t observed=0;int32_t value=0;bool valid=false;};
    struct Range {int32_t minimum=0,maximum=1;bool available=false;};
    void reset(uint32_t ignoredStamp){head_=count_=0;stored_=false;blocked_=true;ignored_=ignoredStamp;}
    // The caller supplies source validity, including any telemetry expiry.
    void update(int32_t value,uint32_t stamp,bool valid,uint32_t now){
        if(stored_&&uint32_t(now-last_)<PeriodMs)return;
        Sample sample;sample.observed=now;sample.value=value;
        if(blocked_&&valid&&stamp!=ignored_)blocked_=false;
        sample.valid=valid&&!blocked_;
        if(count_==Capacity){head_=(head_+1)%Capacity;--count_;}
        samples_[(head_+count_++)%Capacity]=sample;last_=now;stored_=true;
    }
    Range project(uint32_t now,int32_t *values,bool *valid)const{
        for(size_t i=0;i<Capacity;++i){values[i]=0;valid[i]=false;}
        for(size_t i=0;i<count_;++i){
            const auto &s=samples_[(head_+i)%Capacity];const uint32_t age=now-s.observed;
            if(age>WindowMs)continue;
            const size_t slot=Capacity-1-age/PeriodMs;values[slot]=s.value;valid[slot]=s.valid;
        }
        Range range;
        for(size_t i=0;i<Capacity;++i)if(valid[i]){
            if(!range.available){range.minimum=range.maximum=values[i];range.available=true;}
            else {if(values[i]<range.minimum)range.minimum=values[i];if(values[i]>range.maximum)range.maximum=values[i];}
        }
        if(range.available&&range.minimum==range.maximum){
            const int64_t value=range.minimum;
            const int64_t magnitude=value<0?-value:value;
            const int64_t padding=magnitude>=100?magnitude/100:1;
            range.minimum=value-padding<INT32_MIN?INT32_MIN:int32_t(value-padding);
            range.maximum=value+padding>INT32_MAX?INT32_MAX:int32_t(value+padding);
        }
        return range;
    }
private:
    Sample samples_[Capacity];
    size_t head_=0,count_=0;
    uint32_t last_=0,ignored_=0;
    bool stored_=false,blocked_=false;
};

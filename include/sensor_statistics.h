#pragma once
#include <stdint.h>

// Fresh packet samples, with bounded one-second bins for the rolling window.
// Averages are sample means; missing/stale telemetry adds no samples.
class SensorStatistics {
    struct Bin { int64_t sum=0; uint32_t count=0, second=0; int32_t maximum=0,minimum=0; };
    Bin bins_[30];
    int64_t sum_=0;uint64_t count_=0;
    int32_t maximum_=0,minimum_=0;uint32_t stamp_=0;bool seen_=false;
public:
    struct Result {
        bool valid;int32_t maximum,minimum,average;
        Result(bool v=false,int32_t hi=0,int32_t lo=0,int32_t mean=0):valid(v),maximum(hi),minimum(lo),average(mean){}
    };
    void reset(uint32_t stamp) { *this=SensorStatistics{};stamp_=stamp;seen_=true; }
    void update(int32_t value,uint32_t stamp,uint32_t now,bool valid) {
        if(!valid || uint32_t(now-stamp)>3000 || (seen_&&stamp==stamp_))return;
        stamp_=stamp;seen_=true;
        if(!count_ || value>maximum_)maximum_=value;
        if(!count_ || value<minimum_)minimum_=value;
        sum_+=value;++count_;
        const uint32_t second=now/1000;
        auto &bin=bins_[second%30];
        if(!bin.count || bin.second!=second)bin=Bin{};
        bin.second=second;
        if(!bin.count || value>bin.maximum)bin.maximum=value;
        if(!bin.count || value<bin.minimum)bin.minimum=value;
        bin.sum+=value;++bin.count;
    }
    Result result(bool session,uint32_t now) const {
        if(session)return {count_!=0,maximum_,minimum_,count_?int32_t(sum_/int64_t(count_)):0};
        Result result;int64_t sum=0;uint64_t count=0;
        for(const auto &bin:bins_)if(bin.count && uint32_t(now/1000-bin.second)<30){
            if(!count || bin.maximum>result.maximum)result.maximum=bin.maximum;
            if(!count || bin.minimum<result.minimum)result.minimum=bin.minimum;
            sum+=bin.sum;count+=bin.count;
        }
        result.valid=count!=0;result.average=count?int32_t(sum/int64_t(count)):0;return result;
    }
};

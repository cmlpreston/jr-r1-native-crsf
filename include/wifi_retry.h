#pragma once
#include <cstdint>
// Bounded reconnect backoff; unsigned elapsed time remains valid at millis wrap.
class WifiRetry {
    uint32_t scheduledAt_=0, delay_=0;
    uint8_t failures_=0;
public:
    void reset(){failures_=0;delay_=0;}
    uint32_t failed(uint32_t now){
        static constexpr uint32_t delays[]={2000,5000,10000,20000,30000};
        delay_=delays[failures_];if(failures_<4)++failures_;
        scheduledAt_=now;return delay_;
    }
    bool ready(uint32_t now)const{return uint32_t(now-scheduledAt_)>=delay_;}
};

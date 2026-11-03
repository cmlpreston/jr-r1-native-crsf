#pragma once
#include <stdint.h>
struct DriveBatterySettings {
    uint8_t chemistry=1,cells=4;
    uint16_t packMv=14000,cellMv=3500,capacityMah=6500;
    bool packEnabled=true,cellEnabled=true;
};
class DriveBatteryAlarm {
    uint32_t since_=0,next_=0;bool pending_=false,active_=false;
public:
    bool active()const{return active_;}
    bool update(bool valid,uint32_t value,uint32_t limit,uint32_t hysteresis,uint32_t now){
        if(!valid){pending_=active_=false;return false;}
        if(value>limit+(active_?hysteresis:0)){pending_=active_=false;return false;}
        if(!pending_){pending_=true;since_=now;}
        if(!active_ && uint32_t(now-since_)>=2000){active_=true;next_=now;}
        if(active_ && int32_t(now-next_)>=0){next_=now+10000;return true;}
        return false;
    }
};

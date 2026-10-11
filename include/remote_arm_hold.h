#pragma once
#include <stdint.h>

// A remote hold never arms on its own. A fresh, explicit release consumes it.
class RemoteArmHold {
public:
    uint32_t begin(uint32_t page,uint32_t now) {
        if(++sequence_>0x7fffffffU)sequence_=1;
        id_=sequence_;page_=page;start_=last_=now;return id_;
    }
    void cancel(){id_=0;}
    bool heartbeat(uint32_t id,uint32_t page,uint32_t now){
        if(!fresh(id,page,now)){cancel();return false;}last_=now;return true;
    }
    bool finish(uint32_t id,uint32_t page,uint32_t now){
        const bool ok=fresh(id,page,now)&&uint32_t(now-start_)>=800;
        cancel();return ok;
    }
    uint32_t id() const{return id_;}
private:
    bool fresh(uint32_t id,uint32_t page,uint32_t now) const{
        return id_&&id==id_&&page==page_&&uint32_t(now-start_)<=5000&&uint32_t(now-last_)<=900;
    }
    uint32_t sequence_=0,id_=0,page_=0,start_=0,last_=0;
};

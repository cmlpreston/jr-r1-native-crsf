#pragma once
#include <atomic>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct ErsReply {
    uint32_t receivedMs=0;
    uint8_t frameType=0,length=0,payload[60]={};
};

// One CRSF producer, one UI consumer. No allocation, waiting or shared
// telemetry-snapshot copies. Full queues drop the new reply and count it;
// unread slots are never overwritten while the consumer copies them.
class ErsReplyQueue {
public:
    static constexpr uint32_t Capacity=16;
    static_assert((Capacity&(Capacity-1))==0,"Queue capacity must divide counter wrap");
    explicit ErsReplyQueue(uint32_t initial=0):write_(initial),read_(initial){}
    bool push(uint8_t type,const uint8_t *payload,size_t length,uint32_t now){
        if(!payload||length<6||length>sizeof(ErsReply::payload))return false;
        const uint32_t write=write_.load(std::memory_order_relaxed);
        if(uint32_t(write-read_.load(std::memory_order_acquire))>=Capacity){
            dropped_.fetch_add(1,std::memory_order_relaxed);return false;
        }
        auto &reply=replies_[write&(Capacity-1)];
        reply.receivedMs=now;reply.frameType=type;reply.length=uint8_t(length);
        memcpy(reply.payload,payload,length);
        write_.store(write+1,std::memory_order_release);return true;
    }
    bool pop(ErsReply &reply){
        const uint32_t read=read_.load(std::memory_order_relaxed);
        if(read==write_.load(std::memory_order_acquire))return false;
        reply=replies_[read&(Capacity-1)];
        read_.store(read+1,std::memory_order_release);return true;
    }
    uint32_t dropped()const{return dropped_.load(std::memory_order_relaxed);}
private:
    ErsReply replies_[Capacity]={};
    std::atomic<uint32_t> write_{0},read_{0},dropped_{0};
};

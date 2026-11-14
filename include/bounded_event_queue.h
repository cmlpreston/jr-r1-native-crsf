#pragma once
#include <stddef.h>
#include <stdint.h>

// Fixed storage and bounded work. Caller serializes producers/consumer; neither
// push nor pop allocates, waits for space, formats messages or invokes a sink.
template<class T, size_t Capacity> class BoundedEventQueue {
    static_assert(Capacity > 0, "Queue must have storage");
public:
    bool push(const T &value) {
        if(count_==Capacity){++dropped_;return false;}
        data_[(head_+count_)%Capacity]=value;++count_;return true;
    }
    bool pop(T &value) {
        if(!count_)return false;
        value=data_[head_];head_=(head_+1)%Capacity;--count_;return true;
    }
    void clear(){head_=count_=0;}
    size_t size()const{return count_;}
    uint32_t dropped()const{return dropped_;}
private:
    T data_[Capacity]={};size_t head_=0,count_=0;uint32_t dropped_=0;
};

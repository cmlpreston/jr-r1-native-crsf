#pragma once
#include <cstdint>
#include <cassert>
#include <deque>
#include <algorithm>
inline uint32_t listenerUs=0;
inline uint32_t micros(){return listenerUs++;}

inline uint32_t millis(){return listenerUs/1000;}
inline bool outputEnabled=false;
inline int gpioMode=1,padLevel=1,outputSignal=256,outputLatch=1;

inline void(*delayHook)()=nullptr;
inline void delay(unsigned ms){listenerUs+=ms*1000;if(delayHook)delayHook();}

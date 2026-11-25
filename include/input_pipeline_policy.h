#pragma once
#include <stdint.h>
constexpr uint32_t PhysicalInputStaleMs=100;
inline bool physicalInputSampleFresh(uint32_t now,uint32_t sampled) {
    return sampled!=0 && uint32_t(now-sampled)<=PhysicalInputStaleMs;
}
inline bool inputDispatchCurrent(uint32_t binding,uint32_t safety,
                                 uint32_t currentBinding,uint32_t currentSafety) {
    return binding==currentBinding && safety==currentSafety;
}

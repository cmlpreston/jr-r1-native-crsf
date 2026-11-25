#pragma once

// All runtime users of the shared expander/touch Wire bus take this guard.
// Initialize once during setup, before starting the input sampler. The hardware
// mutex is static, recursive and priority-inheriting; no heap allocation or
// retry-on-allocation-failure path may silently use the bus without a lock.
#if defined(JR_R1_INPUT_BUS_HOST_LOCK)
#include <mutex>
#elif defined(ARDUINO) && !defined(JR_R1_SIMULATOR)
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

namespace InputBus {
#if defined(JR_R1_INPUT_BUS_HOST_LOCK)
inline std::recursive_mutex &mutex() { static std::recursive_mutex value;return value; }
inline bool &initialized() { static bool value=false;return value; }
inline bool begin() { initialized()=true;return true; }
#elif defined(ARDUINO) && !defined(JR_R1_SIMULATOR)
inline SemaphoreHandle_t &mutex() { static SemaphoreHandle_t value=nullptr;return value; }
inline StaticSemaphore_t &storage() { static StaticSemaphore_t value;return value; }
inline bool begin() {
    if(!mutex())mutex()=xSemaphoreCreateRecursiveMutexStatic(&storage());
    return mutex()!=nullptr;
}
#else
inline bool begin() { return true; }
#endif

class Guard {
public:
    Guard() {
#if defined(JR_R1_INPUT_BUS_HOST_LOCK)
        if(initialized()){mutex().lock();locked_=true;}
#elif defined(ARDUINO) && !defined(JR_R1_SIMULATOR)
        locked_=mutex()&&xSemaphoreTakeRecursive(mutex(),portMAX_DELAY)==pdTRUE;
#else
        locked_=true;
#endif
    }
    ~Guard() {
#if defined(JR_R1_INPUT_BUS_HOST_LOCK)
        if(locked_)mutex().unlock();
#elif defined(ARDUINO) && !defined(JR_R1_SIMULATOR)
        if(locked_)xSemaphoreGiveRecursive(mutex());
#endif
    }
    explicit operator bool() const { return locked_; }
    Guard(const Guard &)=delete;
    Guard &operator=(const Guard &)=delete;
private:
    bool locked_=false;
};
}

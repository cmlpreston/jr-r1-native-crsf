#include "ui_telemetry_history.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>

static bool psramAvailable=true,internalAvailable=true;
static unsigned psramCalls=0,internalCalls=0,constructed=0,destroyed=0;
void *heap_caps_malloc(size_t bytes,unsigned capabilities){
    if(capabilities&MALLOC_CAP_SPIRAM){++psramCalls;if(!psramAvailable)return nullptr;}
    if(capabilities&MALLOC_CAP_INTERNAL){++internalCalls;if(!internalAvailable)return nullptr;}
    return std::malloc(bytes);
}
struct Tracked {
    unsigned value=42;
    Tracked(){++constructed;}
    ~Tracked(){++destroyed;}
};
int main(){
    {
        UiColdStorage<Tracked> storage;
        assert(!storage&&constructed==0);
        assert(storage.initialize()&&storage.get()->value==42);
        const auto *p=storage.get();
        assert(storage.initialize()&&storage.get()==p&&constructed==1&&psramCalls==1);
        storage.reset();assert(!storage&&destroyed==1);
    }
    assert(destroyed==1);
    psramAvailable=false;
    {
        UiColdStorage<Tracked> storage;
        assert(storage.initialize()&&internalCalls==1&&constructed==2);
    }
    internalAvailable=false;
    {
        UiColdStorage<Tracked> storage;
        assert(!storage.initialize()&&!storage&&constructed==2);
        internalAvailable=true;
        assert(storage.initialize()&&constructed==3); // failed allocation is retryable
    }
    assert(destroyed==constructed);
    psramAvailable=true;
    {
        UiColdStorage<UiTelemetryHistory<18>> storage;
        assert(storage.initialize());auto &h=*storage.get();
        assert(!h.frozenRange.available && !h.frozenValid[SensorHistory::Capacity-1]);
        UiColdArrayView<uint16_t[30],16> channels;channels.attach(h.channels);
        assert(channels.bytes()==16*30*sizeof(uint16_t));
        channels[15][29]=321;assert(h.channels[15][29]==321);
        h.statistics[17].update(100,1000,1000,true);
        h.statistics[17].update(300,1100,1100,true);
        assert(h.statistics[17].result(true,1100).average==200);
        assert(!h.statistics[17].result(false,32000).valid);
        assert(h.statistics[17].result(true,32000).maximum==300);
        h.statistics[17].reset(32000);assert(!h.statistics[17].result(true,32000).valid);
    }
    puts("UI cold storage: PSRAM preference, internal fallback, exhaustion/retry, stable lifetime and statistics semantics PASS");
}

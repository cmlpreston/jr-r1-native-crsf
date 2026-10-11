#include "memory_diagnostics.h"
#ifndef JR_R1_SIMULATOR
#include <esp_heap_caps.h>
#include <lvgl.h>
namespace MemoryDiagnostics {
static Snapshot current;
static TaskHandle_t crsfTask=nullptr,controlTask=nullptr;
static bool uiReady=false,hasSample=false;
void configure(TaskHandle_t crsf,TaskHandle_t control,bool ready) {
    crsfTask=crsf;controlTask=control;uiReady=ready;
}
static Pool pool(uint32_t caps) {
    multi_heap_info_t info={};heap_caps_get_info(&info,caps);
    Pool p;p.free=info.total_free_bytes;p.total=p.free+info.total_allocated_bytes;
    p.minimum=info.minimum_free_bytes;p.largest=info.largest_free_block;return p;
}
void sample(bool force) {
    const uint32_t now=millis();
    if(!force&&hasSample&&uint32_t(now-current.sampledMs)<2000)return;
    current.sampledMs=now;hasSample=true;
    current.internal=pool(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    current.psram=pool(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    current.dma=pool(MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA);
    current.lvglReady=uiReady;
    if(uiReady){lv_mem_monitor_t m={};lv_mem_monitor(&m);
        current.lvglTotal=m.total_size;current.lvglUsed=m.total_size-m.free_size;
        if(current.lvglUsed>current.lvglPeak)current.lvglPeak=current.lvglUsed;current.lvglLargest=m.free_biggest_size;current.lvglFragmentation=m.frag_pct;
    }
    // ESP-IDF's FreeRTOS reports these high-water marks in bytes, not words.
    current.loopStack=uxTaskGetStackHighWaterMark(nullptr);
    current.controlStack=controlTask?int32_t(uxTaskGetStackHighWaterMark(controlTask)):-1;
    current.crsfStack=crsfTask?int32_t(uxTaskGetStackHighWaterMark(crsfTask)):-1;
}
const Snapshot &snapshot(){sample();return current;}
void print(Print &out) {
    sample(true);char line[192];
    snprintf(line,sizeof(line),"MEMORY at %lu ms (bytes; minima/peaks since boot)",static_cast<unsigned long>(current.sampledMs));out.println(line);
    const Pool pools[]={current.internal,current.psram,current.dma};
    const char *names[]={"internal 8-bit","PSRAM","internal DMA (subset)"};
    for(int n=0;n<3;++n){const auto &p=pools[n];snprintf(line,sizeof(line),
        "  %s: total=%lu free=%lu minimum=%lu largest=%lu",names[n],
        (unsigned long)p.total,(unsigned long)p.free,(unsigned long)p.minimum,(unsigned long)p.largest);out.println(line);}
    if(current.lvglReady){snprintf(line,sizeof(line),"  LVGL: pool=%lu used=%lu sampled_peak=%lu largest_free=%lu fragmentation=%lu%%",
        (unsigned long)current.lvglTotal,(unsigned long)current.lvglUsed,(unsigned long)current.lvglPeak,
        (unsigned long)current.lvglLargest,(unsigned long)current.lvglFragmentation);out.println(line);}
    else out.println("  LVGL: not initialized");
    snprintf(line,sizeof(line),"  Stack minimum unused: UI=%ld control=%ld CRSF=%ld (-1 = not started)",
        (long)current.loopStack,(long)current.controlStack,(long)current.crsfStack);out.println(line);
}
}
#endif

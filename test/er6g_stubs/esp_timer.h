#pragma once
#include <driver/uart.h>
struct MockTimer{void(*callback)(void*)=nullptr;void* arg=nullptr;bool running=false;uint32_t deadline=0;};
using esp_timer_handle_t=MockTimer*;
constexpr int ESP_TIMER_TASK=0;
struct esp_timer_create_args_t{void(*callback)(void*);void* arg;int dispatch_method;const char* name;bool skip_unhandled_events;};
inline MockTimer mockTimer;
inline bool failTimerStart=false;
inline int esp_timer_create(const esp_timer_create_args_t *c,esp_timer_handle_t *h){mockTimer.callback=c->callback;mockTimer.arg=c->arg;*h=&mockTimer;return ESP_OK;}
inline int esp_timer_start_once(esp_timer_handle_t t,uint64_t us){assert(t==&mockTimer&&(us==69000||us==180000000)&&!t->running);if(failTimerStart)return -1;t->running=true;t->deadline=listenerUs+us;return ESP_OK;}
inline int esp_timer_stop(esp_timer_handle_t t){assert(t==&mockTimer);t->running=false;return ESP_OK;}
inline void dispatchTimer(){if(mockTimer.running&&int32_t(listenerUs-mockTimer.deadline)>=0){mockTimer.running=false;mockTimer.callback(mockTimer.arg);}}

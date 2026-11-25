#include "bounded_event_queue.h"
#include <cassert>
#include <cstdio>
struct Event{unsigned timestamp=0,reason=0;};
int main(){
    BoundedEventQueue<Event,32> q;
    // No sink runs or needs to make progress for the producer to finish. This
    // models a worker indefinitely backpressured by USB: producers drop instead.
    for(unsigned n=0;n<100000;++n){Event e;e.timestamp=n;e.reason=n%9;q.push(e);}
    assert(q.size()==32&&q.dropped()==99968);
    Event e;for(unsigned n=0;n<32;++n){assert(q.pop(e));assert(e.timestamp==n&&e.reason==n%9);}
    assert(!q.pop(e));assert(q.size()==0);
    for(unsigned n=0;n<1000;++n){e.timestamp=n;assert(q.push(e));assert(q.pop(e)&&e.timestamp==n);}
    puts("Critical diagnostic queue: blocked sink, bounded overflow/drop count, FIFO and wrap PASS");
}

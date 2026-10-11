#include "remote_arm_hold.h"
#include <cassert>
#include <cstdio>
int main(){
    RemoteArmHold h;
    auto id=h.begin(1,100);
    assert(!h.finish(id,1,600));assert(!h.finish(id,1,950)); // early release consumes
    id=h.begin(1,100);assert(h.heartbeat(id,1,400));assert(h.heartbeat(id,1,700));
    assert(h.finish(id,1,950));assert(!h.finish(id,1,1000)); // no replay
    id=h.begin(1,100);assert(!h.finish(id,1,1100)); // lost heartbeat
    id=h.begin(1,100);assert(!h.heartbeat(id,2,400));assert(!h.finish(id,1,950)); // changed page
    id=h.begin(1,100);auto newer=h.begin(1,110);assert(id!=newer);
    assert(!h.finish(id,1,950)); // old release cannot accept newer gesture
    id=h.begin(1,100);h.cancel();assert(!h.finish(id,1,950));
    id=h.begin(1,0xfffffff0U);assert(h.heartbeat(id,1,0x100));assert(h.heartbeat(id,1,0x200));
    assert(h.finish(id,1,0x320)); // monotonic timer wrap
    id=h.begin(1,100);for(unsigned t=500;t<=4900;t+=400)assert(h.heartbeat(id,1,t));
    assert(!h.finish(id,1,5200)); // maximum hold duration
    puts("Remote arm hold: timing, cancellation, heartbeat, page, nonce, expiry and wrap PASS");
}

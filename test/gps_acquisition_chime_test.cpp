#include "gps_acquisition_chime.h"
#include <cassert>
#include <cstdio>
int main() {
    GpsAcquisitionChime chime;
    assert(!chime.update(false,true,false,100).durationMs);
    auto a=chime.update(true,true,false,100);assert(a.pitchHz==2093 && a.durationMs==90);
    assert(chime.active(200) && !chime.update(true,true,false,200).durationMs);
    auto b=chime.update(true,true,false,240);assert(b.pitchHz==2637 && b.durationMs==90);
    auto c=chime.update(true,true,false,380);assert(c.pitchHz==3136 && c.durationMs==160);
    assert(!chime.active(540));assert(!chime.update(true,true,false,540).durationMs);
    chime.update(false,true,false,600);assert(!chime.update(true,true,false,700).durationMs);
    GpsAcquisitionChime muted;
    assert(!muted.update(true,false,false,0).durationMs);
    assert(!muted.update(true,true,false,100).durationMs); // No delayed sound on unmute.
    GpsAcquisitionChime interrupted;
    interrupted.update(true,true,false,0);
    assert(interrupted.update(true,false,false,30).stop);
    assert(!interrupted.update(true,true,false,140).durationMs);
    GpsAcquisitionChime priority;
    assert(!priority.update(true,true,true,0).durationMs);
    assert(!priority.update(true,true,false,140).durationMs);
    GpsAcquisitionChime preempted;
    preempted.update(true,true,false,0);
    auto stop=preempted.update(true,true,true,30);assert(!stop.durationMs && !stop.stop);
    assert(!preempted.active(40)); // Never stop the higher-priority tone.
    GpsAcquisitionChime delayed;
    delayed.update(true,true,false,0);
    auto late=delayed.update(true,true,false,300);assert(late.pitchHz==3136 && late.durationMs==140);
    assert(!delayed.update(true,true,false,1000).durationMs);
    GpsAcquisitionChime rollover;
    rollover.update(true,true,false,0xfffffff0u);
    assert(rollover.update(true,true,false,124).pitchHz==2637);
    assert(rollover.update(true,true,false,264).pitchHz==3136);
    puts("GPS first-position chime: rising notes, one-shot, mute, priority, stalls and rollover PASS");
}

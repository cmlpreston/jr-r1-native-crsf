#include "control_history.h"
#include <cassert>
#include <cstdio>

int main(){
    ControlHistory h;assert(h.count()==0);
    h.update(-1000,1000,1000,true,true,1000);
    assert(h.count()==1&&h.sample(0).stampMs==1000&&h.sample(0).breakBefore);
    h.update(-800,800,1080,true,true,1080);assert(h.count()==1);
    h.update(-600,600,1160,true,true,1160);assert(h.count()==2);
    assert(h.sample(1).stampMs==1160&&!h.sample(1).breakBefore&&h.sample(1).steering==-600);
    h.update(-400,400,1240,true,true,1240);assert(h.count()==3);
    h.update(0,0,3000,true,true,3000);
    assert(h.count()==4&&h.sample(3).stampMs==3000&&h.sample(3).breakBefore); // No invented 1.2..3s points.
    h.update(100,100,3000,true,true,4000);
    assert(h.count()==5&&h.sample(4).stampMs==4000&&!h.sample(4).steeringValid&&!h.sample(4).throttleValid);
    h.update(200,300,4100,false,true,4100);
    assert(h.count()==6&&!h.sample(5).steeringValid&&h.sample(5).throttleValid&&h.sample(5).throttle==300);
    h.update(900,900,4050,true,true,4120);assert(h.count()==6);
    h.update(999,999,4100,true,true,4150);assert(h.count()==6); // Duplicate source is not a fresh observation.
    h.update(100,200,4210,true,true,4210);assert(h.count()==7&&h.sample(6).breakBefore);
    h.update(1001,-1001,4310,true,true,4310);
    assert(!h.sample(h.count()-1).steeringValid&&!h.sample(h.count()-1).throttleValid);

    h.reset();assert(h.count()==0);
    for(uint32_t stamp=0;stamp<=20000;stamp+=100)h.update(100,-100,stamp,true,true,stamp);
    assert(h.count()==101&&h.sample(0).stampMs==10000&&h.sample(100).stampMs==20000);
    for(size_t n=1;n<h.count();++n)assert(!h.sample(n).breakBefore);
    h.update(0,0,20000,false,false,31000); // Old samples expire; a stale gap remains visible.
    assert(h.count()==1&&!h.sample(0).steeringValid&&h.sample(0).stampMs==31000);

    ControlHistory wrap;
    wrap.update(-500,500,UINT32_MAX-49,true,true,UINT32_MAX-49);
    wrap.update(500,-500,50,true,true,50);
    assert(wrap.count()==2&&!wrap.sample(1).breakBefore&&wrap.sample(1).stampMs==50);
    wrap.update(0,0,500,true,true,500);
    assert(wrap.count()==3&&wrap.sample(2).breakBefore);
    wrap.update(0,0,500,true,true,11000);
    assert(wrap.count()==1&&!wrap.sample(0).steeringValid);

    ControlHistory invalid;
    invalid.update(100,200,1000,true,true,1000);
    invalid.update(100,200,1000,false,false,1100);
    assert(invalid.count()==2&&!invalid.sample(1).steeringValid&&!invalid.sample(1).throttleValid);
    invalid.update(200,300,1200,true,true,1200);
    assert(invalid.count()==3&&invalid.sample(2).steeringValid&&invalid.sample(2).throttleValid);
    ControlHistory freshness;
    freshness.update(100,200,1000,true,true,1100);
    assert(freshness.count()==1&&freshness.sample(0).steeringValid);
    freshness.update(200,300,1100,true,true,1201);
    assert(freshness.count()==2&&!freshness.sample(1).steeringValid&&freshness.sample(1).stampMs==1201);
    puts("Control history: bounded ten-second raw inputs, timestamp bins, visible gaps, no backfill/duplicates, reset and wrap PASS");
}

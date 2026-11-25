#include "sensor_history.h"
#include <cassert>
#include <cstdio>

int main(){
    SensorHistory h;int32_t values[SensorHistory::Capacity];bool valid[SensorHistory::Capacity];
    h.update(0,1000,true,1000);h.update(900,1500,true,1500);
    auto range=h.project(1000,values,valid);assert(valid[60]&&values[60]==0&&range.available);
    h.update(70000,4000,true,4000);
    range=h.project(4000,values,valid);
    assert(valid[57]&&values[57]==0&&!valid[58]&&!valid[59]&&valid[60]&&values[60]==70000);
    h.update(90000,4000,false,8000);h.project(8000,values,valid);assert(!valid[60]);
    h.reset(8000);h.update(999,8000,true,8000);range=h.project(8000,values,valid);assert(!range.available);
    h.update(-50,9000,true,9000);range=h.project(9000,values,valid);assert(range.available&&range.minimum==-51&&range.maximum==-49);
    for(uint32_t n=10;n<=80;++n)h.update(n,n*1000,true,n*1000);
    h.project(80000,values,valid);assert(values[0]==20&&values[60]==80&&valid[0]&&valid[60]);
    range=h.project(141000,values,valid);assert(!range.available);
    h.reset(0);const uint32_t before=0xfffffe00;
    h.update(INT32_MIN,before,true,before);h.update(INT32_MAX,before+1000,true,before+1000);
    range=h.project(before+1000,values,valid);assert(range.minimum==INT32_MIN&&range.maximum==INT32_MAX);
    assert(valid[59]&&valid[60]);
    h.reset(0);h.update(INT32_MAX,1000,true,1000);range=h.project(1000,values,valid);
    assert(range.minimum<range.maximum&&range.maximum==INT32_MAX);
    // A battery observation can remain valid longer than a fast sensor. The
    // reset excludes an old packet even if the decoder still considers it valid.
    h.reset(1000);
    h.update(7400,1000,true,29000);
    assert(!h.project(29000,values,valid).available);
    h.update(7410,2000,true,32000);
    h.update(7410,2000,false,33000);
    h.project(33000,values,valid);
    assert(valid[59]&&values[59]==7410&&!valid[60]);
    puts("Sensor history: fixed minute, gaps, source validity, bounded ring, zero/negative/RPM, reset and clock wrap PASS");
}

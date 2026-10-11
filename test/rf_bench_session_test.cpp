#include "rf_bench_session.h"
#include <cassert>
#include <cstdio>

int main() {
    RfBenchSession s;
    assert(!s.requestArm(0,true));
    assert(!s.enable(false,0));
    assert(!s.enabled());
    assert(s.enable(true,100));
    assert(!s.armRequested(100));
    assert(!s.requestArm(100,false));
    assert(s.requestArm(101,true));
    assert(s.armRequested(2099));
    assert(!s.armRequested(2100));
    s.heartbeat(2101,true);
    assert(!s.armRequested(2101)); // reconnect cannot re-arm
    assert(s.requestArm(2102,true));
    s.heartbeat(2103,false);
    assert(!s.armRequested(2103)); // missing/faulty controls force CUT
    assert(s.requestArm(2104,true));
    s.heartbeat(5000,true);
    assert(!s.armRequested(5000)); // expiry enforced even before another reader
    assert(s.requestArm(5001,true));
    s.cut();assert(!s.armRequested(5001));
    assert(s.requestArm(5002,true));
    assert(!s.enable(false,5003));assert(!s.armRequested(5003));
    s.heartbeat(5004,true);
    assert(!s.requestArm(5005,true)); // an existing mode is not a valid PIN session
    s.enable(true,0xfffffff0U);
    assert(s.requestArm(0xfffffff1U,true));
    assert(s.armRequested(0x10U));
    assert(!s.armRequested(0x800U)); // millis wrap
    s.enable(true,100);
    assert(!s.requestArm(2200,true)); // stale client cannot arm
    RfBenchSession rebooted;assert(!rebooted.enabled());
    std::puts("RF bench session safety: PASS");
}

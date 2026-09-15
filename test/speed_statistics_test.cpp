#include "speed_statistics.h"
#include "binding_resolver.h"
#include <cassert>
#include <cstdio>
int main() {
    SpeedStatistics stats;
    assert(!stats.available());
    stats.update(100,1000,1000,true);
    stats.update(300,2000,2000,true); // 1 s averaging 20 km/h.
    stats.update(0,5000,5000,true); // 3 s averaging 15 km/h, stopped included.
    assert(stats.maximum()==300 && stats.average()==163);
    stats.update(999,5000,5000,true); // Repeated UI refresh is not another sample.
    assert(stats.maximum()==300 && stats.average()==163);
    stats.update(999,6000,10000,true); // Stale reading.
    stats.update(900,11000,11000,false); // No fix.
    stats.update(200,12000,12000,true); // Resume: don't integrate the outage.
    assert(stats.maximum()==300 && stats.average()==163);
    stats.update(200,13000,13000,true);
    assert(stats.average()==170);
    stats.update(200,20000,20000,true); // Missing packets without explicit invalid state.
    assert(stats.average()==170);
    stats.reset(20000);
    stats.update(200,20000,20001,true);
    assert(!stats.available());
    stats.update(0,21000,21000,true);
    assert(stats.available() && stats.maximum()==0 && stats.average()==0);
    SpeedStatistics wrap;
    wrap.update(100,UINT32_MAX-499,UINT32_MAX-499,true);
    wrap.update(300,500,500,true);
    assert(wrap.average()==200 && wrap.maximum()==300);
    // A persisted button assignment resolves to the new function; old IDs stay stable.
    static_assert(static_cast<int>(TransmitterFunction::MenuIncrease)==26);
    ControlBinding binding;
    binding.target=AssignmentTarget::TransmitterFunction;
    binding.targetId=static_cast<uint16_t>(TransmitterFunction::ResetSpeedStatistics);
    binding.input=PhysicalControlId::TimerButton;
    binding.event=InputEvent::ShortPress;
    BindingResolver resolver;
    assert(resolver.setBindings(&binding,1).ok());
    ResolvedBindingActionBuffer actions;
    PhysicalInputEvent event{};
    event.input=binding.input;event.event=binding.event;event.timestampMs=1000;
    resolver.processEvent(event,ContextAlways,actions);
    assert(actions.size()==1 && actions[0].targetId==binding.targetId && actions[0].value>0);
    puts("Speed statistics: weighting, stopped time, gaps, stale/duplicate packets, reset, wrap and binding PASS");
}

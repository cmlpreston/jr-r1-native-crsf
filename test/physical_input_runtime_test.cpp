#include <assert.h>
#include <stdio.h>

#include "physical_input_runtime.h"
#include "waveshare_touch_orientation.h"

namespace {

constexpr InputEndpoint mcp(uint8_t port)
{
    return {InputSourceKind::Mcp23017, 0x27, port, true};
}

constexpr InputEndpoint gpio(uint8_t pin)
{
    return {InputSourceKind::DirectGpio, 0, pin, true};
}

void testMcpButtonAndDirectGpio()
{
    const PhysicalInputHardwareBinding map[] = {
        {PhysicalControlId::TimerButton, mcp(0), {}},
        {PhysicalControlId::KeyPlus, gpio(6), {}},
    };
    PhysicalInputRuntime runtime;
    PhysicalInputEventBuffer events;
    assert(runtime.begin(map, 2));

    runtime.update(0xffff, uint64_t{1} << 6, 0, events);
    assert(events.size() == 0);

    runtime.update(0xfffe, uint64_t{1} << 6, 10, events);
    runtime.update(0xfffe, uint64_t{1} << 6, 30, events);
    assert(events.size() == 1);
    assert(events[0].input == PhysicalControlId::TimerButton);
    assert(events[0].event == InputEvent::Pressed);
    assert(runtime.state(PhysicalControlId::TimerButton).stablePrimary);

    events.clear();
    runtime.update(0xffff, 0, 40, events);
    runtime.update(0xffff, 0, 60, events);
    assert(events.size() == 2);
    assert(events[0].input == PhysicalControlId::TimerButton);
    assert(events[0].event == InputEvent::Released);
    assert(events[1].input == PhysicalControlId::KeyPlus);
    assert(events[1].event == InputEvent::Pressed);

    events.clear();
    runtime.update(0xffff, 0, 80, events, false);
    assert(runtime.state(PhysicalControlId::TimerButton).faulted);
    assert(!runtime.state(PhysicalControlId::KeyPlus).faulted);
    runtime.update(0xffff, 0, 100, events, true);
    assert(!runtime.state(PhysicalControlId::TimerButton).faulted);
}

void testSpringTrimFaultAndRecovery()
{
    const PhysicalInputHardwareBinding map[] = {
        {PhysicalControlId::GripTrimA, mcp(2), mcp(3)},
    };
    PhysicalInputRuntime runtime;
    PhysicalInputEventBuffer events;
    assert(runtime.begin(map, 1));
    runtime.update(0xffff, ~uint64_t{0}, 0, events);

    runtime.update(0xfff3, ~uint64_t{0}, 10, events); // PA2 + PA3 active
    runtime.update(0xfff3, ~uint64_t{0}, 30, events);
    assert(runtime.state(PhysicalControlId::GripTrimA).faulted);
    assert(events.size() == 1);
    assert(events[0].event == InputEvent::InvalidState);

    events.clear();
    runtime.update(0xfff7, ~uint64_t{0}, 40, events); // positive only
    runtime.update(0xfff7, ~uint64_t{0}, 60, events);
    assert(!runtime.state(PhysicalControlId::GripTrimA).faulted);
    assert(events.size() == 2);
    assert(events[0].event == InputEvent::StateRestored);
    assert(events[1].event == InputEvent::PositivePress);
}

void testProportionalStateAndEvent()
{
    const PhysicalInputHardwareBinding map[] = {
        {PhysicalControlId::ThrottleTrigger,
         {InputSourceKind::Adc, 0, 1, false}, {}},
    };
    PhysicalInputRuntime runtime;
    PhysicalInputEventBuffer events;
    assert(runtime.begin(map, 1));
    runtime.setProportionalValue(PhysicalControlId::ThrottleTrigger, 123, 50,
                                 &events);
    assert(events.size() == 1);
    assert(events[0].event == InputEvent::ValueChanged);
    assert(events[0].value == 123);
    assert(runtime.state(PhysicalControlId::ThrottleTrigger).value == 123);
}

void testBenchStartupBypassRetainsFaults()
{
    const PhysicalInputHardwareBinding map[] = {
        {PhysicalControlId::DriveModeSwitch, mcp(0), {}},
        {PhysicalControlId::SteeringTrim, mcp(1), mcp(2)},
    };
    PhysicalInputRuntime runtime;
    PhysicalInputEventBuffer events;
    assert(runtime.begin(map, 2));
    runtime.update(0xffff, 0, 0, events);
    runtime.update(0xfffc, 0, 10, events);
    runtime.update(0xfffc, 0, 40, events); // switch held, trim minus held
    assert(runtime.startupUnsafeMask() != 0);
    assert(runtime.startupUnsafeMask(true) == 0);
    runtime.update(0xffff, 0, 60, events, false); // missing expander is still blocked
    assert(runtime.startupUnsafeMask(true) != 0);
    runtime.update(0xfff8, 0, 100, events);
    runtime.update(0xfff8, 0, 140, events); // both trim contacts is an invalid state
    assert(runtime.state(PhysicalControlId::SteeringTrim).faulted);
    assert(runtime.startupUnsafeMask(true) & (1UL << static_cast<unsigned>(PhysicalControlId::SteeringTrim)));
}

void testLandscapeLeftTouchMapping()
{
    auto p=waveshareTouchPoint(0,0,false); assert(p.x==0 && p.y==319);
    p=waveshareTouchPoint(319,0,false); assert(p.x==0 && p.y==0);
    p=waveshareTouchPoint(0,479,false); assert(p.x==479 && p.y==319);
    p=waveshareTouchPoint(319,479,false); assert(p.x==479 && p.y==0);
    p=waveshareTouchPoint(100,200,false); assert(p.x==200 && p.y==219);
    p=waveshareTouchPoint(65535,65535,false); assert(p.x==479 && p.y==0);
    p=waveshareTouchPoint(0,0,true); assert(p.x==319 && p.y==479);
    p=waveshareTouchPoint(319,479,true); assert(p.x==0 && p.y==0);
}

} // namespace

int main()
{
    testMcpButtonAndDirectGpio();
    testSpringTrimFaultAndRecovery();
    testProportionalStateAndEvent();
    testBenchStartupBypassRetainsFaults();
    testLandscapeLeftTouchMapping();
    puts("physical_input_runtime_test: PASS");
    return 0;
}

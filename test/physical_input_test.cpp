#include <assert.h>
#include <stdio.h>

#include "physical_input.h"

namespace {

void initialiseReleased(DigitalInputProcessor &input, PhysicalInputEventBuffer &events)
{
    input.update(false, 0, events);
    assert(events.size() == 0);
}

void testDebounceAndShortPress()
{
    DigitalInputTiming timing;
    timing.debounceMs = 20;
    timing.doublePressMs = 100;
    DigitalInputProcessor input(PhysicalControlId::TimerButton, timing);
    PhysicalInputEventBuffer events;
    initialiseReleased(input, events);

    input.update(true, 5, events);
    input.update(false, 10, events); // contact bounce
    input.update(true, 15, events);
    input.update(true, 34, events);
    assert(events.size() == 0);
    input.update(true, 35, events);
    assert(events.size() == 1 && events[0].event == InputEvent::Pressed);

    input.update(false, 50, events);
    input.update(false, 70, events);
    assert(events.size() == 2 && events[1].event == InputEvent::Released);
    input.update(false, 171, events);
    assert(events.size() == 3 && events[2].event == InputEvent::ShortPress);
}

void testDoubleAndLongPress()
{
    DigitalInputTiming timing;
    timing.debounceMs = 10;
    timing.doublePressMs = 120;
    timing.longPressMs = 200;
    DigitalInputProcessor input(PhysicalControlId::TimerButton, timing);
    PhysicalInputEventBuffer events;
    initialiseReleased(input, events);

    input.update(true, 10, events); input.update(true, 20, events);
    input.update(false, 40, events); input.update(false, 50, events);
    input.update(true, 80, events); input.update(true, 90, events);
    input.update(false, 110, events); input.update(false, 120, events);
    assert(events[events.size() - 1].event == InputEvent::DoublePress);

    events.clear();
    input.update(true, 300, events); input.update(true, 310, events);
    input.update(true, 510, events);
    assert(events.size() == 2);
    assert(events[0].event == InputEvent::Pressed);
    assert(events[1].event == InputEvent::LongPress);
    input.update(false, 520, events); input.update(false, 530, events);
    input.update(false, 700, events);
    assert(events.size() == 3); // no ShortPress after a long press
}

void testButtonRepeat()
{
    DigitalInputTiming timing;
    timing.debounceMs = 10;
    timing.repeatDelayMs = 100;
    timing.repeatIntervalMs = 50;
    DigitalInputProcessor input(PhysicalControlId::KeyPage, timing, true);
    PhysicalInputEventBuffer events;
    initialiseReleased(input, events);

    input.update(true, 10, events); input.update(true, 20, events);
    input.update(true, 120, events);
    assert(events.size() == 2);
    assert(events[0].event == InputEvent::Pressed);
    assert(events[1].event == InputEvent::Repeat);
}

void testTrimEventsRepeatAndFault()
{
    DigitalInputTiming timing;
    timing.debounceMs = 10;
    timing.repeatDelayMs = 100;
    timing.repeatIntervalMs = 50;
    SpringTrimInputProcessor trim(PhysicalControlId::GripTrimA, timing);
    PhysicalInputEventBuffer events;
    trim.update(false, false, 0, events);

    trim.update(true, false, 10, events);
    trim.update(true, false, 20, events);
    assert(trim.position() == TrimPosition::Negative);
    assert(events.size() == 1 && events[0].event == InputEvent::NegativePress);
    trim.update(true, false, 120, events);
    assert(events.size() == 2 && events[1].event == InputEvent::NegativeRepeat);

    trim.update(true, true, 130, events);
    trim.update(true, true, 140, events);
    assert(trim.faulted());
    assert(events[events.size() - 1].event == InputEvent::InvalidState);

    trim.update(false, true, 150, events);
    trim.update(false, true, 160, events);
    assert(trim.position() == TrimPosition::Positive);
    assert(events[events.size() - 2].event == InputEvent::StateRestored);
    assert(events[events.size() - 1].event == InputEvent::PositivePress);
}

void testStateStore()
{
    PhysicalInputStateStore store;
    store.setMapped(PhysicalControlId::TimerButton, true);
    PhysicalInputEvent event;
    event.input = PhysicalControlId::TimerButton;
    event.event = InputEvent::ShortPress;
    event.value = 1;
    event.timestampMs = 1234;
    store.apply(event);
    const PhysicalInputState &state = store.state(PhysicalControlId::TimerButton);
    assert(state.mapped && state.value == 1 && state.lastEventMs == 1234);
}

} // namespace

int main()
{
    testDebounceAndShortPress();
    testDoubleAndLongPress();
    testButtonRepeat();
    testTrimEventsRepeatAndFault();
    testStateStore();
    puts("physical_input_test: PASS");
    return 0;
}

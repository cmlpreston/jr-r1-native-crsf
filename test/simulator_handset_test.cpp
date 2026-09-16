#include "../simulator/include/handset_inputs.h"
#include "binding_resolver.h"
#include <cassert>
#include <cstdio>

int main() {
    SimulatorHandsetInputs handset;
    PhysicalInputEventBuffer events;
    auto update = [&](uint64_t contacts, uint32_t now) {
        events.clear(); handset.update(contacts, 0, 0, now, events);
    };
    auto contains = [&](PhysicalControlId input, InputEvent event) {
        for (size_t i = 0; i < events.size(); ++i)
            if (events[i].input == input && events[i].event == event) return true;
        return false;
    };
    // Every displayed control reaches the monitor and uses the expected contact polarity.
    uint32_t now = 100;
    for (uint8_t id = 2; id <= 10; ++id) {
        const auto input = static_cast<PhysicalControlId>(id);
        const bool trim = physicalControlDescriptor(id).kind == PhysicalControlKind::SpringCentredTrim;
        for (int positive = 0; positive <= (trim ? 1 : 0); ++positive) {
            const uint64_t contact = uint64_t{1} << (id * 2 + positive);
            update(contact, now); update(contact, now + 25);
            const auto &state = handset.states().state(input);
            assert(state.mapped && (positive ? state.stableSecondary : state.stablePrimary));
            assert(contains(input, trim ? (positive ? InputEvent::PositivePress : InputEvent::NegativePress) : InputEvent::Pressed));
            update(0, now + 100); update(0, now + 125);
            assert(contains(input, trim ? (positive ? InputEvent::PositiveRelease : InputEvent::NegativeRelease) : InputEvent::Released));
            now += 1000;
        }
    }
    const uint64_t timer = uint64_t{1} << 4;
    ControlBinding binding;
    binding.input = PhysicalControlId::TimerButton;
    binding.target = AssignmentTarget::TransmitterFunction;
    binding.targetId = static_cast<uint16_t>(TransmitterFunction::ResetSpeedStatistics);
    binding.event = InputEvent::ShortPress;
    BindingResolver resolver;
    assert(resolver.setBindings(&binding, 1).ok());
    update(timer, now); update(timer, now + 25);
    update(0, now + 100); update(0, now + 125); update(0, now + 410);
    ResolvedBindingActionBuffer actions;
    resolver.processEvents(events, ContextAlways, actions);
    assert(actions.size() == 1 && actions[0].targetId == binding.targetId && actions[0].value > 0);
    // A long press must not also generate a delayed short press.
    now += 1000;
    update(timer, now); update(timer, now + 25); update(timer, now + 700);
    assert(contains(PhysicalControlId::TimerButton, InputEvent::LongPress));
    update(0, now + 800); update(0, now + 825); update(0, now + 1200);
    assert(!contains(PhysicalControlId::TimerButton, InputEvent::ShortPress));
    // Held trim repeats stop when released; this also covers browser blur cancellation.
    now += 2000;
    const uint64_t trim = uint64_t{1} << 15;
    update(trim, now); update(trim, now + 25); update(trim, now + 500);
    assert(contains(PhysicalControlId::SteeringTrim, InputEvent::PositiveRepeat));
    update(0, now + 550); update(0, now + 575); update(0, now + 900);
    assert(!contains(PhysicalControlId::SteeringTrim, InputEvent::PositiveRepeat));
    puts("Simulator handset: all contacts, monitor, reset assignment, long press and repeat release PASS");
}

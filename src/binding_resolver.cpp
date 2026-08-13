#include "binding_resolver.h"

namespace {

bool timeReached(uint32_t now, uint32_t deadline)
{
    return static_cast<int32_t>(now - deadline) >= 0;
}

bool isNegative(InputEvent event)
{
    return event == InputEvent::NegativePress ||
           event == InputEvent::NegativeRepeat ||
           event == InputEvent::PositionLow;
}

bool isPositive(InputEvent event)
{
    return event == InputEvent::PositivePress ||
           event == InputEvent::PositiveRepeat ||
           event == InputEvent::PositionHigh;
}

bool isCentred(InputEvent event)
{
    return event == InputEvent::NegativeRelease ||
           event == InputEvent::PositiveRelease ||
           event == InputEvent::StateRestored;
}

} // namespace

bool ResolvedBindingActionBuffer::push(const ResolvedBindingAction &action)
{
    if (count_ >= Capacity) {
        overflowed_ = true;
        return false;
    }
    actions_[count_++] = action;
    return true;
}

void BindingResolver::clear()
{
    count_ = 0;
}

bool BindingResolver::contextMatches(uint8_t bindingContexts,
                                     uint8_t activeContexts)
{
    return (bindingContexts & ContextAlways) != 0 ||
           (bindingContexts & activeContexts) != 0;
}

BindingLoadResult BindingResolver::setBindings(const ControlBinding *bindings,
                                               size_t count)
{
    BindingLoadResult result;
    if (count > MaxBindings) {
        result.error = BindingLoadError::TooManyBindings;
        result.bindingIndex = count;
        result.reason = "binding capacity exceeded";
        return result;
    }
    if (count != 0 && bindings == nullptr) {
        result.error = BindingLoadError::InvalidBinding;
        result.reason = "null binding array";
        return result;
    }

    for (size_t index = 0; index < count; ++index) {
        const char *reason = nullptr;
        if (!validateControlBinding(bindings[index], &reason)) {
            result.error = BindingLoadError::InvalidBinding;
            result.bindingIndex = index;
            result.reason = reason;
            return result;
        }
        if (!bindings[index].enabled)
            continue;
        for (size_t previous = 0; previous < index; ++previous) {
            if (!bindings[previous].enabled)
                continue;
            if (bindings[index].target == AssignmentTarget::CrsfChannel &&
                bindings[previous].target == AssignmentTarget::CrsfChannel &&
                bindings[previous].crsfChannel == bindings[index].crsfChannel) {
                result.error = BindingLoadError::ConflictingChannelWriter;
                result.bindingIndex = index;
                result.conflictingIndex = previous;
                result.reason = "multiple bindings write the same CRSF channel";
                return result;
            }
            if (bindings[index].target == AssignmentTarget::ModelParameter &&
                isChannelParameterTarget(bindings[index].targetId) &&
                bindings[previous].target == AssignmentTarget::ModelParameter &&
                bindings[previous].targetId == bindings[index].targetId) {
                result.error = BindingLoadError::ConflictingParameterWriter;
                result.bindingIndex = index;
                result.conflictingIndex = previous;
                result.reason = "multiple controls adjust the same channel parameter";
                return result;
            }
        }
    }

    count_ = count;
    for (size_t index = 0; index < count_; ++index) {
        bindings_[index] = bindings[index];
        states_[index].value = bindings[index].initial;
        states_[index].pulseEndsAtMs = 0;
        states_[index].active = false;
    }
    return result;
}

bool BindingResolver::isReleaseFor(InputEvent trigger, InputEvent event)
{
    if ((trigger == InputEvent::Pressed || trigger == InputEvent::ShortPress ||
         trigger == InputEvent::LongPress || trigger == InputEvent::DoublePress) &&
        event == InputEvent::Released)
        return true;
    if ((trigger == InputEvent::NegativePress ||
         trigger == InputEvent::NegativeRepeat) &&
        event == InputEvent::NegativeRelease)
        return true;
    if ((trigger == InputEvent::PositivePress ||
         trigger == InputEvent::PositiveRepeat) &&
        event == InputEvent::PositiveRelease)
        return true;
    return false;
}

int16_t BindingResolver::clampToBinding(const ControlBinding &binding,
                                        int32_t value)
{
    if (value < binding.minimum) value = binding.minimum;
    if (value > binding.maximum) value = binding.maximum;
    return static_cast<int16_t>(value);
}

int16_t BindingResolver::proportionalValue(const ControlBinding &binding,
                                           int16_t value)
{
    if (value < -1000) value = -1000;
    if (value > 1000) value = 1000;
    if (binding.inverted) value = static_cast<int16_t>(-value);
    const int32_t span = static_cast<int32_t>(binding.maximum) - binding.minimum;
    return clampToBinding(binding,
        binding.minimum + (static_cast<int32_t>(value) + 1000) * span / 2000);
}

bool BindingResolver::resolveEvent(size_t index,
                                   const PhysicalInputEvent &event,
                                   int16_t &newValue, bool &handled)
{
    const ControlBinding &binding = bindings_[index];
    RuntimeState &state = states_[index];
    handled = false;
    newValue = state.value;
    const bool trigger = event.event == binding.event;

    switch (binding.behaviour) {
        case ControlBehaviour::Proportional:
            if (trigger) {
                newValue = proportionalValue(binding, event.value);
                handled = true;
            }
            break;

        case ControlBehaviour::Momentary:
            if (trigger) {
                newValue = binding.inverted ? binding.minimum : binding.maximum;
                state.active = true;
                handled = true;
            } else if (state.active && isReleaseFor(binding.event, event.event)) {
                newValue = binding.initial;
                state.active = false;
                handled = true;
            }
            break;

        case ControlBehaviour::Toggle:
            if (trigger) {
                const int16_t low = binding.inverted ? binding.maximum : binding.minimum;
                const int16_t high = binding.inverted ? binding.minimum : binding.maximum;
                newValue = state.value == high ? low : high;
                handled = true;
            }
            break;

        case ControlBehaviour::TwoPosition:
            if (event.event == InputEvent::PositionLow ||
                event.event == InputEvent::Released) {
                newValue = binding.inverted ? binding.maximum : binding.minimum;
                handled = true;
            } else if (event.event == InputEvent::PositionHigh || trigger) {
                newValue = binding.inverted ? binding.minimum : binding.maximum;
                handled = true;
            }
            break;

        case ControlBehaviour::SpringReturnThreePosition:
            if (isNegative(event.event)) {
                newValue = binding.inverted ? binding.maximum : binding.minimum;
                state.active = true;
                handled = true;
            } else if (isPositive(event.event)) {
                newValue = binding.inverted ? binding.minimum : binding.maximum;
                state.active = true;
                handled = true;
            } else if (isCentred(event.event)) {
                newValue = binding.initial;
                state.active = false;
                handled = true;
            }
            break;

        case ControlBehaviour::Stepped:
            if (isNegative(event.event) || isPositive(event.event) || trigger) {
                int32_t direction = isNegative(event.event) ? -1 : 1;
                if (trigger && event.value < 0) direction = -1;
                if (binding.inverted) direction = -direction;
                newValue = clampToBinding(binding,
                    static_cast<int32_t>(state.value) + direction * binding.step);
                handled = true;
            }
            break;

        case ControlBehaviour::ThreePositionSelector:
            if (isNegative(event.event)) {
                newValue = binding.inverted ? binding.maximum : binding.minimum;
                handled = true;
            } else if (isPositive(event.event)) {
                newValue = binding.inverted ? binding.minimum : binding.maximum;
                handled = true;
            } else if (isCentred(event.event)) {
                newValue = binding.initial;
                handled = true;
            }
            break;

        case ControlBehaviour::Pulse:
            if (trigger) {
                newValue = binding.inverted ? binding.minimum : binding.maximum;
                state.active = true;
                state.pulseEndsAtMs = event.timestampMs + binding.pulseMs;
                handled = true;
            }
            break;
    }
    return handled && newValue != state.value;
}

void BindingResolver::emitAction(size_t index,
                                 const PhysicalInputEvent &event,
                                 ResolvedBindingActionBuffer &actions) const
{
    const ControlBinding &binding = bindings_[index];
    if (binding.target == AssignmentTarget::None ||
        binding.target == AssignmentTarget::CrsfChannel)
        return;
    ResolvedBindingAction action;
    action.target = binding.target;
    action.targetId = binding.targetId;
    action.value = states_[index].value;
    action.input = event.input;
    action.event = event.event;
    action.timestampMs = event.timestampMs;
    actions.push(action);
}

void BindingResolver::processEvent(const PhysicalInputEvent &event,
                                   uint8_t activeContexts,
                                   ResolvedBindingActionBuffer &actions)
{
    for (size_t index = 0; index < count_; ++index) {
        const ControlBinding &binding = bindings_[index];
        if (!binding.enabled || binding.input != event.input) continue;
        // Active momentary controls must see their release even if opening a
        // menu changed context while the physical control was held.
        if (!contextMatches(binding.contexts, activeContexts) &&
            !states_[index].active)
            continue;

        int16_t newValue = states_[index].value;
        bool handled = false;
        const bool changed = resolveEvent(index, event, newValue, handled);
        if (!handled) continue;
        states_[index].value = newValue;
        if (changed || binding.target != AssignmentTarget::CrsfChannel)
            emitAction(index, event, actions);
    }
}

void BindingResolver::processEvents(const PhysicalInputEventBuffer &events,
                                    uint8_t activeContexts,
                                    ResolvedBindingActionBuffer &actions)
{
    for (size_t index = 0; index < events.size(); ++index)
        processEvent(events[index], activeContexts, actions);
}

void BindingResolver::update(uint32_t nowMs,
                             ResolvedBindingActionBuffer &actions)
{
    for (size_t index = 0; index < count_; ++index) {
        const ControlBinding &binding = bindings_[index];
        RuntimeState &state = states_[index];
        if (!binding.enabled || binding.behaviour != ControlBehaviour::Pulse ||
            !state.active || !timeReached(nowMs, state.pulseEndsAtMs))
            continue;
        state.active = false;
        state.value = binding.initial;
        PhysicalInputEvent expiry;
        expiry.input = binding.input;
        expiry.event = InputEvent::Released;
        expiry.value = state.value;
        expiry.timestampMs = nowMs;
        emitAction(index, expiry, actions);
    }
}

void BindingResolver::applyChannelOutputs(CrsfChannelOutputModel &model,
                                          uint16_t reservedChannels) const
{
    for (size_t index = 0; index < count_; ++index) {
        const ControlBinding &binding = bindings_[index];
        if (!binding.enabled || binding.target != AssignmentTarget::CrsfChannel)
            continue;
        const uint16_t bit = static_cast<uint16_t>(1U << (binding.crsfChannel - 1));
        if ((reservedChannels & bit) != 0) continue;
        model.configure(binding.crsfChannel,
                        crsfValueFromPermille(states_[index].value),
                        crsfValueFromPermille(binding.failsafe), true);
    }
}

int16_t BindingResolver::value(size_t bindingIndex) const
{
    return bindingIndex < count_ ? states_[bindingIndex].value : 0;
}

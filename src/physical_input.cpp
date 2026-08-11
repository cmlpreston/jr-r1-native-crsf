#include "physical_input.h"

namespace {

bool elapsed(uint32_t now, uint32_t then, uint32_t duration)
{
    return static_cast<uint32_t>(now - then) >= duration;
}

size_t stateIndex(PhysicalControlId input)
{
    const size_t index = static_cast<size_t>(input);
    return index < kPhysicalControlCount ? index : 0;
}

} // namespace

bool PhysicalInputEventBuffer::push(PhysicalControlId input, InputEvent event,
                                    uint32_t timestampMs, int16_t value)
{
    if (count_ >= Capacity) {
        overflowed_ = true;
        return false;
    }
    PhysicalInputEvent &item = events_[count_++];
    item.input = input;
    item.event = event;
    item.value = value;
    item.timestampMs = timestampMs;
    return true;
}

bool DebouncedContact::update(bool rawActive, uint32_t nowMs)
{
    if (!initialised_) {
        initialised_ = true;
        rawActive_ = rawActive;
        stableActive_ = rawActive;
        rawChangedAtMs_ = nowMs;
        return false;
    }
    if (rawActive != rawActive_) {
        rawActive_ = rawActive;
        rawChangedAtMs_ = nowMs;
    }
    if (rawActive_ != stableActive_ && elapsed(nowMs, rawChangedAtMs_, debounceMs_)) {
        stableActive_ = rawActive_;
        return true;
    }
    return false;
}

DigitalInputProcessor::DigitalInputProcessor(PhysicalControlId input,
                                             const DigitalInputTiming &timing,
                                             bool repeatEnabled)
    : input_(input), timing_(timing), contact_(timing.debounceMs),
      repeatEnabled_(repeatEnabled)
{
}

void DigitalInputProcessor::update(bool rawActive, uint32_t nowMs,
                                   PhysicalInputEventBuffer &events)
{
    const bool wasInitialised = contact_.initialised();
    const bool changed = contact_.update(rawActive, nowMs);
    if (!wasInitialised) {
        if (contact_.active()) {
            pressedAtMs_ = nowMs;
            nextRepeatAtMs_ = nowMs + timing_.repeatDelayMs;
        }
        return;
    }

    if (pendingShortPress_ &&
        elapsed(nowMs, pendingShortAtMs_, timing_.doublePressMs)) {
        events.push(input_, InputEvent::ShortPress, pendingShortAtMs_);
        pendingShortPress_ = false;
    }

    if (changed) {
        if (contact_.active()) {
            pressedAtMs_ = nowMs;
            nextRepeatAtMs_ = nowMs + timing_.repeatDelayMs;
            longPressSent_ = false;
            events.push(input_, InputEvent::Pressed, nowMs);
        } else {
            events.push(input_, InputEvent::Released, nowMs);
            if (!longPressSent_) {
                if (pendingShortPress_ &&
                    !elapsed(nowMs, pendingShortAtMs_, timing_.doublePressMs)) {
                    events.push(input_, InputEvent::DoublePress, nowMs);
                    pendingShortPress_ = false;
                } else {
                    pendingShortPress_ = true;
                    pendingShortAtMs_ = nowMs;
                }
            }
        }
    }

    if (!contact_.active()) return;
    if (!longPressSent_ && elapsed(nowMs, pressedAtMs_, timing_.longPressMs)) {
        events.push(input_, InputEvent::LongPress, nowMs);
        longPressSent_ = true;
    }
    if (repeatEnabled_ && timing_.repeatIntervalMs != 0 &&
        static_cast<int32_t>(nowMs - nextRepeatAtMs_) >= 0) {
        // Emit no more than one repeat per update so a delayed loop cannot
        // flood the binding engine with stale adjustment events.
        events.push(input_, InputEvent::Repeat, nowMs, 1);
        nextRepeatAtMs_ = nowMs + timing_.repeatIntervalMs;
    }
}

SpringTrimInputProcessor::SpringTrimInputProcessor(
    PhysicalControlId input, const DigitalInputTiming &timing)
    : input_(input), timing_(timing), negative_(timing.debounceMs),
      positive_(timing.debounceMs)
{
}

void SpringTrimInputProcessor::leavePosition(TrimPosition oldPosition,
                                             uint32_t nowMs,
                                             PhysicalInputEventBuffer &events)
{
    if (oldPosition == TrimPosition::Negative)
        events.push(input_, InputEvent::NegativeRelease, nowMs, -1);
    else if (oldPosition == TrimPosition::Positive)
        events.push(input_, InputEvent::PositiveRelease, nowMs, 1);
}

void SpringTrimInputProcessor::enterPosition(TrimPosition newPosition,
                                             uint32_t nowMs,
                                             PhysicalInputEventBuffer &events)
{
    if (newPosition == TrimPosition::Negative) {
        events.push(input_, InputEvent::NegativePress, nowMs, -1);
        nextRepeatAtMs_ = nowMs + timing_.repeatDelayMs;
    } else if (newPosition == TrimPosition::Positive) {
        events.push(input_, InputEvent::PositivePress, nowMs, 1);
        nextRepeatAtMs_ = nowMs + timing_.repeatDelayMs;
    }
}

void SpringTrimInputProcessor::update(bool rawNegativeActive,
                                      bool rawPositiveActive,
                                      uint32_t nowMs,
                                      PhysicalInputEventBuffer &events)
{
    const bool negativeChanged = negative_.update(rawNegativeActive, nowMs);
    const bool positiveChanged = positive_.update(rawPositiveActive, nowMs);
    TrimPosition newPosition = TrimPosition::Neutral;
    if (negative_.active() && positive_.active()) newPosition = TrimPosition::Invalid;
    else if (negative_.active()) newPosition = TrimPosition::Negative;
    else if (positive_.active()) newPosition = TrimPosition::Positive;

    if (!initialised_) {
        initialised_ = true;
        position_ = newPosition;
        if (position_ == TrimPosition::Negative || position_ == TrimPosition::Positive)
            nextRepeatAtMs_ = nowMs + timing_.repeatDelayMs;
        return;
    }

    if ((negativeChanged || positiveChanged) && newPosition != position_) {
        const TrimPosition oldPosition = position_;
        leavePosition(oldPosition, nowMs, events);
        if (newPosition == TrimPosition::Invalid) {
            events.push(input_, InputEvent::InvalidState, nowMs);
        } else {
            if (oldPosition == TrimPosition::Invalid)
                events.push(input_, InputEvent::StateRestored, nowMs);
            enterPosition(newPosition, nowMs, events);
        }
        position_ = newPosition;
    }

    if (timing_.repeatIntervalMs == 0) return;
    if ((position_ == TrimPosition::Negative || position_ == TrimPosition::Positive) &&
        static_cast<int32_t>(nowMs - nextRepeatAtMs_) >= 0) {
        const bool negative = position_ == TrimPosition::Negative;
        events.push(input_, negative ? InputEvent::NegativeRepeat : InputEvent::PositiveRepeat,
                    nowMs, negative ? -1 : 1);
        nextRepeatAtMs_ = nowMs + timing_.repeatIntervalMs;
    }
}

void PhysicalInputStateStore::setMapped(PhysicalControlId input, bool mapped)
{
    states_[stateIndex(input)].mapped = mapped;
}

void PhysicalInputStateStore::setContacts(PhysicalControlId input,
                                          bool rawPrimary, bool rawSecondary,
                                          bool stablePrimary, bool stableSecondary,
                                          bool faulted)
{
    PhysicalInputState &inputState = states_[stateIndex(input)];
    inputState.rawPrimary = rawPrimary;
    inputState.rawSecondary = rawSecondary;
    inputState.stablePrimary = stablePrimary;
    inputState.stableSecondary = stableSecondary;
    inputState.faulted = faulted;
}

void PhysicalInputStateStore::setValue(PhysicalControlId input, int16_t value)
{
    states_[stateIndex(input)].value = value;
}

void PhysicalInputStateStore::apply(const PhysicalInputEvent &event)
{
    PhysicalInputState &inputState = states_[stateIndex(event.input)];
    inputState.lastEvent = event.event;
    inputState.lastEventMs = event.timestampMs;
    inputState.value = event.value;
    if (event.event == InputEvent::InvalidState) inputState.faulted = true;
    else if (event.event == InputEvent::StateRestored) inputState.faulted = false;
}

const PhysicalInputState &PhysicalInputStateStore::state(PhysicalControlId input) const
{
    return states_[stateIndex(input)];
}

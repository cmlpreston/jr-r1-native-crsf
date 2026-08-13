#pragma once

#include <stddef.h>
#include <stdint.h>

#include "control_assignment.h"
#include "crsf_channel_output.h"
#include "physical_input.h"

// The resolver deliberately has no Arduino/LVGL dependency. Physical input
// drivers feed it events; local-function consumers drain actions; and the CRSF
// control task asks it to overlay accessory channels onto its output model.
struct ResolvedBindingAction {
    AssignmentTarget target = AssignmentTarget::None;
    uint16_t targetId = 0;
    int16_t value = 0;
    PhysicalControlId input = PhysicalControlId::TimerButton;
    InputEvent event = InputEvent::Pressed;
    uint32_t timestampMs = 0;
};

class ResolvedBindingActionBuffer {
public:
    static constexpr size_t Capacity = 16;

    bool push(const ResolvedBindingAction &action);
    void clear() { count_ = 0; overflowed_ = false; }
    size_t size() const { return count_; }
    bool overflowed() const { return overflowed_; }
    const ResolvedBindingAction &operator[](size_t index) const {
        return actions_[index];
    }

private:
    ResolvedBindingAction actions_[Capacity];
    size_t count_ = 0;
    bool overflowed_ = false;
};

enum class BindingLoadError : uint8_t {
    None,
    TooManyBindings,
    InvalidBinding,
    ConflictingChannelWriter,
    ConflictingParameterWriter,
};

struct BindingLoadResult {
    BindingLoadError error = BindingLoadError::None;
    size_t bindingIndex = 0;
    size_t conflictingIndex = 0;
    const char *reason = nullptr;

    bool ok() const { return error == BindingLoadError::None; }
};

class BindingResolver {
public:
    static constexpr size_t MaxBindings = 32;

    BindingLoadResult setBindings(const ControlBinding *bindings, size_t count);
    void clear();

    // activeContexts is a bitwise combination of ControlContext values.
    // ContextAlways bindings are accepted in every context.
    void processEvent(const PhysicalInputEvent &event, uint8_t activeContexts,
                      ResolvedBindingActionBuffer &actions);
    void processEvents(const PhysicalInputEventBuffer &events,
                       uint8_t activeContexts,
                       ResolvedBindingActionBuffer &actions);

    // Expires timed pulses. Safe across the uint32_t millis() wraparound.
    void update(uint32_t nowMs, ResolvedBindingActionBuffer &actions);

    // Adds all configured CRSF-channel bindings to an existing model. Bits in
    // reservedChannels use channel-1 bit numbering and are never overwritten;
    // the direct throttle/steering path can therefore retain final priority.
    void applyChannelOutputs(CrsfChannelOutputModel &model,
                             uint16_t reservedChannels = 0) const;

    size_t bindingCount() const { return count_; }
    int16_t value(size_t bindingIndex) const;

private:
    struct RuntimeState {
        int16_t value = 0;
        uint32_t pulseEndsAtMs = 0;
        bool active = false;
    };

    static bool contextMatches(uint8_t bindingContexts, uint8_t activeContexts);
    static bool isReleaseFor(InputEvent trigger, InputEvent event);
    static int16_t clampToBinding(const ControlBinding &binding, int32_t value);
    static int16_t proportionalValue(const ControlBinding &binding, int16_t value);
    bool resolveEvent(size_t index, const PhysicalInputEvent &event,
                      int16_t &newValue, bool &handled);
    void emitAction(size_t index, const PhysicalInputEvent &event,
                    ResolvedBindingActionBuffer &actions) const;

    ControlBinding bindings_[MaxBindings];
    RuntimeState states_[MaxBindings];
    size_t count_ = 0;
};

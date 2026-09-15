#include "physical_input_runtime.h"

namespace {

bool isDigital(InputSourceKind source)
{
    return source == InputSourceKind::DirectGpio ||
           source == InputSourceKind::Mcp23017;
}

} // namespace

bool PhysicalInputRuntime::begin(const PhysicalInputHardwareBinding *bindings,
                                 size_t count)
{
    if (bindings == nullptr || count > kPhysicalControlCount) return false;
    count_ = count;
    for (size_t index = 0; index < count_; ++index) {
        bindings_[index] = bindings[index];
        const PhysicalControlId input = bindings[index].input;
        const PhysicalControlKind kind =
            physicalControlDescriptor(static_cast<size_t>(input)).kind;
        digital_[index] = DigitalInputProcessor(
            input, DigitalInputTiming(),
            kind == PhysicalControlKind::KeypadButton);
        trim_[index] = SpringTrimInputProcessor(input);
        states_.setMapped(input,
            bindings[index].primary.source != InputSourceKind::Unmapped);
        if (kind == PhysicalControlKind::SpringCentredTrim &&
            !isDigital(bindings[index].secondary.source))
            return false;
    }
    return true;
}

void PhysicalInputRuntime::update(uint16_t mcpLevels,
                                  uint64_t directGpioLevels, uint32_t nowMs,
                                  PhysicalInputEventBuffer &events,
                                  bool mcpAvailable)
{
    // Compatibility entry point for the original 0x27 prototype/simulator.
    const McpInputSnapshot snapshot{0x27, mcpLevels, mcpAvailable};
    update(&snapshot, 1, directGpioLevels, nowMs, events);
}

void PhysicalInputRuntime::update(const McpInputSnapshot *expanders,
                                  size_t expanderCount,
                                  uint64_t directGpioLevels, uint32_t nowMs,
                                  PhysicalInputEventBuffer &events)
{
    const auto available = [&](const InputEndpoint &endpoint) {
        if (endpoint.source != InputSourceKind::Mcp23017) return true;
        for (size_t i = 0; i < expanderCount; ++i)
            if (expanders[i].address == endpoint.device)
                return expanders[i].available && endpoint.sourceIndex < 16;
        return false;
    };
    const auto active = [&](const InputEndpoint &endpoint) {
        // An absent chip must never appear as pressed, even if its cached
        // snapshot was held low when communication stopped.
        if (!available(endpoint)) return false;
        bool level = false;
        if (endpoint.source == InputSourceKind::Mcp23017) {
            for (size_t i = 0; i < expanderCount; ++i)
                if (expanders[i].address == endpoint.device)
                    level = (expanders[i].levels & (1U << endpoint.sourceIndex)) != 0;
        } else if (endpoint.source == InputSourceKind::DirectGpio) {
            if (endpoint.sourceIndex >= 64) return false;
            level = (directGpioLevels & (uint64_t{1} << endpoint.sourceIndex)) != 0;
        }
        return endpoint.activeLow ? !level : level;
    };
    for (size_t index = 0; index < count_; ++index) {
        const PhysicalInputHardwareBinding &binding = bindings_[index];
        const PhysicalControlKind kind = physicalControlDescriptor(
            static_cast<size_t>(binding.input)).kind;
        if (!isDigital(binding.primary.source)) continue;
        const bool rawPrimary = active(binding.primary);
        if (kind == PhysicalControlKind::SpringCentredTrim) {
            const bool rawSecondary = active(binding.secondary);
            trim_[index].update(rawPrimary, rawSecondary, nowMs, events);
            states_.setContacts(binding.input, rawPrimary, rawSecondary,
                                trim_[index].position() == TrimPosition::Negative,
                                trim_[index].position() == TrimPosition::Positive,
                                trim_[index].faulted());
        } else {
            digital_[index].update(rawPrimary, nowMs, events);
            states_.setContacts(binding.input, rawPrimary, false,
                                digital_[index].active(), false, false);
        }
    }
    for (size_t index = 0; index < events.size(); ++index)
        states_.apply(events[index]);
    // Continue feeding a fail-safe released level through the debouncers when
    // the expander disappears, but keep the monitor state visibly faulted.
    for (size_t index = 0; index < count_; ++index) {
        const auto &binding = bindings_[index];
        if (binding.primary.source == InputSourceKind::Mcp23017 ||
            binding.secondary.source == InputSourceKind::Mcp23017)
            states_.setFaulted(binding.input,
                               !available(binding.primary) ||
                               !available(binding.secondary) ||
                               states_.state(binding.input).faulted);
    }
}

void PhysicalInputRuntime::setProportionalValue(
    PhysicalControlId input, int16_t value, uint32_t nowMs,
    PhysicalInputEventBuffer *events)
{
    const int16_t previousValue = state(input).value;
    if (events != nullptr && previousValue != value)
        events->push(input, InputEvent::ValueChanged, nowMs, value);
    states_.setValue(input, value);
}

uint32_t PhysicalInputRuntime::startupUnsafeMask() const
{
    uint32_t mask = 0;
    for (size_t index = 0; index < count_; ++index) {
        const PhysicalControlId input = bindings_[index].input;
        const PhysicalControlKind kind = physicalControlDescriptor(
            static_cast<size_t>(input)).kind;
        if (kind != PhysicalControlKind::MaintainedSwitch &&
            kind != PhysicalControlKind::SpringCentredTrim)
            continue;
        const PhysicalInputState &inputState = states_.state(input);
        const bool displaced = inputState.stablePrimary ||
            (kind == PhysicalControlKind::SpringCentredTrim &&
             inputState.stableSecondary);
        if (displaced || inputState.faulted)
            mask |= uint32_t{1} << static_cast<uint8_t>(input);
    }
    return mask;
}

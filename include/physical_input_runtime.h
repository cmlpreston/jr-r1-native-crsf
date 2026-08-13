#pragma once

#include <stddef.h>
#include <stdint.h>

#include "physical_input.h"

// Converts one MCP23017 snapshot plus direct-GPIO levels into debounced logical
// events and a monitorable state store. Hardware reads remain outside this
// class, so event behaviour is host-testable.
class PhysicalInputRuntime {
public:
    bool begin(const PhysicalInputHardwareBinding *bindings, size_t count);
    void update(uint16_t mcpLevels, uint64_t directGpioLevels, uint32_t nowMs,
                PhysicalInputEventBuffer &events, bool mcpAvailable = true);
    void setProportionalValue(PhysicalControlId input, int16_t value,
                              uint32_t nowMs,
                              PhysicalInputEventBuffer *events = nullptr);

    const PhysicalInputStateStore &states() const { return states_; }
    const PhysicalInputState &state(PhysicalControlId input) const {
        return states_.state(input);
    }

private:
    bool endpointLevel(const InputEndpoint &endpoint, uint16_t mcpLevels,
                       uint64_t directGpioLevels) const;
    bool endpointActive(const InputEndpoint &endpoint, uint16_t mcpLevels,
                        uint64_t directGpioLevels) const;

    PhysicalInputHardwareBinding bindings_[kPhysicalControlCount];
    DigitalInputProcessor digital_[kPhysicalControlCount];
    SpringTrimInputProcessor trim_[kPhysicalControlCount];
    PhysicalInputStateStore states_;
    size_t count_ = 0;
};

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "physical_input.h"

struct McpInputSnapshot {
    uint8_t address;
    uint16_t levels;
    bool available;
};

// Converts addressed MCP23017 snapshots plus direct-GPIO levels into debounced logical
// events and a monitorable state store. Hardware reads remain outside this
// class, so event behaviour is host-testable.
class PhysicalInputRuntime {
public:
    bool begin(const PhysicalInputHardwareBinding *bindings, size_t count);
    void update(uint16_t mcpLevels, uint64_t directGpioLevels, uint32_t nowMs,
                PhysicalInputEventBuffer &events, bool mcpAvailable = true);
    void update(const McpInputSnapshot *expanders, size_t expanderCount,
                uint64_t directGpioLevels, uint32_t nowMs,
                PhysicalInputEventBuffer &events);
    void setProportionalValue(PhysicalControlId input, int16_t value,
                              uint32_t nowMs,
                              PhysicalInputEventBuffer *events = nullptr);

    const PhysicalInputStateStore &states() const { return states_; }
    const PhysicalInputState &state(PhysicalControlId input) const {
        return states_.state(input);
    }
    // Maintained switches must start released and spring-centred trims must
    // start centred. A faulted mapped input is also unsafe because its actual
    // position is unknown. The bit index is PhysicalControlId.
    uint32_t startupUnsafeMask() const;

private:
    PhysicalInputHardwareBinding bindings_[kPhysicalControlCount];
    DigitalInputProcessor digital_[kPhysicalControlCount];
    SpringTrimInputProcessor trim_[kPhysicalControlCount];
    PhysicalInputStateStore states_;
    size_t count_ = 0;
};

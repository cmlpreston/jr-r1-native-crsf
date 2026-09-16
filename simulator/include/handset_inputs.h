#pragma once

#include "physical_input_runtime.h"

// Synthetic contacts use the same debouncers and trim processors as hardware.
// These bit positions are simulator wiring, never an ESP32 pin map.
class SimulatorHandsetInputs {
public:
    SimulatorHandsetInputs() {
        PhysicalInputHardwareBinding map[11];
        for (uint8_t id = 0; id < 11; ++id) {
            const auto input = static_cast<PhysicalControlId>(id);
            const auto kind = physicalControlDescriptor(id).kind;
            map[id] = {input,
                {id < 2 ? InputSourceKind::Adc : InputSourceKind::DirectGpio,
                 0, static_cast<uint8_t>(id * 2), false},
                {kind == PhysicalControlKind::SpringCentredTrim
                    ? InputSourceKind::DirectGpio : InputSourceKind::Unmapped,
                 0, static_cast<uint8_t>(id * 2 + 1), false}};
        }
        runtime_.begin(map, 11);
        PhysicalInputEventBuffer initial;
        runtime_.update(0, 0, 0, initial);
    }
    void update(uint64_t contacts, int16_t steering, int16_t throttle,
                uint32_t now, PhysicalInputEventBuffer &events) {
        runtime_.update(0, contacts, now, events);
        runtime_.setProportionalValue(PhysicalControlId::SteeringWheel, steering, now, &events);
        runtime_.setProportionalValue(PhysicalControlId::ThrottleTrigger, throttle, now, &events);
    }
    const PhysicalInputStateStore &states() const { return runtime_.states(); }
private:
    PhysicalInputRuntime runtime_;
};

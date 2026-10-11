#pragma once
#include "control_configuration.h"
#include "output_safety.h"

struct ControlSafetyPolicy {
    uint16_t neutralAdcWindow = 80;
    uint32_t neutralDwellMs = 1000;
    uint32_t linkDwellMs = 500;
    uint8_t minimumLinkQuality = 50;
    constexpr ControlSafetyPolicy(uint16_t window = 80, uint32_t neutralMs = 1000,
                                 uint32_t linkMs = 500, uint8_t quality = 50)
        : neutralAdcWindow(window), neutralDwellMs(neutralMs), linkDwellMs(linkMs),
          minimumLinkQuality(quality) {}
};

struct ControlCycleInput {
    uint32_t nowMs = 0, resetSequence = 0;
    uint16_t throttleAdc = 0, steeringAdc = 0;
    bool startupComplete = false, linkValid = false;
    uint8_t linkQuality = 0;
    MixInputSnapshot digitalInputs;
    CrsfChannelOutputModel bindings;
};

struct ControlCycleResult {
    ProportionalInputState throttle, steering;
    CrsfChannelOutputModel outputs;
    OutputSafetyStatus safety = OutputSafetyStatus::Cut;
    bool inputsValid = false;
    uint32_t neutralSinceMs = 0;
};

// No Arduino, LVGL, locks, allocation, persistence or logging in the engine.
// One owner advances it; hardware and simulator supply samples and time.
// Configuration contents are immutable within a revision; every edit must
// publish a new revision so profiles and the compiled mix plan stay coherent.
class ControlEngine {
public:
    explicit ControlEngine(ControlSafetyPolicy policy = {}) : policy_(policy) {}
    ControlCycleResult update(const ControlConfigurationSnapshot &configuration,
                              const ControlCycleInput &input);
private:
    void reset(uint32_t nowMs);
    OutputSafetyStatus qualify(const ControlConfigurationSnapshot &configuration,
        const ControlCycleInput &input, const ProportionalInputState &throttle,
        bool inputsValid, bool mixedNeutral);
    ControlSafetyPolicy policy_;
    ProportionalInputRuntime throttle_, steering_;
    ChannelSlewRuntime slew_;
    ChannelMixRuntime mix_;
    CompiledMixPlan mixPlan_;
    ControlConfigurationSnapshot previous_;
    uint32_t resetSequence_ = 0, neutralSince_ = 0, linkSince_ = 0;
    bool configured_ = false, armed_ = false;
    bool neutralTiming_ = false, linkTiming_ = false, editInhibited_ = false;
};

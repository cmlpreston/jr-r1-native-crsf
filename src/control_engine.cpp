#include "control_engine.h"

void ControlEngine::reset(uint32_t nowMs)
{
    armed_ = neutralTiming_ = linkTiming_ = false;
    neutralSince_ = linkSince_ = 0;
    slew_.reset(nowMs);
    mix_.reset();
}

OutputSafetyStatus ControlEngine::qualify(const ControlConfigurationSnapshot &c,
    const ControlCycleInput &in, const ProportionalInputState &throttle,
    bool inputsValid, bool mixedNeutral)
{
    OutputSafetyStatus veto = OutputSafetyStatus::Armed;
    if (!c.modelStorageValid || !mixPlan_.valid) veto = OutputSafetyStatus::ConfigurationFault;
    else if (c.calibrationActive) veto = OutputSafetyStatus::Calibration;
    else if (!inputsValid) veto = OutputSafetyStatus::InputFault;
    else if (!c.throttleEnabled) veto = OutputSafetyStatus::ChannelDisabled;
    else if (!in.startupComplete) veto = OutputSafetyStatus::StartupWarning;
    else if (!c.armRequested || editInhibited_) veto = OutputSafetyStatus::Cut;
    if (veto != OutputSafetyStatus::Armed) {
        reset(in.nowMs);
        return veto;
    }
    // Return-link loss after arming remains advisory; RX owns RF-loss failsafe.
    if (armed_) return OutputSafetyStatus::Armed;
    if (!in.linkValid || in.linkQuality < policy_.minimumLinkQuality) {
        linkTiming_ = neutralTiming_ = false;
        neutralSince_ = 0;
        return OutputSafetyStatus::LinkRequired;
    }
    if (!linkTiming_) { linkTiming_ = true; linkSince_ = in.nowMs; }
    if (uint32_t(in.nowMs - linkSince_) < policy_.linkDwellMs) {
        neutralTiming_ = false;
        neutralSince_ = 0;
        return OutputSafetyStatus::LinkRequired;
    }
    const int delta = int(throttle.filteredAdc) - c.throttleInput.calibration.centre;
    if (!mixedNeutral || delta < -int(policy_.neutralAdcWindow) ||
        delta > int(policy_.neutralAdcWindow)) {
        neutralTiming_ = false;
        neutralSince_ = 0;
        return OutputSafetyStatus::NeutralRequired;
    }
    if (!neutralTiming_) { neutralTiming_ = true; neutralSince_ = in.nowMs; }
    if (uint32_t(in.nowMs - neutralSince_) < policy_.neutralDwellMs)
        return OutputSafetyStatus::NeutralRequired;
    armed_ = true;
    return OutputSafetyStatus::Armed;
}

ControlCycleResult ControlEngine::update(const ControlConfigurationSnapshot &c,
                                       const ControlCycleInput &in)
{
    if (in.resetSequence != resetSequence_) {
        resetSequence_ = in.resetSequence;
        reset(in.nowMs);
    }
    if (!configured_ || c.revision != previous_.revision) {
        const bool structuralEdit = configured_ && controlConfigurationRequiresRearm(previous_, c);
        if (structuralEdit) {
            reset(in.nowMs);
            editInhibited_ = c.armRequested;
        }
        if (!configured_ || structuralEdit) {
            mixPlan_ = compileConfiguredMixing(c);
            throttle_.setProfile(c.throttleInput);
            steering_.setProfile(c.steeringInput);
        }
        previous_ = c;
        configured_ = true;
    }
    if (!c.armRequested) editInhibited_ = false;
    ControlCycleResult result;
    result.throttle = throttle_.update(in.throttleAdc);
    result.steering = steering_.update(in.steeringAdc);
    auto inputs = in.digitalInputs;
    inputs.valid &= ~uint64_t{3};
    inputs.values[0] = result.steering.normalisedPermille;
    inputs.values[1] = result.throttle.normalisedPermille;
    if (result.steering.valid) inputs.valid |= 1;
    if (result.throttle.valid) inputs.valid |= 2;
    result.outputs = in.bindings;
    const auto inspection = inspectConfiguredMixInputs(c, inputs, result.outputs, mixPlan_);
    result.inputsValid = c.modelStorageValid && result.throttle.valid && inspection.inputsValid;
    result.safety = qualify(c, in, result.throttle, result.inputsValid, inspection.throttleNeutral);
    const bool live = result.safety == OutputSafetyStatus::Armed;
    if (!live) slew_.reset(in.nowMs);
    processConfiguredMixedOutputs(c, result.outputs, inputs, mix_, slew_, in.nowMs, live, &mixPlan_);
    result.neutralSinceMs = neutralSince_;
    return result;
}

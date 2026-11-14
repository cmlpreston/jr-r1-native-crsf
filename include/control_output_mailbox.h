#pragma once
#include "crsf_channel_output.h"
#include "output_safety.h"

struct ControlCommandSnapshot {
    uint32_t resetSequence = 0;
    uint32_t physicalCutSequence = 0;
    bool physicalCutPending = false;
    bool startupPreflightComplete = false;
    bool configurationPending = false;
};

struct SharedChannelOutputState {
    CrsfChannelOutputModel model;
    OutputSafetyStatus controlSafetyStatus = OutputSafetyStatus::Cut;
    uint32_t updatedUs = 0;
};

// Caller holds ONE lock for commands, reset requests and output publication.
// An in-flight cycle must acknowledge the current reset before it can publish.
class ControlOutputMailbox {
public:
    ControlCommandSnapshot commands() const { return commands_; }
    SharedChannelOutputState outputs() const { return outputs_; }
    void requestReset() {
        ++commands_.resetSequence;
        outputs_.controlSafetyStatus = OutputSafetyStatus::Cut;
        // A reset is not evidence of a fresh input sample.
    }
    void completeStartup() {
        commands_.startupPreflightComplete = true;
        requestReset();
    }
    void requireConfiguration() {
        requestReset(); commands_.configurationPending = true;
    }
    void requestPhysicalCut() {
        requireConfiguration();++commands_.physicalCutSequence;
        commands_.physicalCutPending = true;
    }
    bool acceptConfiguration(uint32_t revision, bool armRequested=false,
                             uint32_t observedPhysicalCut=0) {
        // A UI snapshot built before the independent sampler's CUT must never
        // clear its veto. Only a fresh, explicitly disarmed UI revision may ack.
        if(commands_.physicalCutPending &&
           (armRequested || observedPhysicalCut!=commands_.physicalCutSequence))return false;
        configurationRevision_ = revision;
        commands_.configurationPending = false;
        commands_.physicalCutPending = false;
        return true;
    }
    bool publish(const CrsfChannelOutputModel &model, OutputSafetyStatus status,
                 uint32_t updatedUs, uint32_t acknowledgedReset, uint32_t revision = 0) {
        if (commands_.configurationPending || revision != configurationRevision_ ||
            acknowledgedReset != commands_.resetSequence) return false;
        outputs_.model = model;
        outputs_.controlSafetyStatus = status;
        outputs_.updatedUs = updatedUs;
        return true;
    }
private:
    ControlCommandSnapshot commands_;
    SharedChannelOutputState outputs_;
    uint32_t configurationRevision_ = 0;
};

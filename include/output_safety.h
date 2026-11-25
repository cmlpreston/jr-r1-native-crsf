#pragma once

#include <stdint.h>

// Control-path reasons are retained through final arbitration so the UI and
// diagnostics report why safe values are being transmitted, not merely that
// outputs are non-live.
enum class OutputSafetyStatus : uint8_t {
    Cut,
    StartupWarning,
    LinkRequired,
    NeutralRequired,
    Armed,
    Calibration,
    InputFault,
    ControlStale,
    ChannelDisabled,
    ConfigurationFault,
};

// Instantaneous checks from the control owner, including when output is CUT.
// Link and neutral dwell are reported through OutputSafetyStatus during arming.
struct OutputReadiness {
    bool configurationValid = false;
    bool inputsValid = false;
    bool physicalInputsHealthy = false;
    bool startupComplete = false;
    bool throttleEnabled = false;
    bool triggerValid = false;
    bool triggerCentred = false;
    bool throttleMixValid = false;
    bool throttleMixActive = false;
    bool throttleMixNeutral = false;
    bool linkValid = false;
    bool linkMeetsMinimum = false;
    uint8_t throttleChannel = 0;
    uint8_t linkQuality = 0;
    uint8_t minimumLinkQuality = 0;
    int16_t triggerPermille = 0;
    int16_t throttleMixPermille = 0;
};

struct OutputSafetyDecision {
    OutputSafetyStatus status = OutputSafetyStatus::ControlStale;
    bool transmitRequested = false;
};

// This is the final live/safe decision, evaluated by the CRSF task immediately
// before it builds a channel frame. A stale control model always overrides an
// earlier Armed state. Every other reason retains its identity for reporting.
OutputSafetyDecision arbitrateOutputSafety(
    OutputSafetyStatus controlStatus, bool controlFresh);

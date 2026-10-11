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

struct OutputSafetyDecision {
    OutputSafetyStatus status = OutputSafetyStatus::ControlStale;
    bool transmitRequested = false;
};

// This is the final live/safe decision, evaluated by the CRSF task immediately
// before it builds a channel frame. A stale control model always overrides an
// earlier Armed state. Every other reason retains its identity for reporting.
OutputSafetyDecision arbitrateOutputSafety(
    OutputSafetyStatus controlStatus, bool controlFresh);

#pragma once

#include <stdint.h>

// Runtime state for one output channel's rate limiter. Configuration remains
// in ChannelResponseProfile; this object owns only time-dependent live state.
// It is deliberately independent of Arduino/LVGL so the 250 Hz control-path
// behaviour can be characterized on the host.
class ChannelRampRuntime {
public:
    // Advance toward targetPermille using the ramp time for the side being
    // approached. A zero ramp time preserves the existing immediate response.
    int16_t update(int16_t targetPermille, uint32_t nowMs,
                   uint16_t negativeRampMs, uint16_t positiveRampMs);

    // Establish an explicit safe output and time baseline. This is used when
    // CUT, calibration, stale input or a settings transition resets control.
    void reset(int16_t outputPermille, uint32_t nowMs);

    int16_t outputPermille() const { return outputPermille_; }
    uint32_t lastUpdateMs() const { return lastUpdateMs_; }
    bool initialized() const { return initialized_; }

private:
    int16_t outputPermille_ = 0;
    uint32_t lastUpdateMs_ = 0;
    bool initialized_ = false;
};

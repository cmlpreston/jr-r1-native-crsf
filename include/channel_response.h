#pragma once

#include <stdint.h>

constexpr int16_t CHANNEL_RESPONSE_EXTENDED_LIMIT_PERMILLE = 1200;
constexpr uint8_t CHANNEL_RESPONSE_MIN_ENDPOINT_PERCENT = 50;
constexpr uint8_t CHANNEL_RESPONSE_MAX_ENDPOINT_PERCENT = 100;
constexpr uint8_t CHANNEL_RESPONSE_MAX_RATE_PERCENT = 100;
constexpr int8_t CHANNEL_RESPONSE_MIN_EXPO_PERCENT = -100;
constexpr int8_t CHANNEL_RESPONSE_MAX_EXPO_PERCENT = 100;
constexpr int8_t CHANNEL_RESPONSE_MIN_SUBTRIM_PERCENT = -20;
constexpr int8_t CHANNEL_RESPONSE_MAX_SUBTRIM_PERCENT = 20;
constexpr uint16_t CHANNEL_RESPONSE_MAX_RAMP_MS = 2000;

// Response settings belong to one CRSF output channel. They deliberately do
// not contain ADC calibration, filtering or deadband: those belong to the
// physical proportional input that is assigned to the channel.
struct ChannelResponseProfile {
    uint8_t negativeEndpointPercent = 100;
    uint8_t positiveEndpointPercent = 100;
    uint8_t negativeRatePercent = 100;
    uint8_t positiveRatePercent = 100;
    int8_t negativeExpoPercent = 0;
    int8_t positiveExpoPercent = 0;
    uint16_t negativeRampMs = 0;
    uint16_t positiveRampMs = 0;
    int8_t subtrimPercent = 0;
    bool reversed = false;
};

// UI left/right endpoints follow the output direction after reversal, while
// persisted endpoint fields retain their existing input-side meaning.
inline uint8_t channelEndpointForOutput(const ChannelResponseProfile &profile, bool positive)
{
    return positive != profile.reversed ? profile.positiveEndpointPercent : profile.negativeEndpointPercent;
}
inline void setChannelEndpointForOutput(ChannelResponseProfile &profile, bool positive, uint8_t percent)
{
    if (positive != profile.reversed) profile.positiveEndpointPercent = percent;
    else profile.negativeEndpointPercent = percent;
}

// Clamp a profile loaded from persistence or edited by a UI to the supported
// envelope. Kept outside the UI so every future frontend uses identical rules.
void sanitizeChannelResponseProfile(ChannelResponseProfile &profile);

// Apply expo, rate, endpoint, reversal and response-profile sub-trim in that
// order. The input is normalised -1000..+1000; output may use the retained
// +/-1200 internal envelope until the final transport policy is applied.
int16_t applyChannelResponse(const ChannelResponseProfile &profile,
                             int16_t inputPermille);


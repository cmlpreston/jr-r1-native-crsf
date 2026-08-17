#include "channel_response.h"

#include <algorithm>
#include <cstdlib>

namespace {

template <typename T>
T clampValue(T value, T minimum, T maximum)
{
    return std::max(minimum, std::min(value, maximum));
}

int16_t applyExpo(int16_t value, int8_t expoPercent)
{
    value = clampValue<int16_t>(value, -1000, 1000);
    const int32_t sign = value < 0 ? -1 : 1;
    const int32_t magnitude = std::abs(static_cast<int32_t>(value));
    const int32_t cubic = magnitude * magnitude * magnitude / 1000000L;
    const int32_t shaped = magnitude +
        static_cast<int32_t>(expoPercent) * (cubic - magnitude) / 100;
    return static_cast<int16_t>(
        sign * clampValue<int32_t>(shaped, 0, 1000));
}

} // namespace

void sanitizeChannelResponseProfile(ChannelResponseProfile &profile)
{
    profile.negativeEndpointPercent = clampValue<uint8_t>(
        profile.negativeEndpointPercent,
        CHANNEL_RESPONSE_MIN_ENDPOINT_PERCENT,
        CHANNEL_RESPONSE_MAX_ENDPOINT_PERCENT);
    profile.positiveEndpointPercent = clampValue<uint8_t>(
        profile.positiveEndpointPercent,
        CHANNEL_RESPONSE_MIN_ENDPOINT_PERCENT,
        CHANNEL_RESPONSE_MAX_ENDPOINT_PERCENT);
    profile.negativeRatePercent = clampValue<uint8_t>(
        profile.negativeRatePercent, 0, CHANNEL_RESPONSE_MAX_RATE_PERCENT);
    profile.positiveRatePercent = clampValue<uint8_t>(
        profile.positiveRatePercent, 0, CHANNEL_RESPONSE_MAX_RATE_PERCENT);
    profile.negativeExpoPercent = clampValue<int8_t>(
        profile.negativeExpoPercent,
        CHANNEL_RESPONSE_MIN_EXPO_PERCENT,
        CHANNEL_RESPONSE_MAX_EXPO_PERCENT);
    profile.positiveExpoPercent = clampValue<int8_t>(
        profile.positiveExpoPercent,
        CHANNEL_RESPONSE_MIN_EXPO_PERCENT,
        CHANNEL_RESPONSE_MAX_EXPO_PERCENT);
    profile.negativeRampMs = clampValue<uint16_t>(
        profile.negativeRampMs, 0, CHANNEL_RESPONSE_MAX_RAMP_MS);
    profile.positiveRampMs = clampValue<uint16_t>(
        profile.positiveRampMs, 0, CHANNEL_RESPONSE_MAX_RAMP_MS);
    profile.subtrimPercent = clampValue<int8_t>(
        profile.subtrimPercent,
        CHANNEL_RESPONSE_MIN_SUBTRIM_PERCENT,
        CHANNEL_RESPONSE_MAX_SUBTRIM_PERCENT);
}

int16_t applyChannelResponse(const ChannelResponseProfile &profile,
                             int16_t inputPermille)
{
    const bool negative = inputPermille < 0;
    const int8_t expo = negative ? profile.negativeExpoPercent
                                 : profile.positiveExpoPercent;
    const uint8_t rate = negative ? profile.negativeRatePercent
                                  : profile.positiveRatePercent;
    const uint8_t endpoint = negative ? profile.negativeEndpointPercent
                                      : profile.positiveEndpointPercent;
    int32_t value = applyExpo(inputPermille, expo);
    value = value * rate / 100;
    value = value * endpoint / 100;
    if (profile.reversed) value = -value;
    value += static_cast<int32_t>(profile.subtrimPercent) * 10;
    return static_cast<int16_t>(clampValue<int32_t>(
        value, -CHANNEL_RESPONSE_EXTENDED_LIMIT_PERMILLE,
        CHANNEL_RESPONSE_EXTENDED_LIMIT_PERMILLE));
}


#include "channel_ramp.h"

#include <algorithm>

namespace {

constexpr uint32_t kMaximumElapsedMs = 100;

} // namespace

int16_t ChannelRampRuntime::update(int16_t targetPermille, uint32_t nowMs,
                                   uint16_t negativeRampMs,
                                   uint16_t positiveRampMs)
{
    // Preserve the legacy first-use behaviour: a runtime that has never been
    // reset or updated adopts its first target immediately.
    if (!initialized_) {
        initialized_ = true;
        outputPermille_ = targetPermille;
        lastUpdateMs_ = nowMs;
        return outputPermille_;
    }

    // Unsigned subtraction intentionally makes millis() rollover transparent.
    const uint32_t elapsedMs = std::min<uint32_t>(
        nowMs - lastUpdateMs_, kMaximumElapsedMs);
    lastUpdateMs_ = nowMs;

    const bool positiveSide = targetPermille > 0 ||
        (targetPermille == 0 && outputPermille_ > 0);
    const uint16_t travelMs = positiveSide ? positiveRampMs : negativeRampMs;
    if (travelMs == 0 || elapsedMs == 0) {
        outputPermille_ = targetPermille;
        return outputPermille_;
    }

    const int32_t maximumStep = std::max<int32_t>(
        1, (1000L * static_cast<int32_t>(elapsedMs) + travelMs - 1) /
               travelMs);
    const int32_t delta = static_cast<int32_t>(targetPermille) -
                          outputPermille_;
    const int32_t limitedDelta = std::max<int32_t>(
        -maximumStep, std::min<int32_t>(delta, maximumStep));
    outputPermille_ += static_cast<int16_t>(limitedDelta);
    return outputPermille_;
}

void ChannelRampRuntime::reset(int16_t outputPermille, uint32_t nowMs)
{
    outputPermille_ = outputPermille;
    lastUpdateMs_ = nowMs;
    initialized_ = true;
}

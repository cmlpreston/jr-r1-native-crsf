#include "proportional_input.h"

#include <algorithm>
#include <stdlib.h>

bool ProportionalCalibration::valid(uint16_t minimumHalfSpan) const
{
    return minimum + minimumHalfSpan < centre &&
           centre + minimumHalfSpan < maximum;
}

ProportionalInput::ProportionalInput(const ProportionalCalibration &calibration)
    : calibration_(calibration)
{
}

void ProportionalInput::setCalibration(const ProportionalCalibration &calibration)
{
    if (calibration.valid(kMinimumHalfSpan)) calibration_ = calibration;
}

int16_t ProportionalInput::mapToPermille(uint16_t sample) const
{
    sample = std::max(calibration_.minimum,
                      std::min(sample, calibration_.maximum));
    if (sample <= calibration_.centre) {
        const uint32_t span = calibration_.centre - calibration_.minimum;
        return static_cast<int16_t>(-1000 +
            static_cast<int32_t>(sample - calibration_.minimum) * 1000 / span);
    }

    const uint32_t span = calibration_.maximum - calibration_.centre;
    return static_cast<int16_t>(
        static_cast<uint32_t>(sample - calibration_.centre) * 1000 / span);
}

void ProportionalInput::startCalibration(uint16_t sample)
{
    calibrationActive_ = true;
    sawMinimum_ = false;
    sawMaximum_ = false;
    observedMinimum_ = sample;
    observedMaximum_ = sample;
    stableSample_ = sample;
    liveSample_ = sample;
    stableSinceMs_ = 0;
}

void ProportionalInput::cancelCalibration()
{
    calibrationActive_ = false;
}

bool ProportionalInput::updateCalibration(uint16_t sample, uint32_t nowMs)
{
    if (!calibrationActive_) return false;

    liveSample_ = sample;
    observedMinimum_ = std::min(observedMinimum_, sample);
    observedMaximum_ = std::max(observedMaximum_, sample);

    const uint16_t oldLowThreshold = calibration_.centre -
        std::max<uint16_t>(kMinimumHalfSpan,
                           (calibration_.centre - calibration_.minimum) / 3);
    const uint16_t oldHighThreshold = calibration_.centre +
        std::max<uint16_t>(kMinimumHalfSpan,
                           (calibration_.maximum - calibration_.centre) / 3);
    sawMinimum_ |= sample <= oldLowThreshold;
    sawMaximum_ |= sample >= oldHighThreshold;
    if (!sawMinimum_ || !sawMaximum_) {
        stableSinceMs_ = 0;
        return false;
    }

    // Neutral must be comfortably inside the newly observed range. This
    // prevents an endpoint held still from being mistaken for centre.
    const uint16_t span = observedMaximum_ - observedMinimum_;
    if (sample <= observedMinimum_ + span / 8 ||
        sample >= observedMaximum_ - span / 8) {
        stableSinceMs_ = 0;
        stableSample_ = sample;
        return false;
    }

    if (abs(static_cast<int>(sample) - static_cast<int>(stableSample_)) >
        kMovementThreshold) {
        stableSample_ = sample;
        stableSinceMs_ = nowMs;
        return false;
    }
    if (stableSinceMs_ == 0) stableSinceMs_ = nowMs;
    if (nowMs - stableSinceMs_ < kStableCentreMs) return false;

    const ProportionalCalibration candidate {
        observedMinimum_, sample, observedMaximum_
    };
    if (!candidate.valid(kMinimumHalfSpan)) {
        stableSinceMs_ = 0;
        return false;
    }

    calibration_ = candidate;
    calibrationActive_ = false;
    return true;
}

ProportionalCalibrationProgress ProportionalInput::calibrationProgress() const
{
    return {
        observedMinimum_, liveSample_, observedMaximum_,
        sawMinimum_, sawMaximum_
    };
}

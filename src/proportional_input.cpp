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
    calibrationFilterScaled_ =
        static_cast<uint32_t>(sample) << kCalibrationFilterShift;
    stableSinceMs_ = 0;
    activeEndpoint_ = CalibrationEndpoint::None;
    endpointSettleSample_ = sample;
    endpointSettleSinceMs_ = 0;
    endpointAveraging_ = false;
}

void ProportionalInput::cancelCalibration()
{
    calibrationActive_ = false;
}

bool ProportionalInput::updateCalibration(uint16_t sample, uint32_t nowMs)
{
    if (!calibrationActive_) return false;

    // Work only with the calibration-filtered value below. A one-sample spike
    // can therefore move the saved extrema by only 1/8 of its magnitude, and
    // the centre saved after the stability interval is an averaged value.
    sample = filterCalibrationSample(sample);
    liveSample_ = sample;
    const uint16_t oldLowThreshold = calibration_.centre -
        std::max<uint16_t>(kMinimumHalfSpan,
                           (calibration_.centre - calibration_.minimum) / 3);
    const uint16_t oldHighThreshold = calibration_.centre +
        std::max<uint16_t>(kMinimumHalfSpan,
                           (calibration_.maximum - calibration_.centre) / 3);
    const CalibrationEndpoint endpoint = sample <= oldLowThreshold
        ? CalibrationEndpoint::Minimum
        : sample >= oldHighThreshold
            ? CalibrationEndpoint::Maximum
            : CalibrationEndpoint::None;
    updateEndpointAverage(endpoint, sample, nowMs);
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

uint16_t ProportionalInput::filterCalibrationSample(uint16_t sample)
{
    const int32_t target =
        static_cast<int32_t>(sample) << kCalibrationFilterShift;
    const int32_t current = static_cast<int32_t>(calibrationFilterScaled_);
    calibrationFilterScaled_ = static_cast<uint32_t>(
        current + (target - current) / (1 << kCalibrationFilterShift));
    return static_cast<uint16_t>(
        (calibrationFilterScaled_ + (1U << (kCalibrationFilterShift - 1))) >>
        kCalibrationFilterShift);
}

void ProportionalInput::updateEndpointAverage(CalibrationEndpoint endpoint,
                                              uint16_t sample,
                                              uint32_t nowMs)
{
    if (endpoint == CalibrationEndpoint::None) {
        activeEndpoint_ = CalibrationEndpoint::None;
        endpointAveraging_ = false;
        return;
    }

    if (endpoint != activeEndpoint_) {
        activeEndpoint_ = endpoint;
        endpointSettleSample_ = sample;
        endpointSettleSinceMs_ = nowMs;
        endpointAveraging_ = false;
        return;
    }

    if (!endpointAveraging_) {
        if (abs(static_cast<int>(sample) -
                static_cast<int>(endpointSettleSample_)) >
            kEndpointSettleMovement) {
            endpointSettleSample_ = sample;
            endpointSettleSinceMs_ = nowMs;
            return;
        }
        if (nowMs - endpointSettleSinceMs_ < kEndpointSettleMs) return;

        // The trigger has reached and settled at an extreme. Discard the
        // transition samples, then continuously track the calibration EMA for
        // as long as the trigger remains in this endpoint region. Unlike a
        // lifetime arithmetic mean, this progressively forgets the approach
        // to the stop and converges quickly on the resting endpoint.
        endpointAveraging_ = true;
    }

    // Stop updating as soon as the filtered signal moves materially back
    // toward centre. Without this guard, the EMA's release tail would replace
    // a good held endpoint with progressively more central values.
    if ((endpoint == CalibrationEndpoint::Minimum && sawMinimum_ &&
         sample > observedMinimum_ + kEndpointReleaseMovement) ||
        (endpoint == CalibrationEndpoint::Maximum && sawMaximum_ &&
         sample + kEndpointReleaseMovement < observedMaximum_)) {
        activeEndpoint_ = CalibrationEndpoint::None;
        endpointAveraging_ = false;
        return;
    }

    if (endpoint == CalibrationEndpoint::Minimum) {
        observedMinimum_ = sample;
        sawMinimum_ = true;
    } else {
        observedMaximum_ = sample;
        sawMaximum_ = true;
    }
}

ProportionalCalibrationProgress ProportionalInput::calibrationProgress() const
{
    return {
        observedMinimum_, liveSample_, observedMaximum_,
        sawMinimum_, sawMaximum_
    };
}

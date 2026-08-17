#include "proportional_input.h"

#include <algorithm>
#include <stdlib.h>

bool ProportionalCalibration::valid(uint16_t minimumHalfSpan) const
{
    return minimum + minimumHalfSpan < centre &&
           centre + minimumHalfSpan < maximum;
}

bool ProportionalInputProfile::valid() const
{
    return calibration.valid() &&
           validMinimumAdc <= validMaximumAdc &&
           deadbandAdc < calibration.centre - calibration.minimum &&
           deadbandAdc < calibration.maximum - calibration.centre &&
           filterShift <= 15;
}

int16_t mapProportionalInputToPermille(const ProportionalInputProfile &profile,
                                       uint16_t filteredAdc)
{
    const ProportionalCalibration &calibration = profile.calibration;
    if (!calibration.valid()) return 0;

    const uint16_t neutralLow = std::max<uint16_t>(
        calibration.minimum + 1,
        calibration.centre > profile.deadbandAdc
            ? calibration.centre - profile.deadbandAdc
            : calibration.minimum + 1);
    const uint16_t neutralHigh = std::min<uint16_t>(
        calibration.maximum - 1,
        static_cast<uint16_t>(std::min<uint32_t>(
            calibration.maximum,
            static_cast<uint32_t>(calibration.centre) + profile.deadbandAdc)));

    if (filteredAdc >= neutralLow && filteredAdc <= neutralHigh) return 0;
    if (filteredAdc < neutralLow) {
        filteredAdc = std::max(filteredAdc, calibration.minimum);
        return static_cast<int16_t>(-
            static_cast<int32_t>(neutralLow - filteredAdc) * 1000 /
            (neutralLow - calibration.minimum));
    }
    filteredAdc = std::min(filteredAdc, calibration.maximum);
    return static_cast<int16_t>(
        static_cast<uint32_t>(filteredAdc - neutralHigh) * 1000 /
        (calibration.maximum - neutralHigh));
}

ProportionalInputRuntime::ProportionalInputRuntime(
    const ProportionalInputProfile &profile)
    : profile_(profile)
{
}

void ProportionalInputRuntime::setProfile(
    const ProportionalInputProfile &profile)
{
    profile_ = profile;
}

void ProportionalInputRuntime::reset(uint16_t initialSample)
{
    state_.rawAdc = initialSample;
    state_.filteredAdc = initialSample;
    state_.normalisedPermille = mapProportionalInputToPermille(
        profile_, initialSample);
    state_.valid = profile_.valid() &&
                   initialSample >= profile_.validMinimumAdc &&
                   initialSample <= profile_.validMaximumAdc;
    initialised_ = true;
}

ProportionalInputState ProportionalInputRuntime::update(uint16_t rawSample)
{
    if (!initialised_) reset(rawSample);
    state_.rawAdc = rawSample;
    const uint8_t filterShift = std::min<uint8_t>(profile_.filterShift, 15);
    if (filterShift == 0) {
        state_.filteredAdc = rawSample;
    } else {
        const uint32_t weight = (uint32_t{1} << filterShift) - 1U;
        state_.filteredAdc = static_cast<uint16_t>(
            (static_cast<uint32_t>(state_.filteredAdc) * weight + rawSample) /
            (weight + 1U));
    }
    state_.normalisedPermille = mapProportionalInputToPermille(
        profile_, state_.filteredAdc);
    state_.valid = profile_.valid() &&
                   state_.filteredAdc >= profile_.validMinimumAdc &&
                   state_.filteredAdc <= profile_.validMaximumAdc;
    return state_;
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
    endpointReleaseLocked_ = false;
    endpointReleaseAnchor_ = sample;
    endpointSampleSum_ = 0;
    endpointSampleCount_ = 0;
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
        endpointReleaseLocked_ = false;
        endpointSampleSum_ = 0;
        endpointSampleCount_ = 0;
        return;
    }

    if (endpoint != activeEndpoint_) {
        activeEndpoint_ = endpoint;
        endpointSettleSample_ = sample;
        endpointSettleSinceMs_ = nowMs;
        endpointAveraging_ = false;
        endpointReleaseLocked_ = false;
        endpointReleaseAnchor_ = sample;
        endpointSampleSum_ = 0;
        endpointSampleCount_ = 0;
        return;
    }

    // Once release has been recognised, preserve the endpoint and ignore the
    // EMA tail until the signal really leaves this endpoint region. Otherwise
    // a slowly moving trigger can be mistaken for another settled hold.
    if (endpointReleaseLocked_) return;

    if (!endpointAveraging_) {
        if (abs(static_cast<int>(sample) -
                static_cast<int>(endpointSettleSample_)) >
            kEndpointSettleMovement) {
            endpointSettleSample_ = sample;
            endpointSettleSinceMs_ = nowMs;
            return;
        }
        if (nowMs - endpointSettleSinceMs_ < kEndpointSettleMs) return;

        // The input has reached and settled at an extreme. Discard transition
        // samples, then calculate a true mean over the complete held period.
        // This gives the operator visible noise rejection rather than an EMA
        // which eventually becomes indistinguishable from the live value.
        endpointAveraging_ = true;
        endpointReleaseAnchor_ = sample;
        endpointSampleSum_ = 0;
        endpointSampleCount_ = 0;
    }

    // Track a fixed, outward-moving anchor rather than comparing with the
    // immediately previous average. A slow return to neutral may move only a
    // few counts per sample, but its cumulative movement from this anchor is
    // unambiguous.
    if (endpoint == CalibrationEndpoint::Minimum)
        endpointReleaseAnchor_ = std::min(endpointReleaseAnchor_, sample);
    else
        endpointReleaseAnchor_ = std::max(endpointReleaseAnchor_, sample);

    const bool released =
        (endpoint == CalibrationEndpoint::Minimum && sawMinimum_ &&
         sample > endpointReleaseAnchor_ + kEndpointReleaseMovement) ||
        (endpoint == CalibrationEndpoint::Maximum && sawMaximum_ &&
         sample + kEndpointReleaseMovement < endpointReleaseAnchor_);
    if (released) {
        endpointAveraging_ = false;
        endpointReleaseLocked_ = true;
        return;
    }

    endpointSampleSum_ += sample;
    ++endpointSampleCount_;
    const uint16_t heldAverage = static_cast<uint16_t>(
        (endpointSampleSum_ + endpointSampleCount_ / 2U) /
        endpointSampleCount_);

    if (endpoint == CalibrationEndpoint::Minimum) {
        observedMinimum_ = heldAverage;
        sawMinimum_ = true;
    } else {
        observedMaximum_ = heldAverage;
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

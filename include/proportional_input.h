#pragma once

#include <stdint.h>

struct ProportionalCalibration {
    uint16_t minimum = 0;
    uint16_t centre = 2048;
    uint16_t maximum = 4095;

    constexpr ProportionalCalibration() = default;
    constexpr ProportionalCalibration(uint16_t minimumValue,
                                      uint16_t centreValue,
                                      uint16_t maximumValue)
        : minimum(minimumValue), centre(centreValue), maximum(maximumValue) {}

    bool valid(uint16_t minimumHalfSpan = 100) const;
};

struct ProportionalCalibrationProgress {
    uint16_t minimum = 0;
    uint16_t live = 0;
    uint16_t maximum = 0;
    bool sawMinimum = false;
    bool sawMaximum = false;

    constexpr ProportionalCalibrationProgress() = default;
    constexpr ProportionalCalibrationProgress(uint16_t minimumValue,
                                              uint16_t liveValue,
                                              uint16_t maximumValue,
                                              bool minimumSeen,
                                              bool maximumSeen)
        : minimum(minimumValue), live(liveValue), maximum(maximumValue),
          sawMinimum(minimumSeen), sawMaximum(maximumSeen) {}
};

// Persistent conditioning policy for one physical proportional control. This
// belongs to the input (for example the throttle trigger), not to whichever
// CRSF output channel or logical role currently consumes it.
struct ProportionalInputProfile {
    ProportionalCalibration calibration;
    uint16_t deadbandAdc = 0;
    uint16_t validMinimumAdc = 0;
    uint16_t validMaximumAdc = 4095;
    uint8_t filterShift = 2; // EMA alpha = 1 / 2^filterShift; zero is direct.

    bool valid() const;
};

struct ProportionalInputState {
    uint16_t rawAdc = 0;
    uint16_t filteredAdc = 0;
    int16_t normalisedPermille = 0;
    bool valid = false;
};

// Map one already-filtered ADC sample through a profile. Values inside the
// centre deadband produce exact zero; the remaining two spans are remapped so
// calibrated endpoints still produce -1000 and +1000.
int16_t mapProportionalInputToPermille(const ProportionalInputProfile &profile,
                                       uint16_t filteredAdc);

// Live filter and validity state for a physical proportional input. Keeping
// this separate from the profile lets the control task own all rapidly
// changing state while model/UI code edits persistent policy.
class ProportionalInputRuntime {
public:
    explicit ProportionalInputRuntime(
        const ProportionalInputProfile &profile = {});

    void setProfile(const ProportionalInputProfile &profile);
    const ProportionalInputProfile &profile() const { return profile_; }
    void reset(uint16_t initialSample);
    ProportionalInputState update(uint16_t rawSample);
    const ProportionalInputState &state() const { return state_; }

private:
    ProportionalInputProfile profile_;
    ProportionalInputState state_;
    bool initialised_ = false;
};

// Calibration and normalisation for any centre-returning proportional input.
// It deliberately knows nothing about steering, throttle, CRSF channel numbers,
// persistence, or the display that initiated calibration.
class ProportionalInput {
public:
    explicit ProportionalInput(const ProportionalCalibration &calibration = {});

    void setCalibration(const ProportionalCalibration &calibration);
    const ProportionalCalibration &calibration() const { return calibration_; }
    uint16_t centre() const { return calibration_.centre; }
    int16_t mapToPermille(uint16_t sample) const;

    void startCalibration(uint16_t sample);
    void cancelCalibration();
    bool calibrationActive() const { return calibrationActive_; }

    // Returns true once when a new min/centre/max calibration is accepted.
    bool updateCalibration(uint16_t sample, uint32_t nowMs);
    ProportionalCalibrationProgress calibrationProgress() const;

private:
    enum class CalibrationEndpoint : uint8_t {
        None,
        Minimum,
        Maximum,
    };

    static constexpr uint16_t kMinimumHalfSpan = 100;
    static constexpr uint16_t kMovementThreshold = 16;
    static constexpr uint32_t kStableCentreMs = 1500;
    // Calibration deliberately has its own low-pass filter. The normal control
    // path may choose different filtering later, while calibration must never
    // save one noisy ADC sample as a permanent endpoint or centre value.
    static constexpr uint8_t kCalibrationFilterShift = 3; // EMA alpha = 1/8
    static constexpr uint16_t kEndpointSettleMovement = 8;
    static constexpr uint16_t kEndpointReleaseMovement = 24;
    static constexpr uint32_t kEndpointSettleMs = 120;

    ProportionalCalibration calibration_;
    bool calibrationActive_ = false;
    bool sawMinimum_ = false;
    bool sawMaximum_ = false;
    uint16_t observedMinimum_ = 0;
    uint16_t observedMaximum_ = 0;
    uint16_t stableSample_ = 0;
    uint16_t liveSample_ = 0;
    uint32_t calibrationFilterScaled_ = 0;
    uint32_t stableSinceMs_ = 0;
    CalibrationEndpoint activeEndpoint_ = CalibrationEndpoint::None;
    uint16_t endpointSettleSample_ = 0;
    uint32_t endpointSettleSinceMs_ = 0;
    bool endpointAveraging_ = false;
    bool endpointReleaseLocked_ = false;
    uint16_t endpointReleaseAnchor_ = 0;
    uint64_t endpointSampleSum_ = 0;
    uint32_t endpointSampleCount_ = 0;

    uint16_t filterCalibrationSample(uint16_t sample);
    void updateEndpointAverage(CalibrationEndpoint endpoint, uint16_t sample,
                               uint32_t nowMs);
};

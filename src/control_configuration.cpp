#include "control_configuration.h"

#include <stddef.h>

namespace {

template <typename T>
T clampValue(T value, T minimum, T maximum)
{
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

bool equalCalibration(const ProportionalCalibration &left,
                      const ProportionalCalibration &right)
{
    return left.minimum == right.minimum &&
           left.centre == right.centre &&
           left.maximum == right.maximum;
}

bool equalInputProfile(const ProportionalInputProfile &left,
                       const ProportionalInputProfile &right)
{
    return equalCalibration(left.calibration, right.calibration) &&
           left.deadbandAdc == right.deadbandAdc &&
           left.validMinimumAdc == right.validMinimumAdc &&
           left.validMaximumAdc == right.validMaximumAdc &&
           left.filterShift == right.filterShift;
}

bool equalResponseProfile(const ChannelResponseProfile &left,
                          const ChannelResponseProfile &right)
{
    return left.negativeEndpointPercent == right.negativeEndpointPercent &&
           left.positiveEndpointPercent == right.positiveEndpointPercent &&
           left.negativeRatePercent == right.negativeRatePercent &&
           left.positiveRatePercent == right.positiveRatePercent &&
           left.negativeExpoPercent == right.negativeExpoPercent &&
           left.positiveExpoPercent == right.positiveExpoPercent &&
           left.negativeRampMs == right.negativeRampMs &&
           left.positiveRampMs == right.positiveRampMs &&
           left.subtrimPercent == right.subtrimPercent &&
           left.reversed == right.reversed;
}

bool validChannel(uint8_t channel)
{
    return channel >= 1 && channel <= CRSF_CHANNEL_COUNT;
}

} // namespace

bool equivalentControlConfiguration(const ControlConfigurationSnapshot &left,
                                    const ControlConfigurationSnapshot &right)
{
    if (!equalInputProfile(left.throttleInput, right.throttleInput) ||
        !equalInputProfile(left.steeringInput, right.steeringInput) ||
        left.proportionalOutputMask != right.proportionalOutputMask ||
        left.steeringChannel != right.steeringChannel ||
        left.throttleChannel != right.throttleChannel ||
        left.steeringEnabled != right.steeringEnabled ||
        left.throttleEnabled != right.throttleEnabled ||
        left.armRequested != right.armRequested ||
        left.calibrationActive != right.calibrationActive ||
        left.driveMode != right.driveMode ||
        left.walkLimitPercent != right.walkLimitPercent)
        return false;

    for (size_t index = 0; index < CRSF_CHANNEL_COUNT; ++index) {
        if (!equalResponseProfile(left.channelProfiles[index],
                                  right.channelProfiles[index]) ||
            left.channelParameters[index].trimPermille !=
                right.channelParameters[index].trimPermille ||
            left.channelParameters[index].subtrimPermille !=
                right.channelParameters[index].subtrimPermille)
            return false;
    }
    return true;
}

uint16_t configuredRoleOwnedChannels(
    const ControlConfigurationSnapshot &configuration)
{
    uint16_t mask = 0;
    if (validChannel(configuration.steeringChannel))
        mask |= static_cast<uint16_t>(1U << (configuration.steeringChannel - 1));
    if (validChannel(configuration.throttleChannel))
        mask |= static_cast<uint16_t>(1U << (configuration.throttleChannel - 1));
    return mask;
}

int16_t applyConfiguredResponse(
    const ControlConfigurationSnapshot &configuration, uint8_t channel,
    int16_t inputPermille)
{
    if (!validChannel(channel)) return inputPermille;
    return applyChannelResponse(configuration.channelProfiles[channel - 1],
                                inputPermille);
}

int16_t applyConfiguredDriveMode(
    const ControlConfigurationSnapshot &configuration,
    int16_t configuredPermille, int16_t rawInputPermille)
{
    if (!validChannel(configuration.throttleChannel)) return configuredPermille;
    const ChannelResponseProfile &profile =
        configuration.channelProfiles[configuration.throttleChannel - 1];
    const int16_t trim = static_cast<int16_t>(profile.subtrimPercent) * 10;
    int32_t driveComponent = static_cast<int32_t>(configuredPermille) - trim;
    if (configuration.driveMode == DriveMode::Walk && rawInputPermille > 0)
        driveComponent = driveComponent * configuration.walkLimitPercent / 100;
    return static_cast<int16_t>(clampValue<int32_t>(
        driveComponent + trim, -CHANNEL_RESPONSE_EXTENDED_LIMIT_PERMILLE,
        CHANNEL_RESPONSE_EXTENDED_LIMIT_PERMILLE));
}

int16_t applyConfiguredChannelAdjustments(
    const ControlConfigurationSnapshot &configuration, uint8_t channel,
    int16_t inputPermille)
{
    if (!validChannel(channel)) return inputPermille;
    const ChannelParameterValues &values =
        configuration.channelParameters[channel - 1];
    const int16_t shifted = static_cast<int16_t>(clampValue<int32_t>(
        static_cast<int32_t>(inputPermille) + values.subtrimPermille,
        -CHANNEL_RESPONSE_EXTENDED_LIMIT_PERMILLE,
        CHANNEL_RESPONSE_EXTENDED_LIMIT_PERMILLE));
    return static_cast<int16_t>(clampValue<int32_t>(
        static_cast<int32_t>(shifted) + values.trimPermille, -1000, 1000));
}

int16_t configuredChannelNeutral(
    const ControlConfigurationSnapshot &configuration, uint8_t channel)
{
    int16_t neutral = applyConfiguredResponse(configuration, channel, 0);
    if (channel == configuration.throttleChannel)
        neutral = applyConfiguredDriveMode(configuration, neutral, 0);
    return applyConfiguredChannelAdjustments(configuration, channel, neutral);
}

int16_t ChannelSlewRuntime::apply(
    const ControlConfigurationSnapshot &configuration, uint8_t channel,
    int16_t targetPermille, uint32_t nowMs)
{
    if (!validChannel(channel)) return targetPermille;
    const size_t index = channel - 1;
    if (lastUpdateMs_[index] == 0) {
        lastUpdateMs_[index] = nowMs;
        slewedPermille_[index] = 0;
    }
    const uint32_t elapsed = nowMs - lastUpdateMs_[index];
    const uint32_t elapsedMs = elapsed < 100U ? elapsed : 100U;
    lastUpdateMs_[index] = nowMs;

    const bool forwardSide = targetPermille > 0 ||
        (targetPermille == 0 && slewedPermille_[index] > 0);
    const ChannelResponseProfile &profile =
        configuration.channelProfiles[index];
    const uint16_t travelMs = forwardSide ? profile.positiveRampMs
                                          : profile.negativeRampMs;
    if (travelMs == 0 || elapsedMs == 0) {
        slewedPermille_[index] = targetPermille;
        return targetPermille;
    }

    int32_t maximumStep =
        (1000L * static_cast<int32_t>(elapsedMs) + travelMs - 1) / travelMs;
    if (maximumStep < 1) maximumStep = 1;
    const int32_t delta = static_cast<int32_t>(targetPermille) -
                          slewedPermille_[index];
    slewedPermille_[index] += static_cast<int16_t>(
        clampValue<int32_t>(delta, -maximumStep, maximumStep));
    return slewedPermille_[index];
}

void ChannelSlewRuntime::reset(uint32_t nowMs)
{
    for (size_t index = 0; index < CRSF_CHANNEL_COUNT; ++index) {
        slewedPermille_[index] = 0;
        lastUpdateMs_[index] = nowMs;
    }
}

ProcessedProportionalChannel processConfiguredProportionalChannel(
    const ControlConfigurationSnapshot &configuration, uint8_t channel,
    int16_t inputPermille, int16_t safeInputPermille,
    ChannelSlewRuntime &slew, uint32_t nowMs, bool advanceSlew)
{
    int16_t requested = applyConfiguredResponse(configuration, channel,
                                                inputPermille);
    int16_t safe = applyConfiguredResponse(configuration, channel,
                                           safeInputPermille);
    if (channel == configuration.throttleChannel) {
        requested = applyConfiguredDriveMode(configuration, requested,
                                             inputPermille);
        safe = applyConfiguredDriveMode(configuration, safe,
                                        safeInputPermille);
    }
    requested = applyConfiguredChannelAdjustments(configuration, channel,
                                                  requested);
    safe = applyConfiguredChannelAdjustments(configuration, channel, safe);
    if (advanceSlew)
        requested = slew.apply(configuration, channel, requested, nowMs);
    ProcessedProportionalChannel result;
    result.requestedPermille = requested;
    result.safePermille = safe;
    return result;
}

void processConfiguredProportionalOutputs(
    const ControlConfigurationSnapshot &configuration,
    CrsfChannelOutputModel &outputs, ChannelSlewRuntime &slew,
    uint32_t nowMs, bool advanceSlew, uint16_t reservedChannels)
{
    const uint16_t channels = static_cast<uint16_t>(
        configuration.proportionalOutputMask & ~reservedChannels);
    for (uint8_t channel = 1; channel <= CRSF_CHANNEL_COUNT; ++channel) {
        const uint16_t bit = static_cast<uint16_t>(1U << (channel - 1));
        if ((channels & bit) == 0) continue;
        const int16_t raw = permilleFromCrsfValue(outputs.requested(channel));
        const int16_t safeRaw = permilleFromCrsfValue(outputs.safe(channel));
        const ProcessedProportionalChannel processed =
            processConfiguredProportionalChannel(
                configuration, channel, raw, safeRaw, slew, nowMs,
                advanceSlew);
        outputs.setRequested(
            channel, crsfValueFromPermille(processed.requestedPermille));
        outputs.setSafe(channel,
                        crsfValueFromPermille(processed.safePermille));
    }
}

#include "control_configuration.h"

#include <assert.h>
#include <stdio.h>

namespace {

void testEquivalenceIgnoresOnlyRevision()
{
    ControlConfigurationSnapshot left;
    ControlConfigurationSnapshot right;
    left.revision = 3;
    right.revision = 99;
    assert(equivalentControlConfiguration(left, right));
    right.channelProfiles[1].positiveRampMs = 250;
    assert(!equivalentControlConfiguration(left, right));
    right = left;
    right.throttleInput.calibration.centre++;
    assert(!equivalentControlConfiguration(left, right));
    right = left;
    right.proportionalOutputMask = 1U << 7;
    assert(!equivalentControlConfiguration(left, right));
}

void testRoleMaskRejectsInvalidChannels()
{
    ControlConfigurationSnapshot configuration;
    assert(configuredRoleOwnedChannels(configuration) == 0x0003);
    configuration.steeringChannel = 16;
    configuration.throttleChannel = 0;
    assert(configuredRoleOwnedChannels(configuration) == 0x8000);
}

void testWalkModeLimitsOnlyPhysicalForwardAndPreservesProfileSubtrim()
{
    ControlConfigurationSnapshot configuration;
    configuration.throttleChannel = 2;
    configuration.driveMode = DriveMode::Walk;
    configuration.walkLimitPercent = 50;
    configuration.channelProfiles[1].subtrimPercent = 10;
    const int16_t forward = applyConfiguredResponse(configuration, 2, 1000);
    const int16_t reverse = applyConfiguredResponse(configuration, 2, -1000);
    assert(forward == 1100);
    assert(reverse == -900);
    assert(applyConfiguredDriveMode(configuration, forward, 1000) == 600);
    assert(applyConfiguredDriveMode(configuration, reverse, -1000) == -900);

    configuration.channelProfiles[1].reversed = true;
    const int16_t reversedForward =
        applyConfiguredResponse(configuration, 2, 1000);
    assert(reversedForward == -900);
    assert(applyConfiguredDriveMode(configuration, reversedForward, 1000) == -400);
}

void testChannelAdjustmentsAndNeutralAreSnapshotOwned()
{
    ControlConfigurationSnapshot configuration;
    configuration.throttleChannel = 4;
    configuration.channelProfiles[3].subtrimPercent = 5;
    configuration.channelParameters[3].subtrimPermille = 30;
    configuration.channelParameters[3].trimPermille = -20;
    assert(configuredChannelNeutral(configuration, 4) == 60);
    configuration.channelProfiles[0].subtrimPercent = -5;
    configuration.channelParameters[0].trimPermille = 20;
    assert(configuredChannelNeutral(configuration, 1) == -30);
    assert(applyConfiguredChannelAdjustments(configuration, 4, 1190) == 1000);
    assert(applyConfiguredChannelAdjustments(configuration, 0, 123) == 123);
}

void testSlewRuntimeIsIndependentPerChannelAndResettable()
{
    ControlConfigurationSnapshot configuration;
    configuration.channelProfiles[1].positiveRampMs = 1000;
    configuration.channelProfiles[1].negativeRampMs = 500;
    ChannelSlewRuntime slew;
    slew.reset(100);
    assert(slew.apply(configuration, 2, 1000, 200) == 100);
    assert(slew.apply(configuration, 2, 1000, 300) == 200);
    assert(slew.apply(configuration, 2, -1000, 400) == 0);
    // CH3 has no ramp and therefore follows directly without sharing CH2 state.
    assert(slew.apply(configuration, 3, 700, 400) == 700);
    slew.reset(500);
    assert(slew.apply(configuration, 2, 1000, 600) == 100);

    // Unsigned elapsed-time arithmetic remains correct across millis() wrap.
    slew.reset(UINT32_MAX - 50U);
    assert(slew.apply(configuration, 2, 1000, 49) == 100);
}

void testUnifiedProcessorAppliesResponseRoleModifierAdjustmentAndSlew()
{
    ControlConfigurationSnapshot configuration;
    configuration.throttleChannel = 2;
    configuration.driveMode = DriveMode::Walk;
    configuration.walkLimitPercent = 50;
    configuration.channelProfiles[1].positiveRampMs = 1000;
    configuration.channelParameters[1].trimPermille = 100;
    ChannelSlewRuntime slew;
    slew.reset(100);

    const ProcessedProportionalChannel cut =
        processConfiguredProportionalChannel(
            configuration, 2, 1000, 0, slew, 200, false);
    assert(cut.requestedPermille == 600);
    assert(cut.safePermille == 100);

    // Bypassing slew during CUT leaves its state at neutral. The first live
    // update therefore advances by exactly one 100 ms step.
    const ProcessedProportionalChannel live =
        processConfiguredProportionalChannel(
            configuration, 2, 1000, 0, slew, 200, true);
    assert(live.requestedPermille == 100);
    assert(live.safePermille == 100);

    // Steering uses the same response and adjustment stages, without the
    // throttle-only Walk modifier.
    configuration.channelProfiles[0].positiveRatePercent = 50;
    configuration.channelParameters[0].trimPermille = 100;
    const ProcessedProportionalChannel steering =
        processConfiguredProportionalChannel(
            configuration, 1, 1000, 0, slew, 200, false);
    assert(steering.requestedPermille == 600);
    assert(steering.safePermille == 100);
}

void testProportionalMaskUsesUnifiedProcessorAndHonoursRoleReservation()
{
    ControlConfigurationSnapshot configuration;
    configuration.proportionalOutputMask =
        static_cast<uint16_t>((1U << 1) | (1U << 2));
    configuration.channelProfiles[2].positiveRatePercent = 50;
    configuration.channelParameters[2].trimPermille = 100;
    CrsfChannelOutputModel outputs;
    outputs.configure(2, crsfValueFromPermille(1000), CRSF_CHANNEL_MID);
    outputs.configure(3, crsfValueFromPermille(1000), CRSF_CHANNEL_MID);
    ChannelSlewRuntime slew;
    slew.reset(100);
    processConfiguredProportionalOutputs(configuration, outputs, slew, 104,
                                         true,
                                         static_cast<uint16_t>(1U << 1));
    assert(outputs.requested(2) == crsfValueFromPermille(1000));
    assert(outputs.requested(3) == crsfValueFromPermille(600));
    assert(outputs.safe(3) == crsfValueFromPermille(100));
}

} // namespace

int main()
{
    testEquivalenceIgnoresOnlyRevision();
    testRoleMaskRejectsInvalidChannels();
    testWalkModeLimitsOnlyPhysicalForwardAndPreservesProfileSubtrim();
    testChannelAdjustmentsAndNeutralAreSnapshotOwned();
    testSlewRuntimeIsIndependentPerChannelAndResettable();
    testUnifiedProcessorAppliesResponseRoleModifierAdjustmentAndSlew();
    testProportionalMaskUsesUnifiedProcessorAndHonoursRoleReservation();
    puts("control_configuration_test: PASS");
    return 0;
}

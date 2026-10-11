#include "output_safety.h"

#include "crsf_channel_output.h"

#include <assert.h>
#include <stdio.h>

namespace {

void testOnlyFreshArmedControlCanTransmitRequestedValues()
{
    const OutputSafetyDecision armed =
        arbitrateOutputSafety(OutputSafetyStatus::Armed, true);
    assert(armed.status == OutputSafetyStatus::Armed);
    assert(armed.transmitRequested);

    const OutputSafetyDecision cut =
        arbitrateOutputSafety(OutputSafetyStatus::Cut, true);
    assert(cut.status == OutputSafetyStatus::Cut);
    assert(!cut.transmitRequested);
}

void testStaleControlOverridesEveryControlStatus()
{
    const OutputSafetyStatus statuses[] = {
        OutputSafetyStatus::Cut,
        OutputSafetyStatus::StartupWarning,
        OutputSafetyStatus::LinkRequired,
        OutputSafetyStatus::NeutralRequired,
        OutputSafetyStatus::Armed,
        OutputSafetyStatus::Calibration,
        OutputSafetyStatus::InputFault,
        OutputSafetyStatus::ControlStale,
        OutputSafetyStatus::ChannelDisabled,
        OutputSafetyStatus::ConfigurationFault,
    };
    for (OutputSafetyStatus status : statuses) {
        const OutputSafetyDecision decision =
            arbitrateOutputSafety(status, false);
        assert(decision.status == OutputSafetyStatus::ControlStale);
        assert(!decision.transmitRequested);
    }
}

void testDecisionSelectsCompleteRequestedOrSafeModel()
{
    CrsfChannelOutputModel model;
    assert(model.configure(1, 1200, 800));
    assert(model.configure(2, 1500, 992));
    uint16_t channels[CRSF_CHANNEL_COUNT];

    const OutputSafetyDecision armed =
        arbitrateOutputSafety(OutputSafetyStatus::Armed, true);
    model.resolve(armed.transmitRequested, channels);
    assert(channels[0] == 1200);
    assert(channels[1] == 1500);

    const OutputSafetyDecision stale =
        arbitrateOutputSafety(OutputSafetyStatus::Armed, false);
    model.resolve(stale.transmitRequested, channels);
    assert(channels[0] == 800);
    assert(channels[1] == 992);
}

} // namespace

int main()
{
    testOnlyFreshArmedControlCanTransmitRequestedValues();
    testStaleControlOverridesEveryControlStatus();
    testDecisionSelectsCompleteRequestedOrSafeModel();
    puts("output_safety_test: PASS");
    return 0;
}

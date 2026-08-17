#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "channel_ramp.h"

namespace {

struct LegacyRampState {
    int16_t output = 0;
    uint32_t lastUpdateMs = 0;
};

int16_t legacyUpdate(LegacyRampState &state, int16_t targetPermille,
                     uint32_t nowMs, uint16_t negativeRampMs,
                     uint16_t positiveRampMs)
{
    if (state.lastUpdateMs == 0) {
        state.lastUpdateMs = nowMs;
        state.output = 0;
    }
    const uint32_t elapsedRaw = nowMs - state.lastUpdateMs;
    const uint32_t elapsedMs = elapsedRaw < 100 ? elapsedRaw : 100;
    state.lastUpdateMs = nowMs;
    const bool positiveSide = targetPermille > 0 ||
        (targetPermille == 0 && state.output > 0);
    const uint16_t travelMs = positiveSide ? positiveRampMs : negativeRampMs;
    if (travelMs == 0 || elapsedMs == 0) {
        state.output = targetPermille;
        return state.output;
    }
    int32_t maximumStep =
        (1000L * static_cast<int32_t>(elapsedMs) + travelMs - 1) / travelMs;
    if (maximumStep < 1) maximumStep = 1;
    int32_t delta = static_cast<int32_t>(targetPermille) - state.output;
    if (delta > maximumStep) delta = maximumStep;
    if (delta < -maximumStep) delta = -maximumStep;
    state.output += static_cast<int16_t>(delta);
    return state.output;
}

void testFirstUseAdoptsTargetImmediately()
{
    ChannelRampRuntime ramp;
    assert(ramp.update(750, 1000, 1000, 1000) == 750);
    assert(ramp.initialized());
    assert(ramp.lastUpdateMs() == 1000);
}

void testResetStartsRampFromExplicitSafeValue()
{
    ChannelRampRuntime ramp;
    ramp.reset(0, 1000);
    assert(ramp.update(1000, 1010, 1000, 1000) == 10);
    assert(ramp.update(1000, 1020, 1000, 1000) == 20);
}

void testPositiveAndNegativeRampTimesAreIndependent()
{
    ChannelRampRuntime ramp;
    ramp.reset(0, 1000);
    assert(ramp.update(1000, 1100, 2000, 1000) == 100);
    assert(ramp.update(-1000, 1200, 2000, 1000) == 50);
    assert(ramp.update(-1000, 1300, 2000, 1000) == 0);
    assert(ramp.update(-1000, 1400, 2000, 1000) == -50);
}

void testReturnToNeutralUsesCurrentSideRamp()
{
    ChannelRampRuntime positive;
    positive.reset(500, 1000);
    assert(positive.update(0, 1100, 2000, 1000) == 400);

    ChannelRampRuntime negative;
    negative.reset(-500, 1000);
    assert(negative.update(0, 1100, 2000, 1000) == -450);
}

void testElapsedTimeIsCappedAtOneHundredMilliseconds()
{
    ChannelRampRuntime ramp;
    ramp.reset(0, 1000);
    assert(ramp.update(1000, 5000, 1000, 1000) == 100);
}

void testSubMillisecondEquivalentStillMakesMinimumProgress()
{
    ChannelRampRuntime ramp;
    ramp.reset(0, 1000);
    assert(ramp.update(1000, 1001, 2000, 2000) == 1);
}

void testSameTimestampPreservesLegacyImmediateTarget()
{
    ChannelRampRuntime ramp;
    ramp.reset(0, 1000);
    assert(ramp.update(600, 1000, 1000, 1000) == 600);
}

void testZeroRampIsImmediate()
{
    ChannelRampRuntime ramp;
    ramp.reset(0, 1000);
    assert(ramp.update(-900, 1010, 0, 0) == -900);
}

void testMillisRolloverUsesUnsignedElapsedTime()
{
    ChannelRampRuntime ramp;
    ramp.reset(0, UINT32_MAX - 4);
    // Ten milliseconds elapsed across rollover: (5 - (UINT32_MAX - 4)).
    assert(ramp.update(1000, 5, 1000, 1000) == 10);
}

void testSequenceMatchesExtractedLegacyAlgorithm()
{
    struct Sample {
        uint32_t nowMs;
        int16_t target;
        uint16_t negativeRampMs;
        uint16_t positiveRampMs;
    };
    const Sample samples[] = {
        {904, 1000, 1800, 700},
        {908, 1000, 1800, 700},
        {912, 450, 1800, 700},
        {916, 0, 1800, 700},
        {920, -1000, 1800, 700},
        {924, -1000, 1800, 700},
        {1028, -200, 1800, 700},
        {1032, 0, 1800, 700},
        {1036, 850, 0, 0},
    };

    LegacyRampState legacy;
    legacy.output = 0;
    legacy.lastUpdateMs = 900;
    ChannelRampRuntime extracted;
    extracted.reset(0, 900);
    for (const Sample &sample : samples) {
        const int16_t expected = legacyUpdate(
            legacy, sample.target, sample.nowMs,
            sample.negativeRampMs, sample.positiveRampMs);
        assert(extracted.update(sample.target, sample.nowMs,
                                sample.negativeRampMs,
                                sample.positiveRampMs) == expected);
    }
}

} // namespace

int main()
{
    testFirstUseAdoptsTargetImmediately();
    testResetStartsRampFromExplicitSafeValue();
    testPositiveAndNegativeRampTimesAreIndependent();
    testReturnToNeutralUsesCurrentSideRamp();
    testElapsedTimeIsCappedAtOneHundredMilliseconds();
    testSubMillisecondEquivalentStillMakesMinimumProgress();
    testSameTimestampPreservesLegacyImmediateTarget();
    testZeroRampIsImmediate();
    testMillisRolloverUsesUnsignedElapsedTime();
    testSequenceMatchesExtractedLegacyAlgorithm();
    puts("channel_ramp_test: PASS");
    return 0;
}

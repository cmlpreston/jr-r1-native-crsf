#include <assert.h>
#include <stdio.h>

#include "channel_response.h"

namespace {

void testDefaultProfileIsLinear()
{
    const ChannelResponseProfile profile;
    assert(applyChannelResponse(profile, -1000) == -1000);
    assert(applyChannelResponse(profile, -500) == -500);
    assert(applyChannelResponse(profile, 0) == 0);
    assert(applyChannelResponse(profile, 500) == 500);
    assert(applyChannelResponse(profile, 1000) == 1000);
}

void testPositiveAndNegativeSidesAreIndependent()
{
    ChannelResponseProfile profile;
    profile.negativeRatePercent = 80;
    profile.negativeEndpointPercent = 50;
    profile.positiveRatePercent = 75;
    profile.positiveEndpointPercent = 80;
    assert(applyChannelResponse(profile, -1000) == -400);
    assert(applyChannelResponse(profile, 1000) == 600);
}

void testExpoMatchesExistingResponseShape()
{
    ChannelResponseProfile profile;
    profile.positiveExpoPercent = 50;
    profile.negativeExpoPercent = -50;
    assert(applyChannelResponse(profile, 500) == 313);
    assert(applyChannelResponse(profile, -500) == -687);
}

void testReversalDoesNotExchangePhysicalSideSettings()
{
    ChannelResponseProfile profile;
    profile.negativeEndpointPercent = 50;
    profile.positiveEndpointPercent = 80;
    profile.reversed = true;
    assert(applyChannelResponse(profile, -1000) == 500);
    assert(applyChannelResponse(profile, 1000) == -800);
}

void testSubtrimTranslatesAndClampsExtendedEnvelope()
{
    ChannelResponseProfile profile;
    profile.subtrimPercent = 20;
    assert(applyChannelResponse(profile, 0) == 200);
    assert(applyChannelResponse(profile, 1000) == 1200);
    profile.reversed = true;
    assert(applyChannelResponse(profile, -1000) == 1200);
}

void testSanitiseRejectsPersistedOutOfRangeValues()
{
    ChannelResponseProfile profile;
    profile.negativeEndpointPercent = 0;
    profile.positiveEndpointPercent = 255;
    profile.negativeRatePercent = 255;
    profile.negativeExpoPercent = -120;
    profile.positiveExpoPercent = 120;
    profile.negativeRampMs = 65000;
    profile.subtrimPercent = 100;
    sanitizeChannelResponseProfile(profile);
    assert(profile.negativeEndpointPercent == 50);
    assert(profile.positiveEndpointPercent == 100);
    assert(profile.negativeRatePercent == 100);
    assert(profile.negativeExpoPercent == -100);
    assert(profile.positiveExpoPercent == 100);
    assert(profile.negativeRampMs == 2000);
    assert(profile.subtrimPercent == 20);
}

} // namespace

int main()
{
    testDefaultProfileIsLinear();
    testPositiveAndNegativeSidesAreIndependent();
    testExpoMatchesExistingResponseShape();
    testReversalDoesNotExchangePhysicalSideSettings();
    testSubtrimTranslatesAndClampsExtendedEnvelope();
    testSanitiseRejectsPersistedOutOfRangeValues();
    puts("channel_response_test: PASS");
    return 0;
}


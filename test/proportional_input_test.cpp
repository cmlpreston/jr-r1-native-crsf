#include <assert.h>
#include <stdio.h>

#include "proportional_input.h"

namespace {

void testCalibrationRejectsSingleSampleSpike()
{
    ProportionalInput input({1000, 2000, 3000});
    input.startCalibration(2000);
    input.updateCalibration(0, 4);

    const ProportionalCalibrationProgress progress = input.calibrationProgress();
    assert(progress.live > 1700);
    assert(progress.minimum == 2000);
    assert(!progress.sawMinimum);
}

void testCalibrationSavesFilteredEndpointsAndCentre()
{
    ProportionalInput input({1000, 2000, 3000});
    input.startCalibration(2000);

    uint32_t now = 0;
    for (int i = 0; i < 80; ++i) {
        now += 10;
        input.updateCalibration(static_cast<uint16_t>(1000 + (i % 3) * 6), now);
    }
    for (int i = 0; i < 80; ++i) {
        now += 10;
        input.updateCalibration(static_cast<uint16_t>(3000 - (i % 3) * 6), now);
    }

    bool completed = false;
    for (int i = 0; i < 300 && !completed; ++i) {
        now += 10;
        const uint16_t noisyCentre = (i & 1) ? 1985 : 2015;
        completed = input.updateCalibration(noisyCentre, now);
    }

    assert(completed);
    const ProportionalCalibration &calibration = input.calibration();
    assert(calibration.minimum >= 995 && calibration.minimum <= 1020);
    assert(calibration.centre >= 1990 && calibration.centre <= 2010);
    assert(calibration.maximum >= 2980 && calibration.maximum <= 3005);
}

} // namespace

int main()
{
    testCalibrationRejectsSingleSampleSpike();
    testCalibrationSavesFilteredEndpointsAndCentre();
    puts("proportional_input_test: PASS");
    return 0;
}

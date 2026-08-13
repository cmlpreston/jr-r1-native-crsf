#include "crsf_channel_output.h"

#include <assert.h>
#include <stdio.h>

namespace {

void unpack(const uint8_t payload[CRSF_PAYLOAD_BYTES],
            uint16_t channels[CRSF_CHANNEL_COUNT])
{
    uint32_t accumulator = 0;
    uint8_t bits = 0;
    size_t input = 0;
    for (size_t channel = 0; channel < CRSF_CHANNEL_COUNT; ++channel) {
        while (bits < 11) {
            accumulator |= static_cast<uint32_t>(payload[input++]) << bits;
            bits += 8;
        }
        channels[channel] = static_cast<uint16_t>(accumulator & 0x07ffU);
        accumulator >>= 11;
        bits -= 11;
    }
}

void testDefaultsAreSafeAndCentred()
{
    CrsfChannelOutputModel model;
    uint16_t channels[CRSF_CHANNEL_COUNT];
    model.resolve(true, channels);
    assert(model.enabledMask() == 0);
    for (uint16_t value : channels) assert(value == CRSF_CHANNEL_MID);
}

void testLiveAndSafeResolution()
{
    CrsfChannelOutputModel model;
    assert(model.configure(2, CRSF_CHANNEL_MAX, CRSF_CHANNEL_MID));
    assert(model.configure(5, 1200, 800));
    assert(!model.configure(0, 1000, 1000));
    assert(!model.configure(17, 1000, 1000));

    uint16_t channels[CRSF_CHANNEL_COUNT];
    model.resolve(true, channels);
    assert(channels[1] == CRSF_CHANNEL_MAX);
    assert(channels[4] == 1200);
    assert(channels[0] == CRSF_CHANNEL_MID);

    model.resolve(false, channels);
    assert(channels[1] == CRSF_CHANNEL_MID);
    assert(channels[4] == 800);

    assert(model.setEnabled(5, false));
    model.resolve(true, channels);
    assert(channels[4] == 800);
}

void testAllSixteenChannelsPackAndFrame()
{
    CrsfChannelOutputModel model;
    for (uint8_t channel = 1; channel <= CRSF_CHANNEL_COUNT; ++channel)
        assert(model.configure(channel, channel * 100, CRSF_CHANNEL_MID));

    uint8_t frame[CRSF_FRAME_BYTES];
    buildCrsfChannelsFrame(model, true, frame);
    assert(frame[0] == CRSF_SYNC_BYTE);
    assert(frame[1] == 24);
    assert(frame[2] == CRSF_FRAMETYPE_RC_CHANNELS_PACKED);
    assert(frame[25] == crc8DvbS2(&frame[2], 23));

    uint16_t channels[CRSF_CHANNEL_COUNT];
    unpack(&frame[3], channels);
    for (uint8_t channel = 1; channel <= CRSF_CHANNEL_COUNT; ++channel)
        assert(channels[channel - 1] == channel * 100);

    buildCrsfChannelsFrame(model, false, frame);
    unpack(&frame[3], channels);
    for (uint16_t value : channels) assert(value == CRSF_CHANNEL_MID);
}

void testValuesAreClampedToElevenBits()
{
    CrsfChannelOutputModel model;
    assert(model.configure(1, 5000, 6000));
    assert(model.requested(1) == CRSF_CHANNEL_ABSOLUTE_MAX);
    assert(model.safe(1) == CRSF_CHANNEL_ABSOLUTE_MAX);
}

} // namespace

int main()
{
    testDefaultsAreSafeAndCentred();
    testLiveAndSafeResolution();
    testAllSixteenChannelsPackAndFrame();
    testValuesAreClampedToElevenBits();
    puts("crsf_channel_output_test: PASS");
    return 0;
}

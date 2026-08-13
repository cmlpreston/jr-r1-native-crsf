#include "crsf_channel_output.h"

#include <string.h>

uint16_t crsfValueFromPermille(int16_t value)
{
    if (value < -1000) value = -1000;
    if (value > 1000) value = 1000;
    int32_t mapped;
    if (value <= 0) {
        mapped = CRSF_CHANNEL_MID + static_cast<int32_t>(value) *
                 (CRSF_CHANNEL_MID - CRSF_CHANNEL_MIN) / 1000;
    } else {
        mapped = CRSF_CHANNEL_MID + static_cast<int32_t>(value) *
                 (CRSF_CHANNEL_MAX - CRSF_CHANNEL_MID) / 1000;
    }
    return static_cast<uint16_t>(mapped);
}

CrsfChannelOutputModel::CrsfChannelOutputModel()
{
    reset();
}

void CrsfChannelOutputModel::reset()
{
    for (size_t index = 0; index < CRSF_CHANNEL_COUNT; ++index) {
        requested_[index] = CRSF_CHANNEL_MID;
        safe_[index] = CRSF_CHANNEL_MID;
    }
    enabledMask_ = 0;
}

bool CrsfChannelOutputModel::validChannel(uint8_t channel)
{
    return channel >= 1 && channel <= CRSF_CHANNEL_COUNT;
}

uint16_t CrsfChannelOutputModel::clampValue(uint16_t value)
{
    return value > CRSF_CHANNEL_ABSOLUTE_MAX
        ? CRSF_CHANNEL_ABSOLUTE_MAX : value;
}

bool CrsfChannelOutputModel::configure(uint8_t channel, uint16_t requestedValue,
                                       uint16_t safeValue, bool isEnabled)
{
    if (!validChannel(channel)) return false;
    const size_t index = channel - 1;
    requested_[index] = clampValue(requestedValue);
    safe_[index] = clampValue(safeValue);
    return setEnabled(channel, isEnabled);
}

bool CrsfChannelOutputModel::setRequested(uint8_t channel, uint16_t value)
{
    if (!validChannel(channel)) return false;
    requested_[channel - 1] = clampValue(value);
    return true;
}

bool CrsfChannelOutputModel::setSafe(uint8_t channel, uint16_t value)
{
    if (!validChannel(channel)) return false;
    safe_[channel - 1] = clampValue(value);
    return true;
}

bool CrsfChannelOutputModel::setEnabled(uint8_t channel, bool isEnabled)
{
    if (!validChannel(channel)) return false;
    const uint16_t bit = static_cast<uint16_t>(1U << (channel - 1));
    if (isEnabled) enabledMask_ |= bit;
    else enabledMask_ &= static_cast<uint16_t>(~bit);
    return true;
}

uint16_t CrsfChannelOutputModel::requested(uint8_t channel) const
{
    return validChannel(channel) ? requested_[channel - 1] : CRSF_CHANNEL_MID;
}

uint16_t CrsfChannelOutputModel::safe(uint8_t channel) const
{
    return validChannel(channel) ? safe_[channel - 1] : CRSF_CHANNEL_MID;
}

bool CrsfChannelOutputModel::enabled(uint8_t channel) const
{
    if (!validChannel(channel)) return false;
    return (enabledMask_ & static_cast<uint16_t>(1U << (channel - 1))) != 0;
}

void CrsfChannelOutputModel::resolve(bool live,
                                     uint16_t channels[CRSF_CHANNEL_COUNT]) const
{
    for (size_t index = 0; index < CRSF_CHANNEL_COUNT; ++index) {
        const bool outputEnabled =
            (enabledMask_ & static_cast<uint16_t>(1U << index)) != 0;
        channels[index] = live && outputEnabled
            ? requested_[index] : safe_[index];
    }
}

uint8_t crc8DvbS2(const uint8_t *data, size_t length)
{
    uint8_t crc = 0;
    while (length-- != 0) {
        crc ^= *data++;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80U) ? static_cast<uint8_t>((crc << 1U) ^ 0xD5U)
                                : static_cast<uint8_t>(crc << 1U);
        }
    }
    return crc;
}

void packCrsfChannels(const uint16_t channels[CRSF_CHANNEL_COUNT],
                      uint8_t payload[CRSF_PAYLOAD_BYTES])
{
    memset(payload, 0, CRSF_PAYLOAD_BYTES);
    uint32_t accumulator = 0;
    uint8_t bitsInAccumulator = 0;
    size_t outputIndex = 0;

    for (size_t channel = 0; channel < CRSF_CHANNEL_COUNT; ++channel) {
        accumulator |= static_cast<uint32_t>(
            channels[channel] & CRSF_CHANNEL_ABSOLUTE_MAX) << bitsInAccumulator;
        bitsInAccumulator += 11;
        while (bitsInAccumulator >= 8) {
            payload[outputIndex++] = static_cast<uint8_t>(accumulator & 0xFFU);
            accumulator >>= 8;
            bitsInAccumulator -= 8;
        }
    }
}

void buildCrsfChannelsFrame(const CrsfChannelOutputModel &model, bool live,
                            uint8_t frame[CRSF_FRAME_BYTES])
{
    uint16_t channels[CRSF_CHANNEL_COUNT];
    model.resolve(live, channels);

    frame[0] = CRSF_SYNC_BYTE;
    frame[1] = 24; // type + 22-byte channel payload + CRC
    frame[2] = CRSF_FRAMETYPE_RC_CHANNELS_PACKED;
    packCrsfChannels(channels, &frame[3]);
    frame[25] = crc8DvbS2(&frame[2], 23);
}

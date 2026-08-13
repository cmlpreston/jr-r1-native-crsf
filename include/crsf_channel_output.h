#pragma once

#include <stddef.h>
#include <stdint.h>

constexpr uint8_t CRSF_SYNC_BYTE = 0xC8;
constexpr uint8_t CRSF_FRAMETYPE_RC_CHANNELS_PACKED = 0x16;
constexpr uint16_t CRSF_CHANNEL_MIN = 172;
constexpr uint16_t CRSF_CHANNEL_MID = 992;
constexpr uint16_t CRSF_CHANNEL_MAX = 1811;
constexpr uint16_t CRSF_CHANNEL_ABSOLUTE_MIN = 0;
constexpr uint16_t CRSF_CHANNEL_ABSOLUTE_MAX = 2047;
constexpr size_t CRSF_CHANNEL_COUNT = 16;
constexpr size_t CRSF_PAYLOAD_BYTES = 22;
constexpr size_t CRSF_FRAME_BYTES = 26;

// Requested and safe values are kept separately so CUT/stale-input handling
// can atomically substitute all sixteen outputs without destroying the values
// that will resume after a deliberate re-arm. Channels are numbered 1..16 at
// this API boundary to match the transmitter UI and receiver documentation.
class CrsfChannelOutputModel {
public:
    CrsfChannelOutputModel();

    void reset();
    bool configure(uint8_t channel, uint16_t requested, uint16_t safe,
                   bool enabled = true);
    bool setRequested(uint8_t channel, uint16_t value);
    bool setSafe(uint8_t channel, uint16_t value);
    bool setEnabled(uint8_t channel, bool enabled);

    uint16_t requested(uint8_t channel) const;
    uint16_t safe(uint8_t channel) const;
    bool enabled(uint8_t channel) const;
    uint16_t enabledMask() const { return enabledMask_; }

    // Disabled channels always resolve to their safe value. This prevents a
    // stale retained value from escaping merely because an output is disabled.
    void resolve(bool live, uint16_t channels[CRSF_CHANNEL_COUNT]) const;

private:
    static bool validChannel(uint8_t channel);
    static uint16_t clampValue(uint16_t value);

    uint16_t requested_[CRSF_CHANNEL_COUNT];
    uint16_t safe_[CRSF_CHANNEL_COUNT];
    uint16_t enabledMask_ = 0;
};

uint8_t crc8DvbS2(const uint8_t *data, size_t length);
void packCrsfChannels(const uint16_t channels[CRSF_CHANNEL_COUNT],
                      uint8_t payload[CRSF_PAYLOAD_BYTES]);
void buildCrsfChannelsFrame(const CrsfChannelOutputModel &model, bool live,
                            uint8_t frame[CRSF_FRAME_BYTES]);

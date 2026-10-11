#pragma once
#include "control_configuration.h"
#include <stddef.h>

// Channel settings have a model owner independent of pages and LVGL objects.
// Runtime/filter/slew state is deliberately absent.
struct ChannelModelController {
    ChannelResponseProfile responses[CRSF_CHANNEL_COUNT] = {};
    ChannelParameterValues parameters[CRSF_CHANNEL_COUNT] = {};
    ChannelMixSettings mixing[CRSF_CHANNEL_COUNT] = {};
    bool setResponse(uint8_t channel, ChannelResponseProfile profile) {
        if (channel < 1 || channel > CRSF_CHANNEL_COUNT) return false;
        sanitizeChannelResponseProfile(profile);
        responses[channel-1] = profile;
        return true;
    }
    void copyTo(ControlConfigurationSnapshot &snapshot) const {
        for (size_t n = 0; n < CRSF_CHANNEL_COUNT; ++n) {
            snapshot.channelProfiles[n] = responses[n];
            snapshot.channelParameters[n] = parameters[n];
            snapshot.mixing[n] = mixing[n];
        }
    }
};

// Stable, field-by-field, little-endian encoding. Reserved/padding bytes and
// C++ bool representation never enter storage. Decode is transactional.
constexpr size_t kChannelModelRecordSize = 10 + 54 * CRSF_CHANNEL_COUNT;
bool encodeChannelModel(const ChannelModelController &model, uint8_t *data, size_t size);
bool decodeChannelModel(const uint8_t *data, size_t size, ChannelModelController &model);

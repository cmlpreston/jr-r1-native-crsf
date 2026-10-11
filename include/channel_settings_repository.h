#pragma once
#include "channel_model.h"

enum class ChannelSettingsLoad { Missing, Loaded, Invalid };
class ChannelSettingsRepository {
public:
    static ChannelSettingsLoad load(uint8_t slot, ChannelModelController &model);
    static bool save(uint8_t slot, const ChannelModelController &model);
    static ChannelSettingsLoad loadResponses(uint8_t slot, ChannelModelController &model);
    static ChannelSettingsLoad loadParameters(uint8_t slot, ChannelModelController &model);
    static ChannelSettingsLoad loadMixing(uint8_t slot, ChannelModelController &model);
};

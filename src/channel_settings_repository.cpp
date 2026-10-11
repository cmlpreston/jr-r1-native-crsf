#include "channel_settings_repository.h"
#include <Preferences.h>
#include <cstdio>
#include <cstring>

namespace {
void storageNamespace(uint8_t slot, char (&name)[16]) {
    snprintf(name, sizeof(name), "r1-channels-%u", unsigned(slot));
}
const char *legacyNamespace(uint8_t slot, const char *legacy, const char *suffix, char (&name)[16]) {
    if (!slot) return legacy;
    snprintf(name, sizeof(name), "r1m%u-%s", unsigned(slot)+1, suffix); return name;
}
}
ChannelSettingsLoad ChannelSettingsRepository::load(uint8_t slot, ChannelModelController &model) {
    Preferences p; char name[16]; storageNamespace(slot,name);
    if (!p.begin(name,true)) return ChannelSettingsLoad::Missing;
    const auto size = p.getBytesLength("record-v1");
    if (!size) {
        const bool present = p.isKey("record-v1");
        p.end();
        return present ? ChannelSettingsLoad::Invalid : ChannelSettingsLoad::Missing;
    }
    uint8_t data[kChannelModelRecordSize];
    const bool ok = size == sizeof(data) && p.getBytes("record-v1",data,sizeof(data)) == sizeof(data);
    p.end();
    return ok && decodeChannelModel(data,sizeof(data),model) ?
        ChannelSettingsLoad::Loaded : ChannelSettingsLoad::Invalid;
}
bool ChannelSettingsRepository::save(uint8_t slot, const ChannelModelController &model) {
    uint8_t data[kChannelModelRecordSize], check[kChannelModelRecordSize];
    if (!encodeChannelModel(model,data,sizeof(data))) return false;
    Preferences p; char name[16]; storageNamespace(slot,name);
    if (!p.begin(name,false)) return false;
    // A model switch or several coalesced edits must not rewrite identical NVS data.
    if (p.getBytesLength("record-v1") == sizeof(check) &&
        p.getBytes("record-v1",check,sizeof(check)) == sizeof(check) && !memcmp(data,check,sizeof(data))) {
        p.end(); return true;
    }
    const bool ok = p.putBytes("record-v1",data,sizeof(data)) == sizeof(data) &&
        p.getBytesLength("record-v1") == sizeof(check) &&
        p.getBytes("record-v1",check,sizeof(check)) == sizeof(check) && !memcmp(data,check,sizeof(data));
    p.end(); return ok;
}

ChannelSettingsLoad ChannelSettingsRepository::loadResponses(uint8_t slot, ChannelModelController &model) {
    ChannelModelController saved;
    const auto state = load(slot,saved);
    if (state == ChannelSettingsLoad::Loaded) memcpy(model.responses,saved.responses,sizeof(saved.responses));
    if (state != ChannelSettingsLoad::Missing) return state;
    Preferences p; char name[16];
    if (p.begin(legacyNamespace(slot,"jr-r1-response","response",name),true)) {
        if (p.getUShort("version",0) == 1 && p.getBytesLength("profiles") == sizeof(model.responses))
            p.getBytes("profiles",model.responses,sizeof(model.responses));
        p.end();
    }
    for (auto &profile : model.responses) sanitizeChannelResponseProfile(profile);
    return state;
}
ChannelSettingsLoad ChannelSettingsRepository::loadParameters(uint8_t slot, ChannelModelController &model) {
    ChannelModelController saved;
    const auto state = load(slot,saved);
    if (state == ChannelSettingsLoad::Loaded) memcpy(model.parameters,saved.parameters,sizeof(saved.parameters));
    if (state != ChannelSettingsLoad::Missing) return state;
    for (auto &values : model.parameters) values = ChannelParameterValues{};
    Preferences p; char name[16];
    if (p.begin(legacyNamespace(slot,"jr-r1-chparam","param",name),true)) {
        if (p.getUShort("version",0) == 1 && p.getBytesLength("values") == sizeof(model.parameters))
            p.getBytes("values",model.parameters,sizeof(model.parameters));
        p.end();
    }
    for (auto &v : model.parameters) {
        v.trimPermille = v.trimPermille < -200 ? -200 : v.trimPermille > 200 ? 200 : v.trimPermille;
        v.subtrimPermille = v.subtrimPermille < -200 ? -200 : v.subtrimPermille > 200 ? 200 : v.subtrimPermille;
    }
    return state;
}
ChannelSettingsLoad ChannelSettingsRepository::loadMixing(uint8_t slot, ChannelModelController &model) {
    ChannelModelController saved;
    const auto state = load(slot,saved);
    if (state == ChannelSettingsLoad::Loaded) memcpy(model.mixing,saved.mixing,sizeof(saved.mixing));
    if (state != ChannelSettingsLoad::Missing) return state;
    for (auto &m : model.mixing) m = ChannelMixSettings{};
    Preferences p; char name[16];
    if (!p.begin(legacyNamespace(slot,"jr-r1-mix","mix",name),true)) return state;
    if (p.getBytesLength("v4") == sizeof(model.mixing)) p.getBytes("v4",model.mixing,sizeof(model.mixing));
    else if (p.getBytesLength("v3") == sizeof(ChannelMixSettingsV3)*16) {
        ChannelMixSettingsV3 old[16]; p.getBytes("v3",old,sizeof(old));
        for (int n = 0; n < 16; ++n) model.mixing[n] = migrateChannelMix(old[n]);
    } else if (p.getBytesLength("v2") == sizeof(ChannelMixSettingsV2)*16) {
        ChannelMixSettingsV2 old[16]; p.getBytes("v2",old,sizeof(old));
        for (int n = 0; n < 16; ++n) model.mixing[n] = migrateChannelMix(old[n]);
    } else if (p.getBytesLength("v1") == sizeof(LegacyMixSettings)*16) {
        LegacyMixSettings old[16] = {}; p.getBytes("v1",old,sizeof(old));
        for (int n = 0; n < 16; ++n) model.mixing[n] = migrateChannelMix(old[n]);
    }
    p.end();
    for (auto &m : model.mixing) if (!validChannelMix(m)) m = ChannelMixSettings{};
    return state;
}

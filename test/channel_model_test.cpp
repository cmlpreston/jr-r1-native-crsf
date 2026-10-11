#include "channel_settings_repository.h"
#include <Preferences.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <chrono>

static void same(const ChannelModelController &a, const ChannelModelController &b) {
    ControlConfigurationSnapshot x,y; a.copyTo(x); b.copyTo(y);
    assert(equivalentControlConfiguration(x,y));
}
static ChannelModelController fixture() {
    ChannelModelController m;
    for (unsigned n = 0; n < 16; ++n) {
        auto &p = m.responses[n];
        p.negativeEndpointPercent = 50+n; p.positiveEndpointPercent = 100-n;
        p.negativeRatePercent = n*5; p.positiveRatePercent = 100-n*3;
        p.negativeExpoPercent = -100+int(n)*4; p.positiveExpoPercent = 100-int(n)*4;
        p.negativeRampMs = 0x100+n; p.positiveRampMs = 2000-n;
        p.subtrimPercent = -20+int(n); p.reversed = n%2;
        m.parameters[n].trimPermille = -200+int(n)*20;
        m.parameters[n].subtrimPermille = 200-int(n)*15;
        auto &s = m.mixing[n]; s.constantPercent = int(n)-8;
        s.transitionMs = n*100; s.rateEnabled = n%2;
        auto &r = s.rules[0]; r.enabled = 1; r.source = 0;
        r.weight = -50; r.negativeWeight = 35; r.splitWeight = 1; r.followTrim = 1;
        r.condition = n%5; r.sourceMode = n%3; r.thresholdPercent = -5; r.hysteresisPercent = 3;
    }
    return m;
}
static void testCodec() {
    const auto original = fixture(); ChannelModelController restored;
    uint8_t bytes[kChannelModelRecordSize];
    assert(encodeChannelModel(original,bytes,sizeof(bytes)));
    assert(!memcmp(bytes,"R1CM",4) && bytes[4] == 1);
    assert(bytes[12] == 0 && bytes[13] == 1); // 256ms, little endian
    assert(decodeChannelModel(bytes,sizeof(bytes),restored)); same(original,restored);
    const auto unchanged = restored;
    for (size_t n = 0; n < sizeof(bytes); ++n) {
        bytes[n] ^= 1;
        assert(!decodeChannelModel(bytes,sizeof(bytes),restored)); same(unchanged,restored);
        bytes[n] ^= 1;
    }
    assert(!decodeChannelModel(bytes,sizeof(bytes)-1,restored));
    auto bad = original; bad.responses[0].positiveRampMs = 2001;
    assert(!encodeChannelModel(bad,bytes,sizeof(bytes)));
    bad = original; bad.parameters[0].subtrimPermille = 201;
    assert(!encodeChannelModel(bad,bytes,sizeof(bytes)));
    bad = original; bad.mixing[0].rules[0].source = 19;
    assert(!encodeChannelModel(bad,bytes,sizeof(bytes)));
    assert(!bad.setResponse(0,{}));
    ChannelResponseProfile unsanitized; unsanitized.positiveRatePercent = 200;
    assert(bad.setResponse(2,unsanitized) && bad.responses[1].positiveRatePercent == 100);
}
static void testStorageAndMigration() {
    const auto path = (std::filesystem::temp_directory_path() /
        ("r1-channel-model-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))).string();
    assert(std::filesystem::create_directory(path)); setenv("JR_R1_SIM_STATE",path.c_str(),1);
    auto original = fixture(); ChannelModelController loaded;
    assert(ChannelSettingsRepository::load(0,loaded) == ChannelSettingsLoad::Missing);
    // Legacy response and parameter layout import preserves output math.
    Preferences p; p.begin("jr-r1-response",false); p.putUShort("version",1);
    p.putBytes("profiles",original.responses,sizeof(original.responses)); p.end();
    p.begin("jr-r1-chparam",false); p.putUShort("version",1);
    p.putBytes("values",original.parameters,sizeof(original.parameters)); p.end();
    p.begin("jr-r1-mix",false); p.putBytes("v4",original.mixing,sizeof(original.mixing)); p.end();
    assert(ChannelSettingsRepository::loadResponses(0,loaded) == ChannelSettingsLoad::Missing);
    ChannelSettingsRepository::loadParameters(0,loaded); ChannelSettingsRepository::loadMixing(0,loaded);
    same(original,loaded);
    assert(!std::filesystem::exists(std::filesystem::path(path)/"r1-channels-0")); // read-only migration
    assert(ChannelSettingsRepository::save(0,loaded));
    assert(ChannelSettingsRepository::load(0,loaded) == ChannelSettingsLoad::Loaded); same(original,loaded);
    const auto savedTime = std::filesystem::last_write_time(std::filesystem::path(path)/"r1-channels-0/record-v1");
    assert(ChannelSettingsRepository::save(0,loaded));
    assert(savedTime == std::filesystem::last_write_time(std::filesystem::path(path)/"r1-channels-0/record-v1"));
    assert(ChannelSettingsRepository::load(1,loaded) == ChannelSettingsLoad::Missing);
    // A damaged new record is an error, never permission to reload older settings.
    p.begin("r1-channels-0",false); uint8_t corrupt[kChannelModelRecordSize] = {};
    p.putBytes("record-v1",corrupt,sizeof(corrupt)); p.end();
    assert(ChannelSettingsRepository::loadResponses(0,loaded) == ChannelSettingsLoad::Invalid);
    same(original,loaded);
    p.begin("r1-channels-0",false); p.putBytes("record-v1",corrupt,0); p.end();
    assert(ChannelSettingsRepository::loadResponses(0,loaded) == ChannelSettingsLoad::Invalid);
    assert(ChannelSettingsRepository::loadParameters(0,loaded) == ChannelSettingsLoad::Invalid);
    assert(ChannelSettingsRepository::loadMixing(0,loaded) == ChannelSettingsLoad::Invalid);
    same(original,loaded);
    // Exact historical mixer layouts and model namespaces remain readable.
    ChannelMixSettingsV3 v3[16]; v3[2].constantPercent = 20; v3[2].cutOverride = 1; v3[2].cutPercent = -30;
    p.begin("r1m2-mix",false); p.putBytes("v3",v3,sizeof(v3)); p.end();
    ChannelSettingsRepository::loadMixing(1,loaded);
    assert(loaded.mixing[2].constantPercent == 20 && loaded.mixing[2].cutPercent == -30);
    ChannelMixSettingsV2 v2[16]; v2[1].rules[0].enabled = 1; v2[1].rules[0].weight = -70;
    p.begin("r1m3-mix",false); p.putBytes("v2",v2,sizeof(v2)); p.end();
    ChannelSettingsRepository::loadMixing(2,loaded);
    assert(loaded.mixing[1].rules[0].weight == -70 && !loaded.mixing[1].rules[0].sourceMode);
    LegacyMixSettings v1[16] = {};
    for (auto &m : v1) { m.rateControl = 3; m.lowNegative = m.lowPositive = 50; m.highNegative = m.highPositive = 100;
        for (auto &r : m.rules) { r.control = 3; r.weight = 100; } }
    v1[3].rules[0].enabled = 1; v1[3].rules[0].weight = -40;
    p.begin("r1m4-mix",false); p.putBytes("v1",v1,sizeof(v1)); p.end();
    ChannelSettingsRepository::loadMixing(3,loaded);
    assert(loaded.mixing[3].rules[0].negativeWeight == -40 && loaded.mixing[3].includeBase);
    std::filesystem::remove_all(path);
}
int main() {
    testCodec(); testStorageAndMigration();
    puts("Channel model: explicit encoding, corruption, legacy migration, atomic loads and model isolation PASS");
}

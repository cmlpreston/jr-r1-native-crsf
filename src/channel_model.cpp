#include "channel_model.h"
#include <cstring>

namespace {
uint32_t checksum(const uint8_t *data, size_t size) {
    uint32_t crc = 0xffffffffU;
    for (size_t n = 0; n < size; ++n) {
        crc ^= data[n];
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320U : 0);
    }
    return ~crc;
}
struct Writer {
    uint8_t *p;
    void byte(int value) { *p++ = uint8_t(value); }
    void word(int value) { byte(value); byte(uint16_t(value) >> 8); }
};
struct Reader {
    const uint8_t *p;
    uint8_t byte() { return *p++; }
    int signedByte() { const int n = byte(); return n < 128 ? n : n-256; }
    uint16_t word() { const unsigned low = byte(); return low | unsigned(byte()) << 8; }
    int signedWord() { const int n = word(); return n < 32768 ? n : n-65536; }
};
bool validModel(const ChannelModelController &m) {
    for (size_t n = 0; n < CRSF_CHANNEL_COUNT; ++n) {
        const auto &p = m.responses[n]; const auto &v = m.parameters[n];
        if (p.negativeEndpointPercent < 50 || p.negativeEndpointPercent > 100 ||
            p.positiveEndpointPercent < 50 || p.positiveEndpointPercent > 100 ||
            p.negativeRatePercent > 100 || p.positiveRatePercent > 100 ||
            p.negativeExpoPercent < -100 || p.negativeExpoPercent > 100 ||
            p.positiveExpoPercent < -100 || p.positiveExpoPercent > 100 ||
            p.negativeRampMs > 2000 || p.positiveRampMs > 2000 ||
            p.subtrimPercent < -20 || p.subtrimPercent > 20 ||
            v.trimPermille < -200 || v.trimPermille > 200 ||
            v.subtrimPermille < -200 || v.subtrimPermille > 200) return false;
    }
    return validChannelMixGraph(m.mixing);
}
}

bool encodeChannelModel(const ChannelModelController &m, uint8_t *data, size_t size) {
    if (!data || size != kChannelModelRecordSize || !validModel(m)) return false;
    memcpy(data, "R1CM", 4); data[4] = 1; data[5] = CRSF_CHANNEL_COUNT;
    Writer w{data+6};
    for (size_t n = 0; n < CRSF_CHANNEL_COUNT; ++n) {
        const auto &p = m.responses[n]; const auto &v = m.parameters[n]; const auto &s = m.mixing[n];
        w.byte(p.negativeEndpointPercent); w.byte(p.positiveEndpointPercent);
        w.byte(p.negativeRatePercent); w.byte(p.positiveRatePercent);
        w.byte(p.negativeExpoPercent); w.byte(p.positiveExpoPercent);
        w.word(p.negativeRampMs); w.word(p.positiveRampMs);
        w.byte(p.subtrimPercent); w.byte(p.reversed ? 1 : 0);
        w.word(v.trimPermille); w.word(v.subtrimPermille);
        for (const auto &r : s.rules) {
            w.byte(r.enabled); w.byte(r.source); w.byte(r.weight); w.byte(r.replace);
            w.byte(r.condition); w.byte(r.control); w.byte(r.negativeWeight);
            w.byte(r.splitWeight); w.byte(r.followTrim); w.byte(r.sourceMode);
            w.byte(r.thresholdPercent); w.byte(r.hysteresisPercent);
        }
        w.byte(s.includeBase); w.byte(s.rateEnabled); w.byte(s.rateControl);
        w.byte(s.lowNegative); w.byte(s.lowPositive); w.byte(s.highNegative); w.byte(s.highPositive);
        w.word(s.transitionMs); w.byte(s.constantPercent); w.byte(s.outputMinimum);
        w.byte(s.outputMaximum); w.byte(s.cutOverride); w.byte(s.cutPercent);
    }
    const uint32_t crc = checksum(data, size-4);
    for (int n = 0; n < 4; ++n) w.byte(crc >> (8*n));
    return w.p == data+size;
}
bool decodeChannelModel(const uint8_t *data, size_t size, ChannelModelController &model) {
    if (!data || size != kChannelModelRecordSize || memcmp(data,"R1CM",4) ||
        data[4] != 1 || data[5] != CRSF_CHANNEL_COUNT) return false;
    uint32_t stored = 0;
    for (int n = 0; n < 4; ++n) stored |= uint32_t(data[size-4+n]) << (8*n);
    if (stored != checksum(data,size-4)) return false;
    ChannelModelController m; Reader r{data+6};
    for (size_t n = 0; n < CRSF_CHANNEL_COUNT; ++n) {
        auto &p = m.responses[n]; auto &v = m.parameters[n]; auto &s = m.mixing[n];
        p.negativeEndpointPercent = r.byte(); p.positiveEndpointPercent = r.byte();
        p.negativeRatePercent = r.byte(); p.positiveRatePercent = r.byte();
        p.negativeExpoPercent = r.signedByte(); p.positiveExpoPercent = r.signedByte();
        p.negativeRampMs = r.word(); p.positiveRampMs = r.word(); p.subtrimPercent = r.signedByte();
        const auto reversed = r.byte(); if (reversed > 1) return false; p.reversed = reversed;
        v.trimPermille = r.signedWord(); v.subtrimPermille = r.signedWord();
        for (auto &rule : s.rules) {
            rule.enabled = r.byte(); rule.source = r.byte(); rule.weight = r.signedByte();
            rule.replace = r.byte(); rule.condition = r.byte(); rule.control = r.byte();
            rule.negativeWeight = r.signedByte(); rule.splitWeight = r.byte(); rule.followTrim = r.byte();
            rule.sourceMode = r.byte(); rule.thresholdPercent = r.signedByte(); rule.hysteresisPercent = r.byte();
        }
        s.includeBase = r.byte(); s.rateEnabled = r.byte(); s.rateControl = r.byte();
        s.lowNegative = r.byte(); s.lowPositive = r.byte(); s.highNegative = r.byte(); s.highPositive = r.byte();
        s.transitionMs = r.word(); s.constantPercent = r.signedByte(); s.outputMinimum = r.signedByte();
        s.outputMaximum = r.signedByte(); s.cutOverride = r.byte(); s.cutPercent = r.signedByte();
    }
    if (!validModel(m)) return false;
    model = m; return true;
}

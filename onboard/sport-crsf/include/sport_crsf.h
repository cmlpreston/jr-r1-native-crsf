#pragma once
#include <stddef.h>
#include <stdint.h>

namespace onboard {

struct SportSample {
    uint8_t physicalId;
    uint16_t appId;
    uint32_t value;
};

// Feed de-inverted UART bytes including 0x7e and the polled physical ID.
// A future bus-master driver must feed its own poll when TX echo is disabled.
class SportParser {
public:
    bool feed(uint8_t byte, uint32_t nowMs, SportSample &sample);
    uint32_t checksumErrors() const { return checksumErrors_; }
private:
    uint8_t bytes_[9] = {};
    uint8_t count_ = 0;
    bool active_ = false;
    bool escaped_ = false;
    uint32_t lastByteMs_ = 0;
    uint32_t checksumErrors_ = 0;
};

struct SensorKey {
    uint8_t physicalId;
    uint16_t appId;
    bool matches(const SportSample &s) const;
};

// Deliberately requires explicit sensor selection: never merge multiple packs.
// Only VFAS (0x0210..0x021f) and current (0x0200..0x020f) are supported.
class BatteryBridge {
public:
    BatteryBridge(SensorKey voltage, SensorKey current)
        : voltageKey_(voltage), currentKey_(current) {}
    void accept(const SportSample &sample, uint32_t nowMs);
    // Zero means missing/stale data, rate limited, or insufficient output room.
    // Both measurements must be fresh. Capacity and remaining are unavailable
    // and encoded as zero; callers must not display them as measured values.
    size_t nextFrame(uint32_t nowMs, uint8_t *out, size_t capacity);
private:
    SensorKey voltageKey_, currentKey_;
    uint16_t voltage_ = 0, current_ = 0;
    uint32_t voltageMs_ = 0, currentMs_ = 0, sentMs_ = 0;
    bool haveVoltage_ = false, haveCurrent_ = false, sent_ = false;
};

} // namespace onboard

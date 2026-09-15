#pragma once
#include <stdint.h>

// Session statistics, independent of screen refresh rate. Average integrates
// consecutive fresh GPS samples (trapezoidal rule), including stopped time.
// Missing fixes and gaps over three seconds contribute neither time nor speed.
class SpeedStatistics {
public:
    static constexpr uint32_t FreshnessMs = 3000;
    void reset(uint32_t lastPacketMs) {
        *this = SpeedStatistics{};
        seen_ = true;
        stamp_ = lastPacketMs; // Do not re-count a pre-reset packet.
    }
    void update(uint16_t speed, uint32_t stamp, uint32_t now, bool valid) {
        if (!valid || uint32_t(now - stamp) > FreshnessMs) {
            continuous_ = false;
            return;
        }
        if (seen_ && stamp == stamp_) return;
        const uint32_t elapsed = stamp - stamp_;
        if (continuous_ && elapsed <= FreshnessMs) {
            weightedTwice_ += uint64_t(previous_ + uint32_t(speed)) * elapsed;
            durationMs_ += elapsed;
        }
        if (!available_ || speed > maximum_) maximum_ = speed;
        previous_ = speed;
        stamp_ = stamp;
        seen_ = continuous_ = available_ = true;
    }
    bool available() const { return available_; }
    uint16_t maximum() const { return maximum_; }
    uint16_t average() const {
        return durationMs_ ? uint16_t((weightedTwice_ + durationMs_) / (2 * durationMs_)) : previous_;
    }
private:
    uint64_t weightedTwice_ = 0, durationMs_ = 0;
    uint32_t stamp_ = 0;
    uint16_t previous_ = 0, maximum_ = 0;
    bool seen_ = false, continuous_ = false, available_ = false;
};

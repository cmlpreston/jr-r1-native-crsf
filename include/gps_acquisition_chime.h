#pragma once
#include <stdint.h>

// A one-shot notification for first usable GPS position, not proof of fix quality.
// No allocation, delay, replay after dropout, or interference with priority alarms.
class GpsAcquisitionChime {
public:
    struct Note { uint16_t pitchHz = 0, durationMs = 0; bool stop = false; };
    Note update(bool usablePosition, bool enabled, bool priorityBusy, uint32_t now) {
        if (!seen_ && usablePosition) {
            seen_ = true;
            if (enabled && !priorityBusy) { running_ = true; startedMs_ = now; next_ = 0; }
        }
        if (!running_) return {};
        if (!enabled || priorityBusy) {
            cancel();
            Note note; note.stop = !priorityBusy; return note;
        }
        const uint32_t elapsed = uint32_t(now - startedMs_);
        if (elapsed >= 440) { cancel(); return {}; }
        const uint16_t starts[] = {0,140,280}, lengths[] = {90,90,160};
        const uint16_t pitches[] = {2093,2637,3136}; // C7, E7, G7
        const uint8_t index = elapsed >= 280 ? 2 : elapsed >= 140 ? 1 : 0;
        if (next_ > index) return {};
        next_ = index + 1;
        const uint16_t end = starts[index] + lengths[index];
        if (elapsed >= end) return {}; // Skip a missed note rather than queueing it.
        Note note; note.pitchHz = pitches[index]; note.durationMs = end - elapsed;
        return note;
    }
    bool active(uint32_t now) const { return running_ && uint32_t(now-startedMs_) < 440; }
    void cancel() { running_ = false; } // First-position latch is intentionally retained.
private:
    uint32_t startedMs_ = 0;
    uint8_t next_ = 0;
    bool seen_ = false, running_ = false;
};

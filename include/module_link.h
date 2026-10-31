#pragma once
#include <stdint.h>

// Approval is volatile and belongs to one continuous detected supply session.
// This gates CRSF traffic, not the Ranger's physical power rail.
class ModuleLink {
public:
    void update(bool detected, uint32_t now) {
        if (!detected) {
            qualifying_ = present_ = enabled_ = prompt_ = false;
            return;
        }
        if (!qualifying_) { qualifying_ = true; detectedSince_ = now; }
        if (!present_ && uint32_t(now - detectedSince_) >= 250) {
            present_ = true; prompt_ = true;
        }
    }
    bool enable() { if (!present_) return false; enabled_ = true; prompt_ = false; return true; }
    void pause() { enabled_ = false; prompt_ = false; }
    void dismiss() { prompt_ = false; }
    bool present() const { return present_; }
    bool enabled() const { return enabled_; }
    bool prompt() const { return prompt_; }
private:
    bool qualifying_ = false, present_ = false, enabled_ = false, prompt_ = false;
    uint32_t detectedSince_ = 0;
};

#pragma once
#include <stdint.h>

// Temporary USB bench permission. Callers serialize access between tasks.
// Expiry latches CUT: a later heartbeat can never restore an arm request.
class RfBenchSession {
public:
    static constexpr uint32_t TimeoutMs = 2000;
    bool enable(bool pinValid, uint32_t now) {
        authorized_ = pinValid;
        if (!pinValid) { cut(); return false; }
        enabled_ = true;
        armed_ = false;
        lastHeartbeat_ = now;
        return true;
    }
    bool enabled() const { return enabled_; }
    bool authorized() const { return authorized_; }
    bool armRequested(uint32_t now) {
        if (uint32_t(now - lastHeartbeat_) >= TimeoutMs) armed_ = false;
        return enabled_ && authorized_ && armed_;
    }
    void heartbeat(uint32_t now, bool inputsHealthy) {
        armRequested(now); // expire before renewing
        if (!inputsHealthy) cut();
        lastHeartbeat_ = now;
    }
    bool requestArm(uint32_t now, bool ready) {
        const bool fresh = uint32_t(now - lastHeartbeat_) < TimeoutMs;
        armed_ = enabled_ && authorized_ && fresh && ready;
        return armed_;
    }
    void cut() { armed_ = false; }
private:
    bool enabled_ = false, authorized_ = false, armed_ = false;
    uint32_t lastHeartbeat_ = 0;
};

#pragma once

#include <Arduino.h>

struct CrsfTelemetryState {
    bool linkValid = false;
    int16_t uplinkRssiDbm = 0; // Normalised signed dBm, e.g. -31
    uint8_t uplinkLinkQuality = 0;
    int8_t uplinkSnrDb = 0;
    uint8_t activeAntenna = 0;
    uint8_t rfMode = 0;
    uint8_t txPowerCode = 0;
    bool receiverVoltageValid = false;
    uint16_t receiverVoltageDecivolts = 0;
    uint32_t linkUpdatedMs = 0;
    uint32_t batteryUpdatedMs = 0;
    uint32_t validFrames = 0;
    uint32_t crcErrors = 0;
    uint32_t rawBytes = 0;
};

class CrsfTelemetryParser {
public:
    void feed(uint8_t value);
    void updateFreshness();
    const CrsfTelemetryState &state() const { return state_; }

private:
    void processFrame();
    static uint8_t crc8DvbS2(const uint8_t *data, size_t length);

    static constexpr size_t kMaximumFrameBytes = 64;
    uint8_t frame_[kMaximumFrameBytes] = {};
    uint8_t index_ = 0;
    uint8_t expectedBytes_ = 0;
    CrsfTelemetryState state_;
};

#pragma once

#include <Arduino.h>

struct CrsfTelemetryState {
    // Last validated TX configuration response. Single-flight client retries
    // reads if an unsolicited response replaces this mailbox before UI polling.
    uint32_t txParameterSequence = 0;
    uint8_t txParameterType = 0, txParameterLength = 0;
    uint8_t txParameterPayload[60] = {};
    bool gpsValid = false, temperatureValid = false, rpmValid = false;
    int32_t latitudeE7 = 0, longitudeE7 = 0;
    uint16_t groundSpeedDeciKmh = 0, headingCentiDegrees = 0;
    int32_t altitudeMetres = 0, rpm = 0;
    int16_t temperatureDeciC = 0;
    uint8_t satellites = 0;
    uint32_t gpsUpdatedMs = 0, temperatureUpdatedMs = 0, rpmUpdatedMs = 0;
    // Selected vehicle battery, source 0, 1–12 reported cells. Independent of RX supply.
    bool vehicleCellsValid = false;
    static constexpr uint8_t MaxVehicleCells = 12;
    uint8_t vehicleCellCount = 0;
    uint16_t vehicleCellsMv[MaxVehicleCells] = {};
    uint16_t vehiclePackMv = 0, vehicleMinimumCellMv = 0;
    uint32_t vehicleCellsUpdatedMs = 0;
    bool fasVoltageValid = false, fasCurrentValid = false;
    uint16_t fasVoltageMv = 0, fasCurrentDeciA = 0;
    uint32_t fasVoltageUpdatedMs = 0, fasCurrentUpdatedMs = 0;
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
    bool transmitterUidValid = false;
    bool receiverUidValid = false;
    uint8_t transmitterUid[6] = {};
    uint8_t receiverUid[6] = {};
    uint32_t transmitterUidUpdatedMs = 0;
    uint32_t receiverUidUpdatedMs = 0;
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

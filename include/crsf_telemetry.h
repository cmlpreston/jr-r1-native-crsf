#pragma once

#include <Arduino.h>
#include <atomic>

struct CrsfTelemetryState {
    // Last validated TX configuration response. Single-flight client retries
    // reads if an unsolicited response replaces this mailbox before UI polling.
    uint32_t txParameterSequence = 0;
    uint8_t txParameterType = 0, txParameterLength = 0;
    uint8_t txParameterPayload[60] = {};
    uint32_t ersSequence=0;
    uint8_t ersFrameType=0, ersLength=0, ersPayload[60]={};
    // External CRSF battery ID 2, tagged by our ER6G serial ingress.
    bool ersBatteryValid=false, ersRemainingValid=false;
    uint16_t ersVoltageDeciV=0, ersCurrentDeciA=0;
    uint32_t ersCapacityMah=0, ersBatteryUpdatedMs=0;
    uint8_t ersRemainingPercent=0;
    bool gpsCourseValid = false;
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

// S.Port GPS forwarding has no satellite/fix-quality field. A zero count is
// unknown, not a confirmed fix: allow fresh non-placeholder reported values,
// keep known low-satellite reports unavailable and label uncertainty in the UI.
inline bool gpsPositionAvailable(const CrsfTelemetryState &t, uint32_t nowMs) {
    return t.gpsValid && uint32_t(nowMs-t.gpsUpdatedMs)<=3000 &&
        (t.satellites>=4 || (t.satellites==0 && (t.latitudeE7!=0 || t.longitudeE7!=0)));
}
// Course over ground is unreliable near rest; this is a display threshold only.
inline bool gpsCourseAvailable(const CrsfTelemetryState &t, uint32_t nowMs) {
    return gpsPositionAvailable(t,nowMs) && t.gpsCourseValid &&
        t.headingCentiDegrees < 36000 && t.groundSpeedDeciKmh >= 10;
}
inline const char *gpsCourseCardinal(uint16_t centidegrees) {
    const char *names[]={"N","NNE","NE","ENE","E","ESE","SE","SSE","S","SSW","SW","WSW","W","WNW","NW","NNW"};
    return names[((centidegrees+1125)/2250)%16];
}
inline bool gpsFixConfirmed(const CrsfTelemetryState &t, uint32_t nowMs) {
    return gpsPositionAvailable(t,nowMs) && t.satellites>=4;
}

class CrsfTelemetryParser {
public:
    void feed(uint8_t value);
    void setStandardBatteryVehicle(bool vehicle) { requestedBatteryVehicle_.store(vehicle); }
    void updateFreshness();
    const CrsfTelemetryState &state() const { return state_; }

private:
    void processFrame();
    void syncBatteryRole();
    std::atomic<bool> requestedBatteryVehicle_{false};
    bool standardBatteryVehicle_=false;
    static uint8_t crc8DvbS2(const uint8_t *data, size_t length);

    static constexpr size_t kMaximumFrameBytes = 64;
    uint8_t frame_[kMaximumFrameBytes] = {};
    uint8_t index_ = 0;
    uint8_t expectedBytes_ = 0;
    CrsfTelemetryState state_;
};

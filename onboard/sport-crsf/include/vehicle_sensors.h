#pragma once
#include "sport_crsf.h"

namespace onboard {

// Sensor timestamps are independent; callers must check freshness at use time.
template<typename T> struct Reading {
    T value{};
    uint32_t updatedMs = 0;
    bool valid = false;
    bool fresh(uint32_t nowMs) const {
        return valid && uint32_t(nowMs - updatedMs) <= 3000;
    }
    void set(T v, uint32_t nowMs) { value = v; updatedMs = nowMs; valid = true; }
};

struct VehicleSensorSelection {
    SensorKey cells;
    SensorKey motorTemperature;
    uint8_t gpsPhysicalId;
    uint8_t gpsInstance; // low nibble of each GPS application ID, 0..15
};

class VehicleSensors {
public:
    explicit VehicleSensors(VehicleSensorSelection selection) : selection_(selection) {}
    void accept(const SportSample &sample, uint32_t nowMs);
    // Returns false unless all four cells belong to a complete, fresh scan.
    bool pack(uint32_t nowMs, uint16_t &totalMv, uint16_t &minimumMv) const;
    Reading<uint16_t> cellsMv[4];
    Reading<int32_t> motorTemperatureC;
    Reading<int32_t> latitudeE7, longitudeE7;
    Reading<uint32_t> speedDeciKmh;
    Reading<int32_t> altitudeCm;
    Reading<uint16_t> headingCentiDegrees;
private:
    VehicleSensorSelection selection_;
    uint8_t cellMask_ = 0;
};

} // namespace onboard

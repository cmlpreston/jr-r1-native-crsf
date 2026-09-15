#include "vehicle_sensors.h"

namespace onboard {
namespace {
int32_t signedValue(uint32_t raw)
{
    return raw <= 0x7fffffff ? int32_t(raw) : int32_t(int64_t(raw) - 0x100000000LL);
}
}

void VehicleSensors::accept(const SportSample &s, uint32_t nowMs)
{
    if (selection_.cells.matches(s) && (s.appId & 0xfff0) == 0x0300) {
        const uint8_t count = (s.value >> 4) & 15;
        const uint8_t index = s.value & 15;
        // This installation is explicitly 4S. Do not silently reinterpret a
        // different pack or combine old cells after a disconnect/reconnect.
        if (count != 4 || (index != 0 && index != 2)) {
            cellMask_ = 0;
            for (auto &cell : cellsMv) cell.valid = false;
            return;
        }
        if (index == 0) {
            cellMask_ = 0;
            for (auto &cell : cellsMv) cell.valid = false;
        }
        const uint16_t first = uint16_t((s.value >> 8) & 0xfff) * 2;
        const uint16_t second = uint16_t((s.value >> 20) & 0xfff) * 2;
        if (first < 500 || first > 5000 || second < 500 || second > 5000) {
            cellMask_ = 0;
            for (auto &cell : cellsMv) cell.valid = false;
            return;
        }
        if (index == 2 && (cellMask_ != 3 || !cellsMv[0].fresh(nowMs))) return;
        cellsMv[index].set(first, nowMs);
        cellsMv[index + 1].set(second, nowMs);
        cellMask_ |= uint8_t(3 << index);
        return;
    }
    if (selection_.motorTemperature.matches(s) &&
        ((s.appId & 0xfff0) == 0x0400 || (s.appId & 0xfff0) == 0x0410)) {
        const int32_t value = signedValue(s.value);
        motorTemperatureC.valid = value >= -55 && value <= 250;
        if (motorTemperatureC.valid) motorTemperatureC.set(value, nowMs);
        return;
    }
    if (s.physicalId != selection_.gpsPhysicalId ||
        (s.appId & 15) != selection_.gpsInstance) return;
    switch (s.appId & 0xfff0) {
    case 0x0800: {
        // S.Port: absolute minutes * 10000, bit30 sign, bit31 longitude.
        const bool longitude = (s.value & 0x80000000u) != 0;
        auto &reading = longitude ? longitudeE7 : latitudeE7;
        const uint32_t minutes = s.value & 0x3fffffffu;
        reading.valid = minutes <= (longitude ? 180u : 90u) * 600000u;
        if (reading.valid) {
            int32_t degrees = int32_t((uint64_t(minutes) * 50 + 1) / 3);
            if (s.value & 0x40000000u) degrees = -degrees;
            reading.set(degrees, nowMs);
        }
        break;
    }
    case 0x0820:
        altitudeCm.set(signedValue(s.value), nowMs);
        break;
    case 0x0830: {
        // Thousandths of a knot -> tenths of km/h, exact 1 knot = 1.852 km/h.
        const uint64_t speed = (uint64_t(s.value) * 1852 + 50000) / 100000;
        speedDeciKmh.valid = speed <= 65535;
        if (speedDeciKmh.valid) speedDeciKmh.set(uint32_t(speed), nowMs);
        break;
    }
    case 0x0840:
        headingCentiDegrees.valid = s.value < 36000;
        if (headingCentiDegrees.valid) headingCentiDegrees.set(uint16_t(s.value), nowMs);
        break;
    default: break;
    }
}

bool VehicleSensors::pack(uint32_t nowMs, uint16_t &totalMv, uint16_t &minimumMv) const
{
    if (cellMask_ != 15) return false;
    uint16_t total = 0, minimum = 65535;
    for (const auto &cell : cellsMv) {
        if (!cell.fresh(nowMs)) return false;
        total += cell.value;
        if (cell.value < minimum) minimum = cell.value;
    }
    totalMv = total;
    minimumMv = minimum;
    return true;
}

} // namespace onboard

#pragma once
#include <cmath>
#include <cstdint>

namespace GpsMap {
constexpr int32_t OriginLat = -378361917, OriginLon = 1450852289;
constexpr int32_t DemoLat = -378354286, DemoLon = 1450857802;
constexpr double MetresPerE7 = 0.01113195;
inline double longitudeScale(int32_t lat) { return std::cos(lat / 1e7 * 3.141592653589793 / 180.0); }
inline double east(int32_t lat, int32_t lon, int32_t centreLon) {
    return (double(lon) - centreLon) * MetresPerE7 * longitudeScale(lat);
}
inline double north(int32_t lat, int32_t centreLat) { return (double(lat) - centreLat) * MetresPerE7; }
inline bool covers(int32_t lat, int32_t lon) {
    return lat >= -378400000 && lat <= -378320000 && lon >= 1450810000 && lon <= 1450900000;
}
}

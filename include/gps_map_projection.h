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
// Per-line projection identity. Trail coordinates are owned by one UI loop;
// its generation advances whenever points change, including rollover/reset.
struct TrailProjectionCache {
    bool unchanged(uint32_t generation,uint8_t points,int32_t lat,int32_t lon,double scale,
                   uint32_t mapRevision,int w,int h) const {
        return valid&&generation_==generation&&count_==points&&latitude_==lat&&longitude_==lon&&
            scale_==scale&&mapRevision_==mapRevision&&width_==w&&height_==h;
    }
    void remember(uint32_t generation,uint8_t points,int32_t lat,int32_t lon,double scale,
                  uint32_t mapRevision,int w,int h) {
        generation_=generation;count_=points;latitude_=lat;longitude_=lon;scale_=scale;
        mapRevision_=mapRevision;width_=w;height_=h;valid=true;
    }
private:
    uint32_t generation_=0,mapRevision_=0;
    int32_t latitude_=0,longitude_=0;
    double scale_=0;
    int16_t width_=0,height_=0;
    uint8_t count_=0;
    bool valid=false;
};
// Keep the image still inside the central 60%; recenter at the outer 20% margin.
inline bool needsRecenter(int32_t centreLat, int32_t centreLon, int32_t lat, int32_t lon,
                          double scale, int width, int height) {
    return std::abs(east(centreLat,lon,centreLon)*scale)>width*0.3 ||
        std::abs(north(lat,centreLat)*scale)>height*0.3;
}
inline bool covers(int32_t lat, int32_t lon) {
    return lat >= -378400000 && lat <= -378320000 && lon >= 1450810000 && lon <= 1450900000;
}
}

#pragma once
#include <cstdint>
#include <cmath>
namespace OfflineMap {
constexpr unsigned MaxPoints = 512, MaxPaths = 32;
struct Point { int32_t lat = 0, lon = 0; };
struct Path { uint16_t offset = 0, count = 0; };
struct Package {
    char name[41] = {}, attribution[81] = {};
    Point points[MaxPoints];
    Path paths[MaxPaths];
    uint16_t pointCount = 0, pathCount = 0;
    int32_t south = 0, north = 0, west = 0, east = 0;
    uint16_t width = 0, height = 0; // Nonzero for a north-up RGB565 image.
    bool valid = false;
};
const Package &current();
uint32_t revision();
void install(const Package &map, uint16_t *ownedPixels = nullptr);
uint16_t *allocatePixels(unsigned count);
const uint16_t *pixels();
bool covers(int32_t lat, int32_t lon);
inline bool coordinateValid(double lat, double lon) {
    return std::isfinite(lat) && std::isfinite(lon) && lat >= -85 && lat <= 85 && lon >= -180 && lon <= 180;
}
// North-up local maps; reject dateline/world-spanning packages.
inline bool boundsValid(const Package &p) {
    return p.pointCount >= 2 && p.pointCount <= MaxPoints && p.pathCount > 0 && p.pathCount <= MaxPaths &&
        int64_t(p.north)-p.south >= 0 && int64_t(p.north)-p.south <= 2500000 &&
        int64_t(p.east)-p.west >= 0 && int64_t(p.east)-p.west <= 2500000;
}
}

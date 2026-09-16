#pragma once
#include <lvgl.h>
#include "gps_map_projection.h"

struct GpsMapViewport {
    int32_t latitude = GpsMap::OriginLat, longitude = GpsMap::OriginLon;
    double pixelsPerMetre = 0.5;
};
// The viewport must live as long as the LVGL object (owned by TransmitterUi).
lv_obj_t *createGpsBasemap(lv_obj_t *parent, int x, int y, int width, int height,
                          GpsMapViewport &viewport);
void updateGpsBasemap(lv_obj_t *object, GpsMapViewport &viewport,
                      int32_t lat, int32_t lon, double pixelsPerMetre);

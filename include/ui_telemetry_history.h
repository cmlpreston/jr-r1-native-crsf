#pragma once
#include "sensor_statistics.h"
#include "ui_cold_storage.h"
#include "sensor_history.h"

template<size_t Sensors> struct UiTelemetryHistory {
    int32_t frozen[30]={};
    bool frozenValid[30]={};
    uint16_t channels[16][30]={};
    int32_t sensors[Sensors][30]={};
    bool valid[Sensors][30]={};
    SensorStatistics statistics[Sensors];
    SensorHistory graphs[Sensors];
};

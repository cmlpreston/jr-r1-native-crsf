#pragma once
#include "sensor_statistics.h"
#include "ui_cold_storage.h"
#include "sensor_history.h"

template<size_t Sensors> struct UiTelemetryHistory {
    int32_t frozen[SensorHistory::Capacity]={};
    bool frozenValid[SensorHistory::Capacity]={};
    SensorHistory::Range frozenRange;
    uint16_t channels[16][30]={};
    SensorStatistics statistics[Sensors];
    SensorHistory graphs[Sensors];
};

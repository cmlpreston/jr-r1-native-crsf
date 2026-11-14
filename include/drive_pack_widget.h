#pragma once
#include <stdint.h>

// CU01/ERS battery frame only. RX voltage and cell-pack telemetry deliberately
// do not fill missing readings, and remaining capacity is never inferred here.
struct DrivePackWidgetReading {
    bool fresh=false, remainingValid=false;
    uint16_t voltageDeciV=0, currentDeciA=0;
    uint32_t usedMah=0;
    uint8_t remainingPercent=0;
};
template<class Telemetry>
inline DrivePackWidgetReading drivePackWidgetReading(const Telemetry &t,uint32_t now) {
    DrivePackWidgetReading r;
    r.fresh=t.ersBatteryValid&&uint32_t(now-t.ersBatteryUpdatedMs)<=3000;
    r.remainingValid=r.fresh&&t.ersRemainingValid&&t.ersRemainingPercent<=100;
    r.voltageDeciV=t.ersVoltageDeciV;r.currentDeciA=t.ersCurrentDeciA;
    r.usedMah=t.ersCapacityMah;r.remainingPercent=t.ersRemainingPercent;
    return r;
}

struct DrivePackWidgetGrid {
    int columns, rows, columnWidth, rowHeight;
    DrivePackWidgetGrid(int width,int height):columns(width>=220?2:1),rows(4/columns),
        columnWidth((width-16)/columns),rowHeight((height-28)/rows) {}
};

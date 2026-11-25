#pragma once
#include "dashboard_widgets.h"
#include "sensor_statistics.h"
#include "speed_statistics.h"
#include "power_energy_statistics.h"
#include "trip_run_statistics.h"
#include "battery_load_balance.h"
#include "race_timer.h"
#include <type_traits>
#include <new>

// Versioned private checkpoint, not a wire/telemetry packet or settings backup.
// Object-layout changes require a schema bump; size mismatches are also rejected.
struct RunRecords {
    static constexpr uint32_t Schema=1;
    static constexpr unsigned MaxGps=2048;
    struct GpsPoint { int32_t latitude=0,longitude=0;uint32_t uptimeMs=0,segment=0; };
    uint32_t schema=Schema,segment=0,gpsPeriodMs=1000;
    uint16_t gpsCount=0;
    SensorStatistics sensors[DashboardSensorCount];
    SpeedStatistics speed;
    PowerEnergyStatistics power;
    TripRunStatistics trip;
    BatteryLoadBalance battery;
    RaceTimer timer;
    GpsPoint gps[MaxGps];
    // Reinitialize in place: a checkpoint is much larger than the ESP32 UI stack.
    void clear(){this->~RunRecords();new(this) RunRecords{};}
    void clearGps(){for(auto &point:gps)point=GpsPoint{};gpsCount=0;gpsPeriodMs=1000;++segment;}
    void addGps(int32_t latitude,int32_t longitude,uint32_t now){
        if(latitude<-900000000||latitude>900000000||longitude<-1800000000||longitude>1800000000)return;
        if(gpsCount){const auto &last=gps[gpsCount-1];
            if(last.segment==segment&&(uint32_t(now-last.uptimeMs)<gpsPeriodMs||
               (last.latitude==latitude&&last.longitude==longitude)))return;}
        // Keep a bounded overview of the entire route, rather than silently
        // dropping its start. Long tracks reduce temporal detail progressively.
        if(gpsCount==MaxGps){for(unsigned i=0;i<MaxGps/2;++i)gps[i]=gps[i*2];
            gpsCount=MaxGps/2;if(gpsPeriodMs<3600000)gpsPeriodMs*=2;}
        auto &point=gps[gpsCount++];point.latitude=latitude;point.longitude=longitude;
        point.uptimeMs=now;point.segment=segment;
    }
    bool valid()const{
        if(schema!=Schema||gpsCount>MaxGps||gpsPeriodMs<1000||gpsPeriodMs>4096000||!validTimerSettings(timer.settings)||timer.lapCount()>RaceTimer::MaxLaps)return false;
        for(unsigned i=0;i<gpsCount;++i)if(gps[i].latitude<-900000000||gps[i].latitude>900000000||gps[i].longitude<-1800000000||gps[i].longitude>1800000000)return false;
        return true;
    }
    void resumeAfterPowerCycle(uint32_t now){
        ++segment;for(auto &s:sensors)s.resumeAfterPowerCycle();speed.resumeAfterPowerCycle();
        power.resumeAfterPowerCycle();trip.resumeAfterPowerCycle(now);battery.resumeAfterPowerCycle();timer.resumeAfterPowerCycle(now);
    }
};
static_assert(std::is_trivially_copyable<RunRecords>::value,"Checkpoint must contain values only");

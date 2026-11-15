#pragma once
#include <stdint.h>
#include <limits.h>
#include "power_energy_statistics.h"

// One UI-owned observed session shared by Trip and Run summary. Nothing here
// writes a sensor counter or affects control. Elapsed is wall time since the
// first new measured packet; moving time and integrals omit missing intervals.
class TripRunStatistics {
public:
    static constexpr uint32_t FreshnessMs=PowerEnergyStatistics::FreshnessMs;
    struct Result {
        bool elapsedValid=false,speedFresh=false;
        bool distanceValid=false,distanceComplete=false,movingValid=false,movingComplete=false;
        bool averageMovingValid=false,maxSpeedValid=false;
        bool powerValid=false,peakCurrentValid=false,energyValid=false,energyComplete=false;
        bool usedCapacityValid=false,usedCapacityComplete=false;
        uint64_t elapsedMs=0,movingMs=0,usedMah=0;
        double kilometres=0,averageMovingKmh=0,maxSpeedKmh=0,peakAmps=0,peakWatts=0,wattHours=0;
    };
    void reset(){*this=TripRunStatistics{};}
    // Ignore cached packets until each source advances after reset/model switch.
    void reset(uint32_t powerStamp,uint32_t speedStamp){
        reset();powerSeen_=speedSeen_=true;powerStamp_=powerStamp;speedStamp_=speedStamp;
        integrals_.reset(powerStamp,speedStamp);
    }
    void update(uint16_t volts,uint16_t amps,uint32_t usedMah,uint32_t powerStamp,bool powerValid,
                uint16_t speed,uint32_t speedStamp,bool speedValid,uint32_t now){
        advanceClock(now);
        if(!started_){
            const bool newSpeed=admitted(speedStamp,speedSeen_,speedStamp_,speedValid,now);
            const bool newPower=admitted(powerStamp,powerSeen_,powerStamp_,powerValid,now);
            if(newSpeed||newPower){
                const uint32_t first=newSpeed&&(!newPower||uint32_t(now-speedStamp)>=uint32_t(now-powerStamp))?
                    speedStamp:powerStamp;
                startClock(first,now);
            }
        }
        // Reuse only the existing CU01 energy integrator. Trip owns a coherent
        // threshold-clipped distance/time integral instead of collecting a
        // second independent GPS distance/efficiency calculation here.
        integrals_.update(volts,amps,powerStamp,powerValid,0,0,false,now);
        updateSpeed(speed,speedStamp,speedValid,now);
        updatePower(amps,usedMah,powerStamp,powerValid,now);
    }
    template<class Telemetry>void update(const Telemetry &t,uint32_t now){
        update(t.ersVoltageDeciV,t.ersCurrentDeciA,t.ersCapacityMah,t.ersBatteryUpdatedMs,t.ersBatteryValid,
               t.groundSpeedDeciKmh,t.gpsSpeedUpdatedMs,t.gpsSpeedValid,now);
    }
    Result result(uint32_t now)const{
        Result r;
        const auto energy=integrals_.result(now);
        r.elapsedValid=started_;r.elapsedMs=elapsedMs_;
        if(started_&&uint32_t(now-clockStamp_)<0x80000000u)r.elapsedMs+=uint32_t(now-clockStamp_);
        r.speedFresh=haveSpeed_&&speedActive_&&uint32_t(now-speedStamp_)<=FreshnessMs;
        r.distanceValid=haveSpeed_&&!distanceOverflow_;
        r.distanceComplete=r.distanceValid&&movingComplete_&&r.speedFresh;
        // Distance and moving time integrate the SAME portion of each speed
        // interval above the jitter threshold, so moving average cannot exceed
        // the observed endpoint speeds on a simple start/stop ramp.
        r.kilometres=distanceUnits_/72000000000.0;
        r.movingValid=haveSpeed_&&!movingOverflow_;r.movingMs=movingMicroseconds_/1000;
        r.movingComplete=r.movingValid&&movingComplete_&&r.speedFresh;
        r.averageMovingValid=r.movingValid&&r.distanceValid&&movingMicroseconds_!=0;
        if(r.averageMovingValid)r.averageMovingKmh=r.kilometres*3600000000.0/movingMicroseconds_;
        r.maxSpeedValid=haveSpeed_;r.maxSpeedKmh=maxSpeed_/10.0;
        r.powerValid=energy.peakValid;r.peakWatts=energy.peakWatts;
        r.peakCurrentValid=havePower_;r.peakAmps=peakAmps_/10.0;
        r.energyValid=energy.energyValid;r.energyComplete=energy.energyComplete;r.wattHours=energy.wattHours;
        r.usedCapacityValid=havePower_&&!capacityOverflow_;r.usedMah=usedMah_;
        r.usedCapacityComplete=r.usedCapacityValid&&capacityComplete_&&powerActive_&&uint32_t(now-powerStamp_)<=FreshnessMs;
        return r;
    }
private:
    static bool add(uint64_t &total,uint64_t amount){
        if(UINT64_MAX-total<amount)return false;
        total+=amount;return true;
    }
    static bool admitted(uint32_t stamp,bool seen,uint32_t previous,bool valid,uint32_t now){
        const uint32_t elapsed=stamp-previous;
        return valid&&uint32_t(now-stamp)<=FreshnessMs&&(!seen||(elapsed&&elapsed<0x80000000u));
    }
    void advanceClock(uint32_t now){
        if(!started_){clockStamp_=now;return;}
        const uint32_t elapsed=now-clockStamp_;
        if(elapsed>=0x80000000u)return;
        add(elapsedMs_,elapsed);clockStamp_=now;
    }
    void startClock(uint32_t stamp,uint32_t now){
        if(started_)return;
        started_=true;clockStamp_=now;elapsedMs_=uint32_t(now-stamp);
    }
    void updateSpeed(uint16_t speed,uint32_t stamp,bool valid,uint32_t now){
        if(!valid||uint32_t(now-stamp)>FreshnessMs){
            speedActive_=speedContinuous_=false;
            if(haveSpeed_)movingComplete_=false;
            return;
        }
        const uint32_t elapsed=stamp-speedStamp_;
        if(speedSeen_&&!elapsed)return;
        if(speedSeen_&&elapsed>=0x80000000u){
            speedActive_=speedContinuous_=false;movingComplete_=false;return;
        }
        startClock(stamp,now);
        if(speedContinuous_&&elapsed<=FreshnessMs){
            // Estimate the fraction of this interval at >=1 km/h by linearly
            // interpolating the threshold crossing, rather than counting idle
            // time or discarding the whole first/last moving interval.
            uint64_t moving=0,distance=0;
            if(previousSpeed_>=10&&speed>=10){
                moving=uint64_t(elapsed)*1000;
                distance=uint64_t(previousSpeed_+speed)*moving;
            }else if(previousSpeed_<10&&speed>=10){
                moving=uint64_t(elapsed)*1000*(speed-10)/(speed-previousSpeed_);
                distance=uint64_t(10+speed)*moving;
            }else if(previousSpeed_>=10&&speed<10){
                moving=uint64_t(elapsed)*1000*(previousSpeed_-10)/(previousSpeed_-speed);
                distance=uint64_t(previousSpeed_+10)*moving;
            }
            if(!add(movingMicroseconds_,moving)){movingOverflow_=true;movingComplete_=false;}
            if(!add(distanceUnits_,distance)){distanceOverflow_=true;movingComplete_=false;}
        }else if(haveSpeed_){movingComplete_=false;}
        previousSpeed_=speed;speedStamp_=stamp;
        if(!haveSpeed_||speed>maxSpeed_)maxSpeed_=speed;
        haveSpeed_=speedSeen_=speedActive_=speedContinuous_=true;
    }
    void updatePower(uint16_t amps,uint32_t used,uint32_t stamp,bool valid,uint32_t now){
        if(!valid||uint32_t(now-stamp)>FreshnessMs){
            powerActive_=false;if(havePower_)capacityComplete_=false;return;
        }
        const uint32_t elapsed=stamp-powerStamp_;
        if(powerSeen_&&!elapsed)return;
        if(powerSeen_&&elapsed>=0x80000000u){powerActive_=false;capacityComplete_=false;return;}
        startClock(stamp,now);
        if(havePower_){
            if(!powerActive_||elapsed>FreshnessMs)capacityComplete_=false;
            // A decreasing sensor counter denotes a new/restarted counter, not
            // negative consumption or an unsigned wrap-sized capacity jump.
            if(used<previousUsedMah_)capacityComplete_=false;
            else if(!add(usedMah_,uint64_t(used)-previousUsedMah_)){
                capacityOverflow_=true;capacityComplete_=false;
            }
        }
        previousUsedMah_=used;powerStamp_=stamp;
        if(!havePower_||amps>peakAmps_)peakAmps_=amps;
        havePower_=powerSeen_=powerActive_=true;
    }
    PowerEnergyStatistics integrals_;
    uint64_t elapsedMs_=0,movingMicroseconds_=0,distanceUnits_=0,usedMah_=0;
    uint32_t clockStamp_=0,powerStamp_=0,speedStamp_=0,previousUsedMah_=0;
    uint16_t previousSpeed_=0,maxSpeed_=0,peakAmps_=0;
    bool started_=false,powerSeen_=false,speedSeen_=false,havePower_=false,haveSpeed_=false;
    bool powerActive_=false,speedActive_=false,speedContinuous_=false;
    bool movingComplete_=true,capacityComplete_=true,movingOverflow_=false,distanceOverflow_=false,capacityOverflow_=false;
};

#pragma once
#include <stdint.h>
#include <limits.h>

// UI-owned observed statistics, never a sensor counter or control input. Missing
// intervals are omitted and marked incomplete, not reconstructed as zero load.
class PowerEnergyStatistics {
public:
    static constexpr uint32_t FreshnessMs=3000, EfficiencySpeedAgeMs=1000;
    struct Result {
        bool powerValid=false,peakValid=false,energyValid=false,energyComplete=false;
        bool distanceValid=false,distanceComplete=false,efficiencyValid=false,efficiencyComplete=false;
        double watts=0,peakWatts=0,wattHours=0,kilometres=0,wattHoursPerKm=0,efficiencyKilometres=0;
    };

    void reset(){*this=PowerEnergyStatistics{};}
    void resumeAfterPowerCycle(uint32_t powerStamp=0,uint32_t speedStamp=0){
        powerSeen_=speedSeen_=true;powerActive_=speedActive_=powerContinuous_=speedContinuous_=efficiencyContinuous_=false;
        previousSpeedSample_.valid=currentSpeedSample_.valid=false;
        energyComplete_=distanceComplete_=efficiencyComplete_=false;
        powerStamp_=powerStamp;speedStamp_=speedStamp;
    }
    void reset(uint32_t powerStamp,uint32_t speedStamp){
        reset();powerSeen_=speedSeen_=true;powerStamp_=powerStamp;speedStamp_=speedStamp;
    }
    void update(uint16_t voltageDeciV,uint16_t currentDeciA,uint32_t powerStamp,bool powerValid,
                uint16_t speedDeciKmh,uint32_t speedStamp,bool speedValid,uint32_t now){
        updateSpeed(speedDeciKmh,speedStamp,speedValid,now);
        if(!powerValid||uint32_t(now-powerStamp)>FreshnessMs){
            powerActive_=powerContinuous_=efficiencyContinuous_=false;
            if(havePower_)energyComplete_=efficiencyComplete_=false;
            return;
        }
        const uint32_t elapsed=powerStamp-powerStamp_;
        if(powerSeen_&&!elapsed)return;
        if(powerSeen_&&elapsed>=0x80000000u){
            powerActive_=powerContinuous_=efficiencyContinuous_=false;
            energyComplete_=efficiencyComplete_=false;return;
        }
        const uint32_t product=uint32_t(voltageDeciV)*currentDeciA;
        const bool continuous=powerContinuous_&&elapsed<=FreshnessMs;
        if(powerContinuous_&&!continuous)energyComplete_=false;
        if(continuous){
            const uint64_t interval=(uint64_t(previousPower_)+product)*elapsed;
            if(!add(energyUnits_,interval)){energyOverflow_=true;energyComplete_=false;}
        }
        uint16_t speed=0;
        const bool speedAvailable=speedAt(powerStamp,speed);
        if(continuous&&efficiencyContinuous_&&speedAvailable){
            const uint64_t energy=(uint64_t(previousPower_)+product)*elapsed;
            const uint64_t distance=(uint64_t(previousEfficiencySpeed_)+speed)*elapsed;
            if(!add(efficiencyEnergyUnits_,energy)||!add(efficiencyDistanceUnits_,distance)){
                efficiencyOverflow_=true;efficiencyComplete_=false;
            }
            efficiencyObserved_=true;
        }else if(havePower_){
            efficiencyComplete_=false;
        }
        if(!speedAvailable)efficiencyComplete_=false;
        previousEfficiencySpeed_=speed;efficiencyContinuous_=speedAvailable;
        powerStamp_=powerStamp;previousPower_=product;
        if(!havePower_||product>peakPower_)peakPower_=product;
        havePower_=powerSeen_=powerActive_=powerContinuous_=true;
    }
    template<class Telemetry>void update(const Telemetry &t,uint32_t now){
        update(t.ersVoltageDeciV,t.ersCurrentDeciA,t.ersBatteryUpdatedMs,t.ersBatteryValid,
               t.groundSpeedDeciKmh,t.gpsSpeedUpdatedMs,t.gpsSpeedValid,now);
    }
    Result result(uint32_t now)const{
        Result r;
        r.powerValid=havePower_&&powerActive_&&uint32_t(now-powerStamp_)<=FreshnessMs;
        r.peakValid=havePower_;r.energyValid=havePower_&&!energyOverflow_;
        r.energyComplete=r.energyValid&&energyComplete_&&r.powerValid;
        r.distanceValid=haveSpeed_&&!distanceOverflow_;
        r.distanceComplete=r.distanceValid&&distanceComplete_&&speedActive_&&uint32_t(now-speedStamp_)<=FreshnessMs;
        r.watts=previousPower_/100.0;r.peakWatts=peakPower_/100.0;
        // Products are hundredths of a watt; trapezoids retain the factor two.
        r.wattHours=energyUnits_/720000000.0;
        r.kilometres=distanceUnits_/72000000.0;
        r.efficiencyKilometres=efficiencyDistanceUnits_/72000000.0;
        r.efficiencyValid=efficiencyObserved_&&!efficiencyOverflow_&&efficiencyDistanceUnits_>=7200000u;
        r.efficiencyComplete=r.efficiencyValid&&efficiencyComplete_&&r.powerValid&&speedActive_&&uint32_t(now-speedStamp_)<=EfficiencySpeedAgeMs;
        if(r.efficiencyValid)r.wattHoursPerKm=(efficiencyEnergyUnits_/720000000.0)/r.efficiencyKilometres;
        return r;
    }
private:
    struct SpeedSample {uint32_t stamp=0;uint16_t speed=0;bool valid=false;};
    static bool add(uint64_t &total,uint64_t interval){
        if(UINT64_MAX-total<interval)return false;
        total+=interval;return true;
    }
    void updateSpeed(uint16_t speed,uint32_t stamp,bool valid,uint32_t now){
        if(!valid||uint32_t(now-stamp)>FreshnessMs){
            speedActive_=speedContinuous_=efficiencyContinuous_=false;
            previousSpeedSample_.valid=currentSpeedSample_.valid=false;
            if(haveSpeed_)distanceComplete_=false;
            if(havePower_)efficiencyComplete_=false;
            return;
        }
        const uint32_t elapsed=stamp-speedStamp_;
        if(speedSeen_&&!elapsed)return;
        if(speedSeen_&&elapsed>=0x80000000u){
            speedActive_=speedContinuous_=efficiencyContinuous_=false;
            previousSpeedSample_.valid=currentSpeedSample_.valid=false;
            distanceComplete_=efficiencyComplete_=false;return;
        }
        if(speed<10)speed=0; // Avoid distance from sub-1 km/h standstill jitter.
        if(speedContinuous_&&elapsed<=FreshnessMs){
            if(!add(distanceUnits_,(uint64_t(previousSpeed_)+speed)*elapsed)){
                distanceOverflow_=true;distanceComplete_=false;
            }
        }else if(speedContinuous_){
            distanceComplete_=efficiencyComplete_=false;efficiencyContinuous_=false;
        }
        previousSpeedSample_=currentSpeedSample_;
        currentSpeedSample_.stamp=stamp;currentSpeedSample_.speed=speed;currentSpeedSample_.valid=true;
        previousSpeed_=speed;speedStamp_=stamp;
        haveSpeed_=speedSeen_=speedActive_=speedContinuous_=true;
    }
    bool speedAt(uint32_t stamp,uint16_t &speed)const{
        if(!speedActive_)return false;
        // The newest GPS sample can arrive just after a CU01 packet. Retain one
        // preceding sample so asynchronous arrival does not use future speed.
        const SpeedSample *candidates[]={&currentSpeedSample_,&previousSpeedSample_};
        for(const auto *sample:candidates){
            if(sample->valid&&uint32_t(stamp-sample->stamp)<=EfficiencySpeedAgeMs){
                speed=sample->speed;return true;
            }
        }
        return false;
    }
    uint64_t energyUnits_=0,distanceUnits_=0,efficiencyEnergyUnits_=0,efficiencyDistanceUnits_=0;
    uint32_t powerStamp_=0,speedStamp_=0,previousPower_=0,peakPower_=0;
    uint16_t previousSpeed_=0,previousEfficiencySpeed_=0;
    SpeedSample previousSpeedSample_,currentSpeedSample_;
    bool powerSeen_=false,speedSeen_=false,havePower_=false,haveSpeed_=false;
    bool powerActive_=false,speedActive_=false,powerContinuous_=false,speedContinuous_=false,efficiencyContinuous_=false;
    bool energyComplete_=true,distanceComplete_=true,efficiencyComplete_=true,efficiencyObserved_=false;
    bool energyOverflow_=false,distanceOverflow_=false,efficiencyOverflow_=false;
};

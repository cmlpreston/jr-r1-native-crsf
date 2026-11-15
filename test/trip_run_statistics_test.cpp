#include "trip_run_statistics.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>

static void close(double actual,double expected){assert(std::fabs(actual-expected)<1e-7*(1+std::fabs(expected)));}
static void sample(TripRunStatistics &s,uint32_t stamp,uint32_t used=100,uint16_t speed=360,
                   uint16_t amps=20,uint16_t volts=100){
    s.update(volts,amps,used,stamp,true,speed,stamp,true,stamp);
}
int main(){
    TripRunStatistics s;
    auto r=s.result(0);
    assert(!r.elapsedValid&&!r.distanceValid&&!r.movingValid&&!r.averageMovingValid&&!r.maxSpeedValid);
    assert(!r.powerValid&&!r.peakCurrentValid&&!r.energyValid&&!r.usedCapacityValid);
    s.update(0,0,0,0,false,0,0,false,100);
    sample(s,1000);
    r=s.result(1000);
    assert(r.elapsedValid&&r.elapsedMs==0&&r.movingValid&&r.movingMs==0&&!r.averageMovingValid);
    assert(r.distanceComplete&&r.movingComplete&&r.usedCapacityComplete&&r.usedMah==0&&r.energyComplete);
    for(uint32_t n=1;n<=10;++n)sample(s,1000+n*1000,100+n);
    r=s.result(11000);
    assert(r.elapsedMs==10000&&r.movingMs==10000&&r.averageMovingValid&&r.usedMah==10);
    close(r.kilometres,0.1);close(r.averageMovingKmh,36);close(r.maxSpeedKmh,36);
    close(r.peakAmps,2);close(r.peakWatts,20);close(r.wattHours,20*10/3600.0);
    // The UI can tick/re-render repeatedly, but only a new source timestamp
    // contributes distance, moving time, capacity, power or a new maximum.
    s.update(65535,65535,9000,11000,true,65535,11000,true,11100);
    r=s.result(11100);assert(r.elapsedMs==10100&&r.movingMs==10000&&r.usedMah==10);
    close(r.maxSpeedKmh,36);close(r.peakWatts,20);close(r.peakAmps,2);
    close(r.kilometres,0.1);close(r.wattHours,20*10/3600.0);
    // Idle is valid telemetry: it advances elapsed time without moving time.
    TripRunStatistics idle;
    for(uint32_t n=0;n<5;++n)sample(idle,1000+n*1000,100+n,9,0);
    r=idle.result(5000);
    assert(r.elapsedMs==4000&&r.movingMs==0&&r.movingValid&&!r.averageMovingValid&&r.usedMah==4);
    close(r.kilometres,0);close(r.maxSpeedKmh,0.9);close(r.peakWatts,0);
    assert(r.powerValid&&r.energyValid&&r.energyComplete);
    // Crossing 1 km/h linearly gives half the interval above that threshold.
    TripRunStatistics crossing;
    sample(crossing,1000,100,0);sample(crossing,2000,100,20);sample(crossing,3000,100,0);
    r=crossing.result(3000);assert(r.movingMs==1000&&r.movingComplete);
    close(r.kilometres,1.5/3600);close(r.averageMovingKmh,1.5);
    // Boundary ramps integrate distance and moving time over the same segment.
    // A just-above-cutoff ramp must not report an average higher than its peak.
    for(uint16_t peak: {uint16_t(10),uint16_t(11),uint16_t(20),uint16_t(360)}){
        TripRunStatistics boundary;
        sample(boundary,1000,100,0);sample(boundary,2000,100,peak);sample(boundary,3000,100,0);
        auto b=boundary.result(3000);
        if(peak==10){assert(b.movingMs==0&&!b.averageMovingValid);close(b.kilometres,0);}
        else{
            assert(b.averageMovingValid&&b.averageMovingKmh<=b.maxSpeedKmh);
            close(b.averageMovingKmh,(1.0+peak/10.0)/2);
            const uint64_t halfMoving=1000000u*uint64_t(peak-10)/peak;
            close(b.kilometres,uint64_t(10+peak)*halfMoving*2/72000000000.0);
        }
    }
    TripRunStatistics threshold;
    sample(threshold,1000,100,10);sample(threshold,2000,100,10);
    r=threshold.result(2000);assert(r.movingMs==1000&&r.averageMovingValid);
    close(r.averageMovingKmh,1);close(r.kilometres,1.0/3600);
    // First telemetry often arrives asynchronously: elapsed starts at the
    // earliest admitted sample even though speed is processed before battery.
    TripRunStatistics first;
    first.update(100,20,100,1000,true,360,2000,true,2000);
    assert(first.result(2000).elapsedMs==1000);
    first.reset(1000,2000);
    first.update(100,20,100,1000,true,360,2000,true,2100);
    assert(!first.result(2100).elapsedValid);
    first.update(100,20,101,2050,true,360,2100,true,2100);
    assert(first.result(2100).elapsedMs==50);
    TripRunStatistics firstWrap;
    firstWrap.update(100,20,100,UINT32_MAX-499,true,360,400,true,500);
    assert(firstWrap.result(500).elapsedMs==1000);
    // Stale/missing GPS and battery data retains observed peaks/totals. Wall
    // elapsed keeps running while measured moving/energy intervals are omitted.
    r=s.result(15000);
    assert(r.elapsedMs==14000&&!r.speedFresh&&!r.distanceComplete&&!r.movingComplete&&!r.energyComplete&&!r.usedCapacityComplete);
    assert(r.maxSpeedValid&&r.powerValid&&r.peakCurrentValid&&r.usedCapacityValid);
    sample(s,15000,120);r=s.result(15000);
    assert(r.movingMs==10000&&r.usedMah==20&&!r.movingComplete&&!r.distanceComplete&&!r.usedCapacityComplete);
    close(r.kilometres,0.1);close(r.wattHours,20*10/3600.0);
    sample(s,16000,125);r=s.result(16000);
    assert(r.movingMs==11000&&r.usedMah==25);close(r.kilometres,0.11);close(r.wattHours,20*11/3600.0);
    s.update(100,20,125,16100,false,360,16100,false,16100);
    sample(s,16000,125); // A repeated old packet cannot mend a broken interval.
    assert(!s.result(16100).speedFresh);
    sample(s,17000,127);r=s.result(17000);
    assert(r.movingMs==11000&&r.usedMah==27);close(r.kilometres,0.11);
    sample(s,18000,130);r=s.result(18000);
    assert(r.movingMs==12000&&r.usedMah==30);close(r.kilometres,0.12);
    // A restarting CU01 counter rebases without underflow or claiming zero
    // historical use. The new baseline remains flagged as incomplete.
    sample(s,19000,2);r=s.result(19000);
    assert(r.usedMah==30&&!r.usedCapacityComplete);
    sample(s,20000,5);assert(s.result(20000).usedMah==33);
    // Out-of-order packets cannot overwrite baselines or inject bogus peaks.
    s.update(65535,65535,0xffffffffu,19500,true,65535,19500,true,20100);
    r=s.result(20100);assert(!r.speedFresh&&r.movingMs==14000&&r.usedMah==33);
    close(r.maxSpeedKmh,36);close(r.peakWatts,20);close(r.peakAmps,2);
    sample(s,21000,6);r=s.result(21000);
    assert(r.movingMs==14000&&r.usedMah==34);close(r.kilometres,0.14);
    sample(s,22000,9);r=s.result(22000);
    assert(r.movingMs==15000&&r.usedMah==37);close(r.kilometres,0.15);
    // Shared Trip/Run reset needs a new packet from each independent source,
    // so a cached sample from the old model cannot become a new session.
    s.reset(22000,22000);sample(s,22000,9);
    r=s.result(22000);
    assert(!r.elapsedValid&&!r.distanceValid&&!r.maxSpeedValid&&!r.powerValid&&!r.usedCapacityValid);
    s.update(100,20,9,22000,true,360,22100,true,22100);
    r=s.result(22100);
    assert(r.elapsedValid&&r.elapsedMs==0&&r.distanceValid&&!r.powerValid&&!r.usedCapacityValid);
    sample(s,22200,12);r=s.result(22200);
    assert(r.usedMah==0&&r.elapsedMs==100&&r.movingMs==100);
    sample(s,23200,15);assert(s.result(23200).usedMah==3);
    s.reset();assert(!s.result(23200).elapsedValid);
    // Telemetry and UI milliseconds wrap independently without resetting the
    // session; long elapsed time accumulates in 64 bits across UI updates.
    TripRunStatistics wrap;
    sample(wrap,UINT32_MAX-499);sample(wrap,500,101);
    r=wrap.result(500);assert(r.elapsedMs==1000&&r.movingMs==1000&&r.usedMah==1);
    close(r.kilometres,0.01);assert(r.distanceComplete&&r.movingComplete&&r.usedCapacityComplete);
    const uint32_t sequence[]={0x70000000u,0xe0000000u,0x50000000u,0xc0000000u,0x30000000u};
    uint64_t expected=1000;uint32_t last=500;
    for(uint32_t now:sequence){
        wrap.update(0,0,0,0,false,0,0,false,now);
        expected+=uint32_t(now-last);last=now;
        assert(wrap.result(now).elapsedMs==expected);
    }
    assert(expected>UINT32_MAX);
    // Going backward in caller time must not double-count the next forward tick.
    auto before=wrap.result(last).elapsedMs;
    wrap.update(0,0,0,0,false,0,0,false,last-100);
    wrap.update(0,0,0,0,false,0,0,false,last+100);
    assert(wrap.result(last+100).elapsedMs==before+100);
    // CU01 and GPS are independent: either can start a useful partial session.
    TripRunStatistics batteryOnly;
    batteryOnly.update(160,100,500,1000,true,0,0,false,1000);
    batteryOnly.update(150,200,510,2000,true,0,0,false,2000);
    r=batteryOnly.result(2000);
    assert(r.elapsedMs==1000&&!r.distanceValid&&!r.movingValid&&!r.maxSpeedValid&&r.usedMah==10);
    close(r.peakWatts,300);close(r.peakAmps,20);close(r.wattHours,230.0/3600);
    TripRunStatistics gpsOnly;
    gpsOnly.update(0,0,0,0,false,360,1000,true,1000);
    gpsOnly.update(0,0,0,0,false,360,2000,true,2000);
    r=gpsOnly.result(2000);
    assert(r.elapsedMs==1000&&r.movingMs==1000&&r.averageMovingValid&&!r.powerValid&&!r.energyValid&&!r.usedCapacityValid);
    close(r.kilometres,0.01);
    // Large wire values are promoted before arithmetic, not 16/32-bit wrapped.
    TripRunStatistics maximum;
    sample(maximum,1000,0,65535,65535,65535);sample(maximum,4000,0xffffffu,65535,65535,65535);
    r=maximum.result(4000);
    assert(r.movingMs==3000&&r.usedMah==0xffffffu);
    close(r.maxSpeedKmh,6553.5);close(r.peakWatts,42948362.25);
    close(r.kilometres,6553.5*3/3600);close(r.wattHours,42948362.25*3/3600);
    puts("Trip/Run: distance/moving time, wall elapsed, peaks, CU01 deltas, independent sources, gaps/reset/order/wrap PASS");
}

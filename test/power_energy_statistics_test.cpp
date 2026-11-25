#include "power_energy_statistics.h"
#include <cassert>
#include <cmath>
#include <cstdio>

static void close(double actual,double expected){assert(std::fabs(actual-expected)<1e-8*(1+std::fabs(expected)));}
static void sample(PowerEnergyStatistics &s,uint32_t stamp,uint16_t amps=20,uint16_t speed=360){
    s.update(100,amps,stamp,true,speed,stamp,true,stamp);
}
int main(){
    PowerEnergyStatistics s;
    auto r=s.result(0);assert(!r.powerValid&&!r.peakValid&&!r.energyValid&&!r.distanceValid&&!r.efficiencyValid);
    s.update(0,0,0,false,0,0,false,0); // Waiting for first telemetry is not a lost measured interval.
    sample(s,1000);
    r=s.result(1000);assert(r.powerValid&&r.peakValid&&r.energyValid&&r.energyComplete&&r.distanceComplete);
    close(r.watts,20);close(r.wattHours,0);
    for(uint32_t n=1;n<=10;++n)sample(s,1000+n*1000);
    r=s.result(11000);close(r.wattHours,20*10/3600.0);close(r.kilometres,0.1);
    close(r.wattHoursPerKm,20/36.0);assert(r.efficiencyValid&&r.efficiencyComplete&&r.energyComplete);
    s.update(65535,65535,11000,true,65535,11000,true,11001);
    auto duplicate=s.result(11001);close(duplicate.peakWatts,20);close(duplicate.wattHours,r.wattHours);close(duplicate.kilometres,0.1);
    assert(!s.result(15000).powerValid&&!s.result(15000).energyComplete);
    sample(s,15000);r=s.result(15000);
    close(r.wattHours,20*10/3600.0);close(r.kilometres,0.1);
    assert(!r.energyComplete&&!r.distanceComplete&&!r.efficiencyComplete);
    sample(s,16000);r=s.result(16000);close(r.wattHours,20*11/3600.0);close(r.kilometres,0.11);

    PowerEnergyStatistics ramp;
    sample(ramp,1000,0,0);sample(ramp,2000,40,0);
    close(ramp.result(2000).wattHours,20/3600.0);close(ramp.result(2000).peakWatts,40);
    ramp.update(100,100,1500,true,0,1500,true,2100); // Out-of-order never lowers either baseline.
    assert(!ramp.result(2100).powerValid&&!ramp.result(2100).energyComplete);
    sample(ramp,2500,40,0);close(ramp.result(2500).wattHours,20/3600.0);
    sample(ramp,3500,40,0);close(ramp.result(3500).wattHours,60/3600.0);

    PowerEnergyStatistics missing;
    sample(missing,1000);
    missing.update(100,20,1100,false,360,1100,true,1100);
    sample(missing,1000); // Old packet still cannot restore a broken baseline.
    assert(!missing.result(1200).powerValid);
    sample(missing,1300);sample(missing,2300);
    close(missing.result(2300).wattHours,20/3600.0);assert(!missing.result(2300).energyComplete);
    missing.reset(2300,2300);sample(missing,2300);
    r=missing.result(2300);assert(!r.energyValid&&!r.peakValid&&!r.powerValid&&!r.distanceValid);
    sample(missing,2400);close(missing.result(2400).wattHours,0);
    sample(missing,3400);close(missing.result(3400).wattHours,20/3600.0);

    PowerEnergyStatistics wrap;
    sample(wrap,UINT32_MAX-499);sample(wrap,500);
    close(wrap.result(500).wattHours,20/3600.0);close(wrap.result(500).kilometres,0.01);
    assert(wrap.result(500).energyComplete&&wrap.result(500).distanceComplete);

    PowerEnergyStatistics maximum;
    maximum.update(65535,65535,1000,true,65535,1000,true,1000);
    maximum.update(65535,65535,4000,true,65535,4000,true,4000);
    r=maximum.result(4000);
    close(r.watts,42948362.25);close(r.wattHours,42948362.25*3/3600.0);
    close(r.kilometres,6553.5*3/3600.0);close(r.wattHoursPerKm,6553.5);
    assert(r.efficiencyValid&&r.energyComplete);

    PowerEnergyStatistics stopped;
    for(uint32_t stamp=1000;stamp<=11000;stamp+=1000)sample(stopped,stamp,20,9);
    r=stopped.result(11000);close(r.kilometres,0);close(r.wattHours,20*10/3600.0);assert(!r.efficiencyValid);

    PowerEnergyStatistics asynchronous;
    asynchronous.update(100,20,1000,true,200,1000,true,1000);
    asynchronous.update(100,20,2000,true,600,2100,true,2100);
    r=asynchronous.result(2100);
    close(r.efficiencyKilometres,20/3600.0); // Uses preceding GPS speed, never future speed.
    close(r.kilometres,40*1.1/3600.0);
    asynchronous.update(100,20,3000,true,600,3100,true,3100);
    close(asynchronous.result(3100).efficiencyKilometres,60/3600.0);

    PowerEnergyStatistics unmatched;
    sample(unmatched,1000,20,360);
    unmatched.update(100,20,2000,true,360,1000,true,2000);
    unmatched.update(100,20,3000,true,360,1000,true,3000);
    r=unmatched.result(3000);close(r.wattHours,40/3600.0);
    close(r.efficiencyKilometres,0.01);assert(!r.efficiencyValid&&!r.efficiencyComplete);
    // Missing GPS cannot fabricate distance or pair total energy to unrelated later distance.
    for(uint32_t stamp=4000;stamp<=13000;stamp+=1000)sample(unmatched,stamp,20,360);
    r=unmatched.result(13000);assert(r.efficiencyValid&&!r.efficiencyComplete);
    close(r.wattHoursPerKm,20/36.0);

    puts("Power/energy: CU01 trapezoids, timestamps, duplicates, gaps, reset/wrap, extremes and matched GPS efficiency PASS");
}

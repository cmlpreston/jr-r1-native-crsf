#include "drive_battery.h"
#include "sensor_statistics.h"
#include "crsf_telemetry.h"
#include <cassert>
#include <cstdio>
int main(){
    DriveBatteryAlarm alarm;
    assert(!alarm.update(false,0,14000,400,100));
    assert(!alarm.update(true,13900,14000,400,200));
    assert(!alarm.update(true,13900,14000,400,2199));
    assert(alarm.update(true,13900,14000,400,2200)&&alarm.active());
    assert(!alarm.update(true,14100,14000,400,3000)&&alarm.active());
    assert(alarm.update(true,13900,14000,400,12200));
    assert(!alarm.update(true,14401,14000,400,13000)&&!alarm.active());
    assert(!alarm.update(true,13000,14000,400,14000));
    assert(!alarm.update(false,13000,14000,400,15000));
    assert(!alarm.update(true,13000,14000,400,16000));
    assert(!alarm.active());
    SensorStatistics stats;
    stats.update(30000,1000,1000,true);stats.update(1000,1000,1000,true);
    stats.update(10000,2000,2000,true);
    auto result=stats.result(true,2000);assert(result.valid&&result.maximum==30000&&result.average==20000);
    assert(!stats.result(false,32000).valid);
    assert(stats.result(true,32000).maximum==30000);
    stats.update(40000,2000,32000,true);assert(stats.result(true,32000).maximum==30000);
    stats.update(-2000,32000,32000,true);assert(stats.result(false,32000).average==-2000);
    stats.reset(32000);stats.update(90000,32000,32000,true);assert(!stats.result(true,32000).valid);
    stats.update(200,33000,33000,true);assert(stats.result(true,33000).average==200);
    assert(crsfPowerMilliwatts(7)==250&&crsfPowerMilliwatts(4)==500&&crsfPowerMilliwatts(5)==1000);
    assert(crsfPowerMilliwatts(99)==0);
    puts("First-drive battery alarms, session/window statistics and RF power units PASS");
}

#include "race_timer.h"
#include <cassert>
#include <cstring>
int main(){
 RaceTimer t;t.settings.mode=TimerMode::Lap;t.tick(0);t.primary(0);
 assert(!t.recordLap(2999));assert(t.recordLap(3000));
 t.pause(4000);t.tick(10000);assert(t.elapsed()==4000);
 t.resume(12000);assert(t.recordLap(14000));assert(t.lap(1)==3000&&t.bestLap()==3000&&t.averageLap()==3000);
 t.reset(14000);assert(t.lapCount()==2&&t.elapsed()==0&&!t.running());assert(t.clearLaps());
 t.settings.autoStart=true;assert(t.setReady(0));t.tick(500,300,true);assert(!t.running());
 t.tick(600,0,true);t.tick(700,300,false);t.tick(800,300,true);assert(!t.running());
 t.tick(900,0,true);t.tick(1000,100,true);assert(t.running());t.tick(1100);assert(t.elapsed()==100);
 t.reset(0);t.settings.durationSeconds=5;t.settings.targetSeconds=2;t.primary(0);
 t.tick(2000);assert(t.takeAlarms()==RaceTimer::Target);assert(t.takeAlarms()==0);
 t.tick(6000);assert(t.takeAlarms()==(RaceTimer::Target|RaceTimer::RaceEnd));t.tick(6100);assert(!t.takeAlarms());
 t.pause(6100);assert(t.clearLaps());t.reset(0);t.settings.minimumLapSeconds=1;t.primary(0);
 for(unsigned i=1;i<=100;++i)assert(t.recordLap(i*1000));assert(!t.recordLap(101000)&&t.full());
 RaceTimer other;assert(other.lapCount()==0);t.reset(0);assert(t.lapCount()==100);
 RaceTimer rollover;rollover.resume(0xfffffff0);rollover.tick(0x20);assert(rollover.elapsed()==48);
 RaceTimer down;down.settings.mode=TimerMode::Down;down.settings.durationSeconds=1;down.resume(0);down.tick(1500);
 assert(down.display()==-500);char text[32];formatTimer(text,sizeof(text),down.display(),true);assert(!strcmp(text,"+00:00.50"));
 assert(validTimerSettings(down.settings));down.settings.mode=static_cast<TimerMode>(255);assert(!validTimerSettings(down.settings));
 puts("Race timer: laps, pause accounting, ready/validity gates, alarms, capacity, isolation and wraparound PASS");
}

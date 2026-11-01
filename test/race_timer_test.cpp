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
 // Countdown warnings follow elapsed timer time, not wall time, exactly once.
 RaceTimer cues;cues.settings.mode=TimerMode::Down;cues.settings.durationSeconds=30;cues.resume(0);
 cues.tick(9999);assert(!cues.takeAlarms());cues.tick(10000);assert(cues.takeAlarms()==RaceTimer::Remaining20);
 cues.tick(10001);assert(!cues.takeAlarms());cues.pause(15000);cues.tick(90000);assert(!cues.takeAlarms());
 cues.resume(100000);cues.tick(105000);assert(cues.takeAlarms()==RaceTimer::Remaining10);
 cues.tick(109999);assert(!cues.takeAlarms());
 for(unsigned second=5;second>=1;--second){cues.tick(115000-second*1000);assert(cues.takeAlarms()==RaceTimer::CountdownTick);}
 cues.tick(115000);assert(cues.takeAlarms()==RaceTimer::RaceEnd);cues.tick(116000);assert(!cues.takeAlarms());
 cues.reset(200000);cues.resume(200000);cues.tick(210000);assert(cues.takeAlarms()==RaceTimer::Remaining20);
 // Short countdowns start with only their applicable cue; no warnings above it.
 RaceTimer shortDown;shortDown.settings.mode=TimerMode::Down;shortDown.settings.durationSeconds=5;shortDown.resume(0);
 shortDown.tick(0);assert(shortDown.takeAlarms()==RaceTimer::CountdownTick);
 shortDown.tick(5000);auto skipped=shortDown.takeAlarms();assert(skipped&RaceTimer::RaceEnd);
 assert(!(skipped&(RaceTimer::Remaining20|RaceTimer::Remaining10)));
 // Muted milestones are consumed, not queued to sound after unmuting.
 RaceTimer muted;muted.settings.mode=TimerMode::Down;muted.settings.durationSeconds=30;muted.settings.raceAlarm=false;
 muted.resume(0);muted.tick(10000);assert(!muted.takeAlarms());muted.settings.raceAlarm=true;
 muted.tick(11000);assert(!muted.takeAlarms());muted.tick(20000);assert(muted.takeAlarms()==RaceTimer::Remaining10);
 TimerBuzzerSequence sounds;sounds.begin(RaceTimer::Remaining20,TimerMode::Down,0);
 assert(sounds.poll(0)==100&&sounds.active(99)&&!sounds.active(100));assert(!sounds.poll(200));
 sounds.begin(RaceTimer::Remaining10,TimerMode::Down,1000);
 assert(sounds.poll(1000)==100);assert(!sounds.poll(1099));assert(!sounds.poll(1219));
 assert(sounds.poll(1220)==100);assert(!sounds.poll(1221));assert(!sounds.active(1320));
 sounds.begin(RaceTimer::Remaining10,TimerMode::Down,0);assert(sounds.poll(0)==100);
 sounds.cancel();assert(!sounds.active(100)&&!sounds.poll(220)); // pause/reset/mute cancellation
 sounds.begin(RaceTimer::Remaining10,TimerMode::Down,0);assert(sounds.poll(0)==100);
 assert(!sounds.poll(1000)&&!sounds.active(1000)); // stale second beep discarded
 sounds.begin(RaceTimer::RaceEnd|RaceTimer::CountdownTick|RaceTimer::Remaining10,TimerMode::Down,0);
 assert(sounds.poll(0)==800&&sounds.active(799)&&!sounds.active(800)); // end supersedes missed cues
 sounds.begin(RaceTimer::CountdownTick|RaceTimer::Remaining10,TimerMode::Down,0);assert(sounds.poll(0)==100);
 assert(!sounds.poll(220)); // latest countdown second supersedes the older double cue
 sounds.begin(RaceTimer::Remaining10,TimerMode::Down,0xfffffff0);assert(sounds.poll(0xfffffff0)==100);
 assert(!sounds.poll(0xcb));assert(sounds.poll(0xcc)==100); // scheduler clock wrap
 sounds.begin(RaceTimer::Target,TimerMode::Lap,0);assert(sounds.poll(0)==100);
 sounds.begin(RaceTimer::RaceEnd,TimerMode::Up,0);assert(sounds.poll(0)==450);
 puts("Countdown sound cues: milestones, pause/resume/reset, short runs, mute, non-blocking double beep, priority and wraparound PASS");
 puts("Race timer: laps, pause accounting, ready/validity gates, alarms, capacity, isolation and wraparound PASS");
}

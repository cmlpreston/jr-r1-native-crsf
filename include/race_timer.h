#pragma once
#include <cstdint>
#include <cstdio>
#include <limits>

enum class TimerMode:uint8_t { Up,Down,Lap };
struct TimerSettings {
    uint32_t version=1;
    TimerMode mode=TimerMode::Up;
    bool autoStart=false,raceAlarm=true;
    uint8_t reserved=0;
    uint16_t durationSeconds=300,targetSeconds=0,minimumLapSeconds=3;
    uint16_t padding=0;
    uint32_t onSeconds=0;
};
inline bool validTimerSettings(const TimerSettings &s){return s.version==1&&
    unsigned(s.mode)<=unsigned(TimerMode::Lap)&&s.durationSeconds>=1&&s.durationSeconds<=5999&&
    s.targetSeconds<=5999&&s.minimumLapSeconds>=1&&s.minimumLapSeconds<=60;}
inline const char *timerModeName(TimerMode mode){return mode==TimerMode::Lap?"Lap":mode==TimerMode::Down?"Down":"Up";}
inline void formatTimer(char *out,size_t size,int64_t ms,bool hundredths=false){
    const bool overtime=ms<0;const uint64_t value=overtime?uint64_t(-ms):uint64_t(ms);
    if(hundredths)std::snprintf(out,size,"%s%02llu:%02llu.%02llu",overtime?"+":"",
        (unsigned long long)(value/60000),(unsigned long long)(value/1000%60),(unsigned long long)(value/10%100));
    else std::snprintf(out,size,"%s%02llu:%02llu",overtime?"+":"",
        (unsigned long long)(value/60000),(unsigned long long)(value/1000%60));
}
class RaceTimer {
public:
    static constexpr unsigned MaxLaps=100;
    enum Alarm:uint8_t { None=0,RaceEnd=1,Target=2 };
    TimerSettings settings;
    bool running()const{return running_;}
    bool ready()const{return ready_;}
    uint64_t elapsed()const{return elapsed_;}
    uint64_t currentLap()const{return elapsed_-lapStart_;}
    int64_t display()const{return settings.mode==TimerMode::Down?int64_t(settings.durationSeconds)*1000-int64_t(elapsed_):
        int64_t(settings.mode==TimerMode::Lap?currentLap():elapsed_);}
    unsigned lapCount()const{return count_;}
    uint32_t lap(unsigned index)const{return index<count_?laps_[index]:0;}
    uint32_t lastLap()const{return count_?laps_[count_-1]:0;}
    uint32_t bestLap()const{uint32_t best=UINT32_MAX;for(unsigned i=0;i<count_;++i)if(laps_[i]<best)best=laps_[i];return count_?best:0;}
    uint64_t totalLaps()const{uint64_t total=0;for(unsigned i=0;i<count_;++i)total+=laps_[i];return total;}
    uint32_t averageLap()const{return count_?uint32_t(totalLaps()/count_):0;}
    bool full()const{return count_==MaxLaps;}
    void tick(uint32_t now,int forward=0,bool validInput=false){
        const uint32_t delta=clockSet_?uint32_t(now-lastTick_):0;lastTick_=now;clockSet_=true;
        if(running_)elapsed_+=delta;
        if(ready_){
            if(!validInput)neutralSeen_=false;
            else if(forward>=-50&&forward<=50)neutralSeen_=true;
            else if(neutralSeen_&&forward>=100){ready_=false;running_=true;}
        }
        if(!running_)return;
        if(settings.raceAlarm&&!endAlarm_&&elapsed_>=uint64_t(settings.durationSeconds)*1000){alarms_|=RaceEnd;endAlarm_=true;}
        if(settings.mode==TimerMode::Lap&&settings.targetSeconds){
            const uint64_t period=uint64_t(settings.targetSeconds)*1000;
            const uint64_t interval=currentLap()/period;
            if(interval>targetInterval_){alarms_|=Target;targetInterval_=interval;}
        }
    }
    void pause(uint32_t now){tick(now);running_=false;ready_=false;neutralSeen_=false;}
    void resume(uint32_t now){tick(now);ready_=false;running_=true;}
    void toggle(uint32_t now){if(running_)pause(now);else resume(now);}
    void reset(uint32_t now){running_=ready_=neutralSeen_=false;elapsed_=lapStart_=targetInterval_=0;
        lastTick_=now;clockSet_=true;endAlarm_=false;alarms_=None;}
    bool setReady(uint32_t now){if(!settings.autoStart)return false;reset(now);ready_=true;return true;}
    bool recordLap(uint32_t now){
        tick(now);
        if(!running_)return false;
        const uint64_t duration=currentLap();
        if(duration<uint64_t(settings.minimumLapSeconds)*1000||duration>UINT32_MAX||full())return false;
        laps_[count_++]=uint32_t(duration);lapStart_=elapsed_;targetInterval_=0;return true;
    }
    bool primary(uint32_t now){
        if(settings.mode!=TimerMode::Lap){toggle(now);return true;}
        if(!running_){resume(now);return true;}
        return recordLap(now);
    }
    bool clearLaps(){if(running_||ready_)return false;count_=0;lapStart_=elapsed_;targetInterval_=0;return true;}
    uint8_t takeAlarms(){const auto result=alarms_;alarms_=None;return result;}
private:
    uint32_t laps_[MaxLaps]={};
    uint64_t elapsed_=0,lapStart_=0,targetInterval_=0;
    uint32_t lastTick_=0;
    uint16_t count_=0;
    bool clockSet_=false,running_=false,ready_=false,neutralSeen_=false,endAlarm_=false;
    uint8_t alarms_=None;
};

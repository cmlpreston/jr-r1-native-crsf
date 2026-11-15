#include "gps_local_time.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>

using GpsLocalTime::sydneyMelbourne;
using GpsLocalTime::Zone;
using GpsLocalTime::Settings;

static Settings settings(Zone zone,int16_t offset=600) {
    Settings result;result.zone=zone;result.customOffsetMinutes=offset;return result;
}

static void expectZone(const Settings &setting,unsigned y,unsigned m,unsigned d,
                       unsigned h,unsigned n,unsigned s,unsigned ly,unsigned lm,
                       unsigned ld,unsigned lh,unsigned ln,unsigned ls,
                       int offset,bool daylight=false) {
    const auto original=setting;
    const auto t=GpsLocalTime::convert(setting,y,m,d,h,n,s);
    assert(t.valid&&t.year==ly&&t.month==lm&&t.day==ld);
    assert(t.hour==lh&&t.minute==ln&&t.second==ls);
    assert(t.offsetMinutes==offset&&t.daylightSaving==daylight);
    assert(setting.zone==original.zone&&setting.customOffsetMinutes==original.customOffsetMinutes);
}

static void expect(unsigned y, unsigned m, unsigned d, unsigned h, unsigned n, unsigned s,
                   unsigned ly, unsigned lm, unsigned ld, unsigned lh, unsigned ln,
                   unsigned ls, bool daylight) {
    const auto t = sydneyMelbourne(y,m,d,h,n,s);
    assert(t.valid && t.year == ly && t.month == lm && t.day == ld);
    assert(t.hour == lh && t.minute == ln && t.second == ls);
    assert(t.daylightSaving == daylight && t.offsetMinutes == (daylight ? 660 : 600));
}

int main() {
    // Published 2026/2027 eastern-Australian transitions, checked one second
    // before and at the instant; neither duplicated nor skipped hours are UTC.
    expect(2026,4,4,15,59,59, 2026,4,5,2,59,59,true);
    expect(2026,4,4,16,0,0,   2026,4,5,2,0,0,false);
    expect(2026,10,3,15,59,59,2026,10,4,1,59,59,false);
    expect(2026,10,3,16,0,0,  2026,10,4,3,0,0,true);
    expect(2027,4,3,15,59,59, 2027,4,4,2,59,59,true);
    expect(2027,4,3,16,0,0,   2027,4,4,2,0,0,false);
    expect(2027,10,2,15,59,59,2027,10,3,1,59,59,false);
    expect(2027,10,2,16,0,0,  2027,10,3,3,0,0,true);

    // First Sunday on day1 puts the UTC transition in the preceding month.
    expect(2023,9,30,15,59,59,2023,10,1,1,59,59,false);
    expect(2023,9,30,16,0,0,  2023,10,1,3,0,0,true);
    expect(2029,3,31,15,59,59,2029,4,1,2,59,59,true);
    expect(2029,3,31,16,0,0,  2029,4,1,2,0,0,false);

    expect(2026,11,15,11,32,6,2026,11,15,22,32,6,true);
    expect(2026,6,15,13,59,59,2026,6,15,23,59,59,false);
    expect(2026,6,15,14,0,0,  2026,6,16,0,0,0,false);
    expect(2026,4,30,18,30,0, 2026,5,1,4,30,0,false);
    expect(2024,2,28,13,30,0, 2024,2,29,0,30,0,true);
    expect(2024,2,29,13,30,0, 2024,3,1,0,30,0,true);
    expect(2100,2,28,13,30,0, 2100,3,1,0,30,0,true);
    expect(2000,2,28,13,30,0, 2000,2,29,0,30,0,true);
    expect(2400,2,28,13,30,0, 2400,2,29,0,30,0,true);
    expect(2026,12,31,15,0,0, 2027,1,1,2,0,0,true);
    expect(9999,12,31,23,59,59,10000,1,1,10,59,59,true);

    // Half-hour Adelaide transitions use16:30UTC, not the eastern16:00UTC.
    const auto adelaide=settings(Zone::Adelaide);
    expectZone(adelaide,2026,4,4,16,29,59,2026,4,5,2,59,59,630,true);
    expectZone(adelaide,2026,4,4,16,30,0,2026,4,5,2,0,0,570);
    expectZone(adelaide,2026,10,3,16,29,59,2026,10,4,1,59,59,570);
    expectZone(adelaide,2026,10,3,16,30,0,2026,10,4,3,0,0,630,true);
    expectZone(adelaide,2023,9,30,16,29,59,2023,10,1,1,59,59,570);
    expectZone(adelaide,2023,9,30,16,30,0,2023,10,1,3,0,0,630,true);
    expectZone(adelaide,2029,3,31,16,29,59,2029,4,1,2,59,59,630,true);
    expectZone(adelaide,2029,3,31,16,30,0,2029,4,1,2,0,0,570);

    // Named presets stay independent of customOffsetMinutes. Fixed zones do
    // not adopt the southern states' summer adjustment.
    for(unsigned month:{1U,6U,11U}) {
        expectZone(settings(Zone::Brisbane,-720),2026,month,15,11,32,6,
                   2026,month,15,21,32,6,600);
        expectZone(settings(Zone::Darwin,840),2026,month,15,11,32,6,
                   2026,month,15,21,2,6,570);
        expectZone(settings(Zone::Perth),2026,month,15,11,32,6,
                   2026,month,15,19,32,6,480);
        expectZone(settings(Zone::Utc),2026,month,15,11,32,6,
                   2026,month,15,11,32,6,0);
    }
    expectZone(adelaide,2026,11,15,11,32,6,2026,11,15,22,2,6,630,true);
    expectZone(adelaide,2026,6,15,11,32,6,2026,6,15,21,2,6,570);
    expectZone(settings(Zone::Custom,345),2026,11,15,22,20,17,2026,11,16,4,5,17,345);
    expectZone(settings(Zone::Custom,-210),2026,11,15,1,20,17,2026,11,14,21,50,17,-210);
    expectZone(settings(Zone::Custom,-720),2000,1,1,0,0,0,1999,12,31,12,0,0,-720);
    expectZone(settings(Zone::Custom,-720),2024,3,1,0,0,0,2024,2,29,12,0,0,-720);
    expectZone(settings(Zone::Custom,-720),2100,3,1,0,0,0,2100,2,28,12,0,0,-720);
    expectZone(settings(Zone::Custom,840),9999,12,31,23,59,59,10000,1,1,13,59,59,840);

    assert(GpsLocalTime::sanitize(settings(Zone::Custom,-32768)).customOffsetMinutes==-720);
    assert(GpsLocalTime::sanitize(settings(Zone::Custom,32767)).customOffsetMinutes==840);
    assert(GpsLocalTime::sanitize(settings(Zone::Custom,16)).customOffsetMinutes==15);
    assert(GpsLocalTime::sanitize(settings(Zone::Custom,-16)).customOffsetMinutes==-15);
    assert(GpsLocalTime::sanitize(settings(Zone::Custom,14)).customOffsetMinutes==0);
    assert(GpsLocalTime::sanitize(settings(Zone::Custom,-14)).customOffsetMinutes==0);
    assert(GpsLocalTime::sanitize(settings(static_cast<Zone>(255))).zone==Zone::SydneyMelbourne);
    assert(GpsLocalTime::sanitize(settings(Zone::Count)).zone==Zone::SydneyMelbourne);
    expectZone(settings(static_cast<Zone>(255)),2026,11,15,11,32,6,
               2026,11,15,22,32,6,660,true);
    expectZone(settings(Zone::Custom,32767),2026,11,15,11,32,6,
               2026,11,16,1,32,6,840);
    assert(GpsLocalTime::observesDaylightSaving(Zone::SydneyMelbourne));
    assert(GpsLocalTime::observesDaylightSaving(Zone::Adelaide));
    assert(!GpsLocalTime::observesDaylightSaving(Zone::Brisbane));
    assert(!GpsLocalTime::observesDaylightSaving(Zone::Custom));
    for(unsigned z=0;z<unsigned(Zone::Count);++z)
        assert(GpsLocalTime::zoneName(static_cast<Zone>(z))[0]);

    // Every permitted custom offset crosses at most one date boundary. Use
    // independently calculated minute/day carry, including both range ends.
    for(int offset=-720;offset<=840;offset+=15)for(unsigned h=0;h<24;++h) {
        const int shifted=int(h)*60+7+offset;
        const int dayCarry=shifted<0?-1:shifted>=1440?1:0;
        const unsigned normalized=unsigned((shifted+1440)%1440);
        expectZone(settings(Zone::Custom,int16_t(offset)),2026,7,15,h,7,23,
                   2026,7,unsigned(15+dayCarry),normalized/60,normalized%60,23,offset);
    }

    assert(!sydneyMelbourne(0,1,1,0,0).valid);
    assert(!sydneyMelbourne(1999,12,31,23,59).valid);
    assert(!sydneyMelbourne(10000,1,1,0,0).valid);
    assert(!sydneyMelbourne(2026,0,1,0,0).valid);
    assert(!sydneyMelbourne(2026,13,1,0,0).valid);
    assert(!sydneyMelbourne(2026,1,0,0,0).valid);
    assert(!sydneyMelbourne(2026,4,31,0,0).valid);
    assert(!sydneyMelbourne(2026,2,29,0,0).valid);
    assert(!sydneyMelbourne(2100,2,29,0,0).valid);
    assert(!sydneyMelbourne(2026,1,1,24,0).valid);
    assert(!sydneyMelbourne(2026,1,1,0,60).valid);
    assert(!sydneyMelbourne(2026,1,1,0,0,60).valid);

    // Independent calendar walk, anchored at Saturday2000-01-01, checks the
    // transition date over the full decoder year range without sharing the
    // implementation's Gregorian weekday formula or packed-date comparison.
    unsigned weekday = 6;
    for (unsigned y=2000;y<=9999;++y) {
        const bool leap = y%4==0 && (y%100!=0 || y%400==0);
        unsigned firstAprilSunday=0,firstOctoberSunday=0;
        for (unsigned m=1;m<=12;++m) {
            const unsigned days[] = {31,28u+unsigned(leap),31,30,31,30,31,31,30,31,30,31};
            for (unsigned d=1;d<=days[m-1];++d) {
                if (weekday==0 && m==4 && !firstAprilSunday) firstAprilSunday=d;
                if (weekday==0 && m==10 && !firstOctoberSunday) firstOctoberSunday=d;
                weekday=(weekday+1)%7;
            }
        }
        const unsigned em=firstAprilSunday==1?3:4;
        const unsigned ed=firstAprilSunday==1?31:firstAprilSunday-1;
        const unsigned sm=firstOctoberSunday==1?9:10;
        const unsigned sd=firstOctoberSunday==1?30:firstOctoberSunday-1;
        expect(y,em,ed,15,59,59,y,4,firstAprilSunday,2,59,59,true);
        expect(y,em,ed,16,0,0,y,4,firstAprilSunday,2,0,0,false);
        expect(y,sm,sd,15,59,59,y,10,firstOctoberSunday,1,59,59,false);
        expect(y,sm,sd,16,0,0,y,10,firstOctoberSunday,3,0,0,true);
        expectZone(adelaide,y,em,ed,16,29,59,y,4,firstAprilSunday,2,59,59,630,true);
        expectZone(adelaide,y,em,ed,16,30,0,y,4,firstAprilSunday,2,0,0,570);
        expectZone(adelaide,y,sm,sd,16,29,59,y,10,firstOctoberSunday,1,59,59,570);
        expectZone(adelaide,y,sm,sd,16,30,0,y,10,firstOctoberSunday,3,0,0,630,true);
    }
    std::puts("GPS local time: Australian presets, Sydney/Adelaide DST boundaries, custom offsets, calendar rollover and invalid inputs PASS");
}

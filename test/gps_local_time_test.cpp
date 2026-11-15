#include "gps_local_time.h"
#include <cassert>
#include <cstdio>

using GpsLocalTime::sydneyMelbourne;

static void expect(unsigned y, unsigned m, unsigned d, unsigned h, unsigned n, unsigned s,
                   unsigned ly, unsigned lm, unsigned ld, unsigned lh, unsigned ln,
                   unsigned ls, bool daylight) {
    const auto t = sydneyMelbourne(y,m,d,h,n,s);
    assert(t.valid && t.year == ly && t.month == lm && t.day == ld);
    assert(t.hour == lh && t.minute == ln && t.second == ls);
    assert(t.daylightSaving == daylight && t.offsetHours == (daylight ? 11 : 10));
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
    }
    std::puts("GPS local time: Sydney/Melbourne DST boundaries, calendar rollover and invalid inputs PASS");
}

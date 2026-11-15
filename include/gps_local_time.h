#pragma once
#include <stdint.h>

// Display-only handset time. Australian daylight-saving presets use the
// current rule, not a historical timezone database; fixed/custom offsets do
// not observe daylight saving. No UTC telemetry, RTC or process TZ is changed.
namespace GpsLocalTime {
enum class Zone : uint8_t {
    SydneyMelbourne=0, Brisbane, Adelaide, Darwin, Perth, Utc, Custom, Count
};
struct Settings {
    Zone zone=Zone::SydneyMelbourne;
    int16_t customOffsetMinutes=600;
};
inline Settings sanitize(Settings settings) {
    if (static_cast<uint8_t>(settings.zone)>=static_cast<uint8_t>(Zone::Count))
        settings.zone=Zone::SydneyMelbourne;
    if (settings.customOffsetMinutes<-720) settings.customOffsetMinutes=-720;
    if (settings.customOffsetMinutes>840) settings.customOffsetMinutes=840;
    settings.customOffsetMinutes=int16_t(settings.customOffsetMinutes/15*15);
    return settings;
}
inline const char *zoneName(Zone zone) {
    switch (zone) {
        case Zone::SydneyMelbourne: return "Sydney / Melbourne";
        case Zone::Brisbane: return "Brisbane";
        case Zone::Adelaide: return "Adelaide";
        case Zone::Darwin: return "Darwin";
        case Zone::Perth: return "Perth";
        case Zone::Utc: return "UTC";
        case Zone::Custom: return "Custom offset";
        default: return "Sydney / Melbourne";
    }
}
inline bool observesDaylightSaving(Zone zone) {
    return zone==Zone::SydneyMelbourne || zone==Zone::Adelaide;
}
struct Time {
    bool valid = false;
    uint16_t year = 0;
    uint8_t month = 0, day = 0, hour = 0, minute = 0, second = 0;
    int16_t offsetMinutes = 0;
    bool daylightSaving = false;
};

namespace Detail {
inline bool leap(uint16_t year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}
inline uint8_t daysInMonth(uint16_t year, uint8_t month) {
    constexpr uint8_t days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return month >= 1 && month <= 12 ?
        uint8_t(days[month - 1] + (month == 2 && leap(year))) : 0;
}
// Gregorian weekday, Sunday=0. Input is already a valid calendar date.
inline uint8_t weekday(uint16_t year, uint8_t month, uint8_t day) {
    constexpr uint8_t offsets[] = {0,3,2,5,0,3,5,1,4,6,2,4};
    const uint32_t y = uint32_t(year) - (month < 3);
    return uint8_t((y + y / 4 - y / 100 + y / 400 + offsets[month - 1] + day) % 7);
}
inline uint32_t minuteKey(uint8_t month, uint8_t day, uint8_t hour, uint8_t minute) {
    return ((uint32_t(month) * 32 + day) * 24 + hour)*60+minute;
}
inline uint32_t transitionUtcKey(uint16_t year, uint8_t month, int16_t standardOffsetMinutes) {
    const uint8_t firstSunday = uint8_t(1 + (7 - weekday(year, month, 1)) % 7);
    // October starts at02:00 standard, April ends at03:00 daylight (+1hour).
    // Therefore both are the same UTC minute on the preceding calendar day:
    // 16:00 for Sydney/Melbourne or16:30 for Adelaide.
    const uint16_t minute=uint16_t(1440+120-standardOffsetMinutes);
    const uint8_t utcMonth=firstSunday>1?month:uint8_t(month-1);
    const uint8_t utcDay=firstSunday>1?uint8_t(firstSunday-1):daysInMonth(year,utcMonth);
    return minuteKey(utcMonth,utcDay,uint8_t(minute/60),uint8_t(minute%60));
}
inline void nextDay(Time &time) {
    if (++time.day>daysInMonth(time.year,time.month)) {
        time.day=1;
        if (++time.month>12) {time.month=1;++time.year;}
    }
}
inline void previousDay(Time &time) {
    if (time.day>1) {--time.day;return;}
    if (time.month>1) --time.month;
    else {time.month=12;--time.year;}
    time.day=daysInMonth(time.year,time.month);
}
} // namespace Detail

inline Time convert(const Settings &settings,uint16_t year,uint8_t month,uint8_t day,
                    uint8_t hour,uint8_t minute,uint8_t second=0) {
    Time result;
    if (year < 2000 || year > 9999 || month < 1 || month > 12 || day < 1 ||
        day > Detail::daysInMonth(year, month) || hour > 23 || minute > 59 || second > 59)
        return result;

    const auto clean=sanitize(settings);
    int16_t offset=0;
    switch (clean.zone) {
        case Zone::SydneyMelbourne: case Zone::Brisbane: offset=600;break;
        case Zone::Adelaide: case Zone::Darwin: offset=570;break;
        case Zone::Perth: offset=480;break;
        case Zone::Custom: offset=clean.customOffsetMinutes;break;
        default: break;
    }
    const auto key=Detail::minuteKey(month,day,hour,minute);
    const bool daylight=observesDaylightSaving(clean.zone) &&
        (key<Detail::transitionUtcKey(year,4,offset) ||
         key>=Detail::transitionUtcKey(year,10,offset));
    if (daylight) offset+=60;
    result.valid = true;
    result.year = year; result.month = month; result.day = day;
    result.second = second;
    result.daylightSaving = daylight;
    result.offsetMinutes=offset;
    int localMinute=int(hour)*60+minute+offset;
    // The supported offset range crosses at most one calendar boundary.
    if (localMinute<0) {localMinute+=1440;Detail::previousDay(result);}
    else if (localMinute>=1440) {localMinute-=1440;Detail::nextDay(result);}
    result.hour=uint8_t(localMinute/60);result.minute=uint8_t(localMinute%60);
    return result;
}
inline Time sydneyMelbourne(uint16_t year,uint8_t month,uint8_t day,
                           uint8_t hour,uint8_t minute,uint8_t second=0) {
    return convert(Settings{},year,month,day,hour,minute,second);
}
} // namespace GpsLocalTime

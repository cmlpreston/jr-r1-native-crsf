#pragma once
#include <stdint.h>

// Display-only Sydney/Melbourne time using the current Australian eastern
// daylight-saving rule. This is not a historical timezone database: the same
// modern rule applies throughout the GPS decoder's supported year range.
// No UTC telemetry, RTC, process timezone or persistent setting is changed.
namespace GpsLocalTime {
struct Time {
    bool valid = false;
    uint16_t year = 0;
    uint8_t month = 0, day = 0, hour = 0, minute = 0, second = 0;
    uint8_t offsetHours = 0;
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
inline uint32_t hourKey(uint8_t month, uint8_t day, uint8_t hour) {
    return (uint32_t(month) * 32 + day) * 24 + hour;
}
inline uint32_t transitionUtcKey(uint16_t year, uint8_t month) {
    const uint8_t firstSunday = uint8_t(1 + (7 - weekday(year, month, 1)) % 7);
    // Both transitions occur at 16:00 UTC on the preceding local-calendar day:
    // October 02:00 AEST -> 03:00 AEDT; April 03:00 AEDT -> 02:00 AEST.
    return firstSunday > 1 ? hourKey(month, uint8_t(firstSunday - 1), 16) :
        hourKey(uint8_t(month - 1), daysInMonth(year, uint8_t(month - 1)), 16);
}
} // namespace Detail

inline Time sydneyMelbourne(uint16_t year, uint8_t month, uint8_t day,
                           uint8_t hour, uint8_t minute, uint8_t second = 0) {
    Time result;
    if (year < 2000 || year > 9999 || month < 1 || month > 12 || day < 1 ||
        day > Detail::daysInMonth(year, month) || hour > 23 || minute > 59 || second > 59)
        return result;

    const auto key = Detail::hourKey(month, day, hour);
    const bool daylight = key < Detail::transitionUtcKey(year, 4) ||
        key >= Detail::transitionUtcKey(year, 10);
    result.valid = true;
    result.year = year; result.month = month; result.day = day;
    result.minute = minute; result.second = second;
    result.daylightSaving = daylight;
    result.offsetHours = daylight ? 11 : 10;
    result.hour = uint8_t(hour + result.offsetHours);
    if (result.hour >= 24) {
        result.hour -= 24;
        if (++result.day > Detail::daysInMonth(year, month)) {
            result.day = 1;
            if (++result.month > 12) {
                result.month = 1;
                ++result.year; // 9999-12-31 UTC can display a date in year10000.
            }
        }
    }
    return result;
}
} // namespace GpsLocalTime

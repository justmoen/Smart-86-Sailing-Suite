#include <cstdint>
#include <ctime>
#ifndef HW_RTC_H
#define HW_RTC_H

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct {
        uint16_t year;
        uint8_t  month;
        uint8_t  day;
        uint8_t  hour;
        uint8_t  minute;
        uint8_t  second;
        uint8_t  weekday; // 0 = Sunday
    } RTC_DateTime;

    void get_rtc_datetime(RTC_DateTime *dt) {
        time_t now;
        struct tm timeinfo;

        // Get internal system time
        time(&now);
        localtime_r(&now, &timeinfo);

        // Populate the single structure
        dt->year    = (uint16_t)(timeinfo.tm_year + 1900);
        dt->month   = (uint8_t)(timeinfo.tm_mon + 1);
        dt->day     = (uint8_t)timeinfo.tm_mday;
        dt->hour    = (uint8_t)timeinfo.tm_hour;
        dt->minute  = (uint8_t)timeinfo.tm_min;
        dt->second  = (uint8_t)timeinfo.tm_sec;
        dt->weekday = (uint8_t)timeinfo.tm_wday;
    }

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
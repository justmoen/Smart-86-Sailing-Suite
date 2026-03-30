#ifndef HW_RTC_H
#define HW_RTC_H

#include <cstdint>

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

// ✅ Declaration only (NO implementation here)
void get_rtc_datetime(RTC_DateTime *dt);

#ifdef __cplusplus
}
#endif

#endif
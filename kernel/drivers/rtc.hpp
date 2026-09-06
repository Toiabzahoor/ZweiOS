#pragma once

#include <stdint.h>
#include <stddef.h>

namespace drivers {

struct rtc_time_t {
    uint8_t  second;
    uint8_t  minute;
    uint8_t  hour;
    uint8_t  day;
    uint8_t  month;
    uint32_t year;
};

void rtc_init();
rtc_time_t rtc_get_time();
void rtc_format_date(const rtc_time_t* t, char* buf, size_t buf_size);
void rtc_format_time(const rtc_time_t* t, char* buf, size_t buf_size);

}

using drivers::rtc_time_t;
using drivers::rtc_init;
using drivers::rtc_get_time;
using drivers::rtc_format_date;
using drivers::rtc_format_time;

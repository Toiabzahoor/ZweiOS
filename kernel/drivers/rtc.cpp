#include "drivers/rtc.hpp"
#include "arch/x86_64/io.hpp"
#include "drivers/serial.hpp"

namespace drivers {

static constexpr uint16_t CMOS_ADDR_PORT = 0x70;
static constexpr uint16_t CMOS_DATA_PORT = 0x71;

static constexpr uint8_t RTC_REG_SECOND   = 0x00;
static constexpr uint8_t RTC_REG_MINUTE   = 0x02;
static constexpr uint8_t RTC_REG_HOUR     = 0x04;
static constexpr uint8_t RTC_REG_DAY      = 0x07;
static constexpr uint8_t RTC_REG_MONTH    = 0x08;
static constexpr uint8_t RTC_REG_YEAR     = 0x09;
static constexpr uint8_t RTC_REG_STATUS_A = 0x0A;
static constexpr uint8_t RTC_REG_STATUS_B = 0x0B;
static constexpr uint8_t RTC_REG_CENTURY  = 0x32;

static constexpr uint8_t NMI_DISABLE_BIT  = 0x80;

static inline uint8_t cmos_read_register(uint8_t reg) {
    arch::outb(CMOS_ADDR_PORT, static_cast<uint8_t>(NMI_DISABLE_BIT | (reg & 0x7F)));
    arch::io_wait();
    uint8_t val = arch::inb(CMOS_DATA_PORT);
    arch::outb(CMOS_ADDR_PORT, 0x00);
    arch::io_wait();
    return val;
}

static inline bool rtc_is_updating() {
    return (cmos_read_register(RTC_REG_STATUS_A) & 0x80) != 0;
}

static void rtc_wait_ready() {
    uint32_t timeout = 100000;
    while (rtc_is_updating() && --timeout > 0) {
        asm volatile("pause");
    }
}

static inline uint8_t bcd_to_bin(uint8_t val) {
    return static_cast<uint8_t>(((val >> 4) * 10) + (val & 0x0F));
}

rtc_time_t rtc_get_time() {
    rtc_time_t t{};
    rtc_wait_ready();

    uint8_t sec = 0, min = 0, hr = 0, day = 0, mon = 0, yr = 0, century = 0;
    uint8_t last_sec = 0, last_min = 0, last_hr = 0, last_day = 0, last_mon = 0, last_yr = 0, last_century = 0;

    int retries = 5;
    do {
        sec = cmos_read_register(RTC_REG_SECOND);
        min = cmos_read_register(RTC_REG_MINUTE);
        hr = cmos_read_register(RTC_REG_HOUR);
        day = cmos_read_register(RTC_REG_DAY);
        mon = cmos_read_register(RTC_REG_MONTH);
        yr = cmos_read_register(RTC_REG_YEAR);
        century = cmos_read_register(RTC_REG_CENTURY);

        if (retries < 5 &&
            sec == last_sec && min == last_min && hr == last_hr &&
            day == last_day && mon == last_mon && yr == last_yr && century == last_century) {
            break;
        }

        last_sec = sec;
        last_min = min;
        last_hr = hr;
        last_day = day;
        last_mon = mon;
        last_yr = yr;
        last_century = century;

        rtc_wait_ready();
    } while (--retries > 0);

    uint8_t reg_b = cmos_read_register(RTC_REG_STATUS_B);
    bool is_bcd = !(reg_b & 0x04);
    bool is_24h = (reg_b & 0x02) != 0;

    if (is_bcd) {
        sec = bcd_to_bin(sec);
        min = bcd_to_bin(min);
        bool pm = (hr & 0x80) != 0;
        hr = bcd_to_bin(hr & 0x7F);
        if (!is_24h) {
            if (pm && hr < 12) hr += 12;
            if (!pm && hr == 12) hr = 0;
        }
        day = bcd_to_bin(day);
        mon = bcd_to_bin(mon);
        yr = bcd_to_bin(yr);
        if (century != 0) {
            century = bcd_to_bin(century);
        }
    } else {
        if (!is_24h) {
            bool pm = (hr & 0x80) != 0;
            hr &= 0x7F;
            if (pm && hr < 12) hr += 12;
            if (!pm && hr == 12) hr = 0;
        }
    }

    uint32_t full_year = 0;
    if (century >= 19 && century <= 21) {
        full_year = static_cast<uint32_t>(century) * 100 + yr;
    } else {
        if (yr < 70) {
            full_year = 2000 + yr;
        } else {
            full_year = 1900 + yr;
        }
    }

    t.second = sec;
    t.minute = min;
    t.hour = hr;
    t.day = day;
    t.month = mon;
    t.year = full_year;

    return t;
}

void rtc_format_date(const rtc_time_t* t, char* buf, size_t buf_size) {
    if (!t || !buf || buf_size < 11) return;
    uint32_t y = t->year;
    buf[0] = static_cast<char>('0' + ((y / 1000) % 10));
    buf[1] = static_cast<char>('0' + ((y / 100) % 10));
    buf[2] = static_cast<char>('0' + ((y / 10) % 10));
    buf[3] = static_cast<char>('0' + (y % 10));
    buf[4] = '-';
    buf[5] = static_cast<char>('0' + ((t->month / 10) % 10));
    buf[6] = static_cast<char>('0' + (t->month % 10));
    buf[7] = '-';
    buf[8] = static_cast<char>('0' + ((t->day / 10) % 10));
    buf[9] = static_cast<char>('0' + (t->day % 10));
    buf[10] = '\0';
}

void rtc_format_time(const rtc_time_t* t, char* buf, size_t buf_size) {
    if (!t || !buf || buf_size < 13) return;
    buf[0] = static_cast<char>('0' + ((t->hour / 10) % 10));
    buf[1] = static_cast<char>('0' + (t->hour % 10));
    buf[2] = ':';
    buf[3] = static_cast<char>('0' + ((t->minute / 10) % 10));
    buf[4] = static_cast<char>('0' + (t->minute % 10));
    buf[5] = ':';
    buf[6] = static_cast<char>('0' + ((t->second / 10) % 10));
    buf[7] = static_cast<char>('0' + (t->second % 10));
    buf[8] = ' ';
    buf[9] = 'U';
    buf[10] = 'T';
    buf[11] = 'C';
    buf[12] = '\0';
}

void rtc_init() {
    rtc_get_time();
    drivers::serial_puts("[RTC] CMOS Real-Time Clock initialized\r\n");
}

}

#pragma once

#include <cstdint>

#include "util.h"

int32_t constexpr secs_per_min = 60;
int32_t constexpr min_per_hour = 60;
int32_t constexpr hour_per_day = 24;
int32_t constexpr secs_per_hour = secs_per_min * min_per_hour;
int32_t constexpr secs_per_day = secs_per_hour * hour_per_day;
int32_t constexpr min_per_day = min_per_hour * hour_per_day;

int32_t constexpr tai_minus_gps = 19;

bool time_test();

struct Ymdhms
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;

    Ymdhms(): Ymdhms(0, 0, 0, 0, 0, 0) {}

    Ymdhms(uint16_t year_, uint8_t month_, uint8_t day_, uint8_t hour_, uint8_t min_, uint8_t sec_);

    void set(uint16_t year_, uint8_t month_, uint8_t day_, uint8_t hour_, uint8_t min_, uint8_t sec_);

    inline void add_days(int64_t dt_days)
    {
        _from_gdays(_to_gdays() + dt_days);
    }

    void add_seconds(int32_t dt_seconds);

private:
    int64_t _to_gdays() const;
    void _from_gdays(int64_t gdays);

    int32_t _to_dsecs() const;
    void _from_dsecs(int32_t dsecs);
};

struct TopOfSecond
{
public:
    TopOfSecond()
    {
        invalidate();
    }

    void show() const
    {
        printf("%s %d-%02d-%02d %02d:%02d:%02d      %s %d-%02d-%02d %02d:%02d:%02d     %d %d     %d %" PRId32 " %" PRId32 "\n",
               utc_ymdhms_valid?"UTC":"utc",
               utc_ymdhms.year,
               utc_ymdhms.month,
               utc_ymdhms.day,
               utc_ymdhms.hour,
               utc_ymdhms.min,
               utc_ymdhms.sec,
               tai_ymdhms_valid?"TAI":"tai",
               tai_ymdhms.year,
               tai_ymdhms.month,
               tai_ymdhms.day,
               tai_ymdhms.hour,
               tai_ymdhms.min,
               tai_ymdhms.sec,
               gps_minus_utc_valid,
               gps_minus_utc,
               next_leap_second_valid,
               next_leap_second_direction,
               next_leap_second_time_until);

    }

    Ymdhms utc_ymdhms;
    bool utc_ymdhms_valid = false;

    Ymdhms tai_ymdhms;
    bool tai_ymdhms_valid = false;

    Ymdhms loc_ymdhms;
    bool loc_ymdhms_valid = false;
    static uint32_t constexpr pdt_minus_utc_seconds = -7 * secs_per_hour;
    static uint32_t constexpr pst_minus_utc_seconds = -8 * secs_per_hour;
    static uint32_t constexpr loc_minus_utc_seconds = pst_minus_utc_seconds;

    int8_t gps_minus_utc;
    bool gps_minus_utc_valid = false;

    int32_t next_leap_second_time_until;
    int32_t next_leap_second_direction;
    bool next_leap_second_valid = false;

    uint32_t error_count = 0;

    void invalidate();
    void set_next_leap_second(int32_t time_until, int32_t direction);
    void set_utc_ymdhms(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec);
    void set_gps_minus_utc(int8_t value);

    void set_from_prev_second(TopOfSecond const & prev);

private:
    void _try_set_tai_ymdhms();
    void _try_set_loc_ymdhms();
};

class TopsOfSeconds
{
public:
    TopsOfSeconds()
    {
        for (ssize_t i = 0; i < buffer_size; ++i)
        {
            _tops[i].invalidate();
        }
    }

    void top_of_second_has_passed();

    // Top of the next second
    TopOfSecond & next() { return _tops[_next]; }
    TopOfSecond const & next() const { return _tops[_next]; }

    // Top of the current second
    TopOfSecond & prev() { return _tops[mod(_next - 1, buffer_size)]; }
    TopOfSecond const & prev() const { return _tops[mod(_next - 1, buffer_size)]; }

    uint32_t error_count() const
    {
        uint32_t x = 0;
        for (ssize_t i = 0; i < buffer_size; ++i)
        {
            x += _tops[i].error_count;
        }
        return x;
    }

private:
    static ssize_t constexpr buffer_size = 2;
    TopOfSecond _tops[buffer_size];
    ssize_t _next = 0;
};

#pragma once

#include <cstdint>

bool time_test();

struct Ymdhms
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;

    Ymdhms(uint16_t year_, uint8_t month_, uint8_t day_, uint8_t hour_, uint8_t min_, uint8_t sec_);

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

struct Time
{
    uint32_t gps_week;
    uint32_t gps_time_of_week_seconds;
    Ymdhms utc_ymdhms;
    Ymdhms tai_ymdhms;
};

int32_t constexpr secs_per_min = 60;
int32_t constexpr min_per_hour = 60;
int32_t constexpr hour_per_day = 24;
int32_t constexpr secs_per_hour = secs_per_min * min_per_hour;
int32_t constexpr secs_per_day = secs_per_hour * hour_per_day;
int32_t constexpr min_per_day = min_per_hour * hour_per_day;

int32_t constexpr tai_minus_gps = 19;

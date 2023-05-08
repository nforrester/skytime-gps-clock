#pragma once

#include <cstdint>
#include <vector>
#include <string>

#if __cpp_exceptions
#include <stdexcept>
#endif

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

    int64_t subtract_and_return_non_leap_seconds(Ymdhms const & other) const;

    uint16_t day_of_year() const;
    bool is_leap_year() const;

    bool operator==(Ymdhms const & other) const
    {
        return year == other.year
            && month == other.month
            && day == other.day
            && hour == other.hour
            && min == other.min
            && sec == other.sec;
    }

    bool operator<(Ymdhms const & other) const
    {
        if (year  < other.year ) { return true; } else if (year  > other.year ) { return false; } else
        if (month < other.month) { return true; } else if (month > other.month) { return false; } else
        if (day   < other.day  ) { return true; } else if (day   > other.day  ) { return false; } else
        if (hour  < other.hour ) { return true; } else if (hour  > other.hour ) { return false; } else
        if (min   < other.min  ) { return true; } else if (min   > other.min  ) { return false; } else
        if (sec   < other.sec  ) { return true; } else { return false; }
    }

    bool operator!=(Ymdhms const & other) const
    {
        return !(*this == other);
    }

    bool operator<=(Ymdhms const & other) const
    {
        return (*this < other) || (*this == other);
    }

    bool operator>(Ymdhms const & other) const
    {
        return !(*this <= other);
    }

    bool operator>=(Ymdhms const & other) const
    {
        return !(*this < other);
    }

    void print() const
    {
        printf("%d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, min, sec);
    }

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
};

class TopsOfSeconds
{
public:
    TopsOfSeconds()
    {
        invalidate();
    }

    void invalidate()
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

class TimeRepresentation
{
public:
    virtual bool make_ymdhms(TopOfSecond const & top_of_second, Ymdhms & ymdhms) const = 0;
    virtual std::string abbrev() const = 0;
    virtual std::string abbrev(Ymdhms const & utc) const = 0;
    virtual bool is_dst(Ymdhms const & utc) const = 0;
};

class TimeRepTai: public TimeRepresentation
{
public:
    bool make_ymdhms(TopOfSecond const & top_of_second, Ymdhms & ymdhms) const override
    {
        if (!top_of_second.tai_ymdhms_valid)
        {
            return false;
        }
        ymdhms = top_of_second.tai_ymdhms;
        return true;
    }

    std::string abbrev() const override
    {
        return _abbrev;
    }

    std::string abbrev(Ymdhms const &) const override
    {
        return _abbrev;
    }

    bool is_dst(Ymdhms const &) const override
    {
        return false;
    }

private:
    std::string static constexpr _abbrev = "TAI";
};

class TimeZoneFixed: public TimeRepresentation
{
public:
    TimeZoneFixed(std::string const & abbrev, int32_t utc_offset_seconds, bool is_dst_):
        _abbrev(abbrev),
        _utc_offset_seconds(utc_offset_seconds),
        _is_dst(is_dst_)
    {}

    bool make_ymdhms(TopOfSecond const & top_of_second, Ymdhms & ymdhms) const override
    {
        if (!top_of_second.utc_ymdhms_valid)
        {
            return false;
        }
        ymdhms = top_of_second.utc_ymdhms;
        ymdhms.add_seconds(_utc_offset_seconds);
        return true;
    }

    std::string abbrev() const override
    {
        return _abbrev;
    }

    std::string abbrev(Ymdhms const &) const override
    {
        return _abbrev;
    }

    bool is_dst(Ymdhms const &) const override
    {
        return _is_dst;
    }

private:
    std::string const _abbrev;
    int32_t _utc_offset_seconds;
    bool _is_dst;
};

class TimeZoneIana: public TimeRepresentation
{
public:
    struct Eon
    {
        Ymdhms date;
        std::string abbreviation;
        bool is_dst;
        int utc_offset;
    };

    bool make_ymdhms(TopOfSecond const & top_of_second, Ymdhms & ymdhms) const override
    {
        if (!top_of_second.utc_ymdhms_valid)
        {
            return false;
        }
        Eon const eon = _get_eon(top_of_second.utc_ymdhms);
        ymdhms = top_of_second.utc_ymdhms;
        ymdhms.add_seconds(eon.utc_offset);
        return true;
    }

    std::string abbrev(Ymdhms const & utc) const override
    {
        Eon const eon = _get_eon(utc);
        return eon.abbreviation;
    }

    bool is_dst(Ymdhms const & utc) const override
    {
        Eon const eon = _get_eon(utc);
        return eon.is_dst;
    }

private:
    virtual Eon _get_eon(Ymdhms const & utc) const = 0;
};

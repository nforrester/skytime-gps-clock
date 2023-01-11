#include "time.h"
#include "util.h"

Ymdhms::Ymdhms(uint16_t year_, uint8_t month_, uint8_t day_, uint8_t hour_, uint8_t min_, uint8_t sec_)
{
    set(year_, month_, day_, hour_, min_, sec_);
}

void Ymdhms::set(uint16_t year_, uint8_t month_, uint8_t day_, uint8_t hour_, uint8_t min_, uint8_t sec_)
{
    year = year_;
    month = month_;
    day = day_;
    hour = hour_;
    min = min_;
    sec = sec_;
}

void Ymdhms::add_seconds(int32_t dt_seconds)
{
    int32_t dsecs = _to_dsecs() + dt_seconds;
    int32_t const dt_days = nwraps(dsecs, secs_per_day);
    if (dt_days != 0)
    {
        add_days(dt_days);
    }
    dsecs = mod(dsecs, secs_per_day);
    _from_dsecs(dsecs);
}

int32_t Ymdhms::_to_dsecs() const
{
    return hour*secs_per_hour + min*secs_per_min + sec;
}

void Ymdhms::_from_dsecs(int32_t dsecs)
{
    hour = dsecs / secs_per_hour;
    dsecs %= secs_per_hour;
    min = dsecs / secs_per_min;
    dsecs %= secs_per_min;
    sec = dsecs;
}

int64_t Ymdhms::_to_gdays() const
{
    int64_t m = (month + 9) % 12;
    int64_t y = year - m/10;
    int64_t d = day;
    return 365*y + y/4 - y/100 + y/400 + (m*306 + 5)/10 + (d - 1);
}

void Ymdhms::_from_gdays(int64_t gdays)
{
    int64_t y = (10000*gdays + 14780)/3652425;
    int64_t ddd = gdays - (365*y + y/4 - y/100 + y/400);
    if (ddd < 0)
    {
        y = y - 1;
        ddd = gdays - (365*y + y/4 - y/100 + y/400);
    }
    int64_t mi = (100*ddd + 52)/3060;
    int64_t mm = (mi + 2)%12 + 1;
    y = y + (mi + 2)/12;
    int64_t dd = ddd - (mi*306 + 5)/10 + 1;

    year = y;
    month = mm;
    day = dd;
}

void TopOfSecond::invalidate()
{
    utc_ymdhms_valid = false;
    tai_ymdhms_valid = false;
    loc_ymdhms_valid = false;
    gps_minus_utc_valid = false;
    next_leap_second_valid = false;
}

void TopOfSecond::set_next_leap_second(int32_t time_until, int32_t direction)
{
    next_leap_second_time_until = time_until;
    next_leap_second_direction = direction;
    next_leap_second_valid = true;
}

void TopOfSecond::set_utc_ymdhms(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec)
{
    if (utc_ymdhms_valid)
    {
        bool const error = (utc_ymdhms.year  != year)
                        || (utc_ymdhms.month != month)
                        || (utc_ymdhms.day   != day)
                        || (utc_ymdhms.hour  != hour)
                        || (utc_ymdhms.min   != min)
                        || (utc_ymdhms.sec   != sec);
        if (error)
        {
            ++error_count;
        }
    }

    utc_ymdhms.set(year, month, day, hour, min, sec);
    utc_ymdhms_valid = true;
    _try_set_tai_ymdhms();
    _try_set_loc_ymdhms();
}

void TopOfSecond::set_gps_minus_utc(int8_t value)
{
    gps_minus_utc = value;
    gps_minus_utc_valid = true;
    _try_set_tai_ymdhms();
}

void TopOfSecond::set_from_prev_second(TopOfSecond const & prev)
{
    if (prev.tai_ymdhms_valid)
    {
        tai_ymdhms = prev.tai_ymdhms;
        tai_ymdhms.add_seconds(1);
        tai_ymdhms_valid = true;
    }

    if (prev.next_leap_second_valid)
    {
        set_next_leap_second(prev.next_leap_second_time_until - 1, prev.next_leap_second_direction);
    }

    if (prev.utc_ymdhms_valid && prev.next_leap_second_valid)
    {
        bool const leap_second_far_future = prev.next_leap_second_time_until > 20;
        bool const leap_second_far_past = prev.next_leap_second_time_until < -20;
        bool const leap_second_this_top_of_minute =
            !(leap_second_far_future || leap_second_far_past);
        bool did_something_special = false;
        if (leap_second_this_top_of_minute)
        {
            if (prev.next_leap_second_direction > 0)
            {
                if (prev.utc_ymdhms.sec == 59)
                {
                    utc_ymdhms = prev.utc_ymdhms;
                    utc_ymdhms.sec = 60;
                    utc_ymdhms_valid = true;
                    did_something_special = true;
                }
                else if (prev.utc_ymdhms.sec == 60)
                {
                    utc_ymdhms = prev.utc_ymdhms;
                    utc_ymdhms.sec = 59;
                    utc_ymdhms.add_seconds(1);
                    utc_ymdhms_valid = true;
                    did_something_special = true;
                }
            }
            else
            {
                if (prev.utc_ymdhms.sec == 58)
                {
                    utc_ymdhms = prev.utc_ymdhms;
                    utc_ymdhms.add_seconds(2);
                    utc_ymdhms_valid = true;
                    did_something_special = true;
                }
            }
        }
        if (!did_something_special)
        {
            utc_ymdhms = prev.utc_ymdhms;
            utc_ymdhms.add_seconds(1);
            utc_ymdhms_valid = true;
        }
    }

    _try_set_loc_ymdhms();
}

void TopOfSecond::_try_set_tai_ymdhms()
{
    if (!(utc_ymdhms_valid && gps_minus_utc_valid))
    {
        return;
    }

    uint16_t old_year = tai_ymdhms.year;
    uint8_t old_month = tai_ymdhms.month;
    uint8_t old_day = tai_ymdhms.day;
    uint8_t old_hour = tai_ymdhms.hour;
    uint8_t old_min = tai_ymdhms.min;
    uint8_t old_sec = tai_ymdhms.sec;
    bool old_valid = tai_ymdhms_valid;

    tai_ymdhms = utc_ymdhms;
    tai_ymdhms.add_seconds(tai_minus_gps + gps_minus_utc);
    tai_ymdhms_valid = true;

    if (old_valid)
    {
        bool const error = (old_year  != tai_ymdhms.year)
                        || (old_month != tai_ymdhms.month)
                        || (old_day   != tai_ymdhms.day)
                        || (old_hour  != tai_ymdhms.hour)
                        || (old_min   != tai_ymdhms.min)
                        || (old_sec   != tai_ymdhms.sec);
        if (error)
        {
            ++error_count;
        }
    }
}

void TopOfSecond::_try_set_loc_ymdhms()
{
    if (!utc_ymdhms_valid)
    {
        return;
    }

    uint16_t old_year = loc_ymdhms.year;
    uint8_t old_month = loc_ymdhms.month;
    uint8_t old_day = loc_ymdhms.day;
    uint8_t old_hour = loc_ymdhms.hour;
    uint8_t old_min = loc_ymdhms.min;
    uint8_t old_sec = loc_ymdhms.sec;
    bool old_valid = loc_ymdhms_valid;

    loc_ymdhms = utc_ymdhms;
    loc_ymdhms.add_seconds(loc_minus_utc_seconds);
    loc_ymdhms_valid = true;

    if (old_valid)
    {
        bool const error = (old_year  != loc_ymdhms.year)
                        || (old_month != loc_ymdhms.month)
                        || (old_day   != loc_ymdhms.day)
                        || (old_hour  != loc_ymdhms.hour)
                        || (old_min   != loc_ymdhms.min)
                        || (old_sec   != loc_ymdhms.sec);
        if (error)
        {
            ++error_count;
        }
    }
}

void TopsOfSeconds::top_of_second_has_passed()
{
    _next = mod(_next + 1, buffer_size);
    next().invalidate();
    next().set_from_prev_second(prev());
}

bool ymdhms_test()
{
    {
        Ymdhms ymdhms(2022, 1, 1, 0, 0, 0);
        ymdhms.add_days(30);
        test_assert_signed_eq(ymdhms.year, 2022);
        test_assert_signed_eq(ymdhms.month, 1);
        test_assert_signed_eq(ymdhms.day, 31);
    }

    {
        Ymdhms ymdhms(2022, 1, 1, 0, 0, 0);
        ymdhms.add_days(-1);
        test_assert_signed_eq(ymdhms.year, 2021);
        test_assert_signed_eq(ymdhms.month, 12);
        test_assert_signed_eq(ymdhms.day, 31);
    }

    {
        Ymdhms ymdhms(2022, 1, 1, 0, 0, 0);
        ymdhms.add_days(59);
        test_assert_signed_eq(ymdhms.year, 2022);
        test_assert_signed_eq(ymdhms.month, 3);
        test_assert_signed_eq(ymdhms.day, 1);
    }

    {
        Ymdhms ymdhms(2024, 1, 1, 0, 0, 0);
        ymdhms.add_days(60);
        test_assert_signed_eq(ymdhms.year, 2024);
        test_assert_signed_eq(ymdhms.month, 3);
        test_assert_signed_eq(ymdhms.day, 1);
    }

    {
        Ymdhms ymdhms(2024, 1, 1, 0, 0, 0);
        ymdhms.add_days(366);
        test_assert_signed_eq(ymdhms.year, 2025);
        test_assert_signed_eq(ymdhms.month, 1);
        test_assert_signed_eq(ymdhms.day, 1);
    }

    {
        Ymdhms ymdhms(2024, 1, 1, 0, 0, 0);
        ymdhms.add_days(-365);
        test_assert_signed_eq(ymdhms.year, 2023);
        test_assert_signed_eq(ymdhms.month, 1);
        test_assert_signed_eq(ymdhms.day, 1);
    }

    {
        Ymdhms ymdhms(2224, 1, 1, 0, 0, 0);
        ymdhms.add_days(366);
        test_assert_signed_eq(ymdhms.year, 2225);
        test_assert_signed_eq(ymdhms.month, 1);
        test_assert_signed_eq(ymdhms.day, 1);
    }

    {
        Ymdhms ymdhms(1776, 7, 4, 0, 0, 0);
        ymdhms.add_seconds(1);
        test_assert_signed_eq(ymdhms.year, 1776);
        test_assert_signed_eq(ymdhms.month, 7);
        test_assert_signed_eq(ymdhms.day, 4);
        test_assert_signed_eq(ymdhms.hour, 0);
        test_assert_signed_eq(ymdhms.min, 0);
        test_assert_signed_eq(ymdhms.sec, 1);
    }

    {
        Ymdhms ymdhms(1776, 7, 4, 0, 0, 0);
        ymdhms.add_seconds(-1);
        test_assert_signed_eq(ymdhms.year, 1776);
        test_assert_signed_eq(ymdhms.month, 7);
        test_assert_signed_eq(ymdhms.day, 3);
        test_assert_signed_eq(ymdhms.hour, 23);
        test_assert_signed_eq(ymdhms.min, 59);
        test_assert_signed_eq(ymdhms.sec, 59);
    }

    {
        Ymdhms ymdhms(1776, 7, 4, 0, 0, 0);
        ymdhms.add_seconds(3661);
        test_assert_signed_eq(ymdhms.year, 1776);
        test_assert_signed_eq(ymdhms.month, 7);
        test_assert_signed_eq(ymdhms.day, 4);
        test_assert_signed_eq(ymdhms.hour, 1);
        test_assert_signed_eq(ymdhms.min, 1);
        test_assert_signed_eq(ymdhms.sec, 1);
    }

    {
        Ymdhms ymdhms(1776, 7, 4, 0, 0, 0);
        ymdhms.add_seconds(-86400);
        test_assert_signed_eq(ymdhms.year, 1776);
        test_assert_signed_eq(ymdhms.month, 7);
        test_assert_signed_eq(ymdhms.day, 3);
        test_assert_signed_eq(ymdhms.hour, 0);
        test_assert_signed_eq(ymdhms.min, 0);
        test_assert_signed_eq(ymdhms.sec, 0);
    }

    {
        Ymdhms ymdhms(1776, 7, 4, 0, 0, 0);
        ymdhms.add_seconds(-3600);
        test_assert_signed_eq(ymdhms.year, 1776);
        test_assert_signed_eq(ymdhms.month, 7);
        test_assert_signed_eq(ymdhms.day, 3);
        test_assert_signed_eq(ymdhms.hour, 23);
        test_assert_signed_eq(ymdhms.min, 0);
        test_assert_signed_eq(ymdhms.sec, 0);
    }

    {
        Ymdhms ymdhms(1776, 7, 4, 0, 0, 0);
        ymdhms.add_seconds(86400);
        test_assert_signed_eq(ymdhms.year, 1776);
        test_assert_signed_eq(ymdhms.month, 7);
        test_assert_signed_eq(ymdhms.day, 5);
        test_assert_signed_eq(ymdhms.hour, 0);
        test_assert_signed_eq(ymdhms.min, 0);
        test_assert_signed_eq(ymdhms.sec, 0);
    }

    {
        Ymdhms ymdhms(1776, 7, 4, 3, 32, 17);
        ymdhms.add_seconds(146);
        test_assert_signed_eq(ymdhms.year, 1776);
        test_assert_signed_eq(ymdhms.month, 7);
        test_assert_signed_eq(ymdhms.day, 4);
        test_assert_signed_eq(ymdhms.hour, 3);
        test_assert_signed_eq(ymdhms.min, 34);
        test_assert_signed_eq(ymdhms.sec, 43);
    }

    return true;
}

bool tos_test()
{
    TopsOfSeconds tos;

    test_assert_unsigned_eq(tos.error_count(), (uint32_t)0);

    test_assert(!tos.prev().utc_ymdhms_valid);
    test_assert(!tos.prev().tai_ymdhms_valid);
    test_assert(!tos.prev().loc_ymdhms_valid);
    test_assert(!tos.next().utc_ymdhms_valid);
    test_assert(!tos.next().tai_ymdhms_valid);
    test_assert(!tos.next().loc_ymdhms_valid);

    tos.prev().set_utc_ymdhms(2015, 5, 18, 14, 3, 24);
    tos.next().set_from_prev_second(tos.prev());

    test_assert(tos.prev().utc_ymdhms_valid);
    test_assert(!tos.prev().tai_ymdhms_valid);
    test_assert(tos.prev().loc_ymdhms_valid);
    test_assert(!tos.next().utc_ymdhms_valid);
    test_assert(!tos.next().tai_ymdhms_valid);
    test_assert(!tos.next().loc_ymdhms_valid);

    tos.prev().set_gps_minus_utc(16);
    tos.prev().set_next_leap_second(-100000, 1);
    tos.next().set_from_prev_second(tos.prev());

    test_assert(tos.prev().utc_ymdhms_valid);
    test_assert_signed_eq(tos.prev().utc_ymdhms.year, 2015);
    test_assert_signed_eq(tos.prev().utc_ymdhms.month, 5);
    test_assert_signed_eq(tos.prev().utc_ymdhms.day, 18);
    test_assert_signed_eq(tos.prev().utc_ymdhms.hour, 14);
    test_assert_signed_eq(tos.prev().utc_ymdhms.min, 3);
    test_assert_signed_eq(tos.prev().utc_ymdhms.sec, 24);

    test_assert(tos.prev().tai_ymdhms_valid);
    test_assert_signed_eq(tos.prev().tai_ymdhms.year, 2015);
    test_assert_signed_eq(tos.prev().tai_ymdhms.month, 5);
    test_assert_signed_eq(tos.prev().tai_ymdhms.day, 18);
    test_assert_signed_eq(tos.prev().tai_ymdhms.hour, 14);
    test_assert_signed_eq(tos.prev().tai_ymdhms.min, 3);
    test_assert_signed_eq(tos.prev().tai_ymdhms.sec, 24+16+19);

    test_assert(tos.prev().loc_ymdhms_valid);
    test_assert_signed_eq(tos.prev().loc_ymdhms.year, 2015);
    test_assert_signed_eq(tos.prev().loc_ymdhms.month, 5);
    test_assert_signed_eq(tos.prev().loc_ymdhms.day, 18);
    test_assert_signed_eq(tos.prev().loc_ymdhms.hour, 6);
    test_assert_signed_eq(tos.prev().loc_ymdhms.min, 3);
    test_assert_signed_eq(tos.prev().loc_ymdhms.sec, 24);

    test_assert(tos.next().utc_ymdhms_valid);
    test_assert_signed_eq(tos.next().utc_ymdhms.year, 2015);
    test_assert_signed_eq(tos.next().utc_ymdhms.month, 5);
    test_assert_signed_eq(tos.next().utc_ymdhms.day, 18);
    test_assert_signed_eq(tos.next().utc_ymdhms.hour, 14);
    test_assert_signed_eq(tos.next().utc_ymdhms.min, 3);
    test_assert_signed_eq(tos.next().utc_ymdhms.sec, 25);

    test_assert(tos.next().tai_ymdhms_valid);
    test_assert_signed_eq(tos.next().tai_ymdhms.year, 2015);
    test_assert_signed_eq(tos.next().tai_ymdhms.month, 5);
    test_assert_signed_eq(tos.next().tai_ymdhms.day, 18);
    test_assert_signed_eq(tos.next().tai_ymdhms.hour, 14);
    test_assert_signed_eq(tos.next().tai_ymdhms.min, 3+1);
    test_assert_signed_eq(tos.next().tai_ymdhms.sec, (25+16+19)%60);

    test_assert(tos.next().loc_ymdhms_valid);
    test_assert_signed_eq(tos.next().loc_ymdhms.year, 2015);
    test_assert_signed_eq(tos.next().loc_ymdhms.month, 5);
    test_assert_signed_eq(tos.next().loc_ymdhms.day, 18);
    test_assert_signed_eq(tos.next().loc_ymdhms.hour, 6);
    test_assert_signed_eq(tos.next().loc_ymdhms.min, 3);
    test_assert_signed_eq(tos.next().loc_ymdhms.sec, 25);

    tos.top_of_second_has_passed();

    test_assert(tos.prev().utc_ymdhms_valid);
    test_assert_signed_eq(tos.prev().utc_ymdhms.year, 2015);
    test_assert_signed_eq(tos.prev().utc_ymdhms.month, 5);
    test_assert_signed_eq(tos.prev().utc_ymdhms.day, 18);
    test_assert_signed_eq(tos.prev().utc_ymdhms.hour, 14);
    test_assert_signed_eq(tos.prev().utc_ymdhms.min, 3);
    test_assert_signed_eq(tos.prev().utc_ymdhms.sec, 25);

    test_assert(tos.prev().tai_ymdhms_valid);
    test_assert_signed_eq(tos.prev().tai_ymdhms.year, 2015);
    test_assert_signed_eq(tos.prev().tai_ymdhms.month, 5);
    test_assert_signed_eq(tos.prev().tai_ymdhms.day, 18);
    test_assert_signed_eq(tos.prev().tai_ymdhms.hour, 14);
    test_assert_signed_eq(tos.prev().tai_ymdhms.min, 3+1);
    test_assert_signed_eq(tos.prev().tai_ymdhms.sec, (25+16+19)%60);

    test_assert(tos.prev().loc_ymdhms_valid);
    test_assert_signed_eq(tos.prev().loc_ymdhms.year, 2015);
    test_assert_signed_eq(tos.prev().loc_ymdhms.month, 5);
    test_assert_signed_eq(tos.prev().loc_ymdhms.day, 18);
    test_assert_signed_eq(tos.prev().loc_ymdhms.hour, 6);
    test_assert_signed_eq(tos.prev().loc_ymdhms.min, 3);
    test_assert_signed_eq(tos.prev().loc_ymdhms.sec, 25);

    test_assert(tos.next().utc_ymdhms_valid);
    test_assert_signed_eq(tos.next().utc_ymdhms.year, 2015);
    test_assert_signed_eq(tos.next().utc_ymdhms.month, 5);
    test_assert_signed_eq(tos.next().utc_ymdhms.day, 18);
    test_assert_signed_eq(tos.next().utc_ymdhms.hour, 14);
    test_assert_signed_eq(tos.next().utc_ymdhms.min, 3);
    test_assert_signed_eq(tos.next().utc_ymdhms.sec, 26);

    test_assert(tos.next().tai_ymdhms_valid);
    test_assert_signed_eq(tos.next().tai_ymdhms.year, 2015);
    test_assert_signed_eq(tos.next().tai_ymdhms.month, 5);
    test_assert_signed_eq(tos.next().tai_ymdhms.day, 18);
    test_assert_signed_eq(tos.next().tai_ymdhms.hour, 14);
    test_assert_signed_eq(tos.next().tai_ymdhms.min, 3+1);
    test_assert_signed_eq(tos.next().tai_ymdhms.sec, (26+16+19)%60);

    test_assert(tos.next().loc_ymdhms_valid);
    test_assert_signed_eq(tos.next().loc_ymdhms.year, 2015);
    test_assert_signed_eq(tos.next().loc_ymdhms.month, 5);
    test_assert_signed_eq(tos.next().loc_ymdhms.day, 18);
    test_assert_signed_eq(tos.next().loc_ymdhms.hour, 6);
    test_assert_signed_eq(tos.next().loc_ymdhms.min, 3);
    test_assert_signed_eq(tos.next().loc_ymdhms.sec, 26);

    test_assert_unsigned_eq(tos.error_count(), (uint32_t)0);
    tos.prev().set_utc_ymdhms(2022, 10, 3, 11, 55, 4);
    test_assert_unsigned_eq(tos.error_count(), (uint32_t)2); // One error from the skip in UTC, one from the skip in LOC
    tos.next().set_from_prev_second(tos.prev());
    test_assert_unsigned_eq(tos.error_count(), (uint32_t)3); // One error from the skip in LOC

    tos.prev().set_gps_minus_utc(18);
    test_assert_unsigned_eq(tos.error_count(), (uint32_t)4); // One error from the skip in TAI
    tos.prev().set_next_leap_second(-100000, 1);
    test_assert_unsigned_eq(tos.error_count(), (uint32_t)4);
    tos.next().set_from_prev_second(tos.prev());
    test_assert_unsigned_eq(tos.error_count(), (uint32_t)4);

    test_assert(tos.prev().utc_ymdhms_valid);
    test_assert_signed_eq(tos.prev().utc_ymdhms.year, 2022);
    test_assert_signed_eq(tos.prev().utc_ymdhms.month, 10);
    test_assert_signed_eq(tos.prev().utc_ymdhms.day, 3);
    test_assert_signed_eq(tos.prev().utc_ymdhms.hour, 11);
    test_assert_signed_eq(tos.prev().utc_ymdhms.min, 55);
    test_assert_signed_eq(tos.prev().utc_ymdhms.sec, 4);

    test_assert(tos.prev().tai_ymdhms_valid);
    test_assert_signed_eq(tos.prev().tai_ymdhms.year, 2022);
    test_assert_signed_eq(tos.prev().tai_ymdhms.month, 10);
    test_assert_signed_eq(tos.prev().tai_ymdhms.day, 3);
    test_assert_signed_eq(tos.prev().tai_ymdhms.hour, 11);
    test_assert_signed_eq(tos.prev().tai_ymdhms.min, 55);
    test_assert_signed_eq(tos.prev().tai_ymdhms.sec, 4+18+19);

    test_assert(tos.prev().loc_ymdhms_valid);
    test_assert_signed_eq(tos.prev().loc_ymdhms.year, 2022);
    test_assert_signed_eq(tos.prev().loc_ymdhms.month, 10);
    test_assert_signed_eq(tos.prev().loc_ymdhms.day, 3);
    test_assert_signed_eq(tos.prev().loc_ymdhms.hour, 3);
    test_assert_signed_eq(tos.prev().loc_ymdhms.min, 55);
    test_assert_signed_eq(tos.prev().loc_ymdhms.sec, 4);

    test_assert(tos.next().utc_ymdhms_valid);
    test_assert_signed_eq(tos.next().utc_ymdhms.year, 2022);
    test_assert_signed_eq(tos.next().utc_ymdhms.month, 10);
    test_assert_signed_eq(tos.next().utc_ymdhms.day, 3);
    test_assert_signed_eq(tos.next().utc_ymdhms.hour, 11);
    test_assert_signed_eq(tos.next().utc_ymdhms.min, 55);
    test_assert_signed_eq(tos.next().utc_ymdhms.sec, 5);

    test_assert(tos.next().tai_ymdhms_valid);
    test_assert_signed_eq(tos.next().tai_ymdhms.year, 2022);
    test_assert_signed_eq(tos.next().tai_ymdhms.month, 10);
    test_assert_signed_eq(tos.next().tai_ymdhms.day, 3);
    test_assert_signed_eq(tos.next().tai_ymdhms.hour, 11);
    test_assert_signed_eq(tos.next().tai_ymdhms.min, 55);
    test_assert_signed_eq(tos.next().tai_ymdhms.sec, 5+18+19);

    test_assert(tos.next().loc_ymdhms_valid);
    test_assert_signed_eq(tos.next().loc_ymdhms.year, 2022);
    test_assert_signed_eq(tos.next().loc_ymdhms.month, 10);
    test_assert_signed_eq(tos.next().loc_ymdhms.day, 3);
    test_assert_signed_eq(tos.next().loc_ymdhms.hour, 3);
    test_assert_signed_eq(tos.next().loc_ymdhms.min, 55);
    test_assert_signed_eq(tos.next().loc_ymdhms.sec, 5);

    tos.top_of_second_has_passed();

    test_assert_unsigned_eq(tos.error_count(), (uint32_t)4);

    test_assert(tos.prev().utc_ymdhms_valid);
    test_assert_signed_eq(tos.prev().utc_ymdhms.year, 2022);
    test_assert_signed_eq(tos.prev().utc_ymdhms.month, 10);
    test_assert_signed_eq(tos.prev().utc_ymdhms.day, 3);
    test_assert_signed_eq(tos.prev().utc_ymdhms.hour, 11);
    test_assert_signed_eq(tos.prev().utc_ymdhms.min, 55);
    test_assert_signed_eq(tos.prev().utc_ymdhms.sec, 5);

    test_assert(tos.prev().tai_ymdhms_valid);
    test_assert_signed_eq(tos.prev().tai_ymdhms.year, 2022);
    test_assert_signed_eq(tos.prev().tai_ymdhms.month, 10);
    test_assert_signed_eq(tos.prev().tai_ymdhms.day, 3);
    test_assert_signed_eq(tos.prev().tai_ymdhms.hour, 11);
    test_assert_signed_eq(tos.prev().tai_ymdhms.min, 55);
    test_assert_signed_eq(tos.prev().tai_ymdhms.sec, 5+18+19);

    test_assert(tos.prev().loc_ymdhms_valid);
    test_assert_signed_eq(tos.prev().loc_ymdhms.year, 2022);
    test_assert_signed_eq(tos.prev().loc_ymdhms.month, 10);
    test_assert_signed_eq(tos.prev().loc_ymdhms.day, 3);
    test_assert_signed_eq(tos.prev().loc_ymdhms.hour, 3);
    test_assert_signed_eq(tos.prev().loc_ymdhms.min, 55);
    test_assert_signed_eq(tos.prev().loc_ymdhms.sec, 5);

    test_assert(tos.next().utc_ymdhms_valid);
    test_assert_signed_eq(tos.next().utc_ymdhms.year, 2022);
    test_assert_signed_eq(tos.next().utc_ymdhms.month, 10);
    test_assert_signed_eq(tos.next().utc_ymdhms.day, 3);
    test_assert_signed_eq(tos.next().utc_ymdhms.hour, 11);
    test_assert_signed_eq(tos.next().utc_ymdhms.min, 55);
    test_assert_signed_eq(tos.next().utc_ymdhms.sec, 6);

    test_assert(tos.next().tai_ymdhms_valid);
    test_assert_signed_eq(tos.next().tai_ymdhms.year, 2022);
    test_assert_signed_eq(tos.next().tai_ymdhms.month, 10);
    test_assert_signed_eq(tos.next().tai_ymdhms.day, 3);
    test_assert_signed_eq(tos.next().tai_ymdhms.hour, 11);
    test_assert_signed_eq(tos.next().tai_ymdhms.min, 55);
    test_assert_signed_eq(tos.next().tai_ymdhms.sec, 6+18+19);

    test_assert(tos.next().loc_ymdhms_valid);
    test_assert_signed_eq(tos.next().loc_ymdhms.year, 2022);
    test_assert_signed_eq(tos.next().loc_ymdhms.month, 10);
    test_assert_signed_eq(tos.next().loc_ymdhms.day, 3);
    test_assert_signed_eq(tos.next().loc_ymdhms.hour, 3);
    test_assert_signed_eq(tos.next().loc_ymdhms.min, 55);
    test_assert_signed_eq(tos.next().loc_ymdhms.sec, 6);

    test_assert_unsigned_eq(tos.error_count(), (uint32_t)4);

    return true;
}

bool time_test()
{
    test_assert(ymdhms_test());
    test_assert(tos_test());

    return true;
}

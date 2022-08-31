#include "time.h"
#include "util.h"

Ymdhms::Ymdhms(uint16_t year_, uint8_t month_, uint8_t day_, uint8_t hour_, uint8_t min_, uint8_t sec_):
    year(year_), month(month_), day(day_), hour(hour_), min(min_), sec(sec_)
{
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

bool time_test()
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

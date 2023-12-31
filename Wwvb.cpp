#include "hardware/pwm.h"

#include "Wwvb.h"
#include "iana_time_zones.h"

Wwvb::Wwvb(uint carrier_pin, uint reduce_pin):
    _reduce(reduce_pin)
{
    _reduce.off();

    gpio_set_function(carrier_pin, GPIO_FUNC_PWM);

    _carrier_pwm_slice = pwm_gpio_to_slice_num(carrier_pin);
    _carrier_pwm_channel = pwm_gpio_to_channel(carrier_pin);
    pwm_set_wrap(_carrier_pwm_slice, _pwm_count_wrap);
    pwm_set_chan_level(_carrier_pwm_slice, _carrier_pwm_channel, _pwm_count_off);
    pwm_set_enabled(_carrier_pwm_slice, true);

    for (auto const & zone : get_iana_timezones())
    {
        if (std::get<std::string>(zone) == "America/Los_Angeles")
        {
            _pacific_time_zone = std::get<decltype(_pacific_time_zone)>(zone);
        }
    }
}

void Wwvb::set_carrier(bool enabled)
{
    pwm_set_chan_level(
        _carrier_pwm_slice,
        _carrier_pwm_channel,
        enabled? _pwm_count_half : _pwm_count_off);
}

namespace
{
    uint64_t convert_whole_portion_to_bit(uint16_t & value, uint16_t portion, uint8_t bit)
    {
        if (value < portion)
        {
            return 0;
        }
        value -= portion;
        return static_cast<uint64_t>(1) << bit;
    }
}

uint32_t Wwvb::top_of_second(TopOfSecond const & tos)
{
    if (!tos.utc_ymdhms_valid)
    {
        return 100000000; // Never
    }

    _reduce.on();

    Ymdhms const & utc = tos.utc_ymdhms;

    uint64_t markers = 0;
    markers |= static_cast<uint64_t>(1) << 0;
    markers |= static_cast<uint64_t>(1) << 9;
    markers |= static_cast<uint64_t>(1) << 19;
    markers |= static_cast<uint64_t>(1) << 29;
    markers |= static_cast<uint64_t>(1) << 39;
    markers |= static_cast<uint64_t>(1) << 49;
    markers |= static_cast<uint64_t>(1) << 59;
    markers |= static_cast<uint64_t>(1) << 60;

    bool mark_bit = (markers   >> utc.sec) & 1;
    if (mark_bit)
    {
        return 800000;
    }

    uint64_t time_code = 0;

    uint16_t min = utc.min;
    time_code |= convert_whole_portion_to_bit(min, 40, 1);
    time_code |= convert_whole_portion_to_bit(min, 20, 2);
    time_code |= convert_whole_portion_to_bit(min, 10, 3);
    time_code |= convert_whole_portion_to_bit(min,  8, 5);
    time_code |= convert_whole_portion_to_bit(min,  4, 6);
    time_code |= convert_whole_portion_to_bit(min,  2, 7);
    time_code |= convert_whole_portion_to_bit(min,  1, 8);

    uint16_t hour = utc.hour;
    time_code |= convert_whole_portion_to_bit(hour, 20, 12);
    time_code |= convert_whole_portion_to_bit(hour, 10, 13);
    time_code |= convert_whole_portion_to_bit(hour,  8, 15);
    time_code |= convert_whole_portion_to_bit(hour,  4, 16);
    time_code |= convert_whole_portion_to_bit(hour,  2, 17);
    time_code |= convert_whole_portion_to_bit(hour,  1, 18);

    uint16_t day_of_year = utc.day_of_year();
    time_code |= convert_whole_portion_to_bit(day_of_year, 200, 22);
    time_code |= convert_whole_portion_to_bit(day_of_year, 100, 23);
    time_code |= convert_whole_portion_to_bit(day_of_year,  80, 25);
    time_code |= convert_whole_portion_to_bit(day_of_year,  40, 26);
    time_code |= convert_whole_portion_to_bit(day_of_year,  20, 27);
    time_code |= convert_whole_portion_to_bit(day_of_year,  10, 28);
    time_code |= convert_whole_portion_to_bit(day_of_year,   8, 30);
    time_code |= convert_whole_portion_to_bit(day_of_year,   4, 31);
    time_code |= convert_whole_portion_to_bit(day_of_year,   2, 32);
    time_code |= convert_whole_portion_to_bit(day_of_year,   1, 33);

    // We have no way to get DUT1

    uint16_t year = utc.year % 100;
    time_code |= convert_whole_portion_to_bit(year, 80, 45);
    time_code |= convert_whole_portion_to_bit(year, 40, 46);
    time_code |= convert_whole_portion_to_bit(year, 20, 47);
    time_code |= convert_whole_portion_to_bit(year, 10, 48);
    time_code |= convert_whole_portion_to_bit(year,  8, 50);
    time_code |= convert_whole_portion_to_bit(year,  4, 51);
    time_code |= convert_whole_portion_to_bit(year,  2, 52);
    time_code |= convert_whole_portion_to_bit(year,  1, 53);

    if (utc.is_leap_year())
    {
        time_code |= static_cast<uint64_t>(1) << 55;
    }

    if (tos.next_leap_second_time_until < 27 * secs_per_day &&
        tos.next_leap_second_time_until >= 0)
    {
        time_code |= static_cast<uint64_t>(1) << 56;
    }

    Ymdhms start_today = utc;
    start_today.hour = 0;
    start_today.min = 0;
    start_today.sec = 0;
    Ymdhms start_tomorrow = start_today;
    start_tomorrow.add_days(1);

    bool const dst_start_today = _pacific_time_zone->is_dst(start_today);
    bool const dst_start_tomorrow = _pacific_time_zone->is_dst(start_tomorrow);

    if (dst_start_tomorrow)
    {
        time_code |= static_cast<uint64_t>(1) << 57;
    }
    if (dst_start_today)
    {
        time_code |= static_cast<uint64_t>(1) << 58;
    }

    bool data_bit = (time_code >> utc.sec) & 1;
    if (data_bit)
    {
        return 500000;
    }
    return 200000;
}

void Wwvb::raise_power()
{
    _reduce.off();
}

#include "hardware/pwm.h"

#include "Wwvb.h"

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
}

void Wwvb::set_carrier(bool enabled)
{
    pwm_set_chan_level(
        _carrier_pwm_slice,
        _carrier_pwm_channel,
        enabled? _pwm_count_half : _pwm_count_off);
}


uint32_t Wwvb::top_of_second(Ymdhms const & utc)
{
    _reduce.on();

    uint64_t markers = 0;
    markers |= static_cast<uint64_t>(1) << 0;
    markers |= static_cast<uint64_t>(1) << 9;
    markers |= static_cast<uint64_t>(1) << 19;
    markers |= static_cast<uint64_t>(1) << 29;
    markers |= static_cast<uint64_t>(1) << 39;
    markers |= static_cast<uint64_t>(1) << 49;
    markers |= static_cast<uint64_t>(1) << 59;
    markers |= static_cast<uint64_t>(1) << 60;

    uint64_t time_code = 0;

    bool mark_bit = (markers   >> utc.sec) & 1;
    if (mark_bit)
    {
        return 800000;
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

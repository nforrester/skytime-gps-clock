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

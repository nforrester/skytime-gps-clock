#pragma once

#include "Gpio.h"

class Wwvb
{
public:
    Wwvb(uint carrier_pin, uint reduce_pin);

    void set_carrier(bool enabled);

private:
    GpioOut _reduce;
    uint _carrier_pwm_slice;
    uint _carrier_pwm_channel;

    float static constexpr _pwm_freq = 60000;
    uint16_t static constexpr _pwm_count_wrap = 125000000 / _pwm_freq;
    uint16_t static constexpr _pwm_count_half = _pwm_count_wrap / 2;
    uint16_t static constexpr _pwm_count_off = 0;
};

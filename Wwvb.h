#pragma once

#include "time.h"
#include "Gpio.h"

class Wwvb
{
public:
    Wwvb(uint carrier_pin, uint reduce_pin);

    void set_carrier(bool enabled);

    // Returns the time within the second in microseconds for the raise_power() callback.
    uint32_t top_of_second(Ymdhms const & utc);
    void raise_power();

private:
    GpioOut _reduce;
    uint _carrier_pwm_slice;
    uint _carrier_pwm_channel;

    float static constexpr _pwm_freq = 60000;
    uint16_t static constexpr _pwm_count_wrap = 125000000 / _pwm_freq;
    uint16_t static constexpr _pwm_count_half = _pwm_count_wrap / 2;
    uint16_t static constexpr _pwm_count_off = 0;
};

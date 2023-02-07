#include "Analog.h"

#include <cstdio>

#include "pico/stdlib.h"

Analog::Analog(Pps const & pps,
               uint const analog_tick_pin,
               uint const sense0_pin,
               uint const sense3_pin,
               uint const sense6_pin,
               uint const sense9_pin):
    _pps(pps),
    _tick(analog_tick_pin),
    _sense0(sense0_pin),
    _sense3(sense3_pin),
    _sense6(sense6_pin),
    _sense9(sense9_pin)
{
}

void Analog::pps_pulsed()
{
    if (_next_tick_us < 1000000)
    {
        _next_tick_us = 0;
    }
    else
    {
        _next_tick_us = _next_tick_us % 1000000;
    }
}

void Analog::dispatch(uint32_t const completed_seconds)
{
    usec_t threshold_time_us = _pps.get_time_us_of(completed_seconds, _next_tick_us);
    if (threshold_time_us <= time_us_64())
    {
        _tick.toggle();
        _next_tick_us += 1000000/16 / _tick_rate;
        if (_next_tick_us >= 1000000)
        {
            _next_tick_us += 10000000;
        }
    }
}

void Analog::show_sensors()
{
    printf("Sensors: %d %d %d %d\n", _sense0.get(), _sense3.get(), _sense6.get(), _sense9.get());
}

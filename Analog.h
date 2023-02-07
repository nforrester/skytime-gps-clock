#include "Pps.h"
#include "Gpio.h"

class Analog
{
public:
    Analog(Pps const & pps,
           uint const analog_tick_pin,
           uint const sense0_pin,
           uint const sense3_pin,
           uint const sense6_pin,
           uint const sense9_pin);
    void pps_pulsed();
    void dispatch(uint32_t const completed_seconds);
    void show_sensors();

private:
    Pps const & _pps;
    GpioOut _tick;
    uint32_t _next_tick_us = 0;
    float _tick_rate = 1.0;

    GpioIn _sense0;
    GpioIn _sense3;
    GpioIn _sense6;
    GpioIn _sense9;
};

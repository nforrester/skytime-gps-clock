#pragma once

#ifndef HOST_BUILD
  #include "hardware/gpio.h"
#endif

class GpioOut
{
public:
    GpioOut(uint const pin): _pin(pin)
    {
#ifndef HOST_BUILD
        gpio_init(_pin);
        gpio_set_dir(_pin, GPIO_OUT);
#endif
    }

    void set(bool state)
    {
#ifndef HOST_BUILD
        gpio_put(_pin, state);
#endif
        _state = state;
    }

    void on()
    {
        set(true);
    }

    void off()
    {
        set(false);
    }

    void toggle()
    {
        set(!_state);
    }

private:
    uint const _pin;
    bool _state = false;
};

class GpioIn
{
public:
    GpioIn(uint const pin): _pin(pin)
    {
#ifndef HOST_BUILD
        gpio_init(_pin);
        gpio_set_dir(_pin, GPIO_IN);
#endif
    }

    bool get()
    {
#ifndef HOST_BUILD
        return gpio_get(_pin);
#else
        return false;
#endif
    }

private:
    uint const _pin;
};

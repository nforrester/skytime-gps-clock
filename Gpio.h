#pragma once

#include "hardware/gpio.h"

class GpioOut
{
public:
    GpioOut(uint const pin): _pin(pin)
    {
        gpio_init(_pin);
        gpio_set_dir(_pin, GPIO_OUT);
    }

    void set(bool state)
    {
        gpio_put(_pin, state);
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
        gpio_init(_pin);
        gpio_set_dir(_pin, GPIO_IN);
    }

    bool get()
    {
        return gpio_get(_pin);
    }

private:
    uint const _pin;
};

#pragma once

#include <array>
#include <cstdio>

#include "Gpio.h"
#include "RingBuffer.h"

enum class Button {
    Up,
    Down,
    Left,
    Right
};

class Buttons
{
public:
    Buttons(uint const pin_up, uint const pin_dn, uint const pin_lf, uint const pin_rt);

    void dispatch();

    bool get_button(Button & button);

    void dump_to_console_if_any_pressed()
    {
        bool any_pressed = _prev_up || _prev_dn || _prev_lf || _prev_rt;

        if (any_pressed)
        {
            printf("Buttons: ");
            printf("%02x ", (_prev_up << 3 | _prev_dn << 2 | _prev_lf << 1 | _prev_rt));
            printf("\n");
        }
    }

    uint32_t error_count() const { return _error_count; }

private:
    uint32_t _error_count = 0;

    GpioIn _gpio_up;
    GpioIn _gpio_dn;
    GpioIn _gpio_lf;
    GpioIn _gpio_rt;

    bool _prev_up;
    bool _prev_dn;
    bool _prev_lf;
    bool _prev_rt;

    RingBuffer<Button, 20> _button_events;
};

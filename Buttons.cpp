#include "Buttons.h"

Buttons::Buttons(uint const pin_up, uint const pin_dn, uint const pin_lf, uint const pin_rt):
    _gpio_up(pin_up),
    _gpio_dn(pin_dn),
    _gpio_lf(pin_lf),
    _gpio_rt(pin_rt)
{
}

bool Buttons::get_button(Button & button)
{
    if (_button_events.empty())
    {
        return false;
    }
    button = _button_events.peek(0);
    _button_events.pop(1);
    return true;
}

void Buttons::dispatch()
{
    bool curr_up = _gpio_up.get();
    bool curr_dn = _gpio_dn.get();
    bool curr_lf = _gpio_lf.get();
    bool curr_rt = _gpio_rt.get();

    if (!_prev_up && curr_up)
    {
        if (!_button_events.full())
        {
            _button_events.push(Button::Up);
        }
        else
        {
            ++_error_count;
        }
    }

    if (!_prev_dn && curr_dn)
    {
        if (!_button_events.full())
        {
            _button_events.push(Button::Down);
        }
        else
        {
            ++_error_count;
        }
    }

    if (!_prev_lf && curr_lf)
    {
        if (!_button_events.full())
        {
            _button_events.push(Button::Left);
        }
        else
        {
            ++_error_count;
        }
    }

    if (!_prev_rt && curr_rt)
    {
        if (!_button_events.full())
        {
            _button_events.push(Button::Right);
        }
        else
        {
            ++_error_count;
        }
    }

    _prev_up = curr_up;
    _prev_dn = curr_dn;
    _prev_lf = curr_lf;
    _prev_rt = curr_rt;
}

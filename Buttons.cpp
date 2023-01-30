#include "Buttons.h"

Buttons::Buttons(FiveSimdHt16k33Busses & busses):
    _busses(busses)
{
    for (uint8_t & byte : _button_data)
    {
        byte = 0;
    }

    for (uint8_t & byte : _last_button_data)
    {
        byte = 0;
    }

    uint8_t constexpr command = 0xa0;
    if (!_busses.blocking_write(chip_addr, 1, &command, &command, &command, &command, &command))
    {
        ::printf("HT16K33 Failed to set ROW/INT register to ROW mode.\n");
        ++_error_count;
    }
}

void Buttons::begin_poll()
{
    if (_poll_requested || _poll_in_progress)
    {
        ++_error_count;
    }
    _poll_requested = true;
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
    constexpr uint8_t button_base_reg = 0x40;

    if (_poll_in_progress)
    {
        bool success;
        if (!_busses.try_end_read(success,
                                  _button_data.data(),
                                  nullptr,
                                  nullptr,
                                  nullptr,
                                  nullptr))
        {
            return;
        }
        if (!success)
        {
            ++_error_count;
        }
        else
        {
            for (size_t i = 0; i < num_button_bytes; ++i)
            {
                for (size_t j = 0; j < 8; ++j)
                {
                    bool button_old = (_last_button_data[i] >> j) & 1;
                    bool button_new = (     _button_data[i] >> j) & 1;
                    if ((!button_old) && button_new)
                    {
                        bool space_avail = !_button_events.full();
                        if (space_avail && i == 1 && j == 4)
                        {
                            _button_events.push(Button::Left);
                        }
                        else if (space_avail && i == 1 && j == 3)
                        {
                            _button_events.push(Button::Down);
                        }
                        else if (space_avail && i == 1 && j == 2)
                        {
                            _button_events.push(Button::Up);
                        }
                        else if (space_avail && i == 3 && j == 4)
                        {
                            _button_events.push(Button::Right);
                        }
                        else if (space_avail && i == 3 && j == 3)
                        {
                            _button_events.push(Button::Plus);
                        }
                        else if (space_avail && i == 3 && j == 2)
                        {
                            _button_events.push(Button::Minus);
                        }
                        else
                        {
                            ++_error_count;
                        }
                    }
                }
                _last_button_data[i] = _button_data[i];
            }
        }
        _poll_in_progress = false;
    }

    if (!_poll_in_progress)
    {
        if (_poll_requested)
        {
            if (!_busses.begin_read(chip_addr,
                                    num_button_bytes,
                                    button_base_reg,
                                    button_base_reg,
                                    button_base_reg,
                                    button_base_reg,
                                    button_base_reg))
            {
                return;
            }
            _poll_in_progress = true;
            _poll_requested = false;
        }
    }
}

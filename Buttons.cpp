#include "Buttons.h"

Buttons::Buttons(FiveSimdHt16k33Busses & busses):
    _busses(busses)
{
    for (uint8_t & byte : _button_data)
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

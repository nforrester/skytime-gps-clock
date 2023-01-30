#pragma once

#include <array>

#include "FiveSimdHt16k33Busses.h"

class Buttons
{
public:
    Buttons(FiveSimdHt16k33Busses & busses);

    void dispatch();
    void begin_poll();

    //bool check_up();
    //bool check_down();
    //bool check_left();
    //bool check_right();
    //bool check_plus();
    //bool check_minus();
    void dump_to_console_if_any_pressed()
    {
        bool any_pressed = false;
        for (uint8_t const byte : _button_data)
        {
            if (byte != 0)
            {
                any_pressed = true;
            }
        }
        if (!any_pressed)
        {
            return;
        }

        printf("Buttons: ");
        for (uint8_t const byte : _button_data)
        {
            printf("%02x ", byte);
        }
        printf("\n");
    }

    uint32_t error_count() const { return _error_count; }

private:
    static constexpr size_t num_button_bytes = 6;
    static constexpr uint8_t chip_addr = 0x70;

    FiveSimdHt16k33Busses & _busses;
    bool _poll_in_progress = false;
    bool _poll_requested = false;
    uint32_t _error_count = 0;
    std::array<uint8_t, num_button_bytes> _button_data;
};

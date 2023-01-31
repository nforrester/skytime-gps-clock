#pragma once

#include "time.h"
#include "Display.h"
#include "Buttons.h"
#include "GpsUBlox.h"

class Artist
{
public:
    Artist(Display & display,
           Buttons & buttons,
           GpsUBlox & gps);

    void top_of_tenth_of_second(uint8_t tenths);

private:
    Display & _disp;
    Buttons & _buttons;
    GpsUBlox & _gps;

    void _print_time(size_t line, TimeRepresentation const & time_rep, uint8_t tenths);

    void _font_debug(size_t line);
    uint32_t _font_debug_count = 0;
};

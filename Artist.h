#pragma once

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
};

#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#include "hardware/pio.h"
#pragma GCC diagnostic pop

class Pps
{
public:
    Pps(PIO pio, uint const pin);
    void dispatch();
    void get_status(uint32_t * completed_seconds, uint32_t * bicycles_in_last_second);

private:
    PIO _pio;
    uint _sm;
    uint32_t _bicycles_in_last_second = 0;
    uint32_t _completed_seconds = 0;

    uint32_t volatile _completed_seconds_main_thread_a = 0;
    uint32_t volatile _bicycles_in_last_second_main_thread = 0;
    uint32_t volatile _completed_seconds_main_thread_b = 0;
};


#include "Pps.h"

#include "pps.pio.h"

Pps::Pps(PIO pio, uint const pin): _pio(pio)
{
    uint offset = pio_add_program(_pio, &pps_program);
    _sm = pio_claim_unused_sm(_pio, true);
    pps_program_init(_pio, _sm, offset, pin);
}

void Pps::update()
{
    if (pps_program_second_completed(_pio, _sm))
    {
        _bicycles_in_last_second = pps_program_get_bicycles_in_last_complete_second(_pio, _sm);
        ++_completed_seconds;
    }

    _completed_seconds_main_thread_a = _completed_seconds;
    _bicycles_in_last_second_main_thread = _bicycles_in_last_second;
    _completed_seconds_main_thread_b = _completed_seconds;
}

void Pps::get_status(uint32_t * completed_seconds, uint32_t * bicycles_in_last_second)
{
    do
    {
        *completed_seconds = _completed_seconds_main_thread_a;
        *bicycles_in_last_second = _bicycles_in_last_second_main_thread;
    } while (*completed_seconds != _completed_seconds_main_thread_b);
}

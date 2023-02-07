#include "Pps.h"

#include <cstdlib>
#include <algorithm>

#include "pico/time.h"

#include "pps.pio.h"

#include "util.h"

Pps::Pps(PIO pio, uint const pin):
    _pin(pin),
    _pio(pio),
    _bicycles_per_gps_second_average(bicycles_per_chip_second)
{
}

void Pps::pio_init()
{
    uint offset = pio_add_program(_pio, &pps_program);
    _sm = pio_claim_unused_sm(_pio, true);
    pps_program_init(_pio, _sm, offset, _pin);
}

void Pps::dispatch_fast_thread()
{
    if (!_pulse_complete)
    {
        if (pps_program_pulse_completed(_pio, _sm))
        {
            _bicycles_in_last_pulse = pps_program_get_bicycles_in_last_complete_pulse(_pio, _sm);
            _pulse_complete = true;
        }
    }

    if (_pulse_complete)
    {
        usec_t possible_top_of_second = time_us_64();
        if (pps_program_second_completed(_pio, _sm))
        {
            _bicycles_in_last_second = pps_program_get_bicycles_in_last_complete_second(_pio, _sm);
            ++_completed_seconds;
            _pulse_complete = false;

            _completed_seconds_main_thread_a = _completed_seconds;
            _bicycles_in_last_second_main_thread = _bicycles_in_last_second;
            _bicycles_in_last_pulse_main_thread = _bicycles_in_last_pulse;
            _top_of_second_main_thread = possible_top_of_second;
            _completed_seconds_main_thread_b = _completed_seconds;
        }
    }
}

void Pps::dispatch_main_thread()
{
    uint32_t completed_seconds;
    uint32_t bicycles_in_last_second;
    uint32_t bicycles_in_last_pulse;

    do
    {
        completed_seconds = _completed_seconds_main_thread_a;
        bicycles_in_last_second = _bicycles_in_last_second_main_thread;
        bicycles_in_last_pulse = _bicycles_in_last_pulse_main_thread;
        _prev_top_of_second_time_us = _top_of_second_main_thread;
    } while (completed_seconds != _completed_seconds_main_thread_b);

    if (completed_seconds != _prev_completed_seconds)
    {
        uint32_t pulse_error_magnitude = abs(static_cast<int32_t>(bicycles_per_nominal_pulse) -
                                             static_cast<int32_t>(bicycles_in_last_pulse));
        bool pulse_indicates_unlocked = pulse_error_magnitude > bicycles_per_nominal_pulse/100;

        bool pps_continuity_indicates_unlocked;
        uint32_t error_magnitude = abs(static_cast<int32_t>(bicycles_per_chip_second) -
                                       static_cast<int32_t>(bicycles_in_last_second));
        if (error_magnitude > bicycles_per_chip_second/10000)
        {
            // PPS discontinuity
            _bicycles_per_gps_second_average.reset(bicycles_per_chip_second);
            pps_continuity_indicates_unlocked = true;
        }
        else
        {
            // PPS ok, error is due to clock drift in microcontroller.
            _bicycles_per_gps_second_average.add_point(bicycles_in_last_second);
            pps_continuity_indicates_unlocked = false;
        }

        if (pps_continuity_indicates_unlocked)
        {
            _lock_persistence = _lock_persistence_saturation_limit_lo;
        }
        else if (pulse_indicates_unlocked)
        {
            _lock_persistence += _lock_persistence_rate_down;
        }
        else
        {
            _lock_persistence += _lock_persistence_rate_up;
        }
        _lock_persistence = std::max(_lock_persistence_saturation_limit_lo,
                                     std::min(_lock_persistence_saturation_limit_hi,
                                              _lock_persistence));

        if (_locked)
        {
            if (_lock_persistence <= _lock_persistence_threshold_lo)
            {
                _locked = false;
            }
        }
        else
        {
            if (_lock_persistence >= _lock_persistence_threshold_hi)
            {
                _locked = true;
            }
        }

        if (_lock_persistence < _lock_persistence_saturation_limit_hi)
        {
            printf("PPS lock persistence: %ld\n", _lock_persistence);
        }

        _prev_completed_seconds = completed_seconds;
    }
}

uint32_t Pps::get_completed_seconds() const
{
    return _completed_seconds;
}

usec_t Pps::get_time_us_of(uint32_t completed_seconds, uint32_t additional_microseconds) const
{
    usec_t constexpr million = 1000000;

    usec_t top_of_desired_second_gps = million * static_cast<usec_t>(completed_seconds);
    usec_t top_of_last_second_gps = million * static_cast<usec_t>(_prev_completed_seconds);
    usec_t top_of_last_second_chip = _prev_top_of_second_time_us;

    double chip_time_per_gps_time =
        _bicycles_per_gps_second_average.get_current_average<double>() /
        static_cast<double>(bicycles_per_chip_second);

    susec_t top_of_second_delta_gps = signed_difference<usec_t>(top_of_desired_second_gps, top_of_last_second_gps);
    susec_t top_of_second_delta_chip = top_of_second_delta_gps * chip_time_per_gps_time;

    usec_t top_of_desired_second_chip = top_of_last_second_chip + top_of_second_delta_chip;

    return top_of_desired_second_chip + (additional_microseconds * chip_time_per_gps_time);
}

void Pps::get_time(uint32_t & completed_seconds, uint32_t & additional_microseconds) const
{
    completed_seconds = _completed_seconds;

    double chip_time_per_gps_time =
        _bicycles_per_gps_second_average.get_current_average<double>() /
        static_cast<double>(bicycles_per_chip_second);

    usec_t chip_time = time_us_64();
    usec_t top_of_last_second_chip = _prev_top_of_second_time_us;
    
    additional_microseconds = (chip_time - top_of_last_second_chip) / chip_time_per_gps_time;
}

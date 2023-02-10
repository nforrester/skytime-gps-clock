#include "Analog.h"

#include <cstdio>
#include <algorithm>

#include "pico/stdlib.h"

Analog::Analog(Pps const & pps,
               uint const analog_tick_pin,
               uint const sense0_pin,
               uint const sense3_pin,
               uint const sense6_pin,
               uint const sense9_pin):
    _pps(pps),
    _tick(analog_tick_pin),
    _sensors({
        { sense0_pin },
        { sense3_pin },
        { sense6_pin },
        { sense9_pin }
    })
{
    for (auto const & zone : get_iana_timezones())
    {
        if (std::get<std::string>(zone) == "America/Los_Angeles")
        {
            _pacific_time_zone = std::get<decltype(_pacific_time_zone)>(zone);
        }
    }
}

void Analog::pps_pulsed(TopOfSecond const & top)
{
    if (_next_tick_us < 1000000)
    {
        _next_tick_us = 0;
    }
    else
    {
        _next_tick_us = _next_tick_us % 1000000;
    }

    _error_vs_actual_time_ms = 0;
    if (_hand_pose_locked())
    {
        if (_pacific_time_zone)
        {
            Ymdhms desired;
            if (_pacific_time_zone->make_ymdhms(top, desired))
            {
                desired.hour = desired.hour % 12;
                if (desired.sec == 60)
                {
                    _leap_second_halt = true;
                    _tick_rate = 1.0;
                }
                else
                {
                    _leap_second_halt = false;
                    Ymdhms hand = desired;
                    hand.sec = _sec_hand.displayed_time_units(-1);
                    hand.min = _min_hand.displayed_time_units(hand.sec);
                    hand.hour = _hour_hand.displayed_time_units(hand.min);

                    int32_t error_vs_actual_time_s =
                        desired.subtract_and_return_non_leap_seconds(hand);
                    error_vs_actual_time_s = mod(error_vs_actual_time_s, secs_per_day/2);
                    if (error_vs_actual_time_s > secs_per_day/4)
                    {
                        error_vs_actual_time_s -= secs_per_day/2;
                    }
                    _error_vs_actual_time_ms = error_vs_actual_time_s * 1000;
                    _error_vs_actual_time_ms += _sec_hand.ticks_remainder() * 1000 / 16;
                }
            }
        }
    }
}

void Analog::dispatch(uint32_t const completed_seconds)
{
    usec_t threshold_time_us = _pps.get_time_us_of(completed_seconds, _next_tick_us);
    if (threshold_time_us <= time_us_64())
    {
        _manage_tick_rate();
        _next_tick_us += 1000000/16 / _tick_rate;
        if (_next_tick_us >= 1000000)
        {
            _next_tick_us += 10000000;
        }
        if (!_leap_second_halt && !_sync_halt)
        {
            _tick.toggle();
            ++_ticks_performed;

            _hour_hand.tick();
            _min_hand.tick();
            _sec_hand.tick();

            for (uint8_t quadrant = 0; quadrant < 4; ++quadrant)
            {
                _sensors[quadrant].tick();

                int32_t pass_duration = _sensors[quadrant].complete_pass_duration();
                if (pass_duration)
                {
                    printf("Detected hand for %ld ticks.\n", pass_duration);
                    if (pass_duration < 3)
                    {
                        printf("Likely spurious\n");
                    }
                    else if (pass_duration < 3*16)
                    {
                        printf("Second hand\n");
                        _sec_hand.new_sensor_reading(quadrant, pass_duration);
                    }
                    else if (20*16 < pass_duration && pass_duration < 3*60*16)
                    {
                        printf("Minute hand\n");
                        _min_hand.new_sensor_reading(quadrant, pass_duration);
                    }
                    else if (10*60*16 < pass_duration && pass_duration < 40*60*16)
                    {
                        printf("Hour hand\n");
                        _hour_hand.new_sensor_reading(quadrant, pass_duration);
                    }
                    else
                    {
                        printf("Likely spurious\n");
                    }
                }
            }
        }
    }
}

void Analog::show_sensors()
{
    printf("Sensors: %d %d %d %d\n", _sensors[0].hand_present(), _sensors[1].hand_present(), _sensors[2].hand_present(), _sensors[3].hand_present());
}

void Analog::Sensor::tick()
{
    bool new_state = !_sensor.get();
    if (new_state != _state)
    {
        if (!new_state)
        {
            _complete_pass_duration = _ticks_in_state;
        }

        _state = new_state;
        _ticks_in_state = 0;
    }
    else
    {
        ++_ticks_in_state;
    }
}

int32_t Analog::Sensor::complete_pass_duration()
{
    int32_t response = _complete_pass_duration;
    _complete_pass_duration = 0;
    return response;
}

void Analog::HandModel::new_sensor_reading(uint8_t quadrant, int32_t pass_duration)
{
    int32_t new_position = ticks_per_revolution/4*quadrant + pass_duration/2;

    if (locked)
    {
        int32_t error = new_position - ticks_since_top;
        if (std::abs(error) < ticks_per_revolution / 100)
        {
            int32_t proposed_correction = error * 0.03;
            if (proposed_correction == 0)
            {
                proposed_correction = std::max(static_cast<int32_t>(-1), std::min(static_cast<int32_t>(1), error));
            }
            ticks_since_top += proposed_correction;
            measurements_contesting_lock = 0;
        }
        else
        {
            ++measurements_contesting_lock;
            if (measurements_contesting_lock >= lock_break_threshold)
            {
                locked = false;
            }
        }
    }

    if (!locked)
    {
        ticks_since_top = new_position;
        measurements_contesting_lock = 0;
        locked = true;
    }
}

int32_t Analog::HandModel::ticks_remainder() const
{
        int32_t my_integral_quantity = static_cast<int32_t>(units_per_revolution) * ticks_since_top / ticks_per_revolution;
        return ticks_since_top - my_integral_quantity * ticks_per_revolution / units_per_revolution;
}

uint8_t Analog::HandModel::displayed_time_units(int32_t child_hand_persexage) const
{
    if (!locked)
    {
        return 99;
    }
    int32_t reported_ticks_since_top = ticks_since_top;
    if (child_hand_persexage >= 0)
    {
        int32_t my_ticks_remainder = ticks_remainder();
        int32_t ticks_per_unit = ticks_per_revolution / units_per_revolution;
        int32_t my_persexage = my_ticks_remainder * 60 / ticks_per_unit;

        int32_t persexage_error = mod(child_hand_persexage - my_persexage, static_cast<int32_t>(60));
        if (persexage_error >= 30)
        {
            persexage_error -= 60;
        }
        reported_ticks_since_top = ticks_since_top + persexage_error * ticks_per_unit / 60;
    }
    return static_cast<int32_t>(units_per_revolution) * reported_ticks_since_top / ticks_per_revolution;
}

void Analog::print_time() const
{
    int32_t sec_rem = _sec_hand.ticks_remainder();
    int8_t sec = _sec_hand.displayed_time_units(-1);
    int8_t min = _min_hand.displayed_time_units(sec);
    int8_t hour = _hour_hand.displayed_time_units(min);
    printf("Analog time: %02d:%02d:%02d.%03ld\n", hour, min, sec, sec_rem*1000/16);
    printf("Ticking at %f\n", _tick_rate);
    printf("Error %f\n", _error_vs_actual_time_ms/1000.0);
    printf("Halts %d %d\n", _leap_second_halt, _sync_halt);
    printf("Locks %d %d %d\n", _hour_hand.locked, _min_hand.locked, _sec_hand.locked);
}

bool Analog::_hand_pose_locked() const
{
    return _hour_hand.locked && _min_hand.locked && _sec_hand.locked;
}

void Analog::_manage_tick_rate()
{
    float desired_tick_rate = _error_vs_actual_time_ms / 1000.0 / 20.0 + 1.0;
    _sync_halt = false;

    float constexpr max_tick_rate_delta = 0.2;

    if (!_hand_pose_locked())
    {
        desired_tick_rate = 1000.0;
    }
    else if (_error_vs_actual_time_ms < -30000)
    {
        _sync_halt = true;
    }
    else if (_error_vs_actual_time_ms >= max_tick_rate_delta * ((12*60*60*1000)/(1+max_tick_rate_delta)))
    {
        _sync_halt = true;
    }

    if (_sync_halt)
    {
        _tick_rate = 1.0;
        return;
    }

    desired_tick_rate = std::max(1 - max_tick_rate_delta, desired_tick_rate);
    desired_tick_rate = std::min(1 + max_tick_rate_delta, desired_tick_rate);

    float tick_rate_error = desired_tick_rate - _tick_rate;
    float max_correction = 0.1 / (15*16*_tick_rate);
    float tick_rate_correction =
        std::max(-max_correction,
                 std::min(max_correction, tick_rate_error));
    if (std::abs(tick_rate_correction) < max_correction * 0.8)
    {
        _tick_rate = desired_tick_rate;
    }
    else
    {
        _tick_rate += tick_rate_correction;
    }
}

#include "Pps.h"
#include "time.h"
#include "iana_time_zones.h"

#include "Gpio.h"

#include <array>
#include <memory>

class Analog
{
public:
    Analog(Pps const & pps,
           uint const analog_tick_pin,
           uint const sense0_pin,
           uint const sense3_pin,
           uint const sense6_pin,
           uint const sense9_pin);
    void pps_pulsed(TopOfSecond const & top);
    void dispatch(uint32_t const completed_seconds);
    void show_sensors();

    void print_time() const;

    static bool unit_test();

private:
    Pps const & _pps;
    GpioOut _tick;
    uint32_t _next_tick_us = 0;
    float _tick_rate = 1.0;
    bool _leap_second_halt = false;
    bool _sync_halt = false;
    uint64_t _ticks_performed = 0;

    class Sensor
    {
    public:
        Sensor(uint const pin): _sensor(pin) {}
        void tick();
        bool hand_present() const { return _state; }
        int32_t complete_pass_duration();
    private:
        GpioIn _sensor;

        bool _state = false;
        uint32_t _ticks_in_state = 0;
        int32_t _complete_pass_duration;
    };

    std::array<Sensor, 4> _sensors;

    struct HandModel
    {
        void tick() { ticks_since_top = (ticks_since_top + 1) % ticks_per_revolution; }
        void new_sensor_reading(uint8_t quadrant, int32_t pass_duration);
        uint8_t displayed_time_units(int32_t child_hand_persexage) const;
        int32_t ticks_remainder() const;

        bool locked;
        uint16_t measurements_contesting_lock;
        uint16_t const lock_break_threshold;
        int32_t const ticks_per_revolution;
        int32_t ticks_since_top;
        uint8_t const units_per_revolution;
    };

    HandModel _hour_hand = {
        .locked = false,
        .measurements_contesting_lock = 0,
        .lock_break_threshold = 3,
        .ticks_per_revolution = 16*60*60*12,
        .ticks_since_top = 0,
        .units_per_revolution = 12,
    };

    HandModel _min_hand = {
        .locked = false,
        .measurements_contesting_lock = 0,
        .lock_break_threshold = 3,
        .ticks_per_revolution = 16*60*60,
        .ticks_since_top = 0,
        .units_per_revolution = 60,
    };

    HandModel _sec_hand = {
        .locked = false,
        .measurements_contesting_lock = 0,
        .lock_break_threshold = 10,
        .ticks_per_revolution = 16*60,
        .ticks_since_top = 0,
        .units_per_revolution = 60,
    };

    bool _hand_pose_locked() const;

    void _manage_tick_rate();

    std::shared_ptr<TimeRepresentation> _pacific_time_zone;

    int32_t _error_vs_actual_time_ms = 0;
};

#pragma once

#ifndef HOST_BUILD
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wvolatile"
  #include "hardware/pio.h"
  #pragma GCC diagnostic pop
#else
  #include <cstdint>
  using uint = unsigned int;
  using PIO = int;

  uint64_t time_us_64();
#endif

#include "MovingAverage.h"

using usec_t = uint64_t;
using susec_t = int64_t;

class Pps
{
public:
    Pps(PIO pio, uint const pin);

    void pio_init();
    void dispatch_fast_thread();
    void dispatch_main_thread();

    uint32_t get_completed_seconds() const;
    usec_t get_time_us_of(uint32_t completed_seconds, usec_t additional_microseconds) const;

    void get_time(uint32_t & completed_seconds, uint32_t & additional_microseconds) const;

    bool locked() const { return _locked; }

    void show_status() const;

private:
    // Constants
    static constexpr uint32_t bicycles_per_chip_second = 125000000/2;
    static constexpr uint32_t bicycles_per_nominal_pulse = bicycles_per_chip_second/10;

    // Fast thread
    uint _pin;
    PIO _pio;
    uint _sm;
    uint32_t _bicycles_in_last_second = 0;
    uint32_t _bicycles_in_last_pulse = 0;
    uint32_t _completed_seconds = 0;
    bool _pulse_complete = false;

    // Shared between threads
    uint32_t volatile _completed_seconds_main_thread_a = 0;
    uint32_t volatile _bicycles_in_last_second_main_thread = 0;
    uint32_t volatile _bicycles_in_last_pulse_main_thread = 0;
    usec_t volatile _top_of_second_main_thread = 0;
    uint32_t volatile _completed_seconds_main_thread_b = 0;

    // Main thread
    uint32_t _prev_completed_seconds = 0;
    usec_t _prev_top_of_second_time_us = 0;
    MovingAverage<uint64_t, 60> _bicycles_per_gps_second_average;

    int32_t static constexpr _lock_persistence_threshold_lo        =   0;
    int32_t static constexpr _lock_persistence_threshold_hi        =  10;
    int32_t static constexpr _lock_persistence_saturation_limit_lo =   0;
    int32_t static constexpr _lock_persistence_saturation_limit_hi = 300;
    int32_t static constexpr _lock_persistence_rate_up             =   3;
    int32_t static constexpr _lock_persistence_rate_down           =  -1;
    int32_t _lock_persistence = _lock_persistence_saturation_limit_lo;
    bool _locked = false;
};


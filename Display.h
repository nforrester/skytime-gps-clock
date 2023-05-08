#pragma once

#include "FiveSimdHt16k33Busses.h"

#include <array>

/*
 * 5 groups of 16 characters (8 dual character modules and 8 HT16K33 chips):
 * 3210FEDCBA9876543210  1
 * 7654FEDCBA9876543210  2
 * BA98FEDCBA9876543210  3
 * FEDCFEDCBA9876543210  4
 *    5
 *
 * PST YYYYMMDD hhmmssT
 * UTC YYYYMMDD hhmmssT
 * TAI YYYYMMDD hhmmssT
 * AREA OF GENERIC TEXT
 */

class Display
{
public:
    static size_t constexpr line_length = 20;
    static size_t constexpr num_lines = 5;

    template <typename T>
    using LineOf = std::array<T, line_length>;

    Display(FiveSimdHt16k33Busses & busses);

    void dispatch();

    bool printf(size_t line_idx, const char *fmt, ...)
        __attribute__ ((format (printf, 3, 4)));

    uint32_t error_count() const { return _error_count; }

    void dump_to_console(bool show_dots);

    void set_brightness(uint8_t const pulse_width) { _desired_pulse_width = pulse_width; }

private:
    FiveSimdHt16k33Busses & _busses;
    bool _command_in_progress = false;

    template <typename T>
    using ScreenOf = std::array<LineOf<T>, num_lines>;

    bool _set_char(size_t line_idx, size_t col, char ch);
    bool _set_dot(size_t line_idx, size_t col, bool dot);

    ScreenOf<char> _screen_text;
    ScreenOf<bool> _screen_dots;
    ScreenOf<bool> _screen_updates_required;

    struct Ht16k33
    {
        Ht16k33(){}

        Ht16k33(size_t line_idx_, size_t left_column_, size_t n_columns_):
            line_idx(line_idx_),
            left_column(left_column_),
            n_columns(n_columns_)
        {
        }

        size_t line_idx;
        size_t left_column;
        size_t n_columns;
    };

    struct SliceOfBusses
    {
        SliceOfBusses(){}

        SliceOfBusses(uint8_t address_,
                      std::array<Ht16k33, 5> chips_):
            address(address_),
            chips(chips_)
        {
        }

        uint8_t address;
        std::array<Ht16k33, 5> chips;
    };

    static size_t constexpr _num_slices = 3;
    std::array<SliceOfBusses, _num_slices> _slices;

    uint8_t _desired_pulse_width;
    std::array<uint8_t, _num_slices> _selected_pulse_width;
    void _make_progress_on_setting_brightness(uint8_t const pulse_width, bool blocking);

    uint32_t _error_count = 0;
};

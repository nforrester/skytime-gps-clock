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
    static size_t constexpr num_lines = 4;

    template <typename T>
    using LineOf = std::array<T, line_length>;

    Display(FiveSimdHt16k33Busses & busses);

    void dispatch();

    void write_text(size_t const line_idx, LineOf<char> const & text);
    void write_dots(size_t const line_idx, LineOf<bool> const & dots);

    uint32_t error_count() const { return _error_count; }

private:
    FiveSimdHt16k33Busses & _busses;
    bool _command_in_progress = false;

    template <typename T>
    using ScreenOf = std::array<LineOf<T>, num_lines>;

    ScreenOf<char> _screen_text;
    ScreenOf<bool> _screen_dots;
    ScreenOf<bool> _screen_updates_required;

    struct Ht16k33
    {
        Ht16k33(){}

        Ht16k33(size_t line_idx_, size_t left_column_):
            line_idx(line_idx_),
            left_column(left_column_)
        {
        }

        size_t line_idx;
        size_t left_column;
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

    std::array<SliceOfBusses, 8> _slices;

    uint32_t _error_count = 0;
};

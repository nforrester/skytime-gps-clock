#include "Display.h"

#include <cstdio>

Display::Display(FiveSimdHt16k33Busses & busses):
    _busses(busses)
{
    for (size_t line_idx = 0; line_idx < num_lines; ++line_idx)
    {
        for (size_t col = 0; col < line_length; ++col)
        {
            _screen_text[line_idx][col] = ' ';
            _screen_dots[line_idx][col] = false;
            _screen_updates_required[line_idx][col] = true;
        }
    }

    _slices = {
            SliceOfBusses(0x70, {
                    Ht16k33(0, 18),
                    Ht16k33(1, 18),
                    Ht16k33(2, 18),
                    Ht16k33(3, 18),
                    Ht16k33(0, 2),
                }),
            SliceOfBusses(0x60, {
                    Ht16k33(0, 16),
                    Ht16k33(1, 16),
                    Ht16k33(2, 16),
                    Ht16k33(3, 16),
                    Ht16k33(0, 0),
                }),
            SliceOfBusses(0x50, {
                    Ht16k33(0, 14),
                    Ht16k33(1, 14),
                    Ht16k33(2, 14),
                    Ht16k33(3, 14),
                    Ht16k33(1, 2),
                }),
            SliceOfBusses(0x40, {
                    Ht16k33(0, 12),
                    Ht16k33(1, 12),
                    Ht16k33(2, 12),
                    Ht16k33(3, 12),
                    Ht16k33(1, 0),
                }),
            SliceOfBusses(0x30, {
                    Ht16k33(0, 10),
                    Ht16k33(1, 10),
                    Ht16k33(2, 10),
                    Ht16k33(3, 10),
                    Ht16k33(2, 2),
                }),
            SliceOfBusses(0x20, {
                    Ht16k33(0, 8),
                    Ht16k33(1, 8),
                    Ht16k33(2, 8),
                    Ht16k33(3, 8),
                    Ht16k33(2, 0),
                }),
            SliceOfBusses(0x10, {
                    Ht16k33(0, 6),
                    Ht16k33(1, 6),
                    Ht16k33(2, 6),
                    Ht16k33(3, 6),
                    Ht16k33(3, 2),
                }),
            SliceOfBusses(0x00, {
                    Ht16k33(0, 4),
                    Ht16k33(1, 4),
                    Ht16k33(2, 4),
                    Ht16k33(3, 4),
                    Ht16k33(3, 0),
                }),
        };

    for (auto const & slice : _slices)
    {
        // Enable internal system clock
        {
            uint8_t constexpr command = 0x21;
            if (!_busses.blocking_command(slice.address, 1, &command, &command, &command, &command, &command))
            {
                printf("HT16K33 Failed to enable internal system clock on bus slice %02x.\n", slice.address);
                ++_error_count;
            }
        }

        // Set brightness
        {
            // valid values range from 0 to 15 inclusive.
            // 0 is the minimum brightness that isn't off.
            uint8_t constexpr pulse_width = 0;
            if (pulse_width > 0xf)
            {
                ++_error_count;
            }
            else
            {
                uint8_t constexpr command = 0xe0 | pulse_width;

                if (!_busses.blocking_command(slice.address, 1, &command, &command, &command, &command, &command))
                {
                    printf("HT16K33 Failed to set brightness on bus slice %02x.\n", slice.address);
                    ++_error_count;
                }
            }
        }

        // Display on, no blinking
        {
            uint8_t constexpr command = 0x81;
            if (!_busses.blocking_command(slice.address, 1, &command, &command, &command, &command, &command))
            {
                printf("HT16K33 Failed to activate display without blinking on bus slice %02x.\n", slice.address);
                ++_error_count;
            }
        }
    }
}

namespace
{
    uint32_t char_to_image(char ch)
    {
        /* -----A-----
         * |\   |   /|
         * | \  |  / |
         * B  C D E  F
         * |   \|/   |
         * *-G--*--H-*
         * |   /|\   |
         * I  J K L  M
         * | /  |  \ |
         * |/   |   \|
         * -----N----- */

        uint32_t constexpr a = 0x0020;
        uint32_t constexpr b = 0x0001;
        uint32_t constexpr c = 0x0002;
        uint32_t constexpr d = 0x0004;
        uint32_t constexpr e = 0x0008;
        uint32_t constexpr f = 0x0040;
        uint32_t constexpr g = 0x0010;
        uint32_t constexpr h = 0x0400;
        uint32_t constexpr i = 0x4000;
        uint32_t constexpr j = 0x2000;
        uint32_t constexpr k = 0x1000;
        uint32_t constexpr l = 0x0800;
        uint32_t constexpr m = 0x0080;
        uint32_t constexpr n = 0x0200;

        uint32_t constexpr letters[26] = {
            /* a */ a|b|f|g|h|i|m,
            /* b */ a|d|f|h|k|m|n,
            /* c */ a|b|i|n,
            /* d */ a|d|f|k|m|n,
            /* e */ a|b|g|h|i|n,
            /* f */ a|b|g|h|i,
            /* g */ a|b|h|i|m|n,
            /* h */ b|f|g|h|i|m,
            /* i */ a|d|k|n,
            /* j */ f|i|m|n,
            /* k */ b|e|g|i|l,
            /* l */ b|i|n,
            /* m */ b|c|e|f|i|m,
            /* n */ b|c|f|i|l|m,
            /* o */ a|b|f|i|m|n,
            /* p */ a|b|f|g|h|i,
            /* q */ a|b|f|i|l|m|n,
            /* r */ a|b|f|g|h|i|l,
            /* s */ a|b|g|h|m|n,
            /* t */ a|d|k,
            /* u */ b|f|i|m|n,
            /* v */ b|e|i|j,
            /* w */ b|f|i|j|l|m,
            /* x */ c|e|j|l,
            /* y */ c|e|k,
            /* z */ a|e|j|n,
        };

        uint32_t constexpr digits[10] = {
            /* 0 */ a|b|e|f|i|j|m|n,
            /* 1 */ f|m,
            /* 2 */ a|f|g|h|i|n,
            /* 3 */ a|f|h|m|n,
            /* 4 */ b|f|g|h|m,
            /* 5 */ a|b|g|h|m|n,
            /* 6 */ b|g|h|i|m|n,
            /* 7 */ a|e|j,
            /* 8 */ a|b|f|g|h|i|m|n,
            /* 9 */ a|b|f|g|h|m,
        };

        if ('a' <= ch && ch <= 'z')
        {
            return letters[ch - 'a'];
        }
        if ('A' <= ch && ch <= 'Z')
        {
            return letters[ch - 'A'];
        }
        if ('0' <= ch && ch <= '9')
        {
            return digits[ch - '0'];
        }
        return 0x0000;
    }

    uint32_t dot_to_image(bool dot)
    {
        return dot ? 0x0100 : 0x0000;
    }
}

void Display::dispatch()
{
    if (_command_in_progress)
    {
        bool success;
        if (_busses.try_end_command(success))
        {
            _command_in_progress = false;
            if (!success)
            {
                ++_error_count;
            }
        }
    }

    if (!_command_in_progress)
    {
        for (auto const & slice : _slices)
        {
            bool needs_update = false;
            for (auto const & chip : slice.chips)
            {
                if (_screen_updates_required[chip.line_idx][chip.left_column] ||
                    _screen_updates_required[chip.line_idx][chip.left_column+1])
                {
                    needs_update = true;
                    break;
                }
            }

            if (needs_update)
            {
                std::array<std::array<uint8_t, 5>, 5> commands;
                for (size_t bus = 0; bus < slice.chips.size(); ++bus)
                {
                    char left_char  = _screen_text[slice.chips[bus].line_idx][slice.chips[bus].left_column];
                    char right_char = _screen_text[slice.chips[bus].line_idx][slice.chips[bus].left_column+1];
                    char left_dot   = _screen_dots[slice.chips[bus].line_idx][slice.chips[bus].left_column];
                    char right_dot  = _screen_dots[slice.chips[bus].line_idx][slice.chips[bus].left_column+1];

                    uint32_t left_image = 0;
                    left_image |= char_to_image(left_char);
                    left_image |= dot_to_image(left_dot);
                    uint32_t right_image = 0;
                    right_image |= char_to_image(right_char);
                    right_image |= dot_to_image(right_dot);

                    commands[bus][0] = 0x00;
                    commands[bus][1] = (right_image >> 0) & 0xff;
                    commands[bus][2] = (right_image >> 8) & 0xff;
                    commands[bus][3] = (left_image  >> 0) & 0xff;
                    commands[bus][4] = (left_image  >> 8) & 0xff;
                }

                if (_busses.begin_command(
                        slice.address,
                        commands[0].size(),
                        commands[0].data(),
                        commands[1].data(),
                        commands[2].data(),
                        commands[3].data(),
                        commands[4].data()))
                {
                    _command_in_progress = true;
                }
                return;
            }
        }
    }
}

void Display::write_text(size_t const line_idx, LineOf<char> const & text)
{
    for (size_t col = 0; col < line_length; ++col)
    {
        if (_screen_text[line_idx][col] != text[col])
        {
            _screen_text[line_idx][col] = text[col];
            _screen_updates_required[line_idx][col] = true;
        }
    }
}

void Display::write_dots(size_t const line_idx, LineOf<bool> const & dots)
{
    for (size_t col = 0; col < line_length; ++col)
    {
        if (_screen_dots[line_idx][col] != dots[col])
        {
            _screen_dots[line_idx][col] = dots[col];
            _screen_updates_required[line_idx][col] = true;
        }
    }
}

void Display::dump_to_console(bool show_dots)
{
    if (EOF == putc('\n', stdout))
    {
        ++_error_count;
    }
    for (size_t line_idx = 0; line_idx < num_lines; ++line_idx)
    {
        for (size_t col = 0; col < line_length; ++col)
        {
            if (EOF == putc(_screen_text[line_idx][col], stdout))
            {
                ++_error_count;
            }
            if (show_dots && _screen_dots[line_idx][col])
            {
                if (EOF == putc('.', stdout))
                {
                    ++_error_count;
                }
            }
        }
        if (EOF == putc('\n', stdout))
        {
            ++_error_count;
        }
    }
}

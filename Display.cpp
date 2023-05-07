#include "Display.h"

#include <cstdio>
#include <cstdarg>

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
                    Ht16k33(0, 0, 8),
                    Ht16k33(1, 0, 8),
                    Ht16k33(2, 0, 8),
                    Ht16k33(3, 0, 8),
                    Ht16k33(4, 0, 8),
                }),
            SliceOfBusses(0x71, {
                    Ht16k33(0, 8, 8),
                    Ht16k33(1, 8, 8),
                    Ht16k33(2, 8, 8),
                    Ht16k33(3, 8, 8),
                    Ht16k33(4, 8, 8),
                }),
            SliceOfBusses(0x72, {
                    Ht16k33(0, 16, 4),
                    Ht16k33(1, 16, 4),
                    Ht16k33(2, 16, 4),
                    Ht16k33(3, 16, 4),
                    Ht16k33(4, 16, 4),
                }),
        };

    for (auto const & slice : _slices)
    {
        // Enable internal system clock
        {
            uint8_t constexpr command = 0x21;
            if (!_busses.blocking_write(slice.address, 1, &command, &command, &command, &command, &command))
            {
                ::printf("HT16K33 Failed to enable internal system clock on bus slice %02x.\n", slice.address);
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

                if (!_busses.blocking_write(slice.address, 1, &command, &command, &command, &command, &command))
                {
                    ::printf("HT16K33 Failed to set brightness on bus slice %02x.\n", slice.address);
                    ++_error_count;
                }
            }
        }

        // Display on, no blinking
        {
            uint8_t constexpr command = 0x81;
            if (!_busses.blocking_write(slice.address, 1, &command, &command, &command, &command, &command))
            {
                ::printf("HT16K33 Failed to activate display without blinking on bus slice %02x.\n", slice.address);
                ++_error_count;
            }
        }
    }
}

namespace
{
    uint16_t char_to_image(char ch)
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
            /* s */ a|c|h|m|n,
            /* t */ a|d|k,
            /* u */ b|f|i|m|n,
            /* v */ b|e|i|j,
            /* w */ b|f|i|j|l|m,
            /* x */ c|e|j|l,
            /* y */ c|e|k,
            /* z */ a|e|j|n,
        };

        uint32_t constexpr table[95] = {
            0,               // space
            0, // !
            0, // "
            0, // #
            a|c|h|m|n|d|k,   // $
            0, // %
            0, // &
            e,               // '
            0, // (
            0, // )
            d|e|h|l|k|j|g|c, // *
            d|k|g|h,         // +
            0, // ,
            g|h,             // -
            0, // .
            j|e,             // /
            a|b|e|f|i|j|m|n, // 0
            f|m,             // 1
            a|f|g|h|i|n,     // 2
            a|f|h|m|n,       // 3
            b|f|g|h|m,       // 4
            a|b|g|h|m|n,     // 5
            b|g|h|i|m|n,     // 6
            a|e|j,           // 7
            a|b|f|g|h|i|m|n, // 8
            a|b|f|g|h|m,     // 9
            0, // :
            0, // ;
            e|l,             // <
            g|h|n,           // =
            c|j,             // >
            0, // ?
            0, // @
            letters[0],      // A
            letters[1],      // B
            letters[2],      // C
            letters[3],      // D
            letters[4],      // E
            letters[5],      // F
            letters[6],      // G
            letters[7],      // H
            letters[8],      // I
            letters[9],      // J
            letters[10],     // K
            letters[11],     // L
            letters[12],     // M
            letters[13],     // N
            letters[14],     // O
            letters[15],     // P
            letters[16],     // Q
            letters[17],     // R
            letters[18],     // S
            letters[19],     // T
            letters[20],     // U
            letters[21],     // V
            letters[22],     // W
            letters[23],     // X
            letters[24],     // Y
            letters[25],     // Z
            0, // [
            c|l,             // backslash
            0, // ]
            e|f,             // ^
            n,               // _
            c,               // `
            letters[0],      // a
            letters[1],      // b
            letters[2],      // c
            letters[3],      // d
            letters[4],      // e
            letters[5],      // f
            letters[6],      // g
            letters[7],      // h
            letters[8],      // i
            letters[9],      // j
            letters[10],     // k
            letters[11],     // l
            letters[12],     // m
            letters[13],     // n
            letters[14],     // o
            letters[15],     // p
            letters[16],     // q
            letters[17],     // r
            letters[18],     // s
            letters[19],     // t
            letters[20],     // u
            letters[21],     // v
            letters[22],     // w
            letters[23],     // x
            letters[24],     // y
            letters[25],     // z
            0, // {
            d|k,             // |
            0, // }
            0, // ~
        };

        if (' ' <= ch && ch <= '~')
        {
            return table[ch - ' '];
        }
        return 0x0000;
    }

    uint16_t dot_to_image(bool dot)
    {
        return dot ? 0x0100 : 0x0000;
    }
}

void Display::dispatch()
{
    if (_command_in_progress)
    {
        bool success;
        if (_busses.try_end_write(success))
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
                for (size_t common = 0; common < chip.n_columns; ++common)
                {
                    uint8_t column = chip.n_columns - common - 1;
                    if (_screen_updates_required[chip.line_idx][chip.left_column+column])
                    {
                        needs_update = true;
                        goto done_checking_for_updates;
                    }
                }
            }
            done_checking_for_updates:

            if (needs_update)
            {
                size_t constexpr overhead_bytes = 1;
                size_t constexpr bytes_per_common = 2;
                size_t constexpr max_commons_per_chip = 8;
                size_t constexpr max_command_length = overhead_bytes + bytes_per_common * max_commons_per_chip;
                static_assert(max_command_length == FiveSimdHt16k33Busses::max_cmd_length);
                std::array<std::array<uint8_t, max_command_length>, 5> commands;
                for (size_t bus = 0; bus < slice.chips.size(); ++bus)
                {
                    commands[bus][0] = 0x00;

                    Ht16k33 const & chip = slice.chips[bus];
                    for (size_t common = 0; common < chip.n_columns; ++common)
                    {
                        uint8_t column = chip.n_columns - common - 1;
                        char the_char = _screen_text[chip.line_idx][chip.left_column+column];
                        bool the_dot  = _screen_dots[chip.line_idx][chip.left_column+column];

                        uint16_t image = 0;
                        image |= char_to_image(the_char);
                        image |= dot_to_image(the_dot);

                        commands[bus][common*2+1] = (image >> 0) & 0xff;
                        commands[bus][common*2+2] = (image >> 8) & 0xff;
                    }
                }

                if (_busses.begin_write(
                        slice.address,
                        overhead_bytes + bytes_per_common * slice.chips[0].n_columns,
                        commands[0].data(),
                        commands[1].data(),
                        commands[2].data(),
                        commands[3].data(),
                        commands[4].data()))
                {
                    _command_in_progress = true;

                    for (auto const & chip : slice.chips)
                    {
                        for (size_t common = 0; common < chip.n_columns; ++common)
                        {
                            uint8_t column = chip.n_columns - common - 1;
                            _screen_updates_required[chip.line_idx][chip.left_column+column] = false;
                        }
                    }
                }
                return;
            }
        }
    }
}

bool Display::printf(size_t line_idx, const char *fmt, ...)
{
    va_list args1;
    va_start(args1, fmt);
    va_list args2;
    va_copy(args2, args1);
    size_t num_chars = vsnprintf(NULL, 0, fmt, args1);
    char buf[1 + num_chars];
    va_end(args1);
    vsnprintf(buf, sizeof(buf), fmt, args2);
    va_end(args2);

    size_t buf_idx = 0;
    ssize_t col_idx = -1;
    while (buf_idx < num_chars)
    {
        char ch = buf[buf_idx];
        if (ch == '.')
        {
            if (!_set_char(line_idx, ++col_idx, ' '))
            {
                goto failure;
            }
            if (!_set_dot(line_idx, col_idx, true))
            {
                goto failure;
            }
            ++buf_idx;
        }
        else if (char_to_image(ch) == 0 && ch != ' ')
        {
            goto failure;
        }
        else
        {
            if (!_set_char(line_idx, ++col_idx, ch))
            {
                goto failure;
            }
            bool dot;
            if (buf[++buf_idx] == '.')
            {
                dot = true;
                ++buf_idx;
            }
            else
            {
                dot = false;
            }
            if (!_set_dot(line_idx, col_idx, dot))
            {
                goto failure;
            }
        }
    }

    while (++col_idx < static_cast<ssize_t>(_screen_text[0].size()))
    {
        if (!_set_char(line_idx, col_idx, ' '))
        {
            goto failure;
        }
        if (!_set_dot(line_idx, col_idx, false))
        {
            goto failure;
        }
    }

    return true;

    failure:
    ++_error_count;
    return false;
}

bool Display::_set_char(size_t line_idx, size_t col, char ch)
{
    if (line_idx >= _screen_text.size())
    {
        return false;
    }

    if (col >= _screen_text[line_idx].size())
    {
        return false;
    }

    if (_screen_text[line_idx][col] != ch)
    {
        _screen_text[line_idx][col] = ch;
        _screen_updates_required[line_idx][col] = true;
    }

    return true;
}

bool Display::_set_dot(size_t line_idx, size_t col, bool dot)
{
    if (line_idx >= _screen_dots.size())
    {
        return false;
    }

    if (col >= _screen_dots[line_idx].size())
    {
        return false;
    }

    if (_screen_dots[line_idx][col] != dot)
    {
        _screen_dots[line_idx][col] = dot;
        _screen_updates_required[line_idx][col] = true;
    }

    return true;
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

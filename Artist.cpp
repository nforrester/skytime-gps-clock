#include "Artist.h"

Artist::Artist(Display & display,
               Buttons & buttons,
               GpsUBlox & gps):
    _disp(display),
    _buttons(buttons),
    _gps(gps)
{
}

void Artist::top_of_tenth_of_second(uint8_t tenths)
{
    //_print_time(0, TimeZone("PDT", -7 * secs_per_hour));
    _print_time(0, TimeZone("PST", -8 * secs_per_hour), tenths);
    _print_time(1, TimeZone("TWT", 8 * secs_per_hour), tenths);
    _print_time(2, TimeZone("UTC", 0), tenths);
    _print_time(3, TimeRepTai(), tenths);

    bool print_result = _disp.printf(4, "Err %4ld %4ld %4ld", _disp.error_count(), _gps.tops_of_seconds().error_count(), _buttons.error_count());
    if (!print_result)
    {
        printf("Unable to format line 4 of display\n");
    }
}

void Artist::_print_time(size_t line, TimeRepresentation const & time_rep, uint8_t tenths)
{
    Ymdhms ymdhms;

    bool print_result;
    if (time_rep.make_ymdhms(_gps.tops_of_seconds().prev(), ymdhms))
    {
        print_result = _disp.printf(
            line,
            "%-4s%04d.%02d.%02d %02d.%02d.%02d.%d",
            time_rep.abbrev().c_str(),
            ymdhms.year,
            ymdhms.month,
            ymdhms.day,
            ymdhms.hour,
            ymdhms.min,
            ymdhms.sec,
            tenths);
    }
    else
    {
        print_result = _disp.printf(line, "%s Initializing...", time_rep.abbrev().c_str());
    }
    if (!print_result)
    {
        printf("Unable to format line %u of display\n", line);
    }
}

void Artist::_font_debug(size_t line)
{
    uint32_t constexpr cycles_per = 5;
    if (++_font_debug_count % cycles_per == 0)
    {
        char ch = ((_font_debug_count/cycles_per) % ('~' - ' ' + 1)) + ' ';
        _disp.printf(line, "                   %c", ch);
    }
}

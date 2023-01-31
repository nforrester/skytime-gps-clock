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
    Ymdhms const & utc = _gps.tops_of_seconds().prev().utc_ymdhms;
    Ymdhms const & tai = _gps.tops_of_seconds().prev().tai_ymdhms;
    Ymdhms const & loc = _gps.tops_of_seconds().prev().loc_ymdhms;
    bool const utc_valid = _gps.tops_of_seconds().prev().utc_ymdhms_valid;
    bool const tai_valid = _gps.tops_of_seconds().prev().tai_ymdhms_valid;
    bool const loc_valid = _gps.tops_of_seconds().prev().loc_ymdhms_valid;

    bool print_result;
    if (loc_valid)
    {
        print_result = _disp.printf(
            0,
            "PST %04d.%02d.%02d %02d.%02d.%02d.%d",
            loc.year,
            loc.month,
            loc.day,
            loc.hour,
            loc.min,
            loc.sec,
            tenths);
    }
    else
    {
        print_result = _disp.printf(0, "PST Initializing...");
    }
    if (!print_result)
    {
        printf("Unable to format line 0 of display\n");
    }

    if (utc_valid)
    {
        print_result = _disp.printf(
            1,
            "UTC %04d.%02d.%02d %02d.%02d.%02d.%d",
            utc.year,
            utc.month,
            utc.day,
            utc.hour,
            utc.min,
            utc.sec,
            tenths);
    }
    else
    {
        print_result = _disp.printf(1, "UTC Initializing...");
    }
    if (!print_result)
    {
        printf("Unable to format line 1 of display\n");
    }

    if (tai_valid)
    {
        print_result = _disp.printf(
            2,
            "TAI %04d.%02d.%02d %02d.%02d.%02d.%d",
            tai.year,
            tai.month,
            tai.day,
            tai.hour,
            tai.min,
            tai.sec,
            tenths);
    }
    else
    {
        print_result = _disp.printf(2, "TAI Initializing...");
    }
    if (!print_result)
    {
        printf("Unable to format line 2 of display\n");
    }

    print_result = _disp.printf(3, "Err %4ld %4ld %4ld", _disp.error_count(), _gps.tops_of_seconds().error_count(), _buttons.error_count());
    if (!print_result)
    {
        printf("Unable to format line 3 of display\n");
    }

    print_result = _disp.printf(4, "01234567890123456789");
    if (!print_result)
    {
        printf("Unable to format line 4 of display\n");
    }
}

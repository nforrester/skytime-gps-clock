/*
 * Need to make the time update 10 times per second
 *
 * Use this to buffer from PPS line to PPS LED
 * https://www.digikey.com/en/products/detail/diodes-incorporated/74LVC1G17W5-7/3829445
 *
 * Search Digikey for "Tactile Switches" to find some nice surface mount buttons.
 *
 * Drive analog clock with 3V H-bridge at 8Hz.
 * Can probably do it with op amps.
 * Use 2 of these to supply power to another 2, which form the differential drive for the analog clock coil.
 * https://www.digikey.com/en/products/detail/onsemi/NCV2002SN2T1G/1483945
 *
 * Try using these to detect the hands:
 * https://www.digikey.com/en/products/detail/vishay-semiconductor-opto-division/TCRT5000L/1681168
 */

#include <cstdio>
#include <memory>
#include <vector>
#include <limits>
#include <charconv>
#include "pico/stdlib.h"

#include "pico/binary_info.h"
#include "pico/multicore.h"

#include "unit_tests.h"
#include "Gpio.h"
#include "Pps.h"
#include "FiveSimdHt16k33Busses.h"
#include "Display.h"
#include "RingBuffer.h"
#include "GpsUBlox.h"

std::unique_ptr<Pps> pps;

void core1_main()
{
    while (true)
    {
        pps->update();
    }
}

int main()
{
    bi_decl(bi_program_description("GPS Clock"));

    stdio_init_all();

    printf("Paused...\n");
    sleep_ms(4000);
    printf("Go!\n");

    uint constexpr led_pin = 25;
    bi_decl(bi_1pin_with_name(led_pin, "LED"));
    GpioOut led(led_pin);
    printf("LED init complete.\n");

    printf("Running self test...\n");
    if (!unit_tests())
    {
        while (true)
        {
            printf("Failed self test.\n");
            led.on();
            sleep_ms(200);
            led.off();
            sleep_ms(200);
        }
    }
    printf("Self test passed.\n");

    uint constexpr pps_pin = 18;
    bi_decl(bi_1pin_with_name(pps_pin, "PPS"));
    pps = std::make_unique<Pps>(pio0, pps_pin);
    printf("PPS init complete.\n");

    uint constexpr gps_tx_pin = 16;
    uint constexpr gps_rx_pin = 17;
    bi_decl(bi_1pin_with_name(gps_tx_pin, "GPS"));
    bi_decl(bi_1pin_with_func(gps_tx_pin, GPIO_FUNC_UART));
    bi_decl(bi_1pin_with_name(gps_rx_pin, "GPS"));
    bi_decl(bi_1pin_with_func(gps_rx_pin, GPIO_FUNC_UART));
    GpsUBlox gps(uart0, gps_tx_pin, gps_rx_pin);
    if (gps.initialized_successfully())
    {
        printf("GPS init complete.\n");
    }
    else
    {
        printf("GPS init FAILED.\n");
    }

    uint constexpr ht16k33_scl_pin = 0;
    uint constexpr ht16k33_sda0_pin = 1;
    uint constexpr ht16k33_sda1_pin = 2;
    uint constexpr ht16k33_sda2_pin = 3;
    uint constexpr ht16k33_sda3_pin = 4;
    uint constexpr ht16k33_sda4_pin = 5;
    bi_decl(bi_1pin_with_name(ht16k33_scl_pin, "DISP SCL"));
    bi_decl(bi_1pin_with_name(ht16k33_sda0_pin, "DISP SDA0"));
    bi_decl(bi_1pin_with_name(ht16k33_sda1_pin, "DISP SDA1"));
    bi_decl(bi_1pin_with_name(ht16k33_sda2_pin, "DISP SDA2"));
    bi_decl(bi_1pin_with_name(ht16k33_sda3_pin, "DISP SDA3"));
    bi_decl(bi_1pin_with_name(ht16k33_sda4_pin, "DISP SDA4"));
    FiveSimdHt16k33Busses five_simd_ht16k33_busses(
        pio1, ht16k33_scl_pin, ht16k33_sda0_pin);

    Display display(five_simd_ht16k33_busses);
    if (display.error_count() == 0)
    {
        printf("Display init complete.\n");
    }
    else
    {
        printf("Display init FAILED. Error count: %ld.\n", display.error_count());
    }

    led.on();

    multicore_launch_core1(core1_main);

    uint32_t prev_completed_seconds = 0;
    while (true)
    {
        uint32_t completed_seconds, bicycles_in_last_second;
        pps->get_status(&completed_seconds, &bicycles_in_last_second);
        if (completed_seconds != prev_completed_seconds)
        {
            Ymdhms const & utc = gps.tops_of_seconds().next().utc_ymdhms;
            Ymdhms const & tai = gps.tops_of_seconds().next().tai_ymdhms;
            Ymdhms const & loc = gps.tops_of_seconds().next().loc_ymdhms;
            bool const utc_valid = gps.tops_of_seconds().next().utc_ymdhms_valid;
            bool const tai_valid = gps.tops_of_seconds().next().tai_ymdhms_valid;
            bool const loc_valid = gps.tops_of_seconds().next().loc_ymdhms_valid;
            uint32_t const error_count = gps.tops_of_seconds().error_count();
            printf("PPS: %ld %ld    %s %d-%02d-%02d %02d:%02d:%02d      %s %d-%02d-%02d %02d:%02d:%02d      %s %d-%02d-%02d %02d:%02d:%02d         %" PRIu32 " errors\n", completed_seconds, 2 * bicycles_in_last_second, utc_valid?"UTC":"utc", utc.year, utc.month, utc.day, utc.hour, utc.min, utc.sec, tai_valid?"TAI":"tai", tai.year, tai.month, tai.day, tai.hour, tai.min, tai.sec, loc_valid?"PST":"pst", loc.year, loc.month, loc.day, loc.hour, loc.min, loc.sec, error_count);

            prev_completed_seconds = completed_seconds;
            gps.pps_pulsed();

            Display::LineOf<bool> dots;
            dots = {
                false, false, false, false, false,
                false, false, false, true,  false,
                true,  false, true,  false, false,
                true,  false, true,  false, false,
            };

            Display::LineOf<char> line;
            int print_result = snprintf(line.data(), line.size(), "PST  %04d%02d%02d %02d%02d%02d", loc.year, loc.month, loc.day, loc.hour, loc.min, loc.sec);
            if (print_result != line.size()+1)
            {
                printf("Unable to format line 0 of display");
            }
            display.write_text(0, line);
            display.write_dots(0, dots);

            print_result = snprintf(line.data(), line.size(), "UTC  %04d%02d%02d %02d%02d%02d", utc.year, utc.month, utc.day, utc.hour, utc.min, utc.sec);
            if (print_result != line.size()+1)
            {
                printf("Unable to format line 1 of display");
            }
            display.write_text(1, line);
            display.write_dots(1, dots);

            print_result = snprintf(line.data(), line.size(), "TAI  %04d%02d%02d %02d%02d%02d", tai.year, tai.month, tai.day, tai.hour, tai.min, tai.sec);
            if (print_result != line.size()+1)
            {
                printf("Unable to format line 2 of display");
            }
            display.write_text(2, line);
            display.write_dots(2, dots);

            print_result = snprintf(line.data(), line.size(), "Errors: %6ld %5ld", display.error_count(), gps.tops_of_seconds().error_count());
            if (print_result != line.size()+1)
            {
                printf("Unable to format line 3 of display");
            }
            display.write_text(3, line);
            dots = {
                false, false, false, false, false,
                false, false, false, false, false,
                false, false, false, false, false,
                false, false, false, false, false,
            };
            display.write_dots(3, dots);
        }

        gps.update();
        five_simd_ht16k33_busses.dispatch();
        display.dispatch();
    }
}

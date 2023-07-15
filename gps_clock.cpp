/*
 * Use this to buffer from PPS line to PPS LED
 * https://www.digikey.com/en/products/detail/diodes-incorporated/74LVC1G17W5-7/3829445
 *
 * Search Digikey for "Tactile Switches" to find some nice surface mount buttons.
 * https://www.digikey.com/en/products/detail/c-k/PTS645SM43SMTR92-LFS/1146840
 *
 * Drive analog clock with 3V H-bridge at 8Hz.
 * Can probably do it with op amps.
 * Use 2 of these to supply power to another 2, which form the differential drive for the analog clock coil.
 * https://www.digikey.com/en/products/detail/onsemi/NCV2002SN2T1G/1483945
 *
 * Try using these to detect the hands:
 * https://www.digikey.com/en/products/detail/vishay-semiconductor-opto-division/TCRT5000L/1681168
 *
 *
 *
 *
 *
 * Transistor for WWVB pulldown
 * https://www.digikey.com/en/products/detail/toshiba-semiconductor-and-storage/SSM3K357R-LF/8611159
 *
 *
 *
 * Header for Pico
 * https://www.digikey.com/en/products/detail/molex/0878982026/3264922
 *
 * Header for ZOE-M8Q
 *
 * Header for Pico
 * https://www.digikey.com/en/products/detail/molex/0878982026/3264922
 * https://www.digikey.com/en/products/detail/molex/0878980826/3264915
 *
 * Gates:
 * https://www.digikey.com/short/j2rpb02v
 * https://www.digikey.com/short/8dnqh54j
 * https://www.digikey.com/short/1rr77d24
 *
 * Power supply:
 * https://www.digikey.com/short/2n95bmww
 * https://www.digikey.com/short/920v422n
 * https://www.digikey.com/en/products/detail/bourns-inc/SRP1265A-470M/4876628
 * https://www.digikey.com/en/products/detail/cui-devices/PJ-037B/1644546
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
#include "Buttons.h"
#include "Artist.h"
#include "Wwvb.h"
#include "Analog.h"

std::unique_ptr<Pps> pps;
bool volatile pps_go = false;

void core1_main()
{
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

    uint constexpr button_up_pin = 6;
    uint constexpr button_rt_pin = 7;
    uint constexpr button_dn_pin = 8;
    uint constexpr button_lf_pin = 9;
    bi_decl(bi_1pin_with_name(button_up_pin, "BTN UP"));
    bi_decl(bi_1pin_with_name(button_dn_pin, "BTN DN"));
    bi_decl(bi_1pin_with_name(button_lf_pin, "BTN LF"));
    bi_decl(bi_1pin_with_name(button_rt_pin, "BTN RT"));
    Buttons buttons(button_up_pin, button_dn_pin, button_lf_pin, button_rt_pin);
    if (buttons.error_count() == 0)
    {
        printf("Buttons init complete.\n");
    }
    else
    {
        printf("Buttons init FAILED. Error count: %ld.\n", buttons.error_count());
    }

    std::vector<std::tuple<std::string, std::shared_ptr<LinePrinter>>> extra_line_options;
    extra_line_options.push_back(pps->los_printer(display));
    Artist artist(display, buttons, gps, extra_line_options);
    printf("Artist init complete.\n");

    uint constexpr wwvb_carrier_pin = 20;
    uint constexpr wwvb_reduce_pin = 19;
    bi_decl(bi_1pin_with_name(wwvb_carrier_pin, "WWVB CARRIER"));
    bi_decl(bi_1pin_with_name(wwvb_reduce_pin, "WWVB REDUCE"));
    Wwvb wwvb(wwvb_carrier_pin, wwvb_reduce_pin);
    wwvb.set_carrier(false);
    printf("WWVB init complete.\n");

    uint constexpr analog_tick_pin = 11;
    uint constexpr sense0_pin = 12;
    uint constexpr sense3_pin = 13;
    uint constexpr sense6_pin = 14;
    uint constexpr sense9_pin = 15;
    bi_decl(bi_1pin_with_name(analog_tick_pin, "ANALOG TICK"));
    bi_decl(bi_1pin_with_name(sense0_pin, "ANALOG SENSE 0"));
    bi_decl(bi_1pin_with_name(sense3_pin, "ANALOG SENSE 3"));
    bi_decl(bi_1pin_with_name(sense6_pin, "ANALOG SENSE 6"));
    bi_decl(bi_1pin_with_name(sense9_pin, "ANALOG SENSE 9"));
    Analog analog(*pps, analog_tick_pin, sense0_pin, sense3_pin, sense6_pin, sense9_pin);

    led.on();

    printf("Releasing PPS monitoring thread.\n");
    pps_go = true;

    printf("Begin main loop.\n");

    /* This is more bits than needed for a single second, but while PPS is unlocked
     * we keep accumulating time here, so it can run up arbitrarily high values. */
    usec_t next_display_update_us = 0;
    uint32_t prev_completed_seconds = 0;

    bool wwvb_needs_top_of_second = false;
    uint32_t wwvb_raise_power_us = 100000000; // Never

    while (true)
    {
        gps.dispatch();
        five_simd_ht16k33_busses.dispatch();
        display.dispatch();
        buttons.dispatch();
        analog.dispatch(prev_completed_seconds);
        pps->dispatch_main_thread();

        uint32_t completed_seconds = pps->get_completed_seconds();
        if (completed_seconds != prev_completed_seconds)
        {
            prev_completed_seconds = completed_seconds;
            next_display_update_us = 0;
            gps.pps_lock_state(pps->locked());
            gps.pps_pulsed();
            analog.pps_pulsed(gps.tops_of_seconds().prev());

            if (pps->locked())
            {
                wwvb.set_carrier(true);
                wwvb_needs_top_of_second = true;
                wwvb_raise_power_us = 100000000; // Never
            }
            else
            {
                wwvb.set_carrier(false);
                wwvb_needs_top_of_second = false;
                wwvb_raise_power_us = 100000000; // Never
            }
        }

        usec_t display_update_time_us = pps->get_time_us_of(completed_seconds, next_display_update_us);
        if (display_update_time_us <= time_us_64())
        {
            uint8_t tenths = next_display_update_us / 100000 % 10;
            next_display_update_us += 100000;
            if (next_display_update_us == 1000000)
            {
                next_display_update_us += 10000000; // Move the next update far into the future.
            }

            artist.top_of_tenth_of_second(tenths);

            display.dump_to_console(true);
            printf("Error counts: %ld %ld %ld %ld\n",
                   display.error_count(),
                   gps.tops_of_seconds().error_count(),
                   buttons.error_count(),
                   artist.error_count());
            gps.show_status();
            pps->show_status();
            analog.show_sensors();
            analog.print_time();

            uint32_t a, b;
            pps->get_time(a, b);
            printf("Time: %lu %lu\n", a, b);
        }

        Button button;
        while (buttons.get_button(button))
        {
            artist.button_pressed(button);
        }

        display.set_brightness(artist.get_brightness());

        if (wwvb_needs_top_of_second)
        {
            wwvb_needs_top_of_second = false;
            wwvb_raise_power_us = wwvb.top_of_second(gps.tops_of_seconds().prev());
        }

        usec_t wwvb_raise_power_time_us = pps->get_time_us_of(completed_seconds, wwvb_raise_power_us);
        if (wwvb_raise_power_time_us <= time_us_64())
        {
            wwvb.raise_power();
        }
    }
}

int main()
{
    bi_decl(bi_program_description("GPS Clock"));

    stdio_init_all();

    printf("Paused...\n");
    sleep_ms(4000);
    printf("Go!\n");

    printf("Launching Main Thread.\n");
    multicore_launch_core1(core1_main);

    while (!pps_go)
    {
    }

    pps->pio_init();
    while (true)
    {
        pps->dispatch_fast_thread();
    }
}

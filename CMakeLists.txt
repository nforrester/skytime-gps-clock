cmake_minimum_required(VERSION 3.13)
add_compile_options(-Wall -Wextra -Werror)

include(pico_sdk_import.cmake)

project(gps_clock_project C CXX ASM)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
pico_sdk_init()

add_executable(gps_clock
    gps_clock.cpp
    time.cpp
    util.cpp
    unit_tests.cpp
    Pps.cpp
    FiveSimdHt16k33Busses.cpp
    Display.cpp
    packing.cpp
    RingBuffer.cpp
    GpsUBlox.cpp
    Buttons.cpp
    Artist.cpp
    Wwvb.cpp
)

pico_enable_stdio_usb(gps_clock 1)
pico_enable_stdio_uart(gps_clock 0)

pico_add_extra_outputs(gps_clock)

pico_generate_pio_header(gps_clock ${CMAKE_CURRENT_LIST_DIR}/pps.pio)
pico_generate_pio_header(gps_clock ${CMAKE_CURRENT_LIST_DIR}/five_simd_ht16k33_busses.pio)

target_link_libraries(gps_clock pico_stdlib hardware_uart pico_multicore hardware_pio hardware_pwm)

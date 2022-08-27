cmake_minimum_required(VERSION 3.13)
add_compile_options(-Wall -Wextra -Werror)

include(pico_sdk_import.cmake)

project(gps_clock_project C CXX ASM)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
pico_sdk_init()

add_executable(gps_clock
    gps_clock.cpp
)

pico_enable_stdio_usb(gps_clock 1)
pico_enable_stdio_uart(gps_clock 0)

pico_add_extra_outputs(gps_clock)

target_link_libraries(gps_clock pico_stdlib hardware_uart pico_multicore)

#!/bin/bash

set -e

mkdir -p bin_test
g++ -std=c++20 -Wall -Wextra -Werror -DHOST_BUILD=1 -o bin_test/unit_tests \
    unit_tests_main.cpp \
    unit_tests.cpp \
    time.cpp \
    util.cpp \
    packing.cpp \
    RingBuffer.cpp \
    Analog.cpp \
    gen/iana_time_zones.cpp \
    Pps.cpp
./bin_test/unit_tests

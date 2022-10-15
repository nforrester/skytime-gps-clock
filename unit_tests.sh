#!/bin/bash

set -e

mkdir -p bin_test
g++ -std=c++20 -Wall -Wextra -Werror unit_tests.cpp time.cpp util.cpp -o bin_test/unit_tests
./bin_test/unit_tests

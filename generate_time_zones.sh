#!/bin/bash

set -e

mkdir -p bin_gen
g++ -std=c++20 -Wall -Wextra -Werror generate_time_zones.cpp time.cpp -o bin_gen/generate_time_zones

mkdir -p gen
./bin_gen/generate_time_zones > gen/iana_time_zones.cpp

g++ -std=c++20 -Wall -Wextra -Werror check_generated_time_zones.cpp time.cpp gen/iana_time_zones.cpp -o bin_gen/check_generated_time_zones
./bin_gen/check_generated_time_zones

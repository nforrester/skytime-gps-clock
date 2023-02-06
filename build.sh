#!/bin/bash

set -ex

./generate_time_zones.sh

export PICO_SDK_PATH=/home/neil/fiddle/gps-clock/3rdparty/pico-sdk

rm -rf bin
mkdir bin
cd bin
cmake ..
make

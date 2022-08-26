#!/bin/bash

set -ex

minicom -b 115200 -D /dev/ttyACM0

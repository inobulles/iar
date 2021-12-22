#!/bin/sh
set -e

# tests the JSON packing functionality of IAR

bin/iar --json tests/test.json --output .testfiles/packed.iar

# unpack and compare files
# TODO compare files

bin/iar --unpack .testfiles/packed.iar --output .testfiles/out

# success

exit 0
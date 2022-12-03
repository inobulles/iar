#!/bin/sh
set -e

# tests the JSON packing functionality of IAR

iar --json test.json --output packed.iar

# unpack and compare files
# TODO compare *all* files

iar --unpack packed.iar --output out
diff iar out/test.json/test/large_file

# success

exit 0

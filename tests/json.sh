#!/bin/sh
set -e

# tests the JSON packing functionality of IAR

bin/iar --json tests/test.json --output .testfiles/packed.iar

# unpack and compare files
# TODO compare *all* files

bin/iar --unpack .testfiles/packed.iar --output .testfiles/out
diff bin/iar .testfiles/out/test.json/test/large_file

# success

exit 0
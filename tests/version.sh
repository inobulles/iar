#!/bin/sh
set -e

# tests the version

bin/iar --version > /dev/null

# success

exit 0
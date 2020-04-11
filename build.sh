#!/bin/bash
set -e

echo "Compiling source ..."
gcc -g main.c -o iar

echo "Moving binary to /bin ..."
sudo mv iar /bin

exit 0

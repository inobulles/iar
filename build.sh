#!/bin/sh
set -e

echo "[IAR Builder] Compiling source ..."
gcc -g main.c -o iar

echo "[IAR Builder] Installing binary ..."
sudo mv iar /usr/local/bin

exit 0

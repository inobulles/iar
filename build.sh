#!/bin/sh
set -e

echo "[IAR Builder] Compiling source ..."
cc main.c -o iar

echo "[IAR Builder] Installing binary ..."
su -l root -c "mv `realpath iar` /usr/local/bin"

exit 0

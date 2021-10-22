#!/bin/sh
set -e

rm -rf bin/
mkdir -p bin/

# '-D_DEFAULT_SOURCE' is necessary for the 'realpath' function

echo "[IAR Builder] Compiling library ..."
cc -std=c99 -D_DEFAULT_SOURCE -fPIC -c src/libiar.c -o bin/libiar.o -I src/

echo "[IAR Builder] Creating static library ..."
ar rc bin/libiar.a bin/libiar.o

echo "[IAR Builder] Indexing static library ..."
ranlib bin/libiar.a

echo "[IAR Builder] Creating shared library ..."
ld -shared bin/libiar.o -o bin/libiar.so

echo "[IAR Builder] Compiling command line tool ..."
cc -std=c99 src/main.c -o bin/iar -I src/ -L bin/ -liar

if [ $# -gt 0 ]; then
	exit 0
fi

echo "[IAR Builder] Installing libraries, binaries, and headers (/usr/local/) ..."

su_list="cp $(realpath src/iar.h) /usr/local/include/"
su_list="$su_list && cp $(realpath bin/libiar.a) $(realpath bin/libiar.so) /usr/local/lib/"
su_list="$su_list && cp $(realpath bin/iar) /usr/local/bin/"

su -l root -c "$su_list"

exit 0

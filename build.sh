#!/bin/sh
set -e

rm -rf bin/
mkdir -p bin/

# cc_flags="-DWITHOUT_JSON"

# '-D_DEFAULT_SOURCE' is necessary for the 'realpath' function
# TODO wonder if this could be a cause of my problem with realpath? (cf. 'src/main.c')

echo "[IAR Builder] Compiling library ..."
cc -std=c99 -D_DEFAULT_SOURCE $cc_flags -fPIC -c src/libiar.c -o bin/libiar.o -I src/

echo "[IAR Builder] Creating static library ..."
ar rc bin/libiar.a bin/libiar.o

echo "[IAR Builder] Indexing static library ..."
ranlib bin/libiar.a

# echo "[IAR Builder] Creating shared library ..."
# ld -shared bin/libiar.o -o bin/libiar.so

echo "[IAR Builder] Compiling command line tool ..."
#cc -std=c99 src/main.c -o bin/iar -I src/ -L bin/ -liar
cc -std=c99 $cc_flags src/main.c -o bin/iar -I src/ bin/libiar.a # linking statically, cf. 'src/main.c'

echo "[IAR Builder] Running tests ..."

rm -rf .testfiles
mkdir -p .testfiles

for path in $(find -L tests/ -maxdepth 1 -type f -name "*.sh"); do
	echo -n "[IAR Builder] Running $path test ..."
	sh $path
	echo " ✅ Passed"
done

rm -r .testfiles

if [ $# -gt 0 ]; then
	exit 0
fi

echo "[IAR Builder] Installing libraries, binaries, and headers (/usr/local/) ..."

su_list="cp $(realpath src/iar.h) /usr/local/include/"
# su_list="$su_list && cp $(realpath bin/libiar.a) $(realpath bin/libiar.so) /usr/local/lib/"
su_list="$su_list && cp $(realpath bin/libiar.a) /usr/local/lib/"
su_list="$su_list && cp $(realpath bin/iar) /usr/local/bin/"

su -l root -c "$su_list"

exit 0

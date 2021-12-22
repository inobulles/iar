#!/bin/sh
set -e

# tests the packing/unpacking functionality of IAR

# programatically create file structure

mkdir .testfiles/root

mkdir .testfiles/root/dir

echo "Microsoft, Google, Facebook, you name it. FAANG's are all evil." > .testfiles/root/first
echo "pears" > .testfiles/root/second

echo "I am a file in a subdirectory" > .testfiles/root/dir/test
cp bin/libiar.o .testfiles/root/dir/bin # test binary files too!
truncate -s 128m .testfiles/root/dir/large_file

# actually package

bin/iar --pack .testfiles/root --output .testfiles/packed.iar

if [ $(du -hA .testfiles/packed.iar | awk '{ print $1 }') != "128M" ]; then
	exit 1
fi

# unpack and compare files

bin/iar --unpack .testfiles/packed.iar --output .testfiles/out

diff .testfiles/out/root/first .testfiles/root/first
diff .testfiles/out/root/second .testfiles/root/second

diff .testfiles/out/root/dir/test .testfiles/root/dir/test
diff .testfiles/out/root/dir/bin .testfiles/root/dir/bin
diff .testfiles/out/root/dir/large_file .testfiles/root/dir/large_file

# success

exit 0
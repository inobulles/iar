#!/bin/sh
set -e

# tests the packing/unpacking functionality of IAR

# programatically create file structure

mkdir -p root/dir

echo "Microsoft, Google, Facebook, you name it. FAANG's are all evil." > root/first
echo "pears" > root/second

echo "I am a file in a subdirectory" > root/dir/test
cp libiar.so root/dir/bin # test binary files too!
truncate -s 128m root/dir/large_file

# actually package

iar --pack root --output packed.iar

if [ "$(uname)" = "aquaBSD" ] || [ "$(uname)" = "FreeBSD" ]; then
	# only on aquaBSD/FreeBSD because GNU 'du' doesn't support the -A flag

	if [ "$(du -hA packed.iar | awk '{ print $1 }')" != "128M" ]; then
		exit 1
	fi
fi

# unpack and compare files

iar --unpack packed.iar --output out

diff out/root/first root/first
diff out/root/second root/second

diff out/root/dir/test root/dir/test
diff out/root/dir/bin root/dir/bin
diff out/root/dir/large_file root/dir/large_file

# success

exit 0

#!/bin/sh

# Generate package folders with variant builds

# Number of parallel make processes
test -z "$PM" && PM=12

NAME="zeromerge"

VER="$(cat version.h | grep '#define VER "' | cut -d\" -f2)"
echo "Program version: $VER"

TA=__NONE__
PKGTYPE=gz

UNAME_S="$(uname -s)"
UNAME_P="$(uname -p)"
UNAME_M="$(uname -m)"

# Detect macOS
if [ "$UNAME_S" = "Darwin" ]
	then
	PKGTYPE=zip
	TA=mac32
	test "$UNAME_M" = "x86_64" && TA=mac64
fi

# Detect Power Macs under macOS
if [ "$UNAME_P" = "Power Macintosh" ]
	then
	PKGTYPE=zip
	TA=macppc32
	test "$(sysctl hw.cpu64bit_capable)" = "hw.cpu64bit_capable: 1" && TA=macppc64
fi

# Detect Linux
if [ "$UNAME_S" = "linux" ]
	then TA="linux-$UNAME_M"
	PKGTYPE=xz
fi

# Fall through - assume Windows
if [ "$TA" = "__NONE__" ]
	then
	PKGTYPE=zip
	TGT=$(gcc -v 2>&1 | grep Target | cut -d\  -f2- | cut -d- -f1)
	test "$TGT" = "i686" && TA=win32
	test "$TGT" = "x86_64" && TA=win64
	test "$UNAME_S" = "MINGW32_NT-5.1" && TA=winxp
	EXT=".exe"
fi

echo "Target architecture: $TA"
test "$TA" = "__NONE__" && echo "Failed to detect system type" && exit 1
PKGNAME="${NAME}-${VER}-$TA"

echo "Generating package for: $PKGNAME"
mkdir -p "$PKGNAME"
test ! -d "$PKGNAME" && echo "Can't create directory for package" && exit 1
cp CHANGES README.md LICENSE $PKGNAME/
E1=1; E2=0; E3=0
make clean && make -j$PM stripped && cp $NAME$EXT $PKGNAME/$NAME$EXT && E1=0
make clean
test $((E1 + E2 + E3)) -gt 0 && echo "Error building packages; aborting." && exit 1
test "$PKGTYPE" = "zip" && zip -9r $PKGNAME.zip $PKGNAME/
test "$PKGTYPE" = "gz"  && tar -c $PKGNAME/ | xz -e > $PKGNAME.tar.xz
test "$PKGTYPE" = "xz"  && tar -c $PKGNAME/ | xz -e > $PKGNAME.tar.xz
echo "Package generation complete."

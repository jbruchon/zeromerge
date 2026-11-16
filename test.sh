#!/bin/sh

# Test zeromerge for correct behavior

ERR=0

PROG="./zeromerge"
OUT=test_output.bin

! $PROG -v && echo "Compile the program first" && exit 1


echo -n "Testing 4K blocks:  "
rm -f "$OUT"
$PROG test/missing1.bin test/missing2.bin $OUT >/dev/null
if cmp -s test/correct.bin $OUT
	then echo "PASSED"; ERR=0
	else echo "FAILED"; ERR=1
fi

echo -n "Testing 4K + tail:  "
rm -f "$OUT"
$PROG test/missing1_short.bin test/missing2_short.bin $OUT >/dev/null
if cmp -s test/correct_short.bin $OUT
	then echo "PASSED"; ERR=0
	else echo "FAILED"; ERR=1
fi

echo -n "Testing mismatch1:  "
rm -f "$OUT"
if ! $PROG test/missing1.bin test/mismatch.bin $OUT 2>/dev/null >/dev/null
	then echo "PASSED"; ERR=0
	else echo "FAILED"; ERR=1
fi

echo -n "Testing mismatch2:  "
rm -f "$OUT"
if ! $PROG test/missing1_short.bin test/mismatch_short.bin $OUT >/dev/null 2>/dev/null
	then echo "PASSED"; ERR=0
	else echo "FAILED"; ERR=1
fi

echo -n "Testing UnicodeNam: "
rm -f "$OUT"
$PROG test/ちゅくちゅく１.bin test/ちゅくちゅく２.bin $OUT >/dev/null
if cmp -s test/万斉.bin $OUT
	then echo "PASSED"; ERR=0
	else echo "FAILED"; ERR=1
fi

echo -n "Testing HardLinked: "
ln -f test/missing1.bin test/hard_linked.bin || echo "ERROR: can't create hard link!"
rm -f "$OUT"
if ! $PROG test/missing1.bin test/hard_linked.bin $OUT >/dev/null 2>/dev/null
	then echo "PASSED"; ERR=0
	else echo "FAILED"; ERR=1
fi
rm -f test/hard_linked.bin

exit $ERR
exit $ERR

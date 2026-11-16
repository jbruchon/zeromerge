#!/bin/sh

# Test zeromerge for correct behavior

ERR=0

PROG="./zeromerge"
OPTS="-C 0"
OUT=test_output.bin

! $PROG -v && echo "Compile the program first" && exit 1


echo -n "Testing 4K blocks:  "
rm -f "./$OUT"
$PROG $OPTS test/missing1.bin test/missing2.bin "$OUT" >/dev/null
if cmp -s test/correct.bin "$OUT"
	then echo "PASSED"; ERR=0
	else echo "FAILED"; ERR=1
fi

echo -n "Testing 4K + tail:  "
rm -f "./$OUT"
$PROG $OPTS test/missing1_short.bin test/missing2_short.bin "$OUT" >/dev/null
if cmp -s test/correct_short.bin "$OUT"
	then echo "PASSED"; ERR=0
	else echo "FAILED"; ERR=1
fi

echo -n "Testing mismatch1:  "
rm -f "./$OUT"
if ! $PROG $OPTS test/missing1.bin test/mismatch.bin "$OUT" 2>/dev/null >/dev/null
	then echo "PASSED"; ERR=0
	else echo "FAILED"; ERR=1
fi

echo -n "Testing mismatch2:  "
rm -f "./$OUT"
if ! $PROG $OPTS test/missing1_short.bin test/mismatch_short.bin "$OUT" >/dev/null 2>/dev/null
	then echo "PASSED"; ERR=0
	else echo "FAILED"; ERR=1
fi

echo -n "Testing UnicodeNam: "
rm -f "./$OUT"
$PROG $OPTS test/ちゅくちゅく１.bin test/ちゅくちゅく２.bin "$OUT" >/dev/null
if cmp -s test/万斉.bin "$OUT"
	then echo "PASSED"; ERR=0
	else echo "FAILED"; ERR=1
fi

echo -n "Testing HardLinked: "
ln -f test/missing1.bin test/hard_linked.bin || echo "ERROR: can't create hard link!"
rm -f "./$OUT"
if ! $PROG $OPTS test/missing1.bin test/hard_linked.bin "$OUT" >/dev/null 2>/dev/null
	then echo "PASSED"; ERR=0
	else echo "FAILED"; ERR=1
fi
rm -f "./test/hard_linked.bin"

echo -n "Testing Recursive:  "
RECTEST="test/rec_scratch"
rm -rf "./$RECTEST"
mkdir -p "./$RECTEST"
cp test/missing1.bin "./$RECTEST/file1.bin"
cp test/missing2.bin "./$RECTEST/file2.bin"
$PROG $OPTS -r "./$RECTEST" >/dev/null
if cmp -s test/correct.bin "./$RECTEST/file1.bin" && cmp -s test/correct.bin "./$RECTEST/file2.bin"
	then echo "PASSED"; ERR=0
	else echo "FAILED"; ERR=1
fi
rm -rf "./$RECTEST"

exit $ERR

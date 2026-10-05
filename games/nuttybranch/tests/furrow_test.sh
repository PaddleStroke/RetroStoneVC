#!/bin/sh
# Nutty Branch: the bark's furrows measured on screen lines just below the squirrel (tests/furrow_measure.py): evenly
# spaced round the cylinder, so on screen they bunch up and thin out toward the sides, following cos(angle).
#   furrow_test.sh <headless binary> <tmp dir> [verbose]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE.
H=$1
T=${2:-/tmp}/nuttybranch-furrows
VERBOSE=$3
mkdir -p "$T"
args=""
for run in "5 300" "6 420" "7 520"; do
    set -- $run
    for y in 200 212 224; do
        f="$T/s$1-f$2-y$y.png"
        out=$($H --frames $(($2 + 1)) --opt music=0 --opt bot=1 --opt ready=1 --opt seed=$1 --opt linedump=$y \
              --shot $2:"$f" 2>&1 | grep "line $y:")
        c=$(echo "$out" | sed 's/.*centre \(-*[0-9]*\) scale \([0-9]*\) row.*/\1/')
        a=$(echo "$out" | sed 's/.*centre \(-*[0-9]*\) scale \([0-9]*\) row.*/\2/')
        args="$args $f $y $c $a"
    done
done
res=$(python3 "$(dirname "$0")/furrow_measure.py" $args)
code=$?
if [ -n "$VERBOSE" ] || [ $code -ne 0 ]; then echo "$res"; fi
if [ $code -eq 0 ]; then
    echo "  ok   bark furrows: $(echo "$res" | grep "neighbouring furrows" | sed "s/ over .*median/, median/; s/ (the texture.*//") apart in angle; on screen closer and thinner toward the sides (cos)"
else
    echo "  FAIL bark furrows"
fi
exit $code

#!/bin/sh
# Nutty Branch: every prop's pre-drawn size as it comes (--opt lodcheck=1, src/draw.c lod_note). Each frame each object
# is drawn at the size nearest its projected scale (the 16 sizes 18% apart); along an approach the drawn size must
# never shrink and must change one size at a time; and whatever sits on the branch or grows out of it must be drawn
# anchored on the branch's projected surface (on a line with bark, within the branch's span on that line) through
# the curves, dips and crests.
#   lod_test.sh <headless binary> [verbose]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE.
H=$1
VERBOSE=$2
fail=0
steps=0
anchors=0
kept=0
runs=0
one() {   # one <label> <options...>
    label=$1
    shift
    line=$($H --opt music=0 --opt bot=1 --opt ready=1 --opt lodcheck=1 "$@" 2>&1 | grep "lodcheck: [0-9]* objects" | tail -1)
    set -- $(echo "$line" | sed 's/.*lodcheck: \([0-9]*\) objects, \([0-9]*\) size steps, \([0-9]*\) backwards, \([0-9]*\) of more than one size, \([0-9]*\) not the nearest size.*; \([0-9]*\) anchors, \([0-9]*\) off the branch (\([0-9]*\) kept.*/\1 \2 \3 \4 \5 \6 \7 \8/')
    if [ $# -ne 8 ] || [ "$1" -eq 0 ]; then
        echo "  FAIL $label: no lodcheck report ($line)"
        fail=1
        return
    fi
    if [ "$3" -ne 0 ] || [ "$4" -ne 0 ] || [ "$5" -ne 0 ] || [ "$7" -ne 0 ]; then
        echo "  FAIL $label: $line"
        fail=1
    elif [ -n "$VERBOSE" ]; then
        echo "  ok   $label: $1 objects, $2 size steps (each one size larger), $6 anchors on the branch"
    fi
    steps=$((steps + $2))
    anchors=$((anchors + $6))
    kept=$((kept + $8))
    runs=$((runs + 1))
}
for s in 1 2 3; do one "seed $s, the whole course" --frames 3000 --opt seed=$s; done
for k in twig hole knot web cater snake cone owl pecker swing gap fork crow snow bees; do
    one "$k" --frames 1500 --opt seed=4 --opt start=1000 --opt only=$k
done
if [ $fail -eq 0 ]; then
    echo "  ok   LODs over $runs runs: $steps size steps, each to the next larger size (18%), none shrinking, none skipped;" \
         "$anchors anchors drawn on the branch ($kept kept on their line near crests and the horizon)"
fi
exit $fail

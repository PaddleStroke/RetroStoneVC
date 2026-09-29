#!/bin/sh
# Bomber Mole smoke tests: every level loads and runs, and scripted inputs prove
# walking, collecting, digging, bombing and changing depth.
#   smoke_test.sh <headless binary> <tmp dir>
H=$1
T=${2:-/tmp}
D=$(dirname "$0")
L=$D/../levels
fail=0
check() {   # check <name> <expected substring> <output>
    if echo "$3" | grep -q "$2"; then echo "  ok   $1"; else echo "  FAIL $1: expected '$2' in: $(echo "$3" | grep state)"; fail=1; fi
}
n=0
for f in "$L"/*.txt; do
    name=$(basename "$f" .txt)
    out=$($H --frames 240 --opt level=$name --opt nointro=1 --opt dump=1 --opt strict=1 2>&1)
    if echo "$out" | grep -q "level error\|cannot start"; then echo "  FAIL $name: $(echo "$out" | grep error)"; fail=1; fi
    if ! echo "$out" | grep -q "state: st=6"; then echo "  FAIL $name does not reach play: $(echo "$out" | grep state)"; fail=1; fi
    n=$((n + 1))
done
echo "  ok   $n levels load and run"
out=$($H --frames 200 --opt level=spring-1 --opt nointro=1 --opt dump=1 --input $D/dig.input 2>&1)
check "dig through the soft dirt wall" "depth=0 x=[6-9] y=1 " "$out"
out=$($H --frames 330 --opt level=spring-1 --opt nointro=1 --opt dump=1 --input $D/hole_grub.input 2>&1)
check "dig, go down the hole, collect a grub below" "grubs_left=4/5 depth=1 x=14 y=3" "$out"
base=$($H --frames 10 --opt level=spring-2 --opt nointro=1 --opt dump=1 2>&1 | sed -n 's/.*rocks=\([0-9]*\).*/\1/p')
out=$($H --frames 260 --opt level=spring-2 --opt nointro=1 --opt dump=1 --input $D/bomb.input 2>&1)
check "a bomb breaks a rock ($base rocks before)" "rocks=$((base - 1)) " "$out"
out=$($H --frames 400 --opt level=spring-1 --opt nointro=1 --opt dump=1 --input $D/selfblast.input 2>&1)
check "own blast: knocked out, restart with 2 lives" "st=6 .*hearts=1 lives=2" "$out"
# a naive player (bomb in its path, walk away; --opt bot=1) beats the tier-1 ferrets of spring 2
for s in 12,9 4,10; do
    out=$($H --frames 6000 --opt level=spring-2 --opt nointro=1 --opt dump=1 --opt bot=1 --opt spawn=1,$s 2>&1)
    check "naive bot bombs both tier-1 ferrets of spring 2 (start 1,$s)" "depth=1 .*lives=3 .*enemies=[0-9]*,0," "$out"
done
# chases: the facing of every enemy type stays steady (no left-right flip-flop)
for t in sleepy_ferret brown_ferret polecat stoat ginger_cat grey_cat black_cat siamese_cat; do
    for inp in none loop; do
        extra=""
        [ $inp = loop ] && extra="--input $D/chase_loop.input"
        out=$($H --frames 600 --data $D/data --opt level=spring-9 --opt nointro=1 --opt dump=1 --opt god=1 \
              --opt enemytype=$t --opt spawn=1,7,5 $extra 2>&1 | grep "facing:")
        ch=$(echo "$out" | sed -n 's/.*changes=\([0-9]*\).*/\1/p'); ti=$(echo "$out" | sed -n 's/.*tiles=\([0-9]*\).*/\1/p')
        ji=$(echo "$out" | sed -n 's/.*jitter=\([0-9]*\).*/\1/p')
        if [ -n "$ch" ] && [ "$ji" -eq 0 ] && [ "$ch" -le "$ti" ]; then echo "  ok   chase $t ($inp mole): $ch facing changes over $ti tiles, no jitter"
        else echo "  FAIL chase $t ($inp mole): $out"; fail=1; fi
    done
done
# the softlock search catches a one-way gate (the pre-fix spring 4)
out=$(python3 $D/../tools/check_levels.py $D/data/levels_bad/softlock-gate.txt 2>&1)
check "check_levels finds the spring 4 gate softlock" "SOFTLOCK" "$out"
# the objective bot (--opt bot=2) finishes spring 1 and 2 using only what the game shows
for l in spring-1 spring-2; do
    out=$($H --frames 20000 --opt level=$l --opt nointro=1 --opt dump=1 --opt bot=2 2>&1)
    check "objective bot clears $l (grubs, then the molehill)" "state: st=12 .*grubs_left=0" "$out"
done
exit $fail

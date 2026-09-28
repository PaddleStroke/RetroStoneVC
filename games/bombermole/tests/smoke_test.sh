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
out=$($H --frames 60 --opt level=spring-1 --opt nointro=1 --opt dump=1 --input $D/grub.input 2>&1)
check "walk and collect a grub" "grubs_left=4/5 depth=0 x=2 y=3" "$out"
out=$($H --frames 240 --opt level=spring-1 --opt nointro=1 --opt dump=1 --input $D/dig.input 2>&1)
check "dig through soft dirt" "x=8 y=3" "$out"
out=$($H --frames 160 --opt level=spring-1 --opt nointro=1 --opt dump=1 --input $D/hole.input 2>&1)
check "fall through a hole to depth 1" "depth=1 x=2 y=6" "$out"
base=$($H --frames 10 --opt level=spring-2 --opt nointro=1 --opt dump=1 2>&1 | sed -n 's/.*rocks=\([0-9]*\).*/\1/p')
out=$($H --frames 260 --opt level=spring-2 --opt nointro=1 --opt dump=1 --input $D/bomb.input 2>&1)
check "a bomb breaks a rock ($base rocks before)" "rocks=$((base - 1)) " "$out"
out=$($H --frames 400 --opt level=spring-1 --opt nointro=1 --opt dump=1 --input $D/selfblast.input 2>&1)
check "own blast: knocked out, restart with 2 lives" "st=6 .*hearts=1 lives=2" "$out"
exit $fail

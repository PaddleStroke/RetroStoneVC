#!/bin/sh
# Leady Squid: the caps test (tests/test_caps.c) for each obstacle theme and for a 2-player race, in parallel.
#   caps_test.sh <test_caps binary> <tmp dir> [runs]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/leadysquid/LICENSE.
B=$1
T=${2:-/tmp}/leadysquid-caps
R=${3:-60}
mkdir -p "$T"
i=0
for cfg in skip=0 skip=10 skip=20 skip=30 players=2; do
    i=$((i + 1))
    ( "$B" --runs "$R" --seed $i --opt $cfg > "$T/$i.txt" 2>&1; echo $? > "$T/$i.rc" ) &
done
wait
fail=0
for k in 1 2 3 4 5; do
    cat "$T/$k.txt"
    [ "$(cat "$T/$k.rc")" = 0 ] || fail=1
done
exit $fail

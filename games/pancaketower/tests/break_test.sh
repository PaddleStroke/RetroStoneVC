#!/bin/sh
# Pancake Tower: the breakthrough test (tests/test_break.c) through the ceiling and the roof, in 1 and 2 players and
# from a pre-stacked tower, in parallel.
#   break_test.sh <test_break binary> <tmp dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pancaketower/LICENSE.
B=$1
T=${2:-/tmp}/pancaketower-break
mkdir -p "$T"
set -- "1 player, seed 1|--frames 5000 --opt bot=1 --opt ready=1 --opt seed=1" \
       "1 player, seed 4|--frames 5000 --opt bot=1 --opt ready=1 --opt seed=4" \
       "2 players|--frames 4500 --opt bot=2 --opt players=2 --opt ready=1 --opt seed=5" \
       "4 players, pre-stacked|--frames 2500 --opt bot=4 --opt players=4 --opt ready=1 --opt seed=5 --opt start=30" \
       "pre-stacked (start=30)|--frames 2500 --opt bot=1 --opt ready=1 --opt seed=2 --opt start=30"
i=0
for c in "$@"; do
    i=$((i + 1))
    ( $B ${c#*|} > "$T/$i.txt" 2>&1; echo $? > "$T/$i.rc" ) &
done
wait
fail=0
i=0
for c in "$@"; do
    i=$((i + 1))
    printf "  %-24s\n" "${c%%|*}"
    cat "$T/$i.txt"
    [ "$(cat "$T/$i.rc")" = 0 ] || fail=1
done
exit $fail

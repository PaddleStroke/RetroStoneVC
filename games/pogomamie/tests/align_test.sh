#!/bin/sh
# Pogo Mamie: the alignment test (tests/test_align.c) in each district, at night and in a 2-player race, in parallel.
#   align_test.sh <test_align binary> <tmp dir> [frames]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pogomamie/LICENSE.
B=$1
T=${2:-/tmp}/pogomamie-align
N=${3:-12000}
mkdir -p "$T"
i=0
for cfg in skip=0 skip=512 skip=1024 skip=1536 skip=2100 players=2; do
    i=$((i + 1))
    ( "$B" --frames "$N" --opt $cfg --opt seed=$((i * 11 + (i == 5))) --opt ready=1 > "$T/$i.txt" 2>&1; echo $? > "$T/$i.rc" ) &
done
wait
fail=0
set -- montmartre seine haussmann eiffel night 2-players
for k in 1 2 3 4 5 6; do
    printf "  %-11s" "$1"
    shift
    cat "$T/$k.txt"
    [ "$(cat "$T/$k.rc")" = 0 ] || fail=1
done
exit $fail

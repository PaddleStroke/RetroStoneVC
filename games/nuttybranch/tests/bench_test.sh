#!/bin/sh
# Nutty Branch: the frame cost. The branch is drawn line by line (an affine matrix and a bark palette per line from
# the raster callback, a line-scroll table), so the per-line work must fit the RetroStone2's budget: the host's
# average x 15-20 (the A20 estimate of docs/spec.md) must stay under 16.7 ms: the test fails above 0.83 ms average
# on the host (16.7 / 20). The worst frame is reported (a busy host's scheduler makes it noisy: it is not checked).
#   bench_test.sh <headless binary> [verbose]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE.
H=$1
V=$2
fail=0
scene() {   # scene <name> <options...>
    name=$1; shift
    out=$($H --frames 2400 --bench 300 --opt music=1 "$@" 2>&1)
    avg=$(echo "$out" | sed -n 's/.*total *avg *\([0-9.]*\) us.*/\1/p')
    max=$(echo "$out" | sed -n 's/.*total *avg *[0-9.]* us *max *\([0-9]*\) us.*/\1/p')
    upd=$(echo "$out" | sed -n 's/.*update+draw *avg *\([0-9.]*\) us.*/\1/p')
    ppu=$(echo "$out" | sed -n 's/.*render (PPU) avg *\([0-9.]*\) us.*/\1/p')
    spr=$(echo "$out" | sed -n 's/.*sprites max \([0-9]*\), max per line \([0-9]*\).*/\1 \2/p')
    a20=$(awk -v a="$avg" 'BEGIN { printf "%.1f-%.1f", a * 15 / 1000, a * 20 / 1000 }')
    echo "  $name: host avg ${avg} us (update+draw ${upd}, PPU ${ppu}), max ${max} us, sprites ${spr% *} (per line ${spr#* }); A20 estimate ${a20} ms"
    [ -n "$V" ] && echo "$out" | grep -v "^\[rs\]"
    if awk -v a="$avg" 'BEGIN { exit !(a > 833) }'; then
        echo "  FAIL $name: over the budget"; fail=1
    fi
}
scene "1 player, spring (the bot)" --opt bot=1 --opt seed=7 --opt ready=1
scene "1 player, from 1500 m (night, fast, busy)" --opt bot=1 --opt seed=11 --opt ready=1 --opt start=1500
scene "4 players (4 bots)" --opt bot=4 --opt seed=7 --opt ready=1
scene "4 players from 900 m" --opt bot=4 --opt seed=5 --opt ready=1 --opt start=900
exit $fail

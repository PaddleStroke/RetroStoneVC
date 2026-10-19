#!/bin/sh
# Prickle Patch: the frame cost. The host's average x 15-20 (the A20 estimate of docs/spec.md) must stay under the
# RetroStone2's 16.7 ms: the test fails above 0.83 ms average on the host (16.7 / 20). The worst frame is reported
# (a busy host's scheduler makes it noisy: it is not checked), with the sprites (the guideline: 128, 32 a line) and the
# soft layer's tiles (its pool: 576; the pixels dropped when it is full must stay 0). Scenes: the first minutes (4:3),
# the busiest gardens (minute 10 with the Stag Beetle and a full build, minute 14 with the most pests and every tool
# evolved), a wide screen and a tall one.
#   bench_test.sh <headless binary> [verbose]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pricklepatch/LICENSE.
H=$1
V=$2
fail=0
scene() {   # scene <name> <frames> <options...>
    name=$1 frames=$2; shift 2
    out=$($H --frames "$frames" --bench 120 --opt music=1 --opt dump=1 "$@" 2>&1)
    avg=$(echo "$out" | sed -n 's/.*total *avg *\([0-9.]*\) us.*/\1/p')
    max=$(echo "$out" | sed -n 's/.*total *avg *[0-9.]* us *max *\([0-9]*\) us.*/\1/p')
    spr=$(echo "$out" | sed -n 's/.*sprites max \([0-9]*\), max per line \([0-9]*\).*/\1 \2/p')
    pests=$(echo "$out" | sed -n 's/.*state: .* pests=\([0-9]*\) .*soft=\([0-9]*\) overflow=\([0-9]*\).*/\1 \2 \3/p')
    vram=$(echo "$out" | sed -n 's/.*vram \([0-9]*\) bytes.*/\1/p')
    a20=$(awk -v a="$avg" 'BEGIN { printf "%.1f-%.1f", a * 15 / 1000, a * 20 / 1000 }')
    set -- $pests
    echo "  $name: host avg ${avg} us, max ${max} us, sprites ${spr% *} (per line ${spr#* }), pests $1, soft tiles at most $2" \
         "(frames short of tiles: $3), VRAM $vram bytes; A20 estimate ${a20} ms"
    [ -n "$V" ] && echo "$out" | grep -v "^\[rs\]"
    if [ -z "$avg" ] || awk -v a="$avg" 'BEGIN { exit !(a > 833) }'; then
        echo "  FAIL $name: over the budget (or no measure)"; fail=1
    fi
}
scene "the first minutes, the bot (4:3)" 7200 --opt bot=1 --opt ready=1 --opt seed=7
scene "minute 10: the Stag Beetle, a full build (4:3)" 2400 --opt bot=1 --opt ready=1 --opt seed=7 --opt time=600 --opt god=1
scene "minute 14: the most pests (4:3)" 2400 --opt bot=1 --opt ready=1 --opt seed=3 --opt time=840 --opt god=1
scene "minute 14 at 520 columns" 2400 --opt bot=1 --opt ready=1 --opt seed=3 --opt time=840 --opt god=1 --width 520
scene "minute 14 upright (320 x 440)" 2400 --opt bot=1 --opt ready=1 --opt seed=3 --opt time=840 --opt god=1 --display 1080x2340
exit $fail

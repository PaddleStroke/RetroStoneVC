#!/bin/sh
# Puffin Plunge: the frame cost. The host's average x 15-20 (the A20 estimate of docs/spec.md) must stay under the
# RetroStone2's 16.7 ms: the test fails above 0.83 ms average on the host (16.7 / 20). The worst frame is reported
# (a busy host's scheduler makes it noisy: it is not checked), with the sprites (the guideline: 128, 32 a line).
# Scenes: the first levels (4:3), the frozen deep (the most critters), the boss, a wide screen and a tall one.
#   bench_test.sh <headless binary> [verbose]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/puffinplunge/LICENSE.
H=$1
V=$2
fail=0
scene() {   # scene <name> <frames> <options...>
    name=$1 frames=$2; shift 2
    out=$($H --frames "$frames" --bench 120 --opt music=1 --opt dump=1 "$@" 2>&1)
    avg=$(echo "$out" | sed -n 's/.*total *avg *\([0-9.]*\) us.*/\1/p')
    max=$(echo "$out" | sed -n 's/.*total *avg *[0-9.]* us *max *\([0-9]*\) us.*/\1/p')
    spr=$(echo "$out" | sed -n 's/.*sprites max \([0-9]*\), max per line \([0-9]*\).*/\1 \2/p')
    vram=$(echo "$out" | sed -n 's/.*vram \([0-9]*\) bytes.*/\1/p')
    a20=$(awk -v a="$avg" 'BEGIN { printf "%.1f-%.1f", a * 15 / 1000, a * 20 / 1000 }')
    echo "  $name: host avg ${avg} us, max ${max} us, sprites ${spr% *} (per line ${spr#* }), VRAM $vram bytes; A20 estimate ${a20} ms"
    [ -n "$V" ] && echo "$out" | grep -v "^\[rs\]"
    if [ -z "$avg" ] || awk -v a="$avg" 'BEGIN { exit !(a > 833) }'; then
        echo "  FAIL $name: over the budget (or no measure)"; fail=1
    fi
}
scene "levels 1-2, the bot (4:3)" 4000 --opt bot=1 --opt ready=1 --opt seed=7
scene "the frozen deep (4:3)" 2400 --opt bot=1 --opt ready=1 --opt seed=7 --opt level=11 --opt god=1
scene "the boss (4:3)" 3000 --opt bot=1 --opt ready=1 --opt seed=7 --opt level=12 --opt god=1
scene "the frozen deep at 520 columns" 2400 --opt bot=1 --opt ready=1 --opt seed=7 --opt level=11 --opt god=1 --width 520
scene "the frozen deep upright (320 x 480)" 2400 --opt bot=1 --opt ready=1 --opt seed=7 --opt level=11 --opt god=1 --display 1080x2340
exit $fail

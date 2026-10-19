#!/bin/sh
# Goat Gorge: the frame cost. The host's average x 15-20 (the A20 estimate of docs/spec.md) must stay under the
# RetroStone2's 16.7 ms: the test fails above 0.83 ms average on the host (16.7 / 20). The worst frame is reported
# (a busy host's scheduler makes it noisy: it is not checked). Scenes: a climb through the meadow (4:3), the busiest
# rooms of the later chapters (wind streaks, snow, platforms), a wide screen and a tall one.
#   bench_test.sh <headless binary> [verbose]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/goatgorge/LICENSE.
H=$1
V=$2
fail=0
scene() {   # scene <name> <options...>
    name=$1; shift
    out=$($H --frames 1500 --bench 120 --opt music=1 "$@" 2>&1)
    avg=$(echo "$out" | sed -n 's/.*total *avg *\([0-9.]*\) us.*/\1/p')
    max=$(echo "$out" | sed -n 's/.*total *avg *[0-9.]* us *max *\([0-9]*\) us.*/\1/p')
    spr=$(echo "$out" | sed -n 's/.*sprites max \([0-9]*\), max per line \([0-9]*\).*/\1 \2/p')
    a20=$(awk -v a="$avg" 'BEGIN { printf "%.1f-%.1f", a * 15 / 1000, a * 20 / 1000 }')
    echo "  $name: host avg ${avg} us, max ${max} us, sprites ${spr% *} (per line ${spr#* }); A20 estimate ${a20} ms"
    [ -n "$V" ] && echo "$out" | grep -v "^\[rs\]"
    if [ -z "$avg" ] || awk -v a="$avg" 'BEGIN { exit !(a > 833) }'; then
        echo "  FAIL $name: over the budget (or no measure)"; fail=1
    fi
}
scene "chapter 1, the bot (4:3)" --opt bot=1 --opt ready=1
for c in 3 4 5; do
    grep -q "^$c 1 " games/goatgorge/levels/solutions.txt 2>/dev/null && \
        scene "chapter $c, the bot" --opt bot=1 --opt ready=1 --opt unlock=1 --opt chapter=$c
done
scene "chapter 1 at 520 columns" --opt bot=1 --opt ready=1 --width 520
scene "chapter 1 upright (320 x 692)" --opt bot=1 --opt ready=1 --display 1080x2340
exit $fail

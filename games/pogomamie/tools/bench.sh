#!/bin/sh
# Pogo Mamie: the per-frame cost of the busiest scenes (headless, host clock). The A20 estimate is 15-20x the host
# (docs/spec.md "Performance").
#   bench.sh <headless binary>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pogomamie/LICENSE.
H=$1
scene() {   # scene <name> <options...>
    name=$1
    shift
    out=$($H --frames 3600 --opt music=1 --opt ready=1 "$@" --bench 300 2>&1)
    avg=$(echo "$out" | sed -n 's/.*total *avg *\([0-9.]*\) us.*/\1/p')
    max=$(echo "$out" | sed -n 's/.*total .*max *\([0-9]*\) us.*/\1/p')
    upd=$(echo "$out" | sed -n 's/.*update+draw *avg *\([0-9.]*\) us.*/\1/p')
    spr=$(echo "$out" | sed -n 's/.*sprites max \([0-9]*\), max per line \([0-9]*\).*/\1 sprites, \2 per line/p')
    printf "  %-34s avg %6s us (update+draw+bot %6s)  worst %5s us  %s\n" "$name" "$avg" "$upd" "$max" "$spr"
}
echo "per frame on this host (budget 16667 us):"
scene "Montmartre, the bot" --opt bot=1 --opt seed=7
scene "the Seine, the bot" --opt bot=1 --opt seed=12 --opt skip=520
scene "Haussmann, gusts, the bot" --opt bot=1 --opt seed=13 --opt skip=1100
scene "night, 2 bots racing (heaviest)" --opt bot=2 --opt players=2 --opt seed=9 --opt skip=2600
scene "night, 2 players, no bot" --opt players=2 --opt seed=9 --opt skip=2600

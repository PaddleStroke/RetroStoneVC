#!/bin/sh
# Blueberry Tumble: the per-frame cost (make blueberrytumble-bench). A long bot run in god mode goes through every
# biome and the night (the affine playfield, the parallax, the most sprites: glides with their gust leaves, cones,
# juice); then the heaviest stretch is measured again on its own. The RetroStone2 (Allwinner A20, Cortex-A7) runs
# these 15 to 20 times slower than a desktop core: the budget is 16667 us per frame there.
#   bench.sh <headless binary>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/blueberrytumble/LICENSE.
H=$1
echo "a whole run, every biome and the night (god mode: no input, it bounces off everything):"
I=${TMPDIR:-/tmp}/bt-bench.input
printf "30 tap A\n" > "$I"
out=$($H --frames 12000 --opt god=1 --opt ready=1 --opt seed=3 --input "$I" --bench 300 2>/dev/null | tail -6)
echo "$out"
worst=$(echo "$out" | sed -n 's/.*(frame \([0-9]*\)).*/\1/p')
tot=$(echo "$out" | sed -n 's/.*total  *avg *\([0-9.]*\) us *max *\([0-9]*\) us.*/\2/p')
if [ -n "$tot" ]; then
    lo=$((tot * 15)); hi=$((tot * 20))
    echo "the heaviest frame: $tot us on this machine -> $lo..$hi us on the A20 (x15..x20; the budget is 16667 us)"
fi

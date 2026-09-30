#!/bin/sh
# Pancake Tower: the per-frame cost of the heaviest scenes (host clock; the A20 is about 15-20x slower).
#   bench.sh <headless binary>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pancaketower/LICENSE.
H=$1
b() {
    name=$1; shift
    echo "== $name"
    $H --opt music=1 "$@" | grep "update+draw\|render\|audio\|total\|sprites max"
}
b "1 player, a bot run through the kitchen, the ceiling, the roof and the sky (frames 300-3300)" \
    --frames 3300 --opt bot=1 --opt seed=1 --opt ready=1 --bench 300
b "1 player in space: stars, the moon and the cow, a falling piece, 125 layers (frames 60-1260)" \
    --frames 1260 --opt bot=1 --opt seed=2 --opt ready=1 --opt start=112 --bench 60
b "2 players, split screen, both towers in the clouds, syrup splashes (frames 60-2460)" \
    --frames 2460 --opt bot=2 --opt seed=5 --opt players=2 --opt ready=1 --opt start=50 --bench 60
b "2 players from the kitchen: both crash through the ceiling at once (frames 300-1500)" \
    --frames 1500 --opt bot=2 --opt seed=2 --opt players=2 --opt ready=1 --bench 300

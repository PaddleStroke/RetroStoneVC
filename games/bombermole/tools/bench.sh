#!/bin/sh
# Per-frame cost of the heaviest Bomber Mole scenes on this host.
#   bench.sh <headless binary>
H=$1
echo "== spring 5, explosion chain (12 bombs, sprinklers, rain layer, 4 BG layers) =="
$H --frames 400 --opt level=spring-5 --opt nointro=1 --opt scene=chain --bench 1 | sed -n '/^bench/,$p'
echo "== spring 7, windmills (gusts, 2 cats, dog, wind particles, weather) =="
$H --frames 600 --opt level=spring-7 --opt nointro=1 --bench 1 | sed -n '/^bench/,$p'
echo "== winter 1 (snow layer, ice) =="
$H --frames 400 --opt level=winter-1 --opt nointro=1 --bench 1 | sed -n '/^bench/,$p'

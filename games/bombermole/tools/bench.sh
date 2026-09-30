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
echo "== summer: a corn field burning, two bee swarms, two harvesters (test arena) =="
printf "180 tap B\n183 LEFT\n200 -\n" > /tmp/bm_bench_summer.input
$H --frames 800 --data "$(dirname "$0")/../tests/data" --opt level=summer-15 --opt nointro=1 --opt spawn=0,10,6 \
   --opt god=1 --input /tmp/bm_bench_summer.input --bench 330 | sed -n '/^bench/,$p'
echo "== autumn: fog, 3 gale lanes with leaves and drifting ferrets, the mole riding a cart loop (test arena) =="
printf "5 tap RIGHT\n" > /tmp/bm_bench_autumn.input
$H --frames 700 --data "$(dirname "$0")/../tests/data" --opt level=autumn-17 --opt nointro=1 --opt spawn=0,1,9 \
   --opt god=1 --input /tmp/bm_bench_autumn.input --bench 100 | sed -n '/^bench/,$p'
echo "== winter: night (the lamp's window + colour math), the owl swooping, big snowballs rolling (test arena) =="
printf "4 tap DOWN\n20 tap B\n24 tap DOWN\n40 tap DOWN\n56 tap B\n60 tap DOWN\n76 tap DOWN\n92 tap B\n96 tap DOWN\n" > /tmp/bm_bench_winter.input
$H --frames 800 --data "$(dirname "$0")/../tests/data" --opt level=winter-19 --opt nointro=1 --opt god=1 \
   --input /tmp/bm_bench_winter.input --bench 150 | sed -n '/^bench/,$p'

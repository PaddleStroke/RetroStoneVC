#!/bin/sh
# Pancake Tower: headless screenshots for games/pancaketower/docs/screenshots (2x, like the 640x480 LCD).
#   screenshots.sh <headless binary> <out dir>
# The bot plays (--opt bot=1); the moments are found in its log (botlog=1). start=N pre-stacks N pancakes (high up).
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pancaketower/LICENSE.
H=$1
O=${2:-games/pancaketower/docs/screenshots}
T=${TMPDIR:-/tmp}/pt_shots.$$
mkdir -p "$O" "$T"
run() { $H --scale 2 --opt music=0 "$@" > "$T/log.txt" 2>&1; }
at() { sed -n "s/$1/\\1/p" "$T/run.txt" | head -1; }

run --frames 130 --shot 120:"$O/title.png"
# one bot run, logged: its moments
$H --frames 4000 --opt music=0 --opt sound=0 --opt bot=1 --opt seed=1 --opt ready=1 --opt botlog=1 > "$T/run.txt" 2>&1
cut=$(at '.*land p1 f\([0-9]*\): h=[0-9]* w=[0-9]* cut=[1-9][0-9]* .*')
chain=$(at '.*perfect chain 7 at frame \([0-9]*\).*')
top=$(at '.*topping 1 landed at frame \([0-9]*\).*')
ceil=$(at '.*crashed through the ceiling at frame \([0-9]*\).*')
roof=$(at '.*broke through the roof at frame \([0-9]*\).*')
syrup=$(at '.*land p1 f\([0-9]*\): h=5 .*')
run --frames 3000 --opt bot=1 --opt seed=1 --opt ready=1 \
    --shot $((cut + 5)):"$O/early-stacking.png" --shot $((chain + 3)):"$O/perfect-chain.png" \
    --shot $((top - 14)):"$O/topping.png" --shot $((ceil + 5)):"$O/ceiling-crash.png" \
    --shot $((roof + 6)):"$O/roof.png" --shot $((syrup + 22)):"$O/syrup.png"
# high up: pre-stacked towers
run --frames 260 --opt bot=1 --opt seed=2 --opt ready=1 --opt start=30 --shot 250:"$O/sky.png"
run --frames 260 --opt bot=1 --opt seed=2 --opt ready=1 --opt start=56 --shot 250:"$O/clouds.png"
run --frames 260 --opt bot=1 --opt seed=2 --opt ready=1 --opt start=80 --shot 250:"$O/stratosphere.png"
run --frames 440 --opt bot=1 --opt seed=2 --opt ready=1 --opt start=112 --shot 420:"$O/space-cow.png"
# game over with a medal: the bot misses on purpose at 30 pancakes
run --frames 6000 --opt bot=1 --opt seed=3 --opt ready=1 --opt botstop=30
f=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$T/log.txt")
m=$(sed -n 's/.*player 1 missed at frame \([0-9]*\).*/\1/p' "$T/log.txt")
run --frames $((f + 60)) --opt bot=1 --opt seed=3 --opt ready=1 --opt botstop=30 --shot $((m + 14)):"$O/miss.png" \
    --shot $((f + 50)):"$O/gameover.png"
printf "600 tap START\n" > "$T/pause.input"
run --frames 640 --opt bot=1 --opt seed=7 --opt ready=1 --input "$T/pause.input" --shot 630:"$O/pause.png"
# two players (the bots play both)
run --frames 130 --opt ready=1 --opt players=2 --shot 120:"$O/ready-2-players.png"
run --frames 1500 --opt bot=2 --opt seed=9 --opt players=2 --opt ready=1 --shot 1480:"$O/versus-2-players.png"
run --frames 20000 --opt bot=2 --opt seed=9 --opt players=2 --opt ready=1
f=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$T/log.txt")
[ -n "$f" ] && run --frames $((f + 60)) --opt bot=2 --opt seed=9 --opt players=2 --opt ready=1 --shot $((f + 50)):"$O/gameover-2-players.png"
rm -rf "$T"
ls "$O"

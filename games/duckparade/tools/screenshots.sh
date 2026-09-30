#!/bin/sh
# Duck Parade: headless screenshots for games/duckparade/docs/screenshots (2x, like the 640x480 LCD).
#   screenshots.sh <headless binary> <out dir>
# The bot plays (--opt bot=1); --opt scenes=1 logs the first frame of each kind of scene, the shots are taken there.
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/duckparade/LICENSE.
H=$1
O=${2:-games/duckparade/docs/screenshots}
T=${TMPDIR:-/tmp}/dp_shots.$$
mkdir -p "$O" "$T"
run() { $H --scale 2 --opt music=0 --opt sound=0 "$@" > "$T/log.txt" 2>&1; }
scene() { sed -n "s/.*scene $1 \([0-9]*\).*/\1/p" "$T/log.txt" | head -1; }

run --frames 130 --shot 120:"$O/title.png"
run --frames 60 --opt ready=1 --shot 50:"$O/get-ready.png"
# one long bot run: a road, a river with ducklings, a train, a long parade, a banking
S=${SEED:-9}
run --frames 30000 --opt bot=1 --opt ready=1 --opt seed=$S --opt scenes=1
road=$(scene road); river=$(scene river); train=$(scene train); bank=$(scene bank)
args=""
[ -n "$road" ] && args="$args --shot $((road + 2)):$O/road.png"
[ -n "$river" ] && args="$args --shot $((river + 4)):$O/river.png"
[ -n "$train" ] && args="$args --shot $((train + 1)):$O/train.png"

[ -n "$bank" ] && args="$args --shot $((bank + 14)):$O/banking-nest-pond.png"
last=0
for f in $road $river $train $bank; do [ "$f" -gt $last ] && last=$f; done
run --frames $((last + 20)) --opt bot=1 --opt ready=1 --opt seed=$S $args
echo "scenes (seed $S): road $road, river $river, train $train, banking $bank"
# a long parade (8 ducklings or more): another course
P=${PARADE_SEED:-17}
run --frames 20000 --opt bot=1 --opt ready=1 --opt seed=$P --opt scenes=1
parade=$(scene parade)
[ -n "$parade" ] && run --frames $((parade + 10)) --opt bot=1 --opt ready=1 --opt seed=$P --shot $((parade + 6)):"$O/long-parade.png"
echo "a parade of 8 (seed $P) at frame $parade"
# the fox: the bot stops hopping at 30 points; the fox watches, then the game-over panel
run --frames 3000 --opt bot=1 --opt ready=1 --opt seed=5 --opt botstop=30 --opt scenes=1
fox=$(scene fox)
over=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$T/log.txt")
caught=$(sed -n 's/.*caught by the fox at frame \([0-9]*\).*/\1/p' "$T/log.txt")
run --frames $((over + 60)) --opt bot=1 --opt ready=1 --opt seed=5 --opt botstop=30 --shot $((fox + 40)):"$O/fox.png" \
    --shot $((caught + 12)):"$O/fox-pounce.png" --shot $((over + 50)):"$O/game-over.png"
# a medal: a longer run, stopped at 110 (the silver egg)
run --frames 12000 --opt bot=1 --opt ready=1 --opt seed=7 --opt botstop=110
over=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$T/log.txt")
[ -n "$over" ] && run --frames $((over + 60)) --opt bot=1 --opt ready=1 --opt seed=7 --opt botstop=110 --shot $((over + 50)):"$O/game-over-silver-egg.png"
# pause
printf "400 tap START\n" > "$T/pause.input"
run --frames 440 --opt bot=1 --opt ready=1 --opt seed=7 --input "$T/pause.input" --shot 430:"$O/pause.png"
# Mother and Father (the bot plays both)
run --frames 1400 --opt bot=2 --opt players=2 --opt ready=1 --opt seed=6 --shot 1300:"$O/coop-2-players.png"
run --frames 60 --opt ready=1 --opt players=2 --shot 50:"$O/get-ready-2-players.png"
rm -rf "$T"
ls "$O"

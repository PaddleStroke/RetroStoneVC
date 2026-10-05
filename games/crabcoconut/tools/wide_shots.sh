#!/bin/sh
# Crab Coconut: the widescreen screenshots (DESIGN.md "Widescreen") at 3x, like the 320 ones (tests/check_game.py):
# NAME-427.png (16:9) and NAME-520.png (phones): the title, a run, a game over, four players.
#   wide_shots.sh <headless binary> <out dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/crabcoconut/LICENSE.
H=$1
O=${2:-games/crabcoconut/docs/screenshots}
T=${TMPDIR:-/tmp}/cc_wide.$$
mkdir -p "$O" "$T"
for W in 427 520; do
    run() { $H --width $W --scale 3 --opt music=0 "$@" > "$T/log.txt" 2>&1; }
    run --frames 130 --shot 120:"$O/title-$W.png"
    run --frames 700 --opt bot=1 --opt seed=7 --opt ready=1 --shot 690:"$O/play-$W.png"
    run --frames 6000 --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=21
    f=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$T/log.txt")
    run --frames $((f + 60)) --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=21 --shot $((f + 50)):"$O/gameover-$W.png"
done
W=520
run --frames 900 --opt bot=4 --opt seed=9 --opt ready=1 --shot 880:"$O/play-4-players-520.png"
rm -rf "$T"
ls "$O"/*-427.png "$O"/*-520.png

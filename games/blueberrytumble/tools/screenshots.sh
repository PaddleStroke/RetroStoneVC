#!/bin/sh
# Blueberry Tumble: the screenshots of games/blueberrytumble/docs/screenshots (make blueberrytumble-screenshots): the
# title, each biome and the night, a mushroom pad, a dew drop, the leaf glider, the snowberry, a splat, a biome gate,
# game over with a medal, pause, get ready and a race with 2 players (tests/ui_test.sh makes and checks them); then
# the title, a run, the game over and a 4-player race at 427 (16:9) and 520 (19.5:9) columns (DESIGN.md "Widescreen").
#   screenshots.sh <headless binary> <out dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/blueberrytumble/LICENSE.
H=$1
O=${2:-games/blueberrytumble/docs/screenshots}
sh "$(dirname "$0")/../tests/ui_test.sh" "$H" "$O" 2 || exit 1
T=$(mktemp -d)
for w in 427 520; do
    run() { $H --width $w --scale 2 --opt music=0 --opt sound=0 "$@" > "$T/log.txt" 2>&1; }
    run --frames 130 --shot 120:"$O/title-$w.png"
    run --frames 3801 --opt bot=1 --opt ready=1 --opt seed=3 --opt god=1 --shot 3800:"$O/play-$w.png"
    run --frames 9000 --opt bot=1 --opt ready=1 --opt seed=11
    f=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$T/log.txt")
    run --frames $((f + 60)) --opt bot=1 --opt ready=1 --opt seed=11 --shot $((f + 50)):"$O/gameover-$w.png"
    run --frames 791 --opt bot=4 --opt seed=9 --opt players=4 --opt ready=1 --shot 790:"$O/race-4-players-$w.png"
done
rm -rf "$T"
ls "$O"

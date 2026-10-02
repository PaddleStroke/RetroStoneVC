#!/bin/sh
# Beaver Rush: headless screenshots for games/beaverrush/docs/screenshots (2x, like the 640x480 LCD).
#   screenshots.sh <headless binary> <out dir>
# The bot plays (--opt bot=1); --opt skip=N starts with N logs gnawed (the scene of N logs: 150 = night,
# 410 = winter).
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/beaverrush/LICENSE.
H=$1
O=${2:-games/beaverrush/docs/screenshots}
T=${TMPDIR:-/tmp}/br_shots.$$
mkdir -p "$O" "$T"
run() { $H --scale 2 --opt music=0 "$@" > "$T/log.txt" 2>&1; }
first() { pat=$1; shift; $H --opt music=0 --frames 20000 "$@" 2>&1 | sed -n "s/.*$pat at frame \([0-9]*\).*/\1/p" | head -1; }
run --frames 130 --shot 120:"$O/title.png"
run --frames 300 --opt bot=1 --opt seed=7 --opt ready=1 --shot 290:"$O/early-play.png"
m=$(first "milestone 1" --opt bot=1 --opt seed=7 --opt ready=1)
run --frames $((m + 20)) --opt bot=1 --opt seed=7 --opt ready=1 --shot $((m + 16)):"$O/milestone.png"
g=$(first "golden log" --opt bot=1 --opt seed=7 --opt ready=1)
run --frames $((g + 6)) --opt bot=1 --opt seed=7 --opt ready=1 --shot $((g + 4)):"$O/golden-log.png"
run --frames 250 --opt bot=1 --opt seed=7 --opt ready=1 --opt skip=150 --shot 240:"$O/night.png"
run --frames 250 --opt bot=1 --opt seed=3 --opt ready=1 --opt skip=410 --shot 240:"$O/winter.png"
run --frames 250 --opt bot=1 --opt seed=4 --opt ready=1 --opt skip=310 --shot 240:"$O/autumn-sunset.png"
run --frames 250 --opt bot=1 --opt seed=8 --opt ready=1 --opt skip=620 --shot 240:"$O/spring.png"
e=$(first "run 1 over" --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=104)
run --frames $((e + 60)) --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=104 --shot $((e + 50)):"$O/gameover-silver.png"
printf "600 tap START\n" > "$T/pause.input"
run --frames 620 --opt bot=1 --opt seed=7 --opt ready=1 --input "$T/pause.input" --shot 610:"$O/pause.png"
run --frames 130 --opt players=2 --shot 120:"$O/title-2-players.png"
run --frames 900 --opt bot=2 --opt players=2 --opt seed=9 --shot 890:"$O/versus.png"
v=$(first "run 1 over" --opt bot=2 --opt players=2 --opt seed=9)
run --frames $((v + 60)) --opt bot=2 --opt players=2 --opt seed=9 --shot $((v + 50)):"$O/versus-over.png"
run --frames 130 --opt players=4 --shot 120:"$O/title-4-players.png"
run --frames 1500 --opt bot=4 --opt players=4 --opt ready=1 --opt seed=9 --shot 800:"$O/play-4-players.png"
run --frames 4000 --opt bot=4 --opt players=4 --opt ready=1 --opt seed=5 --opt botstop=3 --shot 3900:"$O/results-4-players.png"
rm -rf "$T"
ls "$O"

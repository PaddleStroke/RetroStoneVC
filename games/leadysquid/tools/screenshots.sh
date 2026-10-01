#!/bin/sh
# Leady Squid: headless screenshots for games/leadysquid/docs/screenshots (2x, like the 640x480 LCD).
#   screenshots.sh <headless binary> <out dir>
# The bot (--opt bot=1) plays; --opt skip=N starts at obstacle N (10 coral, 20 masts, 30 chains).
H=$1
O=${2:-games/leadysquid/docs/screenshots}
T=${TMPDIR:-/tmp}/ls_shots.$$
mkdir -p "$O" "$T"
run() { $H --scale 2 --opt music=0 "$@" > "$T/log.txt" 2>&1; }
run --frames 130 --shot 120:$O/title.png
# mid-flap: 2 frames after a flap of the bot (the squeeze and the ink puff)
run --frames 700 --opt bot=1 --opt seed=7 --opt botlog=1
f=$(grep -o 'bot f[0-9]*:.*-> 1$' "$T/log.txt" | awk -F'[f:]' '$2 > 520 {print $2; exit}')
run --frames $((f + 4)) --opt bot=1 --opt seed=7 --shot $((f + 2)):$O/mid-flap.png
run --frames 900 --opt bot=1 --opt seed=11 --shot 880:$O/dense-kelp.png
for th in 0:kelp:21 10:coral:22 20:masts:23 30:chains:24; do
    n=${th%%:*}; rest=${th#*:}
    run --frames 700 --opt bot=1 --opt seed=${rest#*:} --opt skip=$n --shot 690:$O/theme-${rest%%:*}.png
done
# game over with a gold shell: start at 30, the bot stops flapping at 33
run --frames 1500 --opt bot=1 --opt seed=5 --opt skip=30 --opt botstop=33
f=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$T/log.txt")
h=$(sed -n 's/.*player 1 hit at frame \([0-9]*\).*/\1/p' "$T/log.txt")
run --frames $((f + 60)) --opt bot=1 --opt seed=5 --opt skip=30 --opt botstop=33 --shot $((h + 20)):$O/sinking.png \
    --shot $((f + 50)):$O/gameover-gold-shell.png
run --frames 1500 --opt bot=1 --opt seed=5 --opt skip=40 --opt botstop=41
f=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$T/log.txt")
run --frames $((f + 60)) --opt bot=1 --opt seed=5 --opt skip=40 --opt botstop=41 --shot $((f + 50)):$O/gameover-pearl-shell.png
printf "600 tap SELECT\n" > "$T/pause.input"
run --frames 640 --opt bot=1 --opt seed=7 --input "$T/pause.input" --shot 630:$O/pause.png
# two and four players (the bot plays them all); the title with players joined; the 4-player results
run --frames 820 --opt bot=2 --opt seed=9 --opt players=2 --shot 800:$O/race-2-players.png
run --frames 820 --opt bot=4 --opt seed=9 --shot 800:$O/race-4-players.png
printf "20 P2 tap A\n30 P3 tap A\n112 P4 tap A\n" > "$T/join.input"
run --frames 130 --input "$T/join.input" --shot 50:$O/title-2-players.png --shot 120:$O/title-4-players.png
run --frames 6000 --opt bot=4 --opt seed=5 --opt botstop=5
f=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$T/log.txt")
run --frames $((f + 60)) --opt bot=4 --opt seed=5 --opt botstop=5 --shot $((f + 50)):$O/results-4-players.png
rm -rf "$T"
ls "$O"

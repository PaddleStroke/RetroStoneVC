#!/bin/sh
# Pogo Mamie: headless screenshots for games/pogomamie/docs/screenshots (2x, like the 640x480 LCD).
#   screenshots.sh <headless binary> <out dir>
# The bot plays (--opt bot=1); --opt skip=M starts M metres along (512 the Seine, 1024 Haussmann, 1536 the Eiffel
# Tower, 2048 the night); --opt evlog=1 logs the moments to catch (big bounces, pigeons, falls).
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pogomamie/LICENSE.
H=$1
O=${2:-games/pogomamie/docs/screenshots}
T=${TMPDIR:-/tmp}/pm_shots.$$
mkdir -p "$O" "$T"
run() { $H --scale 2 --opt music=0 --opt evlog=1 "$@" > "$T/log.txt" 2>&1; }
first() {   # first <event> <after frame>: the frame of the first such event after that frame
    sed -n "s/.*ev f=\([0-9]*\) p=0.* $1 .*/\1/p" "$T/log.txt" | awk -v a="$2" '$1 > a {print; exit}'
}
run --frames 200 --shot 190:$O/title.png
run --frames 130 --opt ready=1 --opt players=2 --shot 120:$O/get-ready-2-players.png
# the districts
run --frames 900 --opt bot=1 --opt ready=1 --opt seed=7 --shot 880:$O/district-montmartre.png
run --frames 900 --opt bot=1 --opt ready=1 --opt seed=12 --opt skip=526 --shot 880:$O/district-seine.png
run --frames 900 --opt bot=1 --opt ready=1 --opt seed=13 --opt skip=1030 --shot 880:$O/district-haussmann.png
run --frames 900 --opt bot=1 --opt ready=1 --opt seed=14 --opt skip=1500 --shot 880:$O/district-eiffel.png
run --frames 900 --opt bot=1 --opt ready=1 --opt seed=15 --opt skip=2080 --shot 880:$O/night.png
# a big bounce: 22 frames after the take-off, high over a gap
run --frames 3000 --opt bot=1 --opt ready=1 --opt seed=7
f=$(first big 200)
[ -n "$f" ] && run --frames $((f + 24)) --opt bot=1 --opt ready=1 --opt seed=7 --shot $((f + 22)):$O/big-bounce.png
# a pigeon bounce: the stunt points pop up
for s in 9 10 11 12 13 14 15 16 17 18; do
    run --frames 6000 --opt bot=1 --opt ready=1 --opt seed=$s --opt skip=900
    f=$(first pigeon 0)
    if [ -n "$f" ]; then
        run --frames $((f + 8)) --opt bot=1 --opt ready=1 --opt seed=$s --opt skip=900 --shot $((f + 6)):$O/pigeon-bounce.png
        break
    fi
done
# the umbrella: a slow glide
run --frames 200 --opt bot=1 --opt ready=1 --opt seed=7 --opt give=1 --shot 150:$O/umbrella-glide.png
# the fall (the bot lets go at 40 m): flailing with the whistle, then the café awning and the cursing
run --frames 4000 --opt bot=1 --opt ready=1 --opt seed=31 --opt botstop=40
d=$(first doomed 0)
w=$(first down 0)
if [ -n "$d" ] && [ -n "$w" ]; then
    run --frames $((w + 60)) --opt bot=1 --opt ready=1 --opt seed=31 --opt botstop=40 --shot $((d + 30)):$O/fall.png \
        --shot $((w + 40)):$O/fall-cafe-cursing.png
fi
# game over with a medal (the bot lets go at 520 m: silver whiskers)
run --frames 20000 --opt bot=1 --opt ready=1 --opt seed=8 --opt botstop=520
f=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$T/log.txt")
[ -n "$f" ] && run --frames $((f + 70)) --opt bot=1 --opt ready=1 --opt seed=8 --opt botstop=520 --shot $((f + 60)):$O/gameover-medal.png
printf "30 tap A\n80 tap A\n600 tap START\n" > "$T/pause.input"
run --frames 640 --opt seed=7 --input "$T/pause.input" --shot 630:$O/pause.png
# the 2-player race (both bots)
run --frames 1400 --opt bot=2 --opt seed=9 --opt players=2 --opt ready=1 --shot 1200:$O/race-2-players.png
rm -rf "$T"
ls "$O"

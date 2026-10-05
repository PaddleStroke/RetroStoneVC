#!/bin/sh
# Nutty Branch: the UI screens (the house kit) and the scenes as screenshots, checked: each screen renders, the
# screens differ, and the same run gives the same picture twice. Also makes games/nuttybranch/docs/screenshots
# (make nuttybranch-screenshots).
#   ui_test.sh <headless binary> <out dir> [scale]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE.
H=$1
O=${2:-/tmp/nuttybranch-ui}
S=${3:-1}
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
run() { $H --scale "$S" --opt music=0 "$@" > "$O/log.txt" 2>&1; }

# the title with 0, 1, 2 and 3 players joined (the last one mid-pop, with its sparkles)
run --frames 130 --shot 120:"$O/title.png"
printf "20 P2 tap A\n" > "$O/j1.input"
run --frames 130 --input "$O/j1.input" --shot 120:"$O/title-2-players.png"
printf "20 P2 tap A\n30 P3 tap A\n" > "$O/j2.input"
run --frames 130 --input "$O/j2.input" --shot 120:"$O/title-3-players.png"
printf "20 P2 tap A\n30 P3 tap A\n112 P4 tap A\n" > "$O/j3.input"
run --frames 130 --input "$O/j3.input" --shot 120:"$O/title-4-players.png"
# play: the bot from the screen, the times of day and seasons (every 400 m), 2 and 4 squirrels
run --frames 700 --opt bot=1 --opt seed=7 --opt ready=1 --shot 690:"$O/play.png"
printf "600 tap SELECT\n" > "$O/pause.input"
run --frames 640 --opt bot=1 --opt seed=7 --opt ready=1 --input "$O/pause.input" --shot 630:"$O/pause.png"
run --frames 700 --opt bot=1 --opt seed=3 --opt ready=1 --opt start=420 --shot 600:"$O/summer.png"
run --frames 700 --opt bot=1 --opt seed=12 --opt ready=1 --opt start=820 --shot 600:"$O/autumn.png"
run --frames 700 --opt bot=1 --opt seed=9 --opt ready=1 --opt start=1220 --shot 600:"$O/dusk.png"
run --frames 700 --opt bot=1 --opt seed=5 --opt ready=1 --opt start=1620 --shot 600:"$O/night.png"
run --frames 700 --opt bot=1 --opt seed=4 --opt ready=1 --opt start=2020 --shot 600:"$O/winter.png"
# the traps, one kind at a time (--opt only=KIND)
scene() { run --frames $(($3 + 5)) --opt bot=1 --opt seed=3 --opt ready=1 --opt only=$1 --opt start=$2 --shot $3:"$O/trap-$1.png"; }
scene gap 1000 138
scene fork 1000 150
scene pecker 1000 150
scene web 1000 150
scene cater 1000 270
scene sap 1000 150
scene knot 1000 270
scene cone 1000 270
scene wind 1000 150
scene owl 1000 250
scene twig 1000 126
scene snake 1000 150
scene swing 1000 270
scene bees 1000 130
scene snow 2050 270
scene crow 1000 280
scene shower 1000 150
scene rain 1000 150
scene hole 1000 136
run --frames 275 --opt bot=1 --opt seed=3 --opt ready=1 --opt only=twig --opt start=1000 --shot 270:"$O/trap-twig-right.png"
# the four forests behind (BG_STYLE in src/tuning.h, --opt bg=a..d), the same moment of the same run
for b in a b c d; do
    run --frames 310 --opt bot=1 --opt seed=5 --opt ready=1 --opt start=1000 --opt bg=$b --shot 300:"$O/bg-$b.png"
done
# the scenery: side branches, leaves overhead, fungi (an ordinary stretch)
run --frames 400 --opt bot=1 --opt seed=5 --opt ready=1 --shot 385:"$O/scenery.png"
run --frames 900 --opt bot=2 --opt seed=9 --opt ready=1 --shot 880:"$O/race-2-players.png"
run --frames 900 --opt bot=4 --opt seed=9 --opt ready=1 --shot 880:"$O/play-4-players.png"
# game over with a medal: the bot lets go of the controls at 260 m (bronze)
run --frames 9000 --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=260
f=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$O/log.txt")
if [ -n "$f" ]; then
    run --frames $((f + 60)) --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=260 --shot $((f + 50)):"$O/gameover-medal.png"
else
    ko "the bot run for the game-over screen did not end"
fi
# the 4-player results: the bots let go at 40, 60, 80 and 100 m (they fall one by one)
run --frames 9000 --opt bot=4 --opt seed=5 --opt ready=1 --opt botstop=40
f=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$O/log.txt")
if [ -n "$f" ]; then
    run --frames $((f + 60)) --opt bot=4 --opt seed=5 --opt ready=1 --opt botstop=40 --shot $((f + 50)):"$O/results-4-players.png"
else
    ko "the 4-player bot run for the results did not end"
fi
shots="title title-2-players title-3-players title-4-players play pause summer autumn dusk night winter race-2-players play-4-players gameover-medal results-4-players trap-gap trap-fork trap-pecker trap-web trap-cater trap-sap trap-knot trap-cone trap-wind trap-owl trap-twig trap-snake trap-swing trap-bees trap-snow trap-crow trap-shower trap-rain trap-hole trap-twig-right scenery bg-a bg-b bg-c bg-d"
n=0
for s in $shots; do
    n=$((n + 1))
    if [ -s "$O/$s.png" ]; then ok "screen: $s"; else ko "screen $s missing"; fi
done
d=$(for s in $shots; do md5sum "$O/$s.png" 2>/dev/null; done | awk '{print $1}' | sort -u | wc -l)
if [ "$d" -ge "$n" ]; then ok "the $n screens all differ"; else ko "only $d different screens of $n"; fi
cp "$O/title.png" "$O/title.1"
run --frames 130 --shot 120:"$O/title.png"
if cmp -s "$O/title.png" "$O/title.1"; then ok "the title renders the same twice"; else ko "the title differs between two runs"; fi
rm -f "$O/title.1" "$O/log.txt" "$O"/*.input
exit $fail

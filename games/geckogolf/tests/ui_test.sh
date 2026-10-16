#!/bin/sh
# Gecko Golf: the UI screens (the house kit) as screenshots, checked: each screen renders, the screens differ, and the
# same run gives the same picture twice. Also makes games/geckogolf/docs/screenshots (make geckogolf-screenshots:
# scale 2, and the title, play, 4 players and the game over at 427 and 520 columns).
#   ui_test.sh <headless binary> <out dir> [scale [all]]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/geckogolf/LICENSE.
H=$1
O=${2:-/tmp/geckogolf-ui}
S=${3:-1}
ALL=$4
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
run() { $H --scale "$S" --opt music=0 "$@" > "$O/log.txt" 2>&1; }
frame_of() { sed -n "s/.*$1 at frame \([0-9]*\).*/\1/p" "$O/log.txt" | head -n "${2:-1}" | tail -n 1; }

# the title alone, with 2 and 4 players (the last one mid-pop, with its sparkles)
run --frames 130 --shot 120:"$O/title.png"
printf "20 P2 tap A\n" > "$O/j1.input"
run --frames 130 --input "$O/j1.input" --shot 120:"$O/title-2-players.png"
printf "20 P2 tap A\n30 P3 tap A\n112 P4 tap A\n" > "$O/j3.input"
run --frames 130 --input "$O/j3.input" --shot 120:"$O/title-4-players.png"
# aiming (the hint on the first hole), charging (the backswing, the gauge, the first part of the arc), a shot in flight
printf "30 tap A\n40 UP\n52 -\n" > "$O/aim.input"
run --frames 70 --opt seed=7 --input "$O/aim.input" --shot 60:"$O/aiming.png"
printf "30 tap A\n40 UP\n52 -\n70 A\n120 -\n" > "$O/shot.input"
run --frames 160 --opt seed=7 --input "$O/shot.input" --shot 112:"$O/charging.png" --shot 150:"$O/flight.png"
printf "300 tap SELECT\n" > "$O/pause.input"
run --frames 320 --opt bot=1 --opt seed=7 --opt ready=1 --input "$O/pause.input" --shot 310:"$O/pause.png"
# the biomes (a later hole: the course gets trickier), and a pan from one biome to the next
run --frames 60 --opt bot=1 --opt seed=7 --opt ready=1 --opt hole=8 --shot 50:"$O/biome-dunes.png"
run --frames 60 --opt bot=1 --opt seed=7 --opt ready=1 --opt hole=14 --shot 50:"$O/biome-snow.png"
run --frames 60 --opt bot=1 --opt seed=7 --opt ready=1 --opt hole=20 --shot 50:"$O/biome-night.png"
run --frames 60 --opt bot=1 --opt seed=7 --opt ready=1 --opt hole=26 --shot 50:"$O/biome-candy.png"
run --frames 3000 --opt bot=1 --opt seed=7 --opt ready=1 --opt hole=5
f=$(frame_of "hole 7")
if [ -n "$f" ]; then
    run --frames $((f + 50)) --opt bot=1 --opt seed=7 --opt ready=1 --opt hole=5 --shot $((f + 40)):"$O/new-biome.png"
else
    ko "the bot did not reach the next biome"
fi
# 2 and 4 players taking turns on the same hole
run --frames 760 --opt bot=2 --opt seed=9 --opt ready=1 --shot 750:"$O/play-2-players.png"
run --frames 1210 --opt bot=4 --opt seed=9 --opt ready=1 --shot 1200:"$O/play-4-players.png"
# game over with a medal: the bot plays 6 holes (bronze), then shoots out of bounds with what is left
run --frames 20000 --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=6 --opt strokes=2
f=$(frame_of "run 1 over")
if [ -n "$f" ]; then
    run --frames $((f + 60)) --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=6 --opt strokes=2 --shot $((f - 30)):"$O/out.png" \
        --shot $((f + 50)):"$O/gameover.png"
else
    ko "the bot run for the game-over screen did not end"
fi
# the 4-player results: the bots give up after 1, 3, 5 and 7 holes
run --frames 30000 --opt bot=4 --opt seed=5 --opt ready=1 --opt botstop=1 --opt strokes=2
f=$(frame_of "run 1 over")
if [ -n "$f" ]; then
    run --frames $((f + 60)) --opt bot=4 --opt seed=5 --opt ready=1 --opt botstop=1 --opt strokes=2 --shot $((f + 50)):"$O/results-4-players.png"
else
    ko "the 4-player bot run for the results did not end"
fi
shots="title title-2-players title-4-players aiming charging flight pause biome-dunes biome-snow biome-night biome-candy new-biome play-2-players play-4-players out gameover results-4-players"
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
# the screenshots: the title, play, 4 players and the game over on wide screens too
if [ -n "$ALL" ]; then
    for w in 427 520; do
        run --width $w --frames 130 --shot 120:"$O/title-$w.png"
        run --width $w --frames 160 --opt seed=7 --input "$O/shot.input" --shot 112:"$O/play-$w.png"
        run --width $w --frames 1210 --opt bot=4 --opt seed=9 --opt ready=1 --shot 1200:"$O/play-4-players-$w.png"
        run --width $w --frames 20000 --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=6 --opt strokes=2
        f=$(frame_of "run 1 over")
        [ -z "$f" ] || run --width $w --frames $((f + 60)) --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=6 \
            --opt strokes=2 --shot $((f + 50)):"$O/gameover-$w.png"
    done
fi
rm -f "$O/title.1" "$O/log.txt" "$O"/*.input
exit $fail

#!/bin/sh
# Gopher Dig: the screens (the house kit and the well) as screenshots, checked: each screen renders, the screens differ,
# and the same run gives the same picture twice. Also makes games/gopherdig/docs/screenshots (make gopherdig-screenshots:
# scale 2, and the title, play and the game over at 427 and 520 columns too).
#   ui_test.sh <headless binary> <out dir> [scale [all]]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/gopherdig/LICENSE.
H=$1
O=${2:-/tmp/gopherdig-ui}
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
# play: near the grass (the hint), a group falling, the first checkpoint, deeper in the red clay, paused
run --frames 400 --opt bot=1 --opt seed=7 --opt ready=1 --shot 300:"$O/play.png" --shot 181:"$O/falling.png"
run --frames 3000 --opt bot=1 --opt seed=7 --opt ready=1
c=$(frame_of "reached 100 m")
if [ -n "$c" ]; then
    run --frames $((c + 2000)) --opt bot=1 --opt seed=7 --opt ready=1 --shot $((c + 30)):"$O/checkpoint.png" \
        --shot $((c + 700)):"$O/play-deep.png"
else
    ko "the bot did not reach 100 m"
fi
printf "300 tap SELECT\n" > "$O/pause.input"
run --frames 320 --opt bot=1 --opt seed=7 --opt ready=1 --input "$O/pause.input" --shot 310:"$O/pause.png"
# split screens: 2 players side by side, 3 (with the race), 4 in quarters
run --frames 700 --opt bot=2 --opt seed=9 --opt ready=1 --shot 690:"$O/play-2-players.png"
run --frames 700 --opt bot=3 --opt seed=9 --opt ready=1 --shot 690:"$O/play-3-players.png"
run --frames 700 --opt bot=4 --opt seed=9 --opt ready=1 --shot 690:"$O/play-4-players.png"
# game over with a medal: the bot gives up from 110 m (bronze)
run --frames 30000 --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=110
f=$(frame_of "run 1 over")
if [ -n "$f" ]; then
    run --frames $((f + 60)) --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=110 --shot $((f + 50)):"$O/gameover.png"
else
    ko "the bot run for the game-over screen did not end"
fi
# the 4-player results: the bots give up from 10, 30, 50 and 70 m
run --frames 30000 --opt bot=4 --opt seed=5 --opt ready=1 --opt botstop=10
f=$(frame_of "run 1 over")
if [ -n "$f" ]; then
    run --frames $((f + 60)) --opt bot=4 --opt seed=5 --opt ready=1 --opt botstop=10 --shot $((f + 50)):"$O/results-4-players.png"
else
    ko "the 4-player bot run for the results did not end"
fi
shots="title title-2-players title-4-players play falling checkpoint play-deep pause play-2-players play-3-players play-4-players gameover results-4-players"
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
        run --width $w --frames 400 --opt bot=1 --opt seed=7 --opt ready=1 --shot 300:"$O/play-$w.png"
        run --width $w --frames 700 --opt bot=4 --opt seed=9 --opt ready=1 --shot 690:"$O/play-4-players-$w.png"
        run --width $w --frames 700 --opt bot=2 --opt seed=9 --opt ready=1 --shot 690:"$O/play-2-players-$w.png"
    done
fi
rm -f "$O/title.1" "$O/log.txt" "$O"/*.input
exit $fail

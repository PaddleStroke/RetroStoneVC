#!/bin/sh
# Kitty Kibble: the UI screens (the house kit) as screenshots, checked: each screen renders, the screens differ, and the
# same run gives the same picture twice. Also makes games/kittykibble/docs/screenshots (make kittykibble-screenshots:
# scale 2, and the title, play, 4 players and the game over at 427 and 520 columns, the other rooms).
#   ui_test.sh <headless binary> <out dir> [scale [all]]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/kittykibble/LICENSE.
H=$1
O=${2:-/tmp/kittykibble-ui}
S=${3:-1}
ALL=$4
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
run() { $H --scale "$S" --opt music=0 --opt sound=0 "$@" > "$O/log.txt" 2>&1; }
frame_of() { sed -n "s/.*$1 at frame \([0-9]*\).*/\1/p" "$O/log.txt" | head -n "${2:-1}" | tail -n 1; }

# the title with 0, 1, 2 and 3 players joined (the last one mid-pop, with its sparkles)
run --frames 130 --shot 120:"$O/title.png"
printf "20 P2 tap A\n" > "$O/j1.input"
run --frames 130 --input "$O/j1.input" --shot 120:"$O/title-2-players.png"
printf "20 P2 tap A\n30 P3 tap A\n112 P4 tap A\n" > "$O/j3.input"
run --frames 130 --input "$O/j3.input" --shot 120:"$O/title-4-players.png"
# play: the room's intro, the kitchen being eaten, paused; 4 kittens
run --frames 60 --opt bot=1 --opt seed=3 --opt ready=1 --shot 50:"$O/intro.png"
run --frames 900 --opt bot=1 --opt seed=3 --opt ready=1 --shot 890:"$O/play.png"
printf "800 tap SELECT\n" > "$O/pause.input"
run --frames 840 --opt bot=1 --opt seed=3 --opt ready=1 --input "$O/pause.input" --shot 830:"$O/pause.png"
run --frames 700 --opt bot=4 --opt botskill=2 --opt seed=9 --opt ready=1 --shot 690:"$O/play-4-players.png"
# yarn time: the vacuums scared, a little after the bot's first ball of yarn
run --frames 4000 --opt bot=1 --opt seed=3 --opt ready=1
f=$(frame_of "yarn")
if [ -n "$f" ]; then
    run --frames $((f + 50)) --opt bot=1 --opt seed=3 --opt ready=1 --shot $((f + 40)):"$O/yarn.png"
else
    ko "the bot ate no yarn"
fi
# game over with a medal: the bot runs into a vacuum from 350 points (bronze)
run --frames 8000 --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=350
f=$(frame_of "run 1 over")
if [ -n "$f" ]; then
    run --frames $((f + 60)) --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=350 --shot $((f + 50)):"$O/gameover.png"
else
    ko "the bot run for the game-over screen did not end"
fi
# the 4-player results: the bots run into the vacuums from 60, 110, 160 and 210 points
run --frames 8000 --opt bot=4 --opt seed=5 --opt ready=1 --opt botstop=60
f=$(frame_of "run 1 over")
if [ -n "$f" ]; then
    run --frames $((f + 60)) --opt bot=4 --opt seed=5 --opt ready=1 --opt botstop=60 --shot $((f + 50)):"$O/results-4-players.png"
else
    ko "the 4-player bot run for the results did not end"
fi
shots="title title-2-players title-4-players intro play pause play-4-players yarn gameover results-4-players"
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
# the screenshots: the other rooms; the title, play, 4 players and the game over on wide screens too
if [ -n "$ALL" ]; then
    for r in 2 3 4 5; do
        run --frames 700 --opt bot=1 --opt seed=3 --opt ready=1 --opt room=$r --shot 690:"$O/room-$r.png"
    done
    for w in 427 520; do
        run --width $w --frames 130 --shot 120:"$O/title-$w.png"
        run --width $w --frames 900 --opt bot=1 --opt seed=3 --opt ready=1 --shot 890:"$O/play-$w.png"
        run --width $w --frames 700 --opt bot=4 --opt botskill=2 --opt seed=9 --opt ready=1 --shot 690:"$O/play-4-players-$w.png"
        run --width $w --frames 700 --opt bot=1 --opt seed=3 --opt ready=1 --opt room=4 --shot 690:"$O/room-4-$w.png"
        run --width $w --frames 8000 --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=350
        f=$(frame_of "run 1 over")
        [ -z "$f" ] || run --width $w --frames $((f + 60)) --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=350 \
            --shot $((f + 50)):"$O/gameover-$w.png"
    done
fi
rm -f "$O/title.1" "$O/log.txt" "$O"/*.input
exit $fail

#!/bin/sh
# Caterpillar Crawl: the UI screens (the house kit) as screenshots, checked: each screen renders, the screens differ,
# and the same run gives the same picture twice. Also makes games/caterpillarcrawl/docs/screenshots (make
# caterpillarcrawl-screenshots: scale 2, and the title, play, the arena and the game over at 427 and 520 columns).
#   ui_test.sh <headless binary> <out dir> [scale [all]]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/caterpillarcrawl/LICENSE.
H=$1
O=${2:-/tmp/caterpillarcrawl-ui}
S=${3:-1}
ALL=$4
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
run() { $H --scale "$S" --opt music=0 "$@" > "$O/log.txt" 2>&1; }
frame_of() { sed -n "s/.*$1 at frame \([0-9]*\).*/\1/p" "$O/log.txt" | head -n "${2:-1}" | tail -n 1; }

# the title with 0, 1, 2 and 3 players joined (the last one mid-pop, with its sparkles)
run --frames 130 --shot 120:"$O/title.png"
printf "20 P2 tap A\n" > "$O/j1.input"
run --frames 130 --input "$O/j1.input" --shot 120:"$O/title-2-players.png"
printf "20 P2 tap A\n30 P3 tap A\n" > "$O/j2.input"
run --frames 130 --input "$O/j2.input" --shot 120:"$O/title-3-players.png"
printf "20 P2 tap A\n30 P3 tap A\n112 P4 tap A\n" > "$O/j3.input"
run --frames 130 --input "$O/j3.input" --shot 120:"$O/title-4-players.png"
# play: early (a short caterpillar, the first leaves), long (garden 3, stones and a long body); paused
run --frames 420 --opt bot=1 --opt seed=7 --opt ready=1 --shot 410:"$O/play.png"
run --frames 1800 --opt bot=1 --opt seed=7 --opt ready=1 --opt garden=3 --shot 1790:"$O/play-long.png"
printf "400 tap SELECT\n" > "$O/pause.input"
run --frames 440 --opt bot=1 --opt seed=7 --opt ready=1 --input "$O/pause.input" --shot 430:"$O/pause.png"
# the butterfly: the cocoon hatching after the 12th leaf, then GARDEN 2
run --frames 2400 --opt bot=1 --opt seed=7 --opt ready=1
f=$(frame_of "the cocoon")
if [ -n "$f" ]; then
    run --frames $((f + 230)) --opt bot=1 --opt seed=7 --opt ready=1 --shot $((f + 60)):"$O/cocoon.png" \
        --shot $((f + 120)):"$O/butterfly.png" --shot $((f + 220)):"$O/garden-2.png"
else
    ko "the bot did not reach the butterfly"
fi
# the arena: 4 caterpillars, ROUND 1, a round's winner
run --frames 120 --opt bot=4 --opt seed=9 --opt ready=1 --shot 60:"$O/round-4-players.png"
run --frames 700 --opt bot=4 --opt seed=9 --opt ready=1 --shot 690:"$O/play-4-players.png"
run --frames 3000 --opt bot=4 --opt seed=9 --opt ready=1
f=$(frame_of "round 1 won by [0-9]*")
[ -n "$f" ] && run --frames $((f + 40)) --opt bot=4 --opt seed=9 --opt ready=1 --shot $((f + 30)):"$O/round-won.png" \
    || ko "the first round did not end"
# game over with a medal: the bot crawls into a wall once it has 25 points (bronze)
run --frames 6000 --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=25
f=$(frame_of "run 1 over")
if [ -n "$f" ]; then
    run --frames $((f + 60)) --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=25 --shot $((f + 50)):"$O/gameover.png"
else
    ko "the bot run for the game-over screen did not end"
fi
# the 4-player results: a whole match of 4 bots
run --frames 20000 --opt bot=4 --opt seed=5 --opt ready=1
f=$(frame_of "run 1 over")
if [ -n "$f" ]; then
    run --frames $((f + 60)) --opt bot=4 --opt seed=5 --opt ready=1 --shot $((f + 50)):"$O/results-4-players.png"
else
    ko "the 4-player match for the results did not end"
fi
shots="title title-2-players title-3-players title-4-players play play-long pause cocoon butterfly garden-2
       round-4-players play-4-players round-won gameover results-4-players"
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
# the screenshots: the title, play, the arena and the game over on wide screens too
if [ -n "$ALL" ]; then
    for w in 427 520; do
        run --width $w --frames 130 --shot 120:"$O/title-$w.png"
        run --width $w --frames 1800 --opt bot=1 --opt seed=7 --opt ready=1 --opt garden=3 --shot 1790:"$O/play-$w.png"
        run --width $w --frames 700 --opt bot=4 --opt seed=9 --opt ready=1 --shot 690:"$O/play-4-players-$w.png"
        run --width $w --frames 6000 --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=25
        f=$(frame_of "run 1 over")
        [ -z "$f" ] || run --width $w --frames $((f + 60)) --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=25 \
            --shot $((f + 50)):"$O/gameover-$w.png"
    done
fi
rm -f "$O/title.1" "$O/log.txt" "$O"/*.input
exit $fail

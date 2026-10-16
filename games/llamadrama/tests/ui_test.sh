#!/bin/sh
# Llama Drama: the UI screens (the house kit) as screenshots, checked: each screen renders, the screens differ, and the
# same run gives the same picture twice. Also makes games/llamadrama/docs/screenshots (make llamadrama-screenshots:
# scale 2, and the title, play, the busy sky, 4 players and the game over at 427 and 520 columns).
#   ui_test.sh <headless binary> <out dir> [scale [all]]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/llamadrama/LICENSE.
H=$1
O=${2:-/tmp/llamadrama-ui}
S=${3:-1}
ALL=$4
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
run() { $H --scale "$S" --opt music=0 "$@" > "$O/log.txt" 2>&1; }
frame_of() { sed -n "s/.*$1 at frame \([0-9]*\).*/\1/p" "$O/log.txt" | head -n "${2:-1}" | tail -n 1; }

# the title with 0, 1 and 3 players joined (the last one mid-pop, with its sparkles): the herd gathers
run --frames 130 --shot 120:"$O/title.png"
printf "20 P2 tap A\n" > "$O/j1.input"
run --frames 130 --input "$O/j1.input" --shot 120:"$O/title-2-players.png"
printf "20 P2 tap A\n30 P3 tap A\n112 P4 tap A\n" > "$O/j3.input"
run --frames 130 --input "$O/j3.input" --shot 120:"$O/title-4-players.png"
# play: early (the first hornets, the aim cue), busy (a human-like bot as the night falls), the night, paused, 4 llamas
run --frames 700 --opt bot=1 --opt seed=7 --opt ready=1 --shot 690:"$O/play.png"
run --frames 700 --opt bot=1 --opt seed=2 --opt ready=1 --opt botskill=6 --opt score=600 --shot 699:"$O/play-busy.png"
run --frames 1400 --opt bot=1 --opt seed=5 --opt ready=1 --opt score=900 --shot 1390:"$O/play-night.png"
printf "600 tap SELECT\n" > "$O/pause.input"
run --frames 640 --opt bot=1 --opt seed=7 --opt ready=1 --input "$O/pause.input" --shot 630:"$O/pause.png"
run --frames 1500 --opt bot=4 --opt seed=9 --opt ready=1 --opt score=200 --shot 1490:"$O/play-4-players.png"
# game over with a medal: the bot stops playing at 160 points (bronze)
run --frames 9000 --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=160
f=$(frame_of "run 1 over")
if [ -n "$f" ]; then
    run --frames $((f + 60)) --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=160 --shot $((f + 50)):"$O/gameover.png"
else
    ko "the bot run for the game-over screen did not end"
fi
# the 4-player results: the bots stop at 60, 80, 100 and 120 points (they are stung one by one)
run --frames 9000 --opt bot=4 --opt seed=5 --opt ready=1 --opt botstop=60
f=$(frame_of "run 1 over")
if [ -n "$f" ]; then
    run --frames $((f + 60)) --opt bot=4 --opt seed=5 --opt ready=1 --opt botstop=60 --shot $((f + 50)):"$O/results-4-players.png"
else
    ko "the 4-player bot run for the results did not end"
fi
shots="title title-2-players title-4-players play play-busy play-night pause play-4-players gameover results-4-players"
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
# the screenshots: the title, play, the busy sky, 4 players and the game over on wide screens too
if [ -n "$ALL" ]; then
    for w in 427 520; do
        run --width $w --frames 130 --shot 120:"$O/title-$w.png"
        run --width $w --frames 700 --opt bot=1 --opt seed=7 --opt ready=1 --shot 690:"$O/play-$w.png"
        run --width $w --frames 700 --opt bot=1 --opt seed=2 --opt ready=1 --opt botskill=6 --opt score=600 \
            --shot 699:"$O/play-busy-$w.png"
        run --width $w --frames 1500 --opt bot=4 --opt seed=9 --opt ready=1 --opt score=200 \
            --shot 1490:"$O/play-4-players-$w.png"
        run --width $w --frames 9000 --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=160
        f=$(frame_of "run 1 over")
        [ -z "$f" ] || run --width $w --frames $((f + 60)) --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=160 \
            --shot $((f + 50)):"$O/gameover-$w.png"
    done
fi
rm -f "$O/title.1" "$O/log.txt" "$O"/*.input
exit $fail

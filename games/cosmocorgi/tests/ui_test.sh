#!/bin/sh
# Cosmo Corgi: the UI screens (the house kit) as screenshots, checked: each screen renders, the screens differ, and the
# same run gives the same picture twice. Also makes games/cosmocorgi/docs/screenshots (make cosmocorgi-screenshots:
# scale 2, and the title, play, the walls, 4 players, the boss and the game over at 427 and 520 columns).
#   ui_test.sh <headless binary> <out dir> [scale [all]]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/cosmocorgi/LICENSE.
H=$1
O=${2:-/tmp/cosmocorgi-ui}
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
# play: the pastel nebula (a puppy pod, a gold group); the asteroid belt's walls; paused; 4 corgis; the owl boss
run --frames 1410 --opt bot=1 --opt seed=7 --opt ready=1 --shot 1400:"$O/play.png"
run --frames 1200 --opt bot=1 --opt seed=7 --opt ready=1 --opt section=1 --shot 1190:"$O/play-walls.png"
printf "1100 tap SELECT\n" > "$O/pause.input"
run --frames 1140 --opt bot=1 --opt seed=7 --opt ready=1 --input "$O/pause.input" --shot 1130:"$O/pause.png"
run --frames 1700 --opt bot=4 --opt seed=9 --opt ready=1 --opt power=1 --shot 1690:"$O/play-4-players.png"
run --frames 400 --opt bot=1 --opt seed=5 --opt ready=1 --opt section=2 --opt boss=1 --shot 390:"$O/boss.png"
# game over with a medal: the bot stops dodging from 2100 points (bronze)
run --frames 8000 --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=2100
f=$(frame_of "run 1 over")
if [ -n "$f" ]; then
    run --frames $((f + 60)) --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=2100 --shot $((f + 50)):"$O/gameover.png"
else
    ko "the bot run for the game-over screen did not end"
fi
# the 4-player results: the bots stop dodging from 300, 400, 500 and 600 points (they are out one by one)
run --frames 8000 --opt bot=4 --opt seed=5 --opt ready=1 --opt botstop=300
f=$(frame_of "run 1 over")
if [ -n "$f" ]; then
    run --frames $((f + 60)) --opt bot=4 --opt seed=5 --opt ready=1 --opt botstop=300 --shot $((f + 50)):"$O/results-4-players.png"
else
    ko "the 4-player bot run for the results did not end"
fi
shots="title title-2-players title-3-players title-4-players play play-walls pause play-4-players boss gameover results-4-players"
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
# the screenshots: the title, play, the walls, 4 players, the boss and the game over on wide screens too
if [ -n "$ALL" ]; then
    for w in 427 520; do
        run --width $w --frames 130 --shot 120:"$O/title-$w.png"
        run --width $w --frames 1410 --opt bot=1 --opt seed=7 --opt ready=1 --shot 1400:"$O/play-$w.png"
        run --width $w --frames 1200 --opt bot=1 --opt seed=7 --opt ready=1 --opt section=1 --shot 1190:"$O/play-walls-$w.png"
        run --width $w --frames 1700 --opt bot=4 --opt seed=9 --opt ready=1 --opt power=1 --shot 1690:"$O/play-4-players-$w.png"
        run --width $w --frames 400 --opt bot=1 --opt seed=5 --opt ready=1 --opt section=2 --opt boss=1 \
            --shot 390:"$O/boss-$w.png"
        run --width $w --frames 8000 --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=2100
        f=$(frame_of "run 1 over")
        [ -z "$f" ] || run --width $w --frames $((f + 60)) --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=2100 \
            --shot $((f + 50)):"$O/gameover-$w.png"
    done
fi
rm -f "$O/title.1" "$O/log.txt" "$O"/*.input
exit $fail

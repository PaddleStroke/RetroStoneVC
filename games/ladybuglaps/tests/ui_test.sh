#!/bin/sh
# Ladybug Laps: the screens as screenshots, checked: each screen renders, the screens differ, and the same run gives the
# same picture twice. Also makes games/ladybuglaps/docs/screenshots (make ladybuglaps-screenshots: scale 2, and the
# title, a race, 4 players and the game over at 427 and 520 columns).
#   ui_test.sh <headless binary> <out dir> [scale [all]]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/ladybuglaps/LICENSE.
H=$1
O=${2:-/tmp/ladybuglaps-ui}
S=${3:-1}
ALL=$4
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
run() { $H --scale "$S" --opt music=0 "$@" > "$O/log.txt" 2>&1; }
frame_of() { sed -n "s/.*$1 at frame \([0-9]*\).*/\1/p" "$O/log.txt" | head -n 1; }
R="--opt bot=1 --opt seed=7 --opt ready=1"

# the title alone, with 2 and 4 players (the last one mid-pop, with its sparkles)
run --frames 130 --shot 120:"$O/title.png"
printf "20 P2 tap A\n" > "$O/j1.input"
run --frames 130 --input "$O/j1.input" --shot 120:"$O/title-2-players.png"
printf "20 P2 tap A\n30 P3 tap A\n112 P4 tap A\n" > "$O/j3.input"
run --frames 130 --input "$O/j3.input" --shot 120:"$O/title-4-players.png"
# a race: the lights (the track's name), racing, a tight corner (the chicane of Tea Time), paused
run --frames 70 $R --shot 60:"$O/race-start.png"
run --frames 610 $R --shot 600:"$O/play.png"
run --frames 700 --opt bot=1 --opt seed=3 --opt ready=1 --shot 690:"$O/corner.png"
printf "600 tap SELECT\n" > "$O/pause.input"
run --frames 620 $R --input "$O/pause.input" --shot 610:"$O/pause.png"
# the other tracks
run --frames 910 $R --opt track=1 --shot 900:"$O/track-desk.png"
run --frames 910 $R --opt track=2 --shot 900:"$O/track-picnic.png"
run --frames 1010 $R --opt track=3 --shot 1000:"$O/track-veg.png"
run --frames 910 $R --opt track=4 --shot 900:"$O/track-patio.png"
# 4 players on one screen (the camera on the leader)
run --frames 1210 --opt bot=4 --opt seed=9 --opt ready=1 --shot 1200:"$O/play-4-players.png"
# the results of the first race, then the game over (the player bot does not drive: 4th, out)
run --frames 14000 $R --opt botout=1
f=$(frame_of "run 1 over")
r=$(frame_of "race 1 done")
if [ -n "$f" ] && [ -n "$r" ]; then
    run --frames $((f + 60)) $R --opt botout=1 --shot $((r + 40)):"$O/results.png" --shot $((f + 50)):"$O/gameover.png"
else
    ko "the run for the results and the game over did not end"
fi
# the 4-player results: the last race of the cup
run --frames 9000 --opt bot=4 --opt seed=9 --opt ready=1 --opt race=2
f=$(frame_of "run 1 over")
if [ -n "$f" ]; then
    run --frames $((f + 60)) --opt bot=4 --opt seed=9 --opt ready=1 --opt race=2 --shot $((f + 50)):"$O/results-4-players.png"
else
    ko "the 4-player cup did not end"
fi
shots="title title-2-players title-4-players race-start play corner pause track-desk track-picnic track-veg track-patio play-4-players results gameover results-4-players"
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
# the screenshots: the title, a race, 4 players and the game over on wide screens too
if [ -n "$ALL" ]; then
    for w in 427 520; do
        run --width $w --frames 130 --shot 120:"$O/title-$w.png"
        run --width $w --frames 1150 $R --shot 1140:"$O/play-$w.png"
        run --width $w --frames 1210 --opt bot=4 --opt seed=9 --opt ready=1 --shot 1200:"$O/play-4-players-$w.png"
        run --width $w --frames 14000 $R --opt botout=1
        f=$(frame_of "run 1 over")
        [ -z "$f" ] || run --width $w --frames $((f + 60)) $R --opt botout=1 --shot $((f + 50)):"$O/gameover-$w.png"
    done
fi
rm -f "$O/title.1" "$O/log.txt" "$O"/*.input
exit $fail

#!/bin/sh
# Seal Shove: the UI screens (the house kit) as screenshots, checked: each screen renders, the screens differ, and the
# same run gives the same picture twice. Also makes games/sealshove/docs/screenshots (make sealshove-screenshots:
# scale 2, and the title and play at 427 and 520 columns).
#   ui_test.sh <headless binary> <out dir> [scale [all]]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/sealshove/LICENSE.
H=$1
O=${2:-/tmp/sealshove-ui}
S=${3:-1}
ALL=$4
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
run() { $H --scale "$S" --opt music=0 "$@" > "$O/log.txt" 2>&1; }
frame_of() { sed -n "s/.*$1 at frame \([0-9]*\).*/\1/p" "$O/log.txt" | head -n "${2:-1}" | tail -n 1; }

# the title alone and with 4 players (the last one mid-pop, with its sparkles)
run --frames 130 --shot 120:"$O/title.png"
printf "20 P2 tap A\n30 P3 tap A\n112 P4 tap A\n" > "$O/j3.input"
run --frames 130 --input "$O/j3.input" --shot 120:"$O/title-4-players.png"
# a round starts (ROUND 1 over the floe, the hint), the fight, a floe shrunk by the cracks, paused
run --frames 60 --opt bot=1 --opt seed=7 --opt ready=1 --shot 50:"$O/round-start.png"
run --frames 400 --opt bot=1 --opt seed=7 --opt ready=1 --shot 330:"$O/play.png"
B="--opt bot=4 --opt seed=21 --opt ready=1"
run --frames 5000 $B
f=$(frame_of "seal [0-9] splashes" 3)
if [ -n "$f" ]; then run --frames $((f + 1)) $B --shot $((f - 12)):"$O/floe-shrunk.png"; else ko "no third splash for the shrunk floe"; fi
# a clash: players 1 and 2 (left and right, facing each other) slide at the same time
printf "100 tap A\n100 P2 tap A\n" > "$O/clash.input"
C="--opt players=2 --opt seed=21 --opt ready=1 --input $O/clash.input"
run --frames 200 $C
f=$(frame_of "clash" 1)
if [ -n "$f" ]; then run --frames $((f + 23)) $C --shot $((f + 22)):"$O/dash-clash.png"; else ko "no clash"; fi
run --frames 900 --opt bot=4 --opt seed=9 --opt ready=1 --shot 700:"$O/play-4-players.png"
printf "300 tap SELECT\n" > "$O/pause.input"
run --frames 320 --opt bot=1 --opt seed=7 --opt ready=1 --input "$O/pause.input" --shot 310:"$O/pause.png"
# the round won (its panel)
run --frames 6000 --opt bot=1 --opt seed=7 --opt ready=1
f=$(frame_of "round 1 of match 1 won by [0-9]")
if [ -n "$f" ]; then run --frames $((f + 40)) --opt bot=1 --opt seed=7 --opt ready=1 --shot $((f + 30)):"$O/round-won.png"
else ko "no round won"; fi
# game over with a medal: the bot walks off from a score of 150 on (bronze)
run --frames 40000 --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=150
f=$(frame_of "run 1 over")
if [ -n "$f" ]; then
    run --frames $((f + 60)) --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=150 --shot $((f + 50)):"$O/gameover.png"
else
    ko "the bot run for the game-over screen did not end"
fi
# the 4-player results
run --frames 40000 --opt bot=4 --opt seed=5 --opt ready=1
f=$(frame_of "run 1 over")
if [ -n "$f" ]; then
    run --frames $((f + 60)) --opt bot=4 --opt seed=5 --opt ready=1 --shot $((f + 50)):"$O/results-4-players.png"
else
    ko "the 4-player bot run for the results did not end"
fi
shots="title title-4-players round-start play floe-shrunk dash-clash play-4-players pause round-won gameover results-4-players"
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
# the screenshots: the title and play on wide screens too
if [ -n "$ALL" ]; then
    for w in 427 520; do
        run --width $w --frames 130 --shot 120:"$O/title-$w.png"
        run --width $w --frames 700 --opt bot=4 --opt seed=9 --opt ready=1 --shot 600:"$O/play-$w.png"
    done
fi
rm -f "$O/title.1" "$O/log.txt" "$O"/*.input
exit $fail

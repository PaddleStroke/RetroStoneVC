#!/bin/sh
# Snowball Siege: the screens as screenshots, checked: each renders, they differ, and the same run gives the same
# picture twice. Also makes games/snowballsiege/docs/screenshots (make snowballsiege-screenshots: "all" adds the
# widescreen ones).
#   ui_test.sh <headless binary> <out dir> [scale] [all]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/snowballsiege/LICENSE.
H=$1
O=${2:-/tmp/snowballsiege-ui}
S=${3:-1}
ALL=$4
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
run() { $H --scale "$S" --opt music=0 "$@" > "$O/log.txt" 2>&1; }
B="--opt bot=1 --opt seed=5 --opt ready=1"
# the frame of the n-th line of the log matching a pattern (the log of the last run)
frame_of() { grep "$1" "$O/log.txt" | sed -n "${2:-1}s/.*frame \([0-9]*\).*/\1/p"; }

# the title, alone and with 4 players (the last one mid-pop, with its sparkles)
run --frames 130 --shot 120:"$O/title.png"
printf "20 P2 tap A\n30 P3 tap A\n112 P4 tap A\n" > "$O/j3.input"
run --frames 130 --input "$O/j3.input" --shot 120:"$O/title-4-players.png"
# match 1 played by the bot: its first throw (aiming: the gauge charging), the snowball in flight, the crater it
# carves in the other side's fort
run --frames 1500 $B
t=$(frame_of "side 0 throws" 1)
s=$(frame_of "splat at" 1)
if [ -n "$t" ] && [ -n "$s" ]; then
    run --frames $((s + 40)) $B --shot $((t - 8)):"$O/aiming.png" --shot $((t + 50)):"$O/flight.png" \
        --shot $((s + 34)):"$O/carved-fort.png"
else
    ko "the bot did not throw"
fi
run --frames 700 $B --shot 690:"$O/play.png"
printf "600 tap SELECT\n" > "$O/pause.input"
run --frames 640 $B --input "$O/pause.input" --shot 630:"$O/pause.png"
# 4 sides (4 bots), and the results of the match
run --frames 16000 --opt bot=4 --opt seed=9 --opt ready=1
f=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$O/log.txt")
run --frames 900 --opt bot=4 --opt seed=9 --opt ready=1 --shot 880:"$O/play-4-sides.png"
if [ -n "$f" ]; then
    run --frames $((f + 60)) --opt bot=4 --opt seed=9 --opt ready=1 --shot $((f + 50)):"$O/results-4-players.png"
else
    ko "the 4-player match did not end"
fi
# 1 player: the game over (the bot wins match 1 and gives up in match 2)
run --frames 9000 $B --opt botstop=1
f=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$O/log.txt")
if [ -n "$f" ]; then
    run --frames $((f + 60)) $B --opt botstop=1 --shot $((f + 50)):"$O/gameover.png"
else
    ko "the bot run for the game-over screen did not end"
fi
shots="title title-4-players aiming flight carved-fort play pause play-4-sides results-4-players gameover"
if [ -n "$ALL" ]; then              # the docs: the same field on wider screens
    for w in 427 520; do
        run --width $w --frames 700 $B --shot 690:"$O/play-$w.png"
        run --width $w --frames 130 --shot 120:"$O/title-$w.png"
        shots="$shots play-$w title-$w"
    done
fi
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

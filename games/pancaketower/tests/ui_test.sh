#!/bin/sh
# Pancake Tower: the UI screens (the house kit) as screenshots, checked: each screen renders, the screens differ, the
# same run gives the same picture twice, and the pictures show what they should (ui_check.py: the logo's colours on
# the title, the kit's panel and gold banner on the game over, the fork medal, the dimmed pause, the split screen).
#   ui_test.sh <headless binary> <out dir> [scale]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pancaketower/LICENSE.
H=$1
O=${2:-/tmp/pancaketower-ui}
S=${3:-1}
D=$(dirname "$0")
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
run() { $H --scale "$S" --opt music=0 "$@" > "$O/log.txt" 2>&1; }

run --frames 130 --shot 120:"$O/title.png"
run --frames 130 --opt ready=1 --opt players=2 --shot 120:"$O/get-ready-2-players.png"
run --frames 700 --opt bot=1 --opt seed=7 --opt ready=1 --shot 690:"$O/play.png"
printf "600 tap START\n" > "$O/pause.input"
run --frames 640 --opt bot=1 --opt seed=7 --opt ready=1 --input "$O/pause.input" --shot 630:"$O/pause.png"
run --frames 1400 --opt bot=2 --opt seed=9 --opt players=2 --opt ready=1 --shot 1380:"$O/versus-2-players.png"
# game over with a medal: the bot misses on purpose at 14 pancakes (a score of 25 or more with its perfects)
run --frames 6000 --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=14
f=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$O/log.txt")
if [ -n "$f" ]; then
    run --frames $((f + 60)) --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=14 --shot $((f + 50)):"$O/gameover-medal.png"
else
    ko "the bot run for the game-over screen did not end"
fi
for s in title get-ready-2-players play pause versus-2-players gameover-medal; do
    if [ -s "$O/$s.png" ]; then ok "screen: $s"; else ko "screen $s missing"; fi
done
n=$(md5sum "$O"/*.png 2>/dev/null | awk '{print $1}' | sort -u | wc -l)
if [ "$n" -ge 6 ]; then ok "the six screens all differ"; else ko "only $n different screens"; fi
cp "$O/title.png" "$O/title.1"
run --frames 130 --shot 120:"$O/title.png"
if cmp -s "$O/title.png" "$O/title.1"; then ok "the title renders the same twice"; else ko "the title differs between two runs"; fi
if python3 "$D/ui_check.py" "$O" "$S"; then :; else fail=1; fi
rm -f "$O/title.1" "$O/log.txt" "$O/pause.input"
exit $fail

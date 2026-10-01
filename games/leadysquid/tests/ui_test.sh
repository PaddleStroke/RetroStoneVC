#!/bin/sh
# Leady Squid: the UI screens with players (the house kit's title and results) as screenshots, checked: each screen
# renders, the screens differ, the same run gives the same picture twice.
#   ui_test.sh <headless binary> <out dir> [scale]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/leadysquid/LICENSE.
H=$1
O=${2:-/tmp/leadysquid-ui}
S=${3:-1}
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
run() { $H --scale "$S" --opt music=0 "$@" > "$O/log.txt" 2>&1; }

# the title with 0, 1, 2 and 3 players joined (the last one mid-pop)
run --frames 130 --shot 120:"$O/title.png"
printf "20 P2 tap A\n" > "$O/j1.input"
run --frames 130 --input "$O/j1.input" --shot 120:"$O/title-2-players.png"
printf "20 P2 tap A\n30 P3 tap A\n" > "$O/j2.input"
run --frames 130 --input "$O/j2.input" --shot 120:"$O/title-3-players.png"
printf "20 P2 tap A\n30 P3 tap A\n112 P4 tap A\n" > "$O/j3.input"
run --frames 130 --input "$O/j3.input" --shot 120:"$O/title-4-players.png"
run --frames 700 --opt bot=1 --opt seed=7 --opt ready=1 --shot 690:"$O/play.png"
run --frames 820 --opt bot=2 --opt seed=9 --opt players=2 --shot 800:"$O/race-2-players.png"
run --frames 820 --opt bot=4 --opt seed=9 --shot 800:"$O/race-4-players.png"
# the 4-player results: the bots stop flapping at 5, 7, 9 and 11 points
run --frames 6000 --opt bot=4 --opt seed=5 --opt botstop=5
f=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$O/log.txt")
if [ -n "$f" ]; then
    run --frames $((f + 60)) --opt bot=4 --opt seed=5 --opt botstop=5 --shot $((f + 50)):"$O/results-4-players.png"
else
    ko "the 4-player bot run for the results did not end"
fi
shots="title title-2-players title-3-players title-4-players play race-2-players race-4-players results-4-players"
n=0
for s in $shots; do
    n=$((n + 1))
    if [ -s "$O/$s.png" ]; then ok "screen: $s"; else ko "screen $s missing"; fi
done
d=$(for s in $shots; do md5sum "$O/$s.png" 2>/dev/null; done | awk '{print $1}' | sort -u | wc -l)
if [ "$d" -ge "$n" ]; then ok "the $n screens all differ"; else ko "only $d different screens of $n"; fi
cp "$O/title-4-players.png" "$O/title.1"
run --frames 130 --input "$O/j3.input" --shot 120:"$O/title-4-players.png"
if cmp -s "$O/title-4-players.png" "$O/title.1"; then ok "the 4-player title renders the same twice"; else ko "the 4-player title differs between two runs"; fi
rm -f "$O/title.1" "$O/log.txt" "$O"/*.input
exit $fail

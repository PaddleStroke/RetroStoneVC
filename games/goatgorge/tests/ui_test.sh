#!/bin/sh
# Goat Gorge: the screens as screenshots, checked: each renders, they differ, the same run gives the same picture twice.
# Also makes games/goatgorge/docs/screenshots (make goatgorge-screenshots: with "all", every chapter, the tall screen
# and the wide ones too).
#   ui_test.sh <headless binary> <out dir> [scale] [all]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/goatgorge/LICENSE.
H=$1
O=${2:-/tmp/goatgorge-ui}
S=${3:-1}
A=$4
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
run() { $H --scale "$S" --opt music=0 "$@" > "$O/log.txt" 2>&1; }

run --frames 130 --shot 125:"$O/title.png"
run --frames 130 --opt unlock=1 --opt chapter=3 --shot 125:"$O/title-chapter3.png"
run --frames 260 --opt bot=1 --opt ready=1 --opt room=4 --shot 250:"$O/play.png"
run --frames 200 --opt bot=1 --opt ready=1 --opt room=6 --shot 50:"$O/dash.png"
printf "150 tap START\n" > "$O/pause.input"
run --frames 170 --opt bot=1 --opt ready=1 --opt room=2 --input "$O/pause.input" --shot 165:"$O/pause.png"
run --frames 4000 --opt bot=1
f=$(sed -n 's/.*chapter 1 climbed at frame \([0-9]*\).*/\1/p' "$O/log.txt")
if [ -n "$f" ]; then
    run --frames $((f + 60)) --opt bot=1 --shot $((f + 50)):"$O/results.png"
else
    ko "the bot did not climb chapter 1 (no results screen)"
fi
shots="title title-chapter3 play dash pause results"
if [ -n "$A" ]; then
    for c in 1 2 3 4 5; do
        run --frames 400 --opt bot=1 --opt ready=1 --opt unlock=1 --opt chapter=$c --opt room=7 --shot 380:"$O/chapter$c.png"
        shots="$shots chapter$c"
    done
    run --frames 130 --display 1080x2340 --shot 125:"$O/title-tall.png"
    run --frames 260 --display 1080x2340 --opt bot=1 --opt ready=1 --opt room=4 --shot 250:"$O/play-tall.png"
    if [ -n "$f" ]; then run --frames $((f + 60)) --display 1080x2340 --opt bot=1 --shot $((f + 50)):"$O/results-tall.png"; fi
    run --frames 130 --width 427 --shot 125:"$O/title-427.png"
    run --frames 260 --width 520 --opt bot=1 --opt ready=1 --opt room=4 --shot 250:"$O/play-520.png"
    shots="$shots title-tall play-tall results-tall title-427 play-520"
fi
n=0
for s in $shots; do
    n=$((n + 1))
    if [ -s "$O/$s.png" ]; then ok "screen: $s"; else ko "screen $s missing"; fi
done
d=$(for s in $shots; do md5sum "$O/$s.png" 2>/dev/null; done | awk '{print $1}' | sort -u | wc -l)
if [ "$d" -ge "$n" ]; then ok "the $n screens all differ"; else ko "only $d different screens of $n"; fi
cp "$O/title.png" "$O/title.1"
run --frames 130 --shot 125:"$O/title.png"
if cmp -s "$O/title.png" "$O/title.1"; then ok "the title renders the same twice"; else ko "the title differs between two runs"; fi
rm -f "$O/title.1" "$O/log.txt" "$O"/*.input
exit $fail

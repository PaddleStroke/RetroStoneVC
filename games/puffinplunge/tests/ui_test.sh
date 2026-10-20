#!/bin/sh
# Puffin Plunge: the screens as screenshots, checked: each renders, they differ, the same run gives the same picture
# twice. Also makes games/puffinplunge/docs/screenshots (make puffinplunge-screenshots: with "all", the tall screen and
# the wide ones too).
#   ui_test.sh <headless binary> <out dir> [scale] [all]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/puffinplunge/LICENSE.
H=$1
O=${2:-/tmp/puffinplunge-ui}
S=${3:-1}
A=$4
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
run() { $H --scale "$S" --opt music=0 "$@" > "$O/log.txt" 2>&1; }

run --frames 130 --shot 125:"$O/title.png"
run --frames 130 --opt fakerun=1 --opt best=1234 --shot 125:"$O/title-continue.png"
run --frames 60 --opt ready=1 --opt seed=3 --shot 50:"$O/card.png"
run --frames 700 --opt bot=1 --opt ready=1 --opt seed=7 --shot 300:"$O/play.png"
run --frames 900 --opt bot=1 --opt ready=1 --opt seed=7 --opt depth=400 --opt god=1 --shot 640:"$O/play-coral.png"
run --frames 900 --opt bot=1 --opt ready=1 --opt seed=7 --opt depth=700 --opt god=1 --shot 700:"$O/play-mushroom.png"
run --frames 900 --opt bot=1 --opt ready=1 --opt seed=7 --opt depth=1000 --opt god=1 --shot 700:"$O/play-frozen.png"
run --frames 40 --opt ready=1 --opt seed=7 --opt shop=1 --opt gems=60 --shot 35:"$O/shop.png"
run --frames 40 --opt ready=1 --opt seed=7 --opt room=2 --shot 35:"$O/gift.png"
printf "300 tap START\n" > "$O/pause.input"
run --frames 320 --opt bot=1 --opt ready=1 --opt seed=7 --input "$O/pause.input" --shot 315:"$O/pause.png"
run --frames 1100 --opt ready=1 --opt seed=7 --opt bot=1 --opt hurtat=900
f=$(sed -n 's/.*dive over at frame \([0-9]*\).*/\1/p' "$O/log.txt" | head -1)
if [ -n "$f" ]; then
    run --frames $((f + 60)) --opt ready=1 --opt seed=7 --opt bot=1 --opt hurtat=900 --shot $((f + 50)):"$O/results.png"
else
    ko "no game over (no results screen)"
fi
shots="title title-continue card play play-coral play-mushroom play-frozen shop gift pause results"
if [ -n "$A" ]; then
    run --frames 130 --display 1080x2340 --shot 125:"$O/title-tall.png"
    run --frames 700 --display 1080x2340 --opt bot=1 --opt ready=1 --opt seed=7 --shot 300:"$O/play-tall.png"
    run --frames 40 --display 1080x2340 --opt ready=1 --opt seed=7 --opt shop=1 --opt gems=60 --shot 35:"$O/shop-tall.png"
    [ -n "$f" ] && run --frames $((f + 60)) --display 1080x2340 --opt ready=1 --opt seed=7 --opt bot=1 --opt hurtat=900 --shot $((f + 50)):"$O/results-tall.png"
    run --frames 130 --width 427 --shot 125:"$O/title-427.png"
    run --frames 700 --width 520 --opt bot=1 --opt ready=1 --opt seed=7 --opt depth=400 --shot 300:"$O/play-520.png"
    run --frames 3000 --opt bot=1 --opt ready=1 --opt seed=7 --opt depth=1150 --opt god=1 --shot 600:"$O/boss.png"
    run --frames 200 --opt bot=1 --opt ready=1 --opt seed=7 --opt depth=290 --opt god=1 --shot 160:"$O/zone-change.png"
    run --frames 600 --opt bot=1 --opt ready=1 --opt seed=7 --opt depth=2700 --opt god=1 --shot 560:"$O/deep.png"
    run --frames 210 --opt bot=1 --opt ready=1 --opt seed=7 --opt bestdepth=40 --shot 200:"$O/best-marker.png"
    run --frames 210 --display 1080x2340 --opt bot=1 --opt ready=1 --opt seed=7 --opt bestdepth=60 --shot 200:"$O/best-marker-tall.png"
    run --frames 600 --display 1080x2340 --opt bot=1 --opt ready=1 --opt seed=7 --opt depth=2700 --opt god=1 --shot 560:"$O/deep-tall.png"
    shots="$shots title-tall play-tall shop-tall results-tall title-427 play-520 boss zone-change deep best-marker"
    shots="$shots best-marker-tall deep-tall"
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
rm -f "${O:?}/title.1" "${O:?}/log.txt" "${O:?}/pause.input"
exit $fail

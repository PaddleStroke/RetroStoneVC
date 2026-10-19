#!/bin/sh
# Prickle Patch: the screens as screenshots, checked: each renders, they differ, the same run gives the same picture
# twice. Also makes games/pricklepatch/docs/screenshots (make pricklepatch-screenshots: with "all", the tall screen and
# the wide ones too).
#   ui_test.sh <headless binary> <out dir> [scale] [all]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pricklepatch/LICENSE.
H=$1
O=${2:-/tmp/pricklepatch-ui}
S=${3:-1}
A=$4
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
run() { $H --scale "$S" --opt music=0 "$@" > "$O/log.txt" 2>&1; }

run --frames 130 --shot 125:"$O/title.png"
run --frames 130 --opt chr=1 --opt coins=120 --opt best=754 --shot 125:"$O/title-locked.png"
run --frames 130 --opt fakerun=1 --opt best=754 --opt coins=380 --shot 125:"$O/title-continue.png"
run --frames 60 --opt shed=1 --opt coins=420 --opt boosts=2 --shot 55:"$O/shed.png"
run --frames 3700 --opt bot=1 --opt ready=1 --opt seed=7 --shot 3690:"$O/play.png"
run --frames 900 --opt bot=1 --opt ready=1 --opt seed=7 --opt time=600 --opt god=1 --shot 890:"$O/play-late.png"
run --frames 40 --opt ready=1 --opt seed=7 --opt time=240 --opt levelup=1 --shot 35:"$O/levelup.png"
run --frames 40 --opt ready=1 --opt seed=7 --opt time=480 --opt chest=1 --shot 35:"$O/chest.png"
printf "600 tap START\n" > "$O/pause.input"
run --frames 620 --opt bot=1 --opt ready=1 --opt seed=7 --input "$O/pause.input" --shot 615:"$O/pause.png"
run --frames 2400 --opt ready=1 --opt frail=1 --opt seed=7
f=$(sed -n 's/.*run over at frame \([0-9]*\).*/\1/p' "$O/log.txt" | head -1)
if [ -n "$f" ]; then
    run --frames $((f + 60)) --opt ready=1 --opt frail=1 --opt seed=7 --shot $((f + 50)):"$O/results.png"
else
    ko "no game over (no results screen)"
fi
shots="title title-locked title-continue shed play play-late levelup chest pause results"
if [ -n "$A" ]; then
    run --frames 130 --display 1080x2340 --shot 125:"$O/title-tall.png"
    run --frames 900 --display 1080x2340 --opt bot=1 --opt ready=1 --opt seed=7 --opt time=600 --opt god=1 --shot 890:"$O/play-tall.png"
    run --frames 40 --display 1080x2340 --opt ready=1 --opt seed=7 --opt time=240 --opt levelup=1 --shot 35:"$O/levelup-tall.png"
    run --frames 60 --display 1080x2340 --opt shed=1 --opt coins=420 --opt boosts=2 --shot 55:"$O/shed-tall.png"
    [ -n "$f" ] && run --frames $((f + 60)) --display 1080x2340 --opt ready=1 --opt frail=1 --opt seed=7 --shot $((f + 50)):"$O/results-tall.png"
    run --frames 130 --width 427 --shot 125:"$O/title-427.png"
    run --frames 900 --width 520 --opt bot=1 --opt ready=1 --opt seed=7 --opt time=600 --opt god=1 --shot 890:"$O/play-520.png"
    run --frames 1700 --opt bot=1 --opt ready=1 --opt seed=7 --opt time=880 --opt god=1 --shot 1600:"$O/boss.png"
    shots="$shots title-tall play-tall levelup-tall shed-tall results-tall title-427 play-520 boss"
    # the Crow King chased off: the garden saved, every pest left pops
    run --frames 3000 --opt bot=1 --opt ready=1 --opt seed=2 --opt time=899 --opt god=1
    k=$(sed -n 's/.*the garden is saved at .* (frame \([0-9]*\)).*/\1/p' "$O/log.txt" | head -1)
    if [ -n "$k" ]; then
        run --frames $((k + 56)) --opt bot=1 --opt ready=1 --opt seed=2 --opt time=899 --opt god=1 --shot $((k + 50)):"$O/saved.png"
        shots="$shots saved"
    fi
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

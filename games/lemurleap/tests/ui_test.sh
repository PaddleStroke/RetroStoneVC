#!/bin/sh
# Lemur Leap: the screens as screenshots, checked: each renders, they differ, the same run gives the same picture twice.
# Also makes games/lemurleap/docs/screenshots (make lemurleap-screenshots: with "all", the tall screen and the wide ones
# too).
#   ui_test.sh <headless binary> <out dir> [scale] [all]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/lemurleap/LICENSE.
H=$1
O=${2:-/tmp/lemurleap-ui}
S=${3:-1}
A=$4
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
run() { $H --scale "$S" --opt music=0 "$@" > "$O/log.txt" 2>&1; }

run --frames 130 --shot 125:"$O/title.png"
run --frames 130 --opt look=5 --opt coins=420 --opt best=640 --shot 125:"$O/title-wardrobe.png"
run --frames 130 --opt fakerun=1 --opt best=640 --opt coins=380 --opt unlock=1 --opt look=9 --shot 125:"$O/title-continue.png"
run --frames 900 --opt bot=1 --opt ready=1 --opt seed=7 --shot 890:"$O/play.png"
run --frames 600 --opt bot=1 --opt ready=1 --opt seed=3 --opt height=330 --opt god=1 --opt best=420 --shot 590:"$O/play-clouds.png"
run --frames 600 --opt bot=1 --opt ready=1 --opt seed=5 --opt height=1200 --opt god=1 --shot 590:"$O/play-space.png"
printf "600 tap START\n" > "$O/pause.input"
run --frames 620 --opt bot=1 --opt ready=1 --opt seed=7 --input "$O/pause.input" --shot 615:"$O/pause.png"
run --frames 260 --opt ready=1 --opt fall=1 --opt height=260 --opt best=240 --shot 250:"$O/results.png"
shots="title title-wardrobe title-continue play play-clouds play-space pause results"
if [ -n "$A" ]; then
    run --frames 130 --display 1080x2340 --visible --shot 125:"$O/title-tall.png"
    run --frames 900 --display 1080x2340 --visible --opt bot=1 --opt ready=1 --opt seed=7 --shot 890:"$O/play-tall.png"
    run --frames 600 --display 1080x2340 --visible --opt bot=1 --opt ready=1 --opt seed=5 --opt height=1200 --opt god=1 --shot 590:"$O/play-space-tall.png"
    run --frames 260 --display 1080x2340 --visible --opt ready=1 --opt fall=1 --opt height=260 --opt best=240 --shot 250:"$O/results-tall.png"
    run --frames 130 --width 427 --shot 125:"$O/title-427.png"
    run --frames 600 --width 520 --opt bot=1 --opt ready=1 --opt seed=3 --opt height=620 --opt god=1 --shot 590:"$O/play-sky-520.png"
    shots="$shots title-tall play-tall play-space-tall results-tall title-427 play-sky-520"
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

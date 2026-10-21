#!/bin/sh
# Prickle Patch on a tall screen (a phone held upright; DESIGN.md "Tall screen": rs_screen_zoom 440) and the phone
# turning. The run is the same at every size: the bot's runs log the same states frame for frame at 320x240, on three
# phones upright and at 520 columns (the pests spawn on a ring of the world, not at the screen's edge). The picture is
# 320 x 440 upright. And the phone turning (the app restarts the game from a save state at the new size): after an
# upright -> landscape turn the picture is exactly the one a landscape run shows at that frame, and back, on the title,
# in a run, a level-up, paused, the results.
# And the garden after the title, at nine sizes: no stray tile over the lawn (the soft layer's empty tile kept clear).
#   tall_test.sh <pricklepatch_headless> <out dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pricklepatch/LICENSE.
H=$1
O=${2:-/tmp}/pricklepatch-tall
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
png_wh() { od -An -tu1 -j16 -N8 "$1" | awk '{print $1 * 16777216 + $2 * 65536 + $3 * 256 + $4 "x" $5 * 16777216 + $6 * 65536 + $7 * 256 + $8}'; }
game_log() { grep -v "RetroStone VC\|vram\|tall screen:\|frame [0-9]* ->" | sed 's/ soft=[0-9]* overflow=[0-9]*//'; }

for d in 1080x2340:320x440 1080x2520:320x440 720x1440:320x440 2340x1080:520x240; do
    disp=${d%%:*} want=${d#*:}
    $H --display "$disp" --frames 130 --opt music=0 --png "$O/t$disp.png" > /dev/null 2>&1
    wh=$(png_wh "$O/t$disp.png")
    if [ "$wh" = "$want" ]; then ok "--display $disp: a $want picture"; else ko "--display $disp: the picture is '$wh' ($want expected)"; fi
done

same() {   # same <what> <headless options...>
    what=$1; shift
    $H --width 320 --opt music=0 --opt sound=0 --opt dump=1 "$@" 2>&1 | game_log > "$O/same-240.log"
    n=$(grep -c "" "$O/same-240.log")
    for d in 1080x2340 1080x2520 720x1440 2340x1080; do
        $H --display "$d" --opt music=0 --opt sound=0 --opt dump=1 "$@" 2>&1 | game_log > "$O/same-$d.log"
        if cmp -s "$O/same-240.log" "$O/same-$d.log"; then ok "$what: the same $n log lines at $d as at 320x240"
        else ko "$what at $d:"; diff "$O/same-240.log" "$O/same-$d.log" | head -4; fi
    done
}
same "a run, the bot" --frames 4000 --opt bot=1 --opt ready=1 --opt seed=7
same "a late garden" --frames 1200 --opt bot=1 --opt ready=1 --opt seed=7 --opt time=600 --opt god=1

# the phone turns: the picture after the turn = the picture of a run on that screen from the start
turn() {   # turn <what> <turn frame> <shot frame> <headless options...>
    what=$1 f=$2 g=$3; shift 3
    for p in "1080x2340 2340x1080" "2340x1080 1080x2340"; do
        a=${p% *} b=${p#* }
        $H --display "$a" --resize "$f:$b" --frames $((g + 1)) --opt music=0 --shot "$g:$O/turn-$a.png" "$@" > /dev/null 2>&1
        $H --display "$b" --frames $((g + 1)) --opt music=0 --shot "$g:$O/turn-ref-$b.png" "$@" > /dev/null 2>&1
        if cmp -s "$O/turn-$a.png" "$O/turn-ref-$b.png"; then ok "$what: turned $a -> $b at frame $f, the picture at $g is the $b one"
        else ko "$what: turned $a -> $b at frame $f, the picture at $g differs from a $b run's"; cp "$O/turn-$a.png" "$O/turn-$a-$f-bad.png"; fi
    done
}
turn "the title" 60 100
turn "a run (the bot)" 400 460 --opt bot=1 --opt ready=1 --opt seed=7
turn "a level-up" 20 40 --opt ready=1 --opt seed=7 --opt levelup=1
printf "300 tap START\n" > "$O/pause.input"
turn "paused" 340 380 --opt bot=1 --opt ready=1 --opt seed=7 --input "$O/pause.input"
f=$($H --frames 2400 --opt music=0 --opt ready=1 --opt frail=1 --opt seed=7 2>&1 | sed -n 's/.*run over at frame \([0-9]*\).*/\1/p')
[ -n "$f" ] && turn "the results" $((f + 20)) $((f + 60)) --opt ready=1 --opt frail=1 --opt seed=7

# the garden after the title, at every size (the phones' heights between their bars, landscape, the console): a run
# started from the title (A) looks like a run started directly, frame for frame (only the hedgehog's step may differ:
# under 1% of the pixels). The title's logo once wrote the soft layer's empty tile: a dash in every cell of the garden
# (about 30% of the pixels changed).
printf "30 tap A\n" > "$O/a.input"
for d in 320x240 2340x1080 1080x2200 1080x2214 1080x2268 1080x2340 1080x2520 1440x3040 720x1500; do
    $H --display "$d" --input "$O/a.input" --frames 101 --opt music=0 --opt seed=7 --shot "100:$O/garden-title-$d.png" > /dev/null 2>&1
    $H --display "$d" --opt ready=1 --frames 71 --opt music=0 --opt seed=7 --shot "70:$O/garden-ref-$d.png" > /dev/null 2>&1
    r=$(python3 -c '
import sys
from PIL import Image
a, b = (Image.open(p).convert("RGB") for p in sys.argv[1:3])
pa, pb = a.load(), b.load()
w, h = a.size
print("%d %d" % (sum(pa[x, y] != pb[x, y] for y in range(h) for x in range(w)) if a.size == b.size else w * h, w * h))
' "$O/garden-title-$d.png" "$O/garden-ref-$d.png" 2>&1)
    n=${r% *} all=${r#* }
    if [ -n "$n" ] && [ "$n" -eq "$n" ] 2>/dev/null && [ $((n * 100)) -lt "$all" ]; then
        ok "$d: the garden after the title as clean as a run's ($n of $all pixels differ)"
    else ko "$d: the garden after the title differs from a run's: '$r'"; fi
done
[ $fail = 0 ] && echo "tall_test: all passed" || { echo "tall_test: FAILED"; exit 1; }

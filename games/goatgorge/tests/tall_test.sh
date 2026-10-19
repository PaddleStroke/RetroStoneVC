#!/bin/sh
# Goat Gorge on a tall screen (a phone held upright; DESIGN.md "Tall screen": rs_screen_tall, every column) and the
# phone turning. The climb is the same at every height: the bot's runs log the same states frame for frame at 320x240
# and on three phones upright. The picture is as tall as the display asks. And the phone turning (the app restarts the
# game from a save state at the new size, rs_host_change_size; house_tall.h ht_shape_reset): after an upright ->
# landscape turn the picture is exactly the one a landscape run shows at that frame, and back, on the title, in a climb,
# paused, on the results.
#   tall_test.sh <goatgorge_headless> <out dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/goatgorge/LICENSE.
H=$1
O=${2:-/tmp}/goatgorge-tall
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
png_wh() { od -An -tu1 -j16 -N8 "$1" | awk '{print $1 * 16777216 + $2 * 65536 + $3 * 256 + $4 "x" $5 * 16777216 + $6 * 65536 + $7 * 256 + $8}'; }
game_log() { grep -v "RetroStone VC\|vram:\|tall screen:\|frame [0-9]* ->"; }

for d in 1080x2340:320x692 1080x2520:320x720 720x1440:320x640 2340x1080:520x240; do
    disp=${d%%:*} want=${d#*:}
    $H --display "$disp" --frames 130 --opt music=0 --png "$O/t$disp.png" > /dev/null 2>&1
    wh=$(png_wh "$O/t$disp.png")
    if [ "$wh" = "$want" ]; then ok "--display $disp: a $want picture"; else ko "--display $disp: the picture is '$wh' ($want expected)"; fi
done

same() {   # same <what> <headless options...>
    what=$1; shift
    $H --width 320 --opt music=0 --opt sound=0 --opt dump=1 "$@" 2>&1 | game_log > "$O/same-240.log"
    n=$(grep -c "" "$O/same-240.log")
    for d in 1080x2340 1080x2520 720x1440; do
        $H --display "$d" --opt music=0 --opt sound=0 --opt dump=1 "$@" 2>&1 | game_log > "$O/same-$d.log"
        if cmp -s "$O/same-240.log" "$O/same-$d.log"; then ok "$what: the same $n log lines at $d as at 320x240"
        else ko "$what at $d:"; diff "$O/same-240.log" "$O/same-$d.log" | head -4; fi
    done
}
same "chapter 1, the bot" --frames 2600 --opt bot=1
printf "0 RIGHT\n90 A\n140 LEFT\n" > "$O/die.input"
same "a death and the room again" --frames 300 --opt ready=1 --opt room=4 --input "$O/die.input"

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
turn "a climb (the bot)" 150 200 --opt bot=1 --opt ready=1
printf "100 tap START\n" > "$O/pause.input"
turn "paused" 150 200 --opt bot=1 --opt ready=1 --input "$O/pause.input"
f=$($H --frames 4000 --opt music=0 --opt bot=1 2>&1 | sed -n 's/.*chapter 1 climbed at frame \([0-9]*\).*/\1/p')
[ -n "$f" ] && turn "the results" $((f + 20)) $((f + 60)) --opt bot=1
[ $fail = 0 ] && echo "tall_test: all passed" || { echo "tall_test: FAILED"; exit 1; }

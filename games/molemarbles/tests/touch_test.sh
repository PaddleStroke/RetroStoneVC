#!/bin/sh
# Mole Marbles: touch (player 1; DESIGN.md "Touch"), scripted through the headless runner at each width: a tap starts
# the run, the mole walks at its normal speed to the finger's x and stays there, a held finger does not shoot, a quick
# tap shoots (and walks there), touches do nothing while paused, a tap on the game-over panel goes back to the title.
# Touch points are given in stage coordinates (@x): the same spot of the arena at every width.
#   touch_test.sh <headless binary> <tmp dir> [widths]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/molemarbles/LICENSE.
H=$1
T=${2:-/tmp}/molemarbles-touch
WIDTHS=${3:-320 427 520}
mkdir -p "$T"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
run() { $H --opt music=0 --opt dump=1 --opt seed=4 "$@" 2>&1 | grep "state:"; }
field() { echo "$2" | sed -n "s/.* $1=\([0-9]*\) .*/\1/p"; }
near() { [ "$1" -ge $(($2 - 3)) ] && [ "$1" -le $(($2 + 3)) ]; }

for w in $WIDTHS; do
    echo "== touch at $w x 240"
    printf "30 tap-at 50%% 60%%\n" > "$T/start.input"
    out=$(run --width $w --frames 40 --input "$T/start.input")
    [ "$(field st "$out")" = 1 ] && ok "a tap on the title starts the run" || ko "tap on the title: $out"

    # the finger held at x 250: the mole walks there (100 -> 250 at 2.7 px/frame: 56 frames) and stops
    printf "30 tap-at 50%% 60%%\n60 touch @250 150\n" > "$T/follow.input"
    out=$(run --width $w --frames 100 --input "$T/follow.input")
    x1=$(field x "$out")
    out=$(run --width $w --frames 140 --input "$T/follow.input")
    x2=$(field x "$out")
    if [ "${x1:-0}" -gt 150 ] && [ "${x1:-0}" -lt 248 ] && near "${x2:-0}" 250; then
        ok "the mole follows the finger at its normal speed (x $x1 at frame 100, $x2 at 140)"
    else ko "follow: x $x1 at frame 100, $x2 at frame 140 (expected about 208 and 250)"; fi
    [ "$(field shot "$out")" = 0 ] && ok "a held finger does not shoot" || ko "a held finger shot: $out"
    # it moves on with the finger, left
    printf "30 tap-at 50%% 60%%\n60 touch @250 150\n120 move @40 150\n" > "$T/slide.input"
    out=$(run --width $w --frames 150 --input "$T/slide.input")
    x3=$(field x "$out")
    [ "${x3:-0}" -gt 150 ] && [ "${x3:-0}" -lt 190 ] && ok "... and turns back when the finger slides left (x $x3 at frame 150)" || ko "slide: $out"
    # a quick tap at x 200: the harpoon fires at once (the start shot is over by then) and the mole walks there
    printf "30 tap-at 50%% 60%%\n150 tap-at @200 120\n" > "$T/tap.input"
    out=$(run --width $w --frames 156 --input "$T/tap.input")
    s=$(field shot "$out")
    out=$(run --width $w --frames 200 --input "$T/tap.input")
    if [ "${s:-0}" -gt 0 ] && near "$(field x "$out")" 200; then ok "a quick tap shoots (shot $s) and sets the target"
    else ko "tap: shot $s at frame 156, then $out"; fi
    # paused (Select / the app's pause button): a finger does nothing
    printf "30 tap-at 50%% 60%%\n50 tap SELECT\n60 touch @250 150\n100 up\n110 tap-at @280 120\n" > "$T/pause.input"
    a=$(run --width $w --frames 55 --input "$T/pause.input")
    out=$(run --width $w --frames 130 --input "$T/pause.input")
    if [ "$(field paused "$out")" = 1 ] && [ "$(field x "$out")" = "$(field x "$a")" ] && \
       [ "$(field shot "$out")" = "$(field shot "$a")" ]; then
        ok "paused: touches neither move nor shoot"
    else ko "paused: $a / $out"; fi
done

# the game over (no more input: a marble hits the mole), then a tap: the title
printf "30 tap-at 50%% 60%%\n" > "$T/over.input"
out=$($H --opt music=0 --opt seed=4 --frames 6000 --input "$T/over.input" 2>&1)
death=$(echo "$out" | sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p')
if [ -n "$death" ]; then
    printf "30 tap-at 50%% 60%%\n%s tap-at 50%% 50%%\n" $((death + 60)) > "$T/back.input"
    out=$(run --frames $((death + 70)) --input "$T/back.input")
    if [ "$(field st "$out")" = 0 ] && [ "$(field runs "$out")" = 1 ]; then ok "a tap on the game-over panel: the title"
    else ko "tap on the game over: $out"; fi
else ko "no game over to tap on"; fi
rm -rf "$T"
exit $fail

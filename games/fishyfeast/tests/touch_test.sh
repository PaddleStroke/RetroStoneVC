#!/bin/sh
# Fishy Feast touch controls (DESIGN.md "Controls", docs/spec.md "Touch"; rs_game.touch_native): headless runs with
# scripted fingers, checked on the final state's log line. A tap starts on the title (as the finger is lifted); in
# play a finger down on the left part of the screen is a stick: dragged, the fish swims that way, at full speed from
# STICK_R (28) px, slower for a shorter drag; a finger elsewhere does nothing; the round SPLIT and EJECT buttons at the
# bottom right; nothing while paused; a tap on the game-over panel goes back to the title (not a finger left down from
# the run, not a tap before the lock). The same run at every width.
#   touch_test.sh <fishyfeast_headless> <out dir> [widths]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/fishyfeast/LICENSE.
H=$1
O=${2:-/tmp}/fishyfeast-touch
WS=${3:-427 520}
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
# run <name> <frames> <script> [width] [options]: the final state's log line
run() {
    printf "$3" > "$O/$1.input"
    $H --width "${4:-320}" --frames "$2" --input "$O/$1.input" --opt seed=4 --opt strict=1 --opt dump=1 --opt music=0 \
        $5 2> "$O/$1.log" > /dev/null
    if grep -q "rs strict" "$O/$1.log"; then ko "$1: strict mode"; grep "rs strict" "$O/$1.log" | head -3; fi
    grep "state:" "$O/$1.log" | tail -1
}
field() { echo "$1" | sed -n "s/.* $2=\([^ ]*\).*/\1/p"; }

START="20 tap-at 50%% 30%%\n"         # a tap: the title starts as the finger is lifted (frame 23)
PAD="23 tap A\n"                      # the same start on the pad

# the title
s=$(run title 60 "")
[ "$(field "$s" st)" = 0 ] && ok "the title waits (no touch)" || ko "the title did not wait: $s"
a=$(run start 50 "$START")
b=$(run start-pad 50 "$PAD")
if [ "$(field "$a" st)" = 1 ] && [ "$(field "$a" hash)" = "$(field "$b" hash)" ]; then ok "a tap on the title starts the run (the very run of A)"
else ko "a tap on the title: $a / pad $b"; fi
s=$(run hold-title 60 "20 touch 50%% 30%%\n")
[ "$(field "$s" st)" = 0 ] && ok "a finger still down on the title does not start yet" || ko "finger down: $s"

# the stick: down at (60, 120), dragged STICK_R to the right: the very swim of the pad's Right
a=$(run stick 150 "${START}60 touch 60 120\n61 move 88 120\n120 up\n")
b=$(run stick-pad 150 "${PAD}61 RIGHT\n120 -\n")
if [ "$(field "$a" x)" = "$(field "$b" x)" ] && [ "$(field "$a" y)" = "$(field "$b" y)" ]; then
    ok "the stick dragged 28 px right: full speed, as the pad (x $(field "$a" x))"
else ko "stick: $a / pad $b"; fi
x0=$(field "$(run still 150 "$START")" x)
h=$(run half 150 "${START}60 touch 60 120\n61 move 74 120\n120 up\n")
hx=$(( $(field "$h" x) - x0 )); fx=$(( $(field "$a" x) - x0 ))
if [ "$hx" -gt 10 ] && [ "$hx" -lt $((fx * 3 / 4)) ]; then ok "half the drag: slower ($hx px against $fx)"; else ko "half drag: $hx vs $fx"; fi
s=$(run right 150 "${START}60 touch 250 60\n61 move 290 60\n120 up\n")
[ "$(field "$s" x)" = "$x0" ] && ok "a finger on the right part (not a button) does nothing" || ko "right part: $s"
# the buttons (a fish of mass 100): SPLIT at the bottom right, EJECT left of it
s=$(run split 100 "${START}60 tap-at 290 206\n" 320 "--opt mass=100")
[ "$(field "$s" cells)" = 2 ] && ok "SPLIT: two pieces" || ko "split: $s"
s=$(run eject 100 "${START}60 tap-at 242 216\n" 320 "--opt mass=100")
[ "$(field "$s" mass)" = 90 ] && ok "EJECT: a pellet (100 -> 90)" || ko "eject: $s"
s=$(run both 100 "${START}50 touch 60 120\n51 move 88 120\n60 T2 tap-at 290 206\n80 up\n" 320 "--opt mass=100")
[ "$(field "$s" cells)" = 2 ] && [ "$(field "$s" x)" != "$x0" ] && ok "the stick and a button at once (two fingers)" || ko "both: $s"

# pause: the fingers do nothing
a=$(run paused 200 "${START}50 tap SELECT\n60 touch 60 120\n61 move 88 120\n90 tap-at 290 206\n")
[ "$(field "$a" x)" = "$x0" ] && [ "$(field "$a" cells)" = 1 ] && ok "paused: the stick and the buttons do nothing" || ko "paused: $a"

# the game over (the bot gives up from mass 20): a tap goes back to the title
s=$(run over 12000 "" 320 "--opt bot=1 --opt botstop=20")
over=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$O/over.log")
if [ "$(field "$s" st)" = 3 ] && [ -n "$over" ]; then ok "a run ends (the panel at frame $over)"
else ko "no game over: $s"; over=3000; fi
B="--opt bot=1 --opt botstop=20"
s=$(run back $((over + 60)) "$((over + 40)) tap-at 50%% 50%%\n" 320 "$B")
[ "$(field "$s" st)" = 0 ] && ok "a tap on the game-over panel goes back to the title" || ko "back: $s"
s=$(run early $((over + 60)) "$((over + 10)) tap-at 50%% 50%%\n" 320 "$B")
[ "$(field "$s" st)" = 3 ] && ok "not a tap before the panel's lock" || ko "early tap: $s"
s=$(run held $((over + 60)) "$((over - 30)) touch 50%% 50%%\n$((over + 45)) up\n" 320 "$B")
[ "$(field "$s" st)" = 3 ] && ok "not a finger left down from the run" || ko "held finger: $s"

# every width: the same game (the stick's drag is the same movement; the buttons follow the screen's right edge)
D="${START}60 touch 60 120\n61 move 80 110\n140 move 50 140\n300 up\n"
ref=$(run w320 400 "$D" 320 "--opt mass=100")
for w in $WS 560; do
    s=$(run "w$w" 400 "$D" "$w" "--opt mass=100")
    [ "$(field "$s" hash)" = "$(field "$ref" hash)" ] && ok "width $w: the same drag, the same run as at 320" || ko "width $w: $s / $ref"
    s=$(run "s$w" 100 "${START}60 tap-at $((w - 30)) 206\n" "$w" "--opt mass=100")
    [ "$(field "$s" cells)" = 2 ] && ok "width $w: SPLIT at the bottom right" || ko "width $w split: $s"
done
exit $fail

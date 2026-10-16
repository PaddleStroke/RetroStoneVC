#!/bin/sh
# Seal Shove touch controls (DESIGN.md "Controls", docs/spec.md "Touch"; rs_game.touch_native): headless runs with
# scripted fingers, checked on the final state's log line. A tap starts on the title (as the finger is lifted); in
# play a finger down on the left part of the screen is a stick: dragged, the seal waddles that way, at full push from
# STICK_R (26) px, less for a shorter drag; a finger elsewhere does nothing; the round DASH button at the bottom right
# belly-slides; nothing while paused; a tap on the game-over panel goes back to the title (not a finger left down from
# the match, not a tap before the lock). The same run at every width.
#   touch_test.sh <sealshove_headless> <out dir> [widths]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/sealshove/LICENSE.
H=$1
O=${2:-/tmp}/sealshove-touch
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

START="20 tap-at 50%% 30%%\n"         # a tap: the title starts as the finger is lifted (frame 23); GO! at 113
PAD="23 tap A\n"                      # the same start on the pad

# the title
s=$(run title 60 "")
[ "$(field "$s" st)" = 0 ] && ok "the title waits (no touch)" || ko "the title did not wait: $s"
a=$(run start 50 "$START")
b=$(run start-pad 50 "$PAD")
if [ "$(field "$a" st)" = 1 ] && [ "$(field "$a" hash)" = "$(field "$b" hash)" ]; then ok "a tap on the title starts the match (the very match of A)"
else ko "a tap on the title: $a / pad $b"; fi
s=$(run hold-title 60 "20 touch 50%% 30%%\n")
[ "$(field "$s" st)" = 0 ] && ok "a finger still down on the title does not start yet" || ko "finger down: $s"

# the stick: down at (60, 120), dragged STICK_R to the right: the very waddle of the pad's Right
a=$(run stick 170 "${START}130 touch 60 120\n131 move 86 120\n160 up\n")
b=$(run stick-pad 170 "${PAD}131 RIGHT\n160 -\n")
if [ "$(field "$a" x)" = "$(field "$b" x)" ] && [ "$(field "$a" y)" = "$(field "$b" y)" ]; then
    ok "the stick dragged 26 px right: full push, as the pad (x $(field "$a" x))"
else ko "stick: $a / pad $b"; fi
x0=$(field "$(run still 170 "$START")" x)
h=$(run half 170 "${START}130 touch 60 120\n131 move 73 120\n160 up\n")
hx=$(( $(field "$h" x) - x0 )); fx=$(( $(field "$a" x) - x0 ))
if [ "$hx" -gt 3 ] && [ "$hx" -lt $((fx * 3 / 4)) ]; then ok "half the drag: slower ($hx px against $fx)"; else ko "half drag: $hx vs $fx"; fi
s=$(run right 170 "${START}130 touch 250 60\n131 move 290 60\n160 up\n")
[ "$(field "$s" x)" = "$x0" ] && ok "a finger on the right part (not the button) does nothing" || ko "right part: $s"
# the DASH button (bottom right): a belly slide; with the stick at once (two fingers)
s=$(run dash 150 "${START}130 tap-at 284 202\n")
[ "$(field "$s" dashcd)" -gt 0 ] 2>/dev/null && ok "DASH: a belly slide (then its cooldown)" || ko "dash: $s"
s=$(run both 150 "${START}125 touch 60 120\n126 move 86 120\n130 T2 tap-at 284 202\n145 up\n")
[ "$(field "$s" dashcd)" -gt 0 ] 2>/dev/null && [ "$(field "$s" x)" != "$x0" ] && ok "the stick and the button at once" || ko "both: $s"

# pause: the fingers do nothing
a=$(run paused 220 "${START}120 tap SELECT\n130 touch 60 120\n131 move 86 120\n150 tap-at 284 202\n")
p=$(run paused-still 220 "${START}120 tap SELECT\n")
[ "$(field "$a" hash)" = "$(field "$p" hash)" ] && [ "$(field "$a" dashcd)" = 0 ] && ok "paused: the stick and the button do nothing" || ko "paused: $a"

# the game over (player 1 walks off every round): a tap goes back to the title
s=$(run over 20000 "" 320 "--opt bot=1 --opt botquit=1")
over=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$O/over.log")
if [ "$(field "$s" st)" = 3 ] && [ -n "$over" ]; then ok "a match lost (the panel at frame $over)"
else ko "no game over: $s"; over=9000; fi
B="--opt bot=1 --opt botquit=1"
s=$(run back $((over + 60)) "$((over + 40)) tap-at 50%% 50%%\n" 320 "$B")
[ "$(field "$s" st)" = 0 ] && ok "a tap on the game-over panel goes back to the title" || ko "back: $s"
s=$(run early $((over + 60)) "$((over + 10)) tap-at 50%% 50%%\n" 320 "$B")
[ "$(field "$s" st)" = 3 ] && ok "not a tap before the panel's lock" || ko "early tap: $s"
s=$(run held $((over + 60)) "$((over - 30)) touch 50%% 50%%\n$((over + 45)) up\n" 320 "$B")
[ "$(field "$s" st)" = 3 ] && ok "not a finger left down from the match" || ko "held finger: $s"

# every width: the same game (the stick's drag is the same waddle; the button follows the screen's right edge)
D="${START}130 touch 60 120\n131 move 80 110\n200 move 50 140\n300 up\n"
ref=$(run w320 400 "$D" 320)
for w in $WS 560; do
    s=$(run "w$w" 400 "$D" "$w")
    [ "$(field "$s" hash)" = "$(field "$ref" hash)" ] && ok "width $w: the same drag, the same match as at 320" || ko "width $w: $s / $ref"
    s=$(run "s$w" 150 "${START}130 tap-at $((w - 36)) 202\n" "$w")
    [ "$(field "$s" dashcd)" -gt 0 ] 2>/dev/null && ok "width $w: DASH at the bottom right" || ko "width $w dash: $s"
done
exit $fail

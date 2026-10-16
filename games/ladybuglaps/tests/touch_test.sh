#!/bin/sh
# Ladybug Laps touch controls (DESIGN.md "Controls", docs/spec.md "Touch"; rs_game.touch_native): headless runs with
# scripted fingers, checked on the final state's log line. A tap starts on the title (as the finger is lifted); in a
# race a finger down on the left part of the screen is a stick: the ladybug turns toward the drag and accelerates while
# the finger is down (the very run of the pad's direction + A); a finger elsewhere does nothing; the round BRAKE button
# at the bottom right; nothing while paused; a tap goes on from the results and leaves the game-over panel (not a
# finger left down from the race, not a tap before the lock). The same run at every width.
#   touch_test.sh <ladybuglaps_headless> <out dir> [widths]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/ladybuglaps/LICENSE.
H=$1
O=${2:-/tmp}/ladybuglaps-touch
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

s=$(run title 60 "")
[ "$(field "$s" st)" = 0 ] && ok "the title waits (no touch)" || ko "the title did not wait: $s"
a=$(run start 50 "$START")
b=$(run start-pad 50 "$PAD")
if [ "$(field "$a" st)" = 1 ] && [ "$(field "$a" hash)" = "$(field "$b" hash)" ]; then ok "a tap on the title starts the race (the very race of A)"
else ko "a tap on the title: $a / pad $b"; fi
s=$(run hold-title 60 "20 touch 50%% 30%%\n")
[ "$(field "$s" st)" = 0 ] && ok "a finger still down on the title does not start yet" || ko "finger down: $s"

# the stick (after the lights, GO at frame 144): down at (60, 150), dragged right: the pad's Right + A
a=$(run stick 260 "${START}160 touch 60 150\n161 move 90 150\n240 up\n")
b=$(run stick-pad 260 "${PAD}160 A\n161 A+RIGHT\n240 -\n")     # the finger down at 160: the gas already
if [ "$(field "$a" x)" = "$(field "$b" x)" ] && [ "$(field "$a" hash)" = "$(field "$b" hash)" ]; then
    ok "the stick dragged right: it accelerates that way, as Right + A (x $(field "$a" x))"
else ko "stick: $a / pad $b"; fi
a=$(run stick-down 260 "${START}160 touch 60 150\n161 move 60 180\n240 up\n")
b=$(run stick-down-pad 260 "${PAD}160 A\n161 A+DOWN\n240 -\n")
[ "$(field "$a" hash)" = "$(field "$b" hash)" ] && ok "dragged down: as Down + A" || ko "stick down: $a / $b"
x0=$(field "$(run still 260 "$START")" x)
s=$(run tiny 260 "${START}160 touch 60 150\n161 move 62 150\n240 up\n")
[ "$(field "$s" x)" -gt "$x0" ] && [ "$(field "$s" ang)" = 0 ] && ok "a finger held still: straight on, accelerating" || ko "tiny drag: $s"
s=$(run right 260 "${START}160 touch 200 60\n161 move 240 60\n240 up\n")
[ "$(field "$s" x)" = "$x0" ] && ok "a finger on the right part (not the button) does nothing" || ko "right part: $s"
# BRAKE (bottom right): with the stick, it stops the ladybug
a=$(run brake 300 "${START}160 touch 60 150\n161 move 90 150\n230 T2 touch 286 206\n299 up\n")
b=$(run nobrake 300 "${START}160 touch 60 150\n161 move 90 150\n299 up\n")
[ "$(field "$a" x)" -lt "$(field "$b" x)" ] && ok "BRAKE with the stick down: it stops (x $(field "$a" x) < $(field "$b" x))" || ko "brake: $a / $b"
# pause: the fingers do nothing
a=$(run paused 300 "${START}150 tap SELECT\n160 touch 60 150\n161 move 90 150\n")
[ "$(field "$a" x)" = "$(field "$(run paused-still 300 "${START}150 tap SELECT\n")" x)" ] && ok "paused: the stick does nothing" || ko "paused: $a"

# the game over (the player bot does not drive): a tap goes on from the results, then back to the title
s=$(run over 14000 "" 320 "--opt bot=1 --opt botout=1")
over=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$O/over.log")
res=$(sed -n 's/.*race 1 done at frame \([0-9]*\).*/\1/p' "$O/over.log")
if [ "$(field "$s" st)" = 4 ] && [ -n "$over" ]; then ok "a championship ends (the panel at frame $over)"
else ko "no game over: $s"; over=6000; res=5800; fi
B="--opt bot=1 --opt botout=1"
s=$(run res-tap $((res + 80)) "$((res + 60)) tap-at 50%% 50%%\n" 320 "$B")
[ "$(field "$s" st)" = 4 ] && ok "a tap on the results goes on (out: the game-over panel)" || ko "results tap: $s"
s=$(run back $((over + 60)) "$((over + 40)) tap-at 50%% 50%%\n" 320 "$B")
[ "$(field "$s" st)" = 0 ] && ok "a tap on the game-over panel goes back to the title" || ko "back: $s"
s=$(run early $((over + 60)) "$((over + 10)) tap-at 50%% 50%%\n" 320 "$B")
[ "$(field "$s" st)" = 4 ] && ok "not a tap before the panel's lock" || ko "early tap: $s"
s=$(run held $((over + 60)) "$((over - 30)) touch 50%% 50%%\n$((over + 45)) up\n" 320 "$B")
[ "$(field "$s" st)" = 4 ] && ok "not a finger left down from before" || ko "held finger: $s"

# every width: the same game (the stick's drag is the same move; the button follows the screen's right edge)
D="${START}160 touch 60 150\n161 move 90 140\n230 move 60 180\n300 up\n"
ref=$(run w320 400 "$D" 320)
for w in $WS 560; do
    s=$(run "w$w" 400 "$D" "$w")
    [ "$(field "$s" hash)" = "$(field "$ref" hash)" ] && ok "width $w: the same drag, the same race as at 320" || ko "width $w: $s / $ref"
    a=$(run "b$w" 300 "${START}160 touch 60 150\n161 move 90 150\n230 T2 touch $((w - 34)) 206\n299 up\n" "$w")
    [ "$(field "$a" x)" -lt "$(field "$b" x)" ] && ok "width $w: BRAKE at the bottom right" || ko "width $w brake: $a"
done
exit $fail

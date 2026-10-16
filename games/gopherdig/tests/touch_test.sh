#!/bin/sh
# Gopher Dig touch controls (DESIGN.md "Controls", docs/spec.md "Touch"; rs_game.touch_native): headless runs with
# scripted fingers, checked on the final state's log line. A tap starts on the title (as the finger is lifted); in play
# a finger on the block under the gopher (or anywhere below it in its column) drills down, on the block beside it drills
# that side, a finger left or right of it elsewhere walks it to that column; nothing while paused; a tap on the
# game-over panel goes back to the title (not before the lock, not a finger left down from the run). The same run at
# every width (the fingers in stage coordinates: @x).
#   touch_test.sh <gopherdig_headless> <out dir> [widths]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/gopherdig/LICENSE.
H=$1
O=${2:-/tmp}/gopherdig-touch
WS=${3:-427 520}
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
run() {   # run <name> <frames> <script> [width] [options]: the final state's log line
    printf "$3" > "$O/$1.input"
    $H --width "${4:-320}" --frames "$2" --input "$O/$1.input" --opt seed=4 --opt strict=1 --opt dump=1 --opt music=0 \
        $5 2> "$O/$1.log" > /dev/null
    if grep -q "rs strict" "$O/$1.log"; then ko "$1: strict mode"; grep "rs strict" "$O/$1.log" | head -3; fi
    grep "state:" "$O/$1.log" | tail -1
}
field() { echo "$1" | sed -n "s/.* $2=\([^ ]*\).*/\1/p"; }

START="20 tap-at 50%% 30%%\n"       # a tap: the title starts as the finger is lifted
PAD="23 tap A\n"
# 1 player at 320: the well's left edge at stage x 88, a block is 16 px; once the camera has settled (frame 90) the
# gopher's cell (column 4, row -1) is at y 76: the block under it at (@160, 100)

s=$(run title 60 "")
[ "$(field "$s" st)" = 0 ] && ok "the title waits (no touch)" || ko "the title did not wait: $s"
a=$(run start 50 "$START")
b=$(run start-pad 50 "$PAD")
if [ "$(field "$a" st)" = 1 ] && [ "$(field "$a" hash)" = "$(field "$b" hash)" ]; then ok "a tap on the title starts the run (the very run of A)"
else ko "a tap on the title: $a / pad $b"; fi
s=$(run hold-title 60 "20 touch 50%% 30%%\n")
[ "$(field "$s" st)" = 0 ] && ok "a finger still down on the title does not start yet" || ko "finger down: $s"

s=$(run dig 150 "${START}90 tap-at @160 100\n")
[ "$(field "$s" pops)" -ge 1 ] 2>/dev/null && [ "$(field "$s" row)" -ge 0 ] && ok "a tap on the block under the gopher drills it (row $(field "$s" row))" || ko "dig: $s"
s=$(run deep 150 "${START}90 tap-at @160 200\n")
[ "$(field "$s" pops)" -ge 1 ] 2>/dev/null && ok "...anywhere below it in its column too" || ko "deep tap: $s"
s=$(run walk 200 "${START}90 tap-at @296 84\n")
[ "$(field "$s" col)" = 8 ] && ok "a tap right of the gopher walks it there (column 8)" || ko "walk: $s"
s=$(run walk-left 200 "${START}90 tap-at @20 84\n")
[ "$(field "$s" col)" = 0 ] && ok "beside the well: to its last column (0)" || ko "walk left: $s"
s=$(run paused 200 "${START}50 tap SELECT\n90 tap-at @160 100\n120 tap-at @296 84\n")
[ "$(field "$s" col)" = 4 ] && [ "$(field "$s" pops)" = 0 ] && ok "paused: the fingers do nothing" || ko "paused: $s"

# the game over (the bot gives up from 20 m): a tap goes back to the title
s=$(run over 12000 "" 320 "--opt bot=1 --opt botstop=20")
over=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$O/over.log")
if [ "$(field "$s" st)" = 3 ] && [ -n "$over" ]; then ok "a run ends (the panel at frame $over)"
else ko "no game over: $s"; over=6000; fi
B="--opt bot=1 --opt botstop=20"
s=$(run back $((over + 60)) "$((over + 40)) tap-at 50%% 50%%\n" 320 "$B")
[ "$(field "$s" st)" = 0 ] && ok "a tap on the game-over panel goes back to the title" || ko "back: $s"
s=$(run early $((over + 60)) "$((over + 10)) tap-at 50%% 50%%\n" 320 "$B")
[ "$(field "$s" st)" = 3 ] && ok "not a tap before the panel's lock" || ko "early tap: $s"
s=$(run held $((over + 60)) "$((over - 30)) touch 50%% 50%%\n$((over + 45)) up\n" 320 "$B")
[ "$(field "$s" st)" = 3 ] && ok "not a finger left down from the run" || ko "held finger: $s"

# every width: the same taps (stage coordinates), the same run
D="${START}90 tap-at @160 100\n140 tap-at @296 120\n200 tap-at @150 220\n260 tap-at @40 200\n"
ref=$(run w320 400 "$D" 320)
for w in $WS 560; do
    s=$(run "w$w" 400 "$D" "$w")
    [ "$(field "$s" hash)" = "$(field "$ref" hash)" ] && ok "width $w: the same taps, the same run as at 320" || ko "width $w: $s / $ref"
done
exit $fail

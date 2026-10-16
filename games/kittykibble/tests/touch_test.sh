#!/bin/sh
# Kitty Kibble touch controls (DESIGN.md "Controls", docs/spec.md "Touch"; rs_game.touch_native): headless runs with
# scripted fingers, checked on the final state's log line and against the same runs played on the pad. A tap starts
# on the title (as the finger is lifted); in play a swipe turns the kitten (the very run of the d-pad tapped that way:
# buffered for the next junction), a finger can swipe on without lifting, a finger that only rests does nothing;
# nothing while paused; a tap on the game-over panel goes back to the title (not a finger left down from the run, not
# a tap before the lock). The same run at every width.
#   touch_test.sh <kittykibble_headless> <out dir> [widths]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/kittykibble/LICENSE.
H=$1
O=${2:-/tmp}/kittykibble-touch
WS=${3:-427 520}
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
# run <name> <frames> <script> [width]: the final state's log line
run() {
    printf "$3" > "$O/$1.input"
    $H --width "${4:-320}" --frames "$2" --input "$O/$1.input" --opt seed=4 --opt strict=1 --opt dump=1 --opt music=0 \
        2> "$O/$1.log" > /dev/null
    if grep -q "rs strict" "$O/$1.log"; then ko "$1: strict mode"; grep "rs strict" "$O/$1.log" | head -3; fi
    grep "state:" "$O/$1.log" | tail -1
}
field() { echo "$1" | sed -n "s/.* $2=\([^ ]*\).*/\1/p"; }
same() { [ "$(field "$1" hash)" = "$(field "$2" hash)" ] && [ "$(field "$1" x)" = "$(field "$2" x)" ] &&
         [ "$(field "$1" y)" = "$(field "$2" y)" ] && [ "$(field "$1" score)" = "$(field "$2" score)" ]; }

START="20 tap-at 50%% 30%%\n"         # a tap: the title starts as the finger is lifted (frame 23)
PAD="23 tap A\n"                      # the same start on the pad

# the title
s=$(run title 60 "")
[ "$(field "$s" st)" = 0 ] && ok "the title waits (no touch)" || ko "the title did not wait: $s"
a=$(run start 50 "$START")
b=$(run start-pad 50 "$PAD")
if [ "$(field "$a" st)" = 1 ] && same "$a" "$b"; then ok "a tap on the title starts the run (the very run of A)"
else ko "a tap on the title: $a / pad $b"; fi
s=$(run hold-title 60 "20 touch 50%% 30%%\n")
[ "$(field "$s" st)" = 0 ] && ok "a finger still down on the title does not start yet" || ko "finger down: $s"

# play: a swipe up before the first junction (row 16, column 10) turns the kitten up there; the pad's tap the same
a=$(run swipe 220 "${START}150 swipe 50%% 60%% 0 -30\n")
b=$(run swipe-pad 220 "${PAD}152 tap UP\n")
if [ "$(field "$a" dir)" = 3 ] && [ "$(field "$a" y)" -lt 132 ] && same "$a" "$b"; then
    ok "a swipe up turns the kitten at the junction (the run of Up tapped)"
else ko "swipe: $a / pad $b"; fi
# a finger that stays down and swipes on: up, then right at the top
a=$(run swipe-on 330 "${START}150 touch 50%% 60%%\n153 move 50%% 50%%\n200 move 50%% 50%%\n240 move 60%% 50%%\n")
if [ "$(field "$a" dir)" = 0 ] || [ "$(field "$a" x)" -gt 84 ]; then ok "a finger swipes on without lifting: up, then right"
else ko "swipe on: $a"; fi
# a finger resting (a tap in play) changes nothing
a=$(run rest 220 "${START}150 tap-at 50%% 60%%\n")
b=$(run rest-none 220 "${START}")
same "$a" "$b" && ok "a tap or a resting finger does not turn the kitten" || ko "rest: $a / $b"

# pause: the fingers do nothing
s=$(run paused 260 "${START}100 tap SELECT\n150 swipe 50%% 60%% 0 -30\n")
b=$(run paused-none 260 "${START}100 tap SELECT\n")
if [ "$(field "$s" paused)" = 1 ] && same "$s" "$b"; then ok "paused: a swipe does nothing"
else ko "paused: $s"; fi

# the game over (a kitten left alone is caught): a tap goes back to the title
s=$(run over 5000 "$START")
over=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$O/over.log")
if [ "$(field "$s" st)" = 3 ] && [ -n "$over" ]; then ok "an idle run ends (the panel at frame $over)"
else ko "no game over: $s"; over=3000; fi
s=$(run back $((over + 60)) "${START}$((over + 40)) tap-at 50%% 50%%\n")
[ "$(field "$s" st)" = 0 ] && ok "a tap on the game-over panel goes back to the title" || ko "back: $s"
s=$(run early $((over + 60)) "${START}$((over + 10)) tap-at 50%% 50%%\n")
[ "$(field "$s" st)" = 3 ] && ok "not a tap before the panel's lock" || ko "early tap: $s"
s=$(run held $((over + 60)) "${START}$((over - 30)) touch 50%% 50%%\n$((over + 45)) up\n")
[ "$(field "$s" st)" = 3 ] && ok "not a finger left down from the run" || ko "held finger: $s"
s=$(run again $((over + 80)) "${START}$((over + 40)) tap-at 50%% 50%%\n$((over + 60)) tap-at 50%% 50%%\n")
[ "$(field "$s" st)" = 1 ] && ok "and another tap starts another run" || ko "again: $s"

# every width: the same game
ref=$(run w320 500 "${START}150 swipe 50%% 60%% 0 -30\n260 swipe 40%% 40%% 30 0\n380 swipe 50%% 50%% 0 30\n" 320)
for w in $WS 560; do
    s=$(run "w$w" 500 "${START}150 swipe 50%% 60%% 0 -30\n260 swipe 40%% 40%% 30 0\n380 swipe 50%% 50%% 0 30\n" "$w")
    same "$s" "$ref" && ok "width $w: the same run as at 320" || ko "width $w: $s / $ref"
done
exit $fail

#!/bin/sh
# Hamster Havoc touch controls (DESIGN.md "Controls", docs/spec.md "Touch"; rs_game.touch_native): headless runs with
# scripted fingers, checked on the final state's log line and against the same runs played on the pad. A tap starts
# on the title (as the finger is lifted); in play a finger down makes the ship slide toward its x at the ship's top
# speed (the very run of the d-pad held that way) and fire by itself (A held); it stops under the finger; it follows a
# finger that moves; nothing while paused; a tap on the game-over panel goes back to the title (not a finger left down
# from the run, not a tap before the lock). The same run at every width (the finger at the same stage x).
#   touch_test.sh <hamsterhavoc_headless> <out dir> [widths]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/hamsterhavoc/LICENSE.
H=$1
O=${2:-/tmp}/hamsterhavoc-touch
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
         [ "$(field "$1" shots)" = "$(field "$2" shots)" ] && [ "$(field "$1" score)" = "$(field "$2" score)" ]; }

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

# play: slide toward the finger at full speed, firing (the run of the d-pad and A held)
a=$(run slide 170 "${START}130 touch @300 120\n")
b=$(run slide-pad 170 "${PAD}130 RIGHT+A\n")
if [ "$(field "$a" x)" = 220 ] && [ "$(field "$a" shots)" -gt 0 ] && same "$a" "$b"; then
    ok "a finger on the right: the ship slides at 1.5 px a frame and fires (the run of Right and A held)"
else ko "slide: $a / pad $b"; fi
a=$(run stop 330 "${START}130 touch @200 120\n")
[ "$(field "$a" x)" = 200 ] && ok "it stops under the finger" || ko "stop: $a"
a=$(run follow 330 "${START}130 touch @250 120\n200 move @90 120\n")
[ "$(field "$a" x)" = 90 ] && ok "it follows the finger as it moves" || ko "follow: $a"
a=$(run nofire 300 "${START}")
[ "$(field "$a" shots)" = 0 ] && ok "no finger, no seed" || ko "no finger: $a"
a=$(run lift 200 "${START}130 touch @200 120\n180 up\n")
b=$(run lift-later 330 "${START}130 touch @200 120\n180 up\n")
if [ "$(field "$a" x)" = 200 ] && [ "$(field "$b" x)" = 200 ] && [ "$(field "$a" shots)" = "$(field "$b" shots)" ]; then
    ok "lifted, it stays and holds its fire ($(field "$a" shots) seeds)"
else ko "lift: $a / later $b"; fi

# pause: the fingers do nothing
s=$(run paused 260 "${START}100 tap SELECT\n130 touch @300 120\n")
if [ "$(field "$s" paused)" = 1 ] && [ "$(field "$s" x)" = 160 ] && [ "$(field "$s" shots)" = 0 ]; then
    ok "paused: a finger neither moves nor fires"
else ko "paused: $s"; fi

# the game over (an idle ship is shot): a tap goes back to the title
s=$(run over 4000 "$START")
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

# every width: the same game (the finger at the same stage x)
ref=$(run w320 400 "${START}130 touch @80 120\n250 move @260 100\n" 320)
for w in $WS 560; do
    s=$(run "w$w" 400 "${START}130 touch @80 120\n250 move @260 100\n" "$w")
    same "$s" "$ref" && ok "width $w: the same run as at 320" || ko "width $w: $s / $ref"
done
exit $fail

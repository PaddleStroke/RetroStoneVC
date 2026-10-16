#!/bin/sh
# Caterpillar Crawl touch controls (DESIGN.md "Controls", docs/spec.md "Touch"; rs_game.touch_native): headless runs with
# scripted fingers, checked on the final state's log line and against the same runs played on the pad. A tap starts
# on the title (as the finger is lifted); in play a swipe turns the caterpillar that way, buffered like the d-pad (the
# very run of the d-pad pressed on the same frame); a finger kept down steers on (an L drawn: two turns); a swipe
# straight back is ignored, a slanted one too; nothing while paused; a tap on the game-over panel goes back to the
# title (not a finger left down from the run, not a tap before the lock). The same run at every width.
#   touch_test.sh <caterpillarcrawl_headless> <out dir> [widths]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/caterpillarcrawl/LICENSE.
H=$1
O=${2:-/tmp}/caterpillarcrawl-touch
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
same() { [ "$(field "$1" hash)" = "$(field "$2" hash)" ] && [ "$(field "$1" dir)" = "$(field "$2" dir)" ] &&
         [ "$(field "$1" hx)" = "$(field "$2" hx)" ] && [ "$(field "$1" hy)" = "$(field "$2" hy)" ]; }

START="20 tap-at 50%% 50%%\n"         # a tap: the title starts as the finger is lifted (frame 23)
PAD="23 tap A\n"                      # the same start on the pad

# the title
s=$(run title 60 "")
[ "$(field "$s" st)" = 0 ] && ok "the title waits (no touch)" || ko "the title did not wait: $s"
a=$(run start 70 "$START")
b=$(run start-pad 70 "$PAD")
if [ "$(field "$a" st)" = 1 ] && same "$a" "$b"; then ok "a tap on the title starts the run (the very run of A)"
else ko "a tap on the title: $a / pad $b"; fi
s=$(run hold-title 60 "20 touch 50%% 50%%\n")
[ "$(field "$s" st)" = 0 ] && ok "a finger still down on the title does not start yet" || ko "finger down: $s"

# play: a swipe up (recognised 2 frames after it starts: 20 px) is the d-pad's Up on that frame
a=$(run swipe 120 "${START}50 swipe @200 150 0 -40\n")
b=$(run swipe-pad 120 "${PAD}52 tap UP\n")
if [ "$(field "$a" dir)" = 3 ] && same "$a" "$b"; then ok "a swipe up turns it up (the run of Up on the pad)"
else ko "swipe: $a / pad $b"; fi
a=$(run swipe-down 120 "${START}50 swipe @80 60 0 40\n")
[ "$(field "$a" dir)" = 1 ] && ok "a swipe down, anywhere on the screen: down" || ko "swipe down: $a"
a=$(run swipe-back 120 "${START}50 swipe @200 150 -40 0\n")
b=$(run nothing 120 "${START}")
if [ "$(field "$a" dir)" = 0 ] && same "$a" "$b"; then ok "a swipe straight back is ignored (never into itself)"
else ko "swipe back: $a / no touch $b"; fi
a=$(run slant 120 "${START}50 swipe @200 150 -30 -30\n")
if same "$a" "$b"; then ok "a slanted swipe does nothing"; else ko "slanted: $a"; fi
a=$(run tap-play 120 "${START}50 tap-at @200 150\n")
if same "$a" "$b"; then ok "a tap in play does nothing"; else ko "tap in play: $a"; fi
# a finger kept down steers on: up 20 px, then left 20 px (an L): up, then left, a U-turn in two steps
a=$(run drag 120 "${START}50 touch @200 150\n51 move @200 140\n52 move @200 130\n54 move @190 130\n55 move @180 130\n90 up\n")
b=$(run drag-pad 120 "${PAD}52 tap UP\n55 tap LEFT\n")
if [ "$(field "$a" dir)" = 2 ] && same "$a" "$b"; then ok "a finger kept down draws an L: up, then left (two buffered turns)"
else ko "drag: $a / pad $b"; fi

# pause: a swipe does nothing
s=$(run paused 120 "${START}40 tap SELECT\n50 swipe @200 150 0 -40\n")
if [ "$(field "$s" paused)" = 1 ] && [ "$(field "$s" dir)" = 0 ]; then ok "paused: a swipe does nothing"
else ko "paused: $s"; fi

# the game over (no turn: it crawls into the wall): a tap goes back to the title
s=$(run over 600 "$START")
over=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$O/over.log")
if [ "$(field "$s" st)" = 3 ] && [ -n "$over" ]; then ok "the wall ends the run (the panel at frame $over)"
else ko "no game over: $s"; over=300; fi
s=$(run back $((over + 60)) "${START}$((over + 40)) tap-at 50%% 50%%\n")
[ "$(field "$s" st)" = 0 ] && ok "a tap on the game-over panel goes back to the title" || ko "back: $s"
s=$(run early $((over + 60)) "${START}$((over + 10)) tap-at 50%% 50%%\n")
[ "$(field "$s" st)" = 3 ] && ok "not a tap before the panel's lock" || ko "early tap: $s"
s=$(run held $((over + 60)) "${START}$((over - 30)) touch 50%% 50%%\n$((over + 45)) up\n")
[ "$(field "$s" st)" = 3 ] && ok "not a finger left down from the run" || ko "held finger: $s"
s=$(run again $((over + 80)) "${START}$((over + 40)) tap-at 50%% 50%%\n$((over + 60)) tap-at 50%% 50%%\n")
[ "$(field "$s" st)" = 1 ] && ok "and another tap starts another run" || ko "again: $s"

# every width: the same game (the finger at the same stage spot)
ref=$(run w320 200 "${START}50 swipe @200 150 0 -40\n90 swipe @100 60 -40 0\n130 swipe @60 100 0 40\n" 320)
for w in $WS 560; do
    s=$(run "w$w" 200 "${START}50 swipe @200 150 0 -40\n90 swipe @100 60 -40 0\n130 swipe @60 100 0 40\n" "$w")
    same "$s" "$ref" && ok "width $w: the same run as at 320" || ko "width $w: $s / $ref"
done
exit $fail

#!/bin/sh
# Jelly Jam touch controls (DESIGN.md "Controls", docs/spec.md "Touch"; rs_game.touch_native): headless runs with
# scripted fingers, checked on the final state's log line. A tap starts on the title (as the finger is lifted); in play
# a finger dragged left or right moves the pair column by column (a cell's width of drag per column), a tap turns it, a
# swipe down drops it at once, a finger dragged down drops it faster; nothing while paused; a tap on the game-over panel
# goes back to the title (not a finger left down from the run, not a tap before the lock). The same run at every width.
#   touch_test.sh <jellyjam_headless> <out dir> [widths]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/jellyjam/LICENSE.
H=$1
O=${2:-/tmp}/jellyjam-touch
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
# the pair comes in at frame 23 + 110 + 1; the fingers below go down at 150 (stage coordinates: @x)

s=$(run title 60 "")
[ "$(field "$s" st)" = 0 ] && ok "the title waits (no touch)" || ko "the title did not wait: $s"
a=$(run start 60 "$START")
b=$(run start-pad 60 "$PAD")
if [ "$(field "$a" st)" = 1 ] && [ "$(field "$a" hash)" = "$(field "$b" hash)" ]; then ok "a tap on the title starts the duel (the very run of A)"
else ko "a tap on the title: $a / pad $b"; fi
s=$(run hold-title 60 "20 touch 50%% 30%%\n")
[ "$(field "$s" st)" = 0 ] && ok "a finger still down on the title does not start yet" || ko "finger down: $s"

# drag: a cell (16 px) right per column
s=$(run drag1 170 "${START}150 touch @60 120\n152 move @70 120\n154 move @77 120\n")
[ "$(field "$s" col)" = 3 ] && ok "dragged 17 px right: one column (col 3)" || ko "drag 1: $s"
s=$(run drag2 170 "${START}150 touch @60 120\n152 move @80 120\n154 move @94 120\n")
[ "$(field "$s" col)" = 4 ] && ok "dragged 34 px right: two columns" || ko "drag 2: $s"
s=$(run drag3 175 "${START}150 touch @200 120\n152 move @150 120\n154 move @120 120\n")
[ "$(field "$s" col)" = 0 ] && ok "dragged far left: to the wall" || ko "drag 3: $s"
s=$(run small 170 "${START}150 touch @60 120\n152 move @65 120\n")
[ "$(field "$s" col)" = 2 ] && ok "a drag under half a cell does not move it" || ko "small drag: $s"
# tap: turn
s=$(run tap 170 "${START}150 tap-at @160 120\n")
[ "$(field "$s" rot)" = 1 ] && [ "$(field "$s" col)" = 2 ] && ok "a tap turns the pair clockwise" || ko "tap: $s"
s=$(run tap2 180 "${START}150 tap-at @160 120\n160 tap-at @160 120\n")
[ "$(field "$s" rot)" = 2 ] && ok "two taps: turned twice" || ko "tap twice: $s"
# swipe down: drops at once (lands within a few frames); a finger dragged down: faster
s=$(run swipe 168 "${START}150 swipe @160 100 0 40\n")
if [ "$(field "$s" fstate)" != 2 ] && [ "$(field "$s" count)" -ge 2 ]; then ok "a swipe down drops the pair at once"; else ko "swipe: $s"; fi
s=$(run nothing 168 "$START")
[ "$(field "$s" fstate)" = 2 ] && ok "...(without it, still falling)" || ko "no swipe: $s"
s=$(run softdrag 200 "${START}150 touch @160 100\n152 move @160 115\n154 move @160 134\n")
if [ "$(field "$s" count)" -ge 2 ] && [ "$(field "$s" score)" -ge 8 ]; then ok "a finger dragged down drops it faster (score $(field "$s" score))"; else ko "soft drag: $s"; fi

# pause: the fingers do nothing
a=$(run paused 220 "${START}145 tap SELECT\n150 touch @60 120\n152 move @94 120\n170 up\n180 tap-at @160 120\n")
[ "$(field "$a" col)" = 2 ] && [ "$(field "$a" rot)" = 0 ] && ok "paused: drags and taps do nothing" || ko "paused: $a"

# the game over (the bot gives up after 600 frames): a tap goes back to the title
B="--opt bot=1 --opt botstop=600"
s=$(run over 6000 "" 320 "$B")
over=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$O/over.log")
if [ "$(field "$s" st)" = 3 ] && [ -n "$over" ]; then ok "a run ends (the panel at frame $over)"
else ko "no game over: $s"; over=3000; fi
s=$(run back $((over + 60)) "$((over + 40)) tap-at 50%% 50%%\n" 320 "$B")
[ "$(field "$s" st)" = 0 ] && ok "a tap on the game-over panel goes back to the title" || ko "back: $s"
s=$(run early $((over + 60)) "$((over + 10)) tap-at 50%% 50%%\n" 320 "$B")
[ "$(field "$s" st)" = 3 ] && ok "not a tap before the panel's lock" || ko "early tap: $s"
s=$(run held $((over + 60)) "$((over - 30)) touch 50%% 50%%\n$((over + 45)) up\n" 320 "$B")
[ "$(field "$s" st)" = 3 ] && ok "not a finger left down from the run" || ko "held finger: $s"

# every width: the same game (the fields keep their place on the stage)
D="${START}150 touch @60 120\n152 move @80 120\n160 up\n170 tap-at @160 120\n200 swipe @160 100 0 40\n300 touch @200 120\n310 move @150 120\n330 up\n"
ref=$(run w320 500 "$D")
for w in $WS 560; do
    s=$(run "w$w" 500 "$D" "$w")
    [ "$(field "$s" hash)" = "$(field "$ref" hash)" ] && ok "width $w: the same drags and taps, the same run as at 320" || ko "width $w: $s / $ref"
done
exit $fail

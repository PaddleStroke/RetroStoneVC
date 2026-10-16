#!/bin/sh
# Gecko Golf touch controls (DESIGN.md "Controls", docs/spec.md "Touch"; rs_game.touch_native): headless runs with
# scripted fingers, checked on the final state's log line. A tap starts on the title (as the finger is lifted); in
# play the slingshot: a finger down anywhere, pulled back (the shot goes the other way, the power by the pull's length,
# full from TOUCH_FULL 110 px), let go: the shot, the very shot of the pad at the same aim and power; a short pull is no
# shot; a pull upwards shoots along the ground; nothing while it is another player's turn or paused; a tap on the
# game-over panel goes back to the title (not a finger left down from the run, not a tap before the lock). The same run
# at every width.
#   touch_test.sh <geckogolf_headless> <out dir> [widths]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/geckogolf/LICENSE.
H=$1
O=${2:-/tmp}/geckogolf-touch
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
if [ "$(field "$a" st)" = 1 ] && [ "$(field "$a" shots)" = 0 ] && [ "$(field "$b" st)" = 1 ]; then
    ok "a tap on the title starts the run (no shot)"
else ko "a tap on the title: $a / pad $b"; fi
s=$(run hold-title 60 "20 touch 50%% 30%%\n")
[ "$(field "$s" st)" = 0 ] && ok "a finger still down on the title does not start yet" || ko "finger down: $s"

# the slingshot: pulled 50 px down and 50 left: 45 degrees up to the right (the default aim), the gauge at 72
SL="${START}60 touch 200 100\n61 move 180 120\n62 move 150 150\n"
s=$(run pull 90 "$SL")
if [ "$(field "$s" phase)" = 1 ] && [ "$(field "$s" gauge)" = 72 ] && [ "$(field "$s" aim)" = 90 ]; then
    ok "pulled back: the aim and the gauge follow (aim 90, gauge 72)"
else ko "pull: $s"; fi
a=$(run sling 900 "${SL}100 up\n")
b=$(run sling-pad 900 "${PAD}60 A\n97 -\n")
if [ "$(field "$a" shots)" = 1 ] && [ "$(field "$a" x)" = "$(field "$b" x)" ] && [ "$(field "$a" y)" = "$(field "$b" y)" ]; then
    ok "let go: the shot, the pad's very shot (the ball at $(field "$a" x),$(field "$a" y))"
else ko "slingshot: $a / pad $b"; fi
s=$(run short 200 "${START}60 touch 200 100\n61 move 196 104\n100 up\n")
[ "$(field "$s" shots)" = 0 ] && [ "$(field "$s" phase)" = 0 ] && ok "a short pull is no shot" || ko "short pull: $s"
s=$(run back 200 "${SL}80 move 198 102\n100 up\n")
[ "$(field "$s" shots)" = 0 ] && ok "pulled back to where it went down: no shot" || ko "back to the start: $s"
s=$(run upwards 90 "${START}60 touch 200 150\n61 move 230 110\n")
[ "$(field "$s" aim)" = 352 ] && ok "a pull upwards: along the ground (aim 352)" || ko "upwards: $s"
s=$(run long 90 "${START}60 touch 300 60\n61 move 100 160\n")
[ "$(field "$s" gauge)" = 120 ] && ok "a long pull: full power" || ko "long pull: $s"

# not player 1's turn: nothing (2 players: P1 shoots by the pad, then P2's turn)
s=$(run p2turn 600 "10 P2 tap A\n${PAD}60 A\n80 -\n420 touch 200 100\n421 move 150 150\n480 up\n" 320 "")
[ "$(field "$s" shots)" = 1 ] && ok "another player's turn: the finger does nothing" || ko "P2's turn: $s"
# pause: the fingers do nothing
s=$(run paused 200 "${START}50 tap SELECT\n60 touch 200 100\n62 move 150 150\n100 up\n")
[ "$(field "$s" shots)" = 0 ] && [ "$(field "$s" paused)" = 1 ] && ok "paused: the slingshot does nothing" || ko "paused: $s"

# the game over (the bot gives up after a hole): a tap goes back to the title
B="--opt bot=1 --opt botstop=1 --opt strokes=2"
s=$(run over 3000 "" 320 "$B")
over=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$O/over.log")
if [ "$(field "$s" st)" = 3 ] && [ -n "$over" ]; then ok "a run ends (the panel at frame $over)"
else ko "no game over: $s"; over=1000; fi
s=$(run tapback $((over + 60)) "$((over + 40)) tap-at 50%% 50%%\n" 320 "$B")
[ "$(field "$s" st)" = 0 ] && ok "a tap on the game-over panel goes back to the title" || ko "back: $s"
s=$(run early $((over + 60)) "$((over + 10)) tap-at 50%% 50%%\n" 320 "$B")
[ "$(field "$s" st)" = 3 ] && ok "not a tap before the panel's lock" || ko "early tap: $s"
s=$(run held $((over + 60)) "$((over - 30)) touch 50%% 50%%\n$((over + 45)) up\n" 320 "$B")
[ "$(field "$s" st)" = 3 ] && ok "not a finger left down from the run" || ko "held finger: $s"

# every width: the same pull, the same run as at 320
ref=$(run w320 900 "${SL}100 up\n" 320)
for w in $WS 560; do
    s=$(run "w$w" 900 "${SL}100 up\n" "$w")
    [ "$(field "$s" hash)" = "$(field "$ref" hash)" ] && ok "width $w: the same pull, the same run as at 320" || ko "width $w: $s / $ref"
done
exit $fail

#!/bin/sh
# Cosmo Corgi touch controls (DESIGN.md "Controls", docs/spec.md "Touch"; rs_game.touch_native): headless runs with
# scripted fingers, checked on the final state's log line. A tap starts on the title (as the finger is lifted); in
# play a RELATIVE drag anywhere moves the rocket by the finger's movement, 1:1 in screen pixels, at most TOUCH_MAX (8)
# a frame, clamped to the stage; a new finger (or one put down again elsewhere) never makes it jump; it fires by
# itself all along; nothing while paused, and no jump when the game resumes; a tap on the game-over panel goes back to
# the title (not a finger left down from the run, not a tap before the lock). The same run at every width.
#   touch_test.sh <cosmocorgi_headless> <out dir> [widths]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/cosmocorgi/LICENSE.
H=$1
O=${2:-/tmp}/cosmocorgi-touch
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
at() { [ "$(field "$1" x)" = "$2" ] && [ "$(field "$1" y)" = "$3" ]; }

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

# play: the rocket (64, 120) follows the finger's movement, wherever the finger is
a=$(run drag 150 "${START}100 touch 200 200\n101 move 205 196\n102 move 210 192\n103 move 215 188\n104 move 220 184\n")
at "$a" 84 104 && ok "a drag far from the rocket moves it by the finger's movement, 1:1 (+20, -16)" || ko "drag: $a"
[ "$(field "$a" shots)" -gt 0 ] && ok "it fires by itself" || ko "no shots: $a"
a=$(run fast 150 "${START}100 touch 100 100\n101 move 200 100\n")
at "$a" 72 120 && ok "at most 8 px a frame (a 100-px flick moves it 8)" || ko "fast: $a"
a=$(run again 150 "${START}100 touch 100 100\n101 move 106 100\n102 move 110 100\n103 up\n110 touch 20 220\n111 move 26 220\n112 move 30 220\n")
at "$a" 84 120 && ok "lifted and put down elsewhere: no jump, the next drag goes on from where it is" || ko "again: $a"
s="${START}100 touch 10 120\n"
i=101; while [ $i -lt 160 ]; do s="${s}$i move $((10 + (i - 100) * 6)) 120\n"; i=$((i + 1)); done
a=$(run edge 170 "$s")
[ "$(field "$a" x)" = 300 ] && ok "the drag is clamped to the stage" || ko "edge: $a"

# pause: the fingers do nothing, and the game resumes without a jump
a=$(run paused 200 "${START}90 tap SELECT\n100 touch 100 100\n110 move 150 150\n120 tap SELECT\n130 move 154 150\n")
at "$a" 68 120 && ok "paused: a drag does nothing; after it only the new movement counts" || ko "paused: $a"

# the game over (the run with the bot flying into the robots): a tap goes back to the title
s=$(run over 4000 "" 320 "--opt bot=1 --opt botstop=100")
over=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$O/over.log")
if [ "$(field "$s" st)" = 3 ] && [ -n "$over" ]; then ok "a run ends (the panel at frame $over)"
else ko "no game over: $s"; over=3000; fi
B="--opt bot=1 --opt botstop=100"
s=$(run back $((over + 60)) "$((over + 40)) tap-at 50%% 50%%\n" 320 "$B")
[ "$(field "$s" st)" = 0 ] && ok "a tap on the game-over panel goes back to the title" || ko "back: $s"
s=$(run early $((over + 60)) "$((over + 10)) tap-at 50%% 50%%\n" 320 "$B")
[ "$(field "$s" st)" = 3 ] && ok "not a tap before the panel's lock" || ko "early tap: $s"
s=$(run held $((over + 60)) "$((over - 30)) touch 50%% 50%%\n$((over + 45)) up\n" 320 "$B")
[ "$(field "$s" st)" = 3 ] && ok "not a finger left down from the run" || ko "held finger: $s"

# every width: the same game (the drag in screen pixels is the same movement; the bot sees the same world)
D="${START}100 touch 200 200\n101 move 205 196\n102 move 210 192\n140 move 230 150\n300 up\n"
ref=$(run w320 600 "$D" 320)
bref=$(run b320 2000 "" 320 "--opt bot=1 --opt seed=9")
for w in $WS 560; do
    s=$(run "w$w" 600 "$D" "$w")
    [ "$(field "$s" hash)" = "$(field "$ref" hash)" ] && ok "width $w: the same drag, the same run as at 320" || ko "width $w: $s / $ref"
    s=$(run "b$w" 2000 "" "$w" "--opt bot=1 --opt seed=9")
    [ "$(field "$s" hash)" = "$(field "$bref" hash)" ] && ok "width $w: the bot's run is that of 320 (the robots come at the same time and place)" ||
        ko "width $w (bot): $s / $bref"
done
exit $fail

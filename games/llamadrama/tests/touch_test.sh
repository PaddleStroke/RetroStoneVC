#!/bin/sh
# Llama Drama touch controls (DESIGN.md "Controls", docs/spec.md "Touch"; rs_game.touch_native): headless runs with
# scripted fingers, checked on the final state's log line. A tap starts on the title (as the finger is lifted); in play
# a finger put down to the right of the llama's mouth turns its head toward that spot and spits there at once (the
# same reload as A: a second tap during it spits when the reload ends); a finger held down: the head follows it; a
# finger left of the mouth does nothing; nothing while paused; a tap on the game-over panel goes back to the title (not
# a finger left down from the run, not a tap before the lock). The same taps (in stage pixels) play the same at every
# width, and so does the bot.
#   touch_test.sh <llamadrama_headless> <out dir> [widths]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/llamadrama/LICENSE.
H=$1
O=${2:-/tmp}/llamadrama-touch
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
PAD="23 tap START\n"                  # the same start on the pad (Start: no spit)

# the title
s=$(run title 60 "")
[ "$(field "$s" st)" = 0 ] && ok "the title waits (no touch)" || ko "the title did not wait: $s"
a=$(run start 50 "$START")
b=$(run start-pad 50 "$PAD")
if [ "$(field "$a" st)" = 1 ] && [ "$(field "$a" hash)" = "$(field "$b" hash)" ]; then ok "a tap on the title starts the run (the very run of Start)"
else ko "a tap on the title: $a / pad $b"; fi
s=$(run hold-title 60 "20 touch 50%% 30%%\n")
[ "$(field "$s" st)" = 0 ] && ok "a finger still down on the title does not start yet" || ko "finger down: $s"

# play: a tap high in the sky, low over the grass, left of the mouth
a=$(run high 60 "${START}40 tap-at @220 @40\n")
[ "$(field "$a" spits)" = 1 ] && [ "$(field "$a" aim)" -gt 60 ] && ok "a tap high in the sky: the head turns up and spits at once (aim $(field "$a" aim))" ||
    ko "high tap: $a"
a=$(run low 60 "${START}40 tap-at @200 @185\n")
[ "$(field "$a" spits)" = 1 ] && [ "$(field "$a" aim)" -lt -20 ] && ok "a tap over the grass: the head turns down (aim $(field "$a" aim))" ||
    ko "low tap: $a"
a=$(run left 60 "${START}40 tap-at @30 @100\n")
[ "$(field "$a" spits)" = 0 ] && [ "$(field "$a" aim)" = 14 ] && ok "a tap left of the mouth does nothing" || ko "left tap: $a"
a=$(run aimed 60 "${START}40 tap-at @200 @118\n")
b=$(run aimed2 60 "${START}40 tap-at @300 @118\n")
[ "$(field "$a" aim)" -lt "$(field "$b" aim)" ] && ok "farther, at the same height: a higher aim (the spit's arc)" || ko "arc: $a / $b"
# the reload: a second tap 4 frames later spits when the reload ends; 3 gobs in the air at most
a=$(run reload 47 "${START}40 tap-at @250 @100\n44 tap-at @250 @100\n")
b=$(run reload2 55 "${START}40 tap-at @250 @100\n44 tap-at @250 @100\n")
[ "$(field "$a" spits)" = 1 ] && [ "$(field "$b" spits)" = 2 ] && ok "a tap during the reload spits when it ends (frame 52), not before" ||
    ko "reload: $a / $b"
# a finger held and moved: the head follows it
a=$(run follow 80 "${START}40 touch @220 @150\n50 move @220 @100\n60 move @220 @40\n")
b=$(run follow2 80 "${START}40 tap-at @220 @40\n")
[ "$(field "$a" aim)" = "$(field "$b" aim)" ] && [ "$(field "$a" spits)" = 1 ] && ok "a finger held and moved up: the head follows it (one spit)" ||
    ko "follow: $a / $b"

# pause: the fingers do nothing
a=$(run paused 120 "${START}60 tap SELECT\n70 tap-at @220 @40\n90 tap SELECT\n")
[ "$(field "$a" spits)" = 0 ] && [ "$(field "$a" aim)" = 14 ] && ok "paused: a tap does nothing" || ko "paused: $a"

# the game over (the bot stops at 100 points): a tap goes back to the title
B="--opt bot=1 --opt botstop=100"
s=$(run over 6000 "" 320 "$B")
over=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$O/over.log")
if [ "$(field "$s" st)" = 3 ] && [ -n "$over" ]; then ok "a run ends (the panel at frame $over)"
else ko "no game over: $s"; over=3000; fi
s=$(run back $((over + 60)) "$((over + 40)) tap-at 50%% 50%%\n" 320 "$B")
[ "$(field "$s" st)" = 0 ] && ok "a tap on the game-over panel goes back to the title" || ko "back: $s"
s=$(run early $((over + 60)) "$((over + 10)) tap-at 50%% 50%%\n" 320 "$B")
[ "$(field "$s" st)" = 3 ] && ok "not a tap before the panel's lock" || ko "early tap: $s"
s=$(run held $((over + 60)) "$((over - 30)) touch 90%% 50%%\n$((over + 45)) up\n" 320 "$B")
[ "$(field "$s" st)" = 3 ] && ok "not a finger left down from the run" || ko "held finger: $s"

# every width: the same game (taps in stage pixels aim the same; the bot sees the same hornets at the same time)
D="${START}100 tap-at @260 @80\n160 tap-at @200 @150\n230 touch @240 @120\n240 move @240 @60\n300 up\n420 tap-at @300 @110\n"
ref=$(run w320 600 "$D" 320)
bref=$(run b320 2400 "" 320 "--opt bot=1 --opt seed=9")
for w in $WS 560; do
    s=$(run "w$w" 600 "$D" "$w")
    [ "$(field "$s" hash)" = "$(field "$ref" hash)" ] && ok "width $w: the same taps, the same run as at 320" || ko "width $w: $s / $ref"
    s=$(run "b$w" 2400 "" "$w" "--opt bot=1 --opt seed=9")
    [ "$(field "$s" hash)" = "$(field "$bref" hash)" ] && ok "width $w: the bot's run is that of 320 (the hornets come at the same time and place)" ||
        ko "width $w (bot): $s / $bref"
done
exit $fail

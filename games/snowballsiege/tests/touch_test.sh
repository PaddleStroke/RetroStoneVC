#!/bin/sh
# Snowball Siege touch controls (DESIGN.md "Touch", docs/spec.md "Touch"; rs_game.touch_native): headless runs with
# scripted fingers, checked on the final state's log line. A tap starts on the title (as the finger is lifted); on
# your turn, a finger down on your critter pulls a slingshot: the aim and the gauge follow the pull, letting go throws
# (a short pull cancels); a tap beside the critter shuffles it; the round button at the bottom right switches the
# throwable; a finger dragged on the field looks along it (the camera only); nothing while paused; a tap on the
# game-over panel goes back to the title (not before the lock). The same pull at every width: the same throw.
#   touch_test.sh <snowballsiege_headless> <out dir> [widths]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/snowballsiege/LICENSE.
H=$1
O=${2:-/tmp}/snowballsiege-touch
WS=${3:-427 520}
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
# run <name> <frames> <script> [width] [options]: the final state's log line
run() {
    printf "$3" > "$O/$1.input"
    $H --width "${4:-320}" --frames "$2" --input "$O/$1.input" --opt seed=5 --opt strict=1 --opt dump=1 --opt music=0 \
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
if [ "$(field "$a" st)" = 1 ] && [ "$(field "$a" hash)" = "$(field "$b" hash)" ]; then ok "a tap on the title starts (the very run of A)"
else ko "a tap on the title: $a / pad $b"; fi
s=$(run hold-title 60 "20 touch 50%% 30%%\n")
[ "$(field "$s" st)" = 0 ] && ok "a finger still down on the title does not start yet" || ko "finger down: $s"

# where the active critter is on the screen (stage px: @x)
probe() { s=$(run "probe$1" 40 "$START" "$1"); X=$(field "$s" sx); Y=$(field "$s" sy); }
probe 320
# the slingshot: down on the critter, pulled 40 px left and 40 px down: thrown up-right at 45 degrees, power 780
SL="${START}40 touch @$X $Y\n41 move @$((X - 20)) $((Y + 20))\n42 move @$((X - 40)) $((Y + 40))\n"
s=$(run sling 60 "$SL")
if [ "$(field "$s" phase)" = 1 ] && [ "$(field "$s" angle)" = 45 ] && [ "$(field "$s" power)" = 780 ] && \
   [ "$(field "$s" facing)" = 1 ]; then ok "pulled: the aim (45 degrees) and the gauge (780) follow the finger"
else ko "slingshot pulled: $s"; fi
s=$(run fire 66 "${SL}62 up\n")
[ "$(field "$s" phase)" = 2 ] && ok "let go: thrown" || ko "let go: $s"
ref=$(run fire-later 400 "${SL}62 up\n")
s=$(run short 80 "${START}40 touch @$X $Y\n41 move @$((X - 4)) $((Y + 3))\n60 up\n")
[ "$(field "$s" phase)" = 1 ] && ok "a short pull is cancelled" || ko "short pull: $s"
SL2="${START}40 touch @$X $Y\n41 move @$((X + 30)) $((Y + 50))\n"
s=$(run left 60 "$SL2")
[ "$(field "$s" facing)" = -1 ] && ok "pulled to the right: aims left" || ko "pull right: $s"
# a tap beside the critter shuffles it
s=$(run shuffle 120 "${START}40 tap-at @$((X + 30)) $Y\n")
[ "$(field "$s" moved)" = 8 ] && ok "a tap beside the critter: it shuffles 8 px that way" || ko "shuffle: $s"
s=$(run shuffle-far 120 "${START}40 tap-at @$((X + 120)) $((Y - 90))\n")
[ "$(field "$s" moved)" = 0 ] && ok "a tap far from it does not" || ko "far tap: $s"
# the round button at the bottom right: the next throwable
s=$(run button 60 "${START}40 tap-at 296 200\n")
[ "$(field "$s" weapon)" = 1 ] && ok "the button: the big snowball" || ko "button: $s"
# a finger dragged on the field: the camera looks along it, the game is unchanged
base=$(run look0 100 "$START")
s=$(run look 100 "${START}40 touch 260 60\n50 move 160 60\n60 move 60 60\n90 up\n")
if [ "$(field "$s" hash)" = "$(field "$base" hash)" ] && [ "$(field "$s" cam)" != "$(field "$base" cam)" ]; then
    ok "a drag on the field pans the camera ($(field "$base" cam) -> $(field "$s" cam)), nothing else"
else ko "look: $s / $base"; fi
# paused: the fingers do nothing
s=$(run paused 120 "${START}30 tap SELECT\n40 touch @$X $Y\n41 move @$((X - 40)) $((Y + 40))\n80 up\n")
[ "$(field "$s" phase)" = 1 ] && [ "$(field "$s" angle)" = 45 ] && [ "$(field "$s" power)" = 0 ] && \
    ok "paused: the slingshot does nothing" || ko "paused: $s"

# the game over (the bot gives up after a match won): a tap goes back to the title
B="--opt bot=1 --opt botstop=1 --opt ready=1"
s=$(run over 9000 "" 320 "$B")
over=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$O/over.log")
if [ "$(field "$s" st)" = 3 ] && [ -n "$over" ]; then ok "a run ends (the panel at frame $over)"
else ko "no game over: $s"; over=3000; fi
s=$(run back $((over + 60)) "$((over + 40)) tap-at 50%% 50%%\n" 320 "$B")
[ "$(field "$s" st)" = 0 ] && ok "a tap on the game-over panel goes back to the title" || ko "back: $s"
s=$(run early $((over + 60)) "$((over + 10)) tap-at 50%% 50%%\n" 320 "$B")
[ "$(field "$s" st)" = 3 ] && ok "not a tap before the panel's lock" || ko "early tap: $s"

# every width: the same pull from the critter, the same throw and the same run
for w in $WS 560; do
    probe "$w"
    SLW="${START}40 touch @$X $Y\n41 move @$((X - 20)) $((Y + 20))\n42 move @$((X - 40)) $((Y + 40))\n62 up\n"
    s=$(run "w$w" 400 "$SLW" "$w")
    [ "$(field "$s" hash)" = "$(field "$ref" hash)" ] && ok "width $w: the same pull, the same throw as at 320" || \
        ko "width $w: $s / $ref"
    s=$(run "b$w" 60 "${START}40 tap-at $((w - 24)) 200\n" "$w")
    [ "$(field "$s" weapon)" = 1 ] && ok "width $w: the button at the bottom right" || ko "width $w button: $s"
done
exit $fail

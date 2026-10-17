#!/bin/sh
# Hamster Havoc touch controls (DESIGN.md "Touch"): the game reads no touches (rs_game.touch_native = 0); the phone
# apps turn them into P1's pad (apps/launcher/games.cfg: the left / right half holds Left / Right, two drawn shoot
# buttons hold A). Headless runs, checked on the final state's log line: raw fingers do nothing (title, play); the
# app's inputs do the scheme: a tap of Left, Right or A starts on the title; Left / Right held move the ship and stop
# when released; A held fires (one thumb on a button, the other on a half: both); a tap leaves the game-over panel
# after its lock. The same run at every width.
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

START="20 tap A\n"                    # the start (a shoot button's tap)

# the title: a raw finger does nothing; the app's Left, Right or A (a tap anywhere) starts
s=$(run raw-title 60 "20 tap-at 50%% 30%%\n")
[ "$(field "$s" st)" = 0 ] && ok "a raw tap does nothing on the title (touch_native off)" || ko "raw tap: $s"
for b in LEFT RIGHT A; do
    s=$(run "start-$b" 50 "20 tap $b\n")
    [ "$(field "$s" st)" = 1 ] && ok "a tap of $b (the app's touch) starts the run" || ko "start with $b: $s"
done

# play: a raw finger neither moves nor fires
s=$(run raw-play 200 "${START}130 touch @300 120\n")
if [ "$(field "$s" x)" = 160 ] && [ "$(field "$s" shots)" = 0 ]; then ok "a raw finger in play does nothing"
else ko "raw finger in play: $s"; fi
# the left / right half (Left / Right held): 1.5 px a frame, no seed
s=$(run left 170 "${START}130 LEFT\n")
if [ "$(field "$s" x)" = 100 ] && [ "$(field "$s" shots)" = 0 ]; then ok "the left half: the ship moves left"
else ko "left half: $s"; fi
s=$(run right 170 "${START}130 RIGHT\n")
[ "$(field "$s" x)" = 220 ] && ok "the right half: the ship moves right" || ko "right half: $s"
s=$(run release 200 "${START}130 LEFT\n150 -\n")
[ "$(field "$s" x)" = 130 ] && ok "released: it stops" || ko "release: $s"
# a shoot button (A held): seeds, the ship stays
s=$(run fire 300 "${START}130 A\n")
if [ "$(field "$s" x)" = 160 ] && [ "$(field "$s" shots)" -gt 1 ]; then
    ok "a shoot button held: it fires again and again ($(field "$s" shots) seeds)"
else ko "fire: $s"; fi
s=$(run both 170 "${START}130 RIGHT+A\n")
if [ "$(field "$s" x)" = 220 ] && [ "$(field "$s" shots)" -gt 0 ]; then ok "a half and a button: it moves and fires"
else ko "both: $s"; fi

# the game over (an idle ship is shot): a tap (Left, Right or A) after the lock goes back to the title
s=$(run over 4000 "$START")
over=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$O/over.log")
if [ "$(field "$s" st)" = 3 ] && [ -n "$over" ]; then ok "an idle run ends (the panel at frame $over)"
else ko "no game over: $s"; over=3000; fi
for b in LEFT RIGHT A; do
    s=$(run "back-$b" $((over + 60)) "${START}$((over + 40)) tap $b\n")
    [ "$(field "$s" st)" = 0 ] && ok "a tap of $b on the game-over panel: the title" || ko "back with $b: $s"
done
s=$(run early $((over + 60)) "${START}$((over + 10)) tap A\n")
[ "$(field "$s" st)" = 3 ] && ok "not a tap before the panel's lock" || ko "early tap: $s"
s=$(run raw-over $((over + 60)) "${START}$((over + 40)) tap-at 50%% 50%%\n")
[ "$(field "$s" st)" = 3 ] && ok "a raw tap does nothing on the panel" || ko "raw tap on the panel: $s"

# every width: the same game
ref=$(run w320 400 "${START}130 LEFT+A\n200 RIGHT\n300 -\n" 320)
for w in $WS 560; do
    s=$(run "w$w" 400 "${START}130 LEFT+A\n200 RIGHT\n300 -\n" "$w")
    if [ "$(field "$s" hash)" = "$(field "$ref" hash)" ] && [ "$(field "$s" x)" = "$(field "$ref" x)" ]; then
        ok "width $w: the same run as at 320"
    else ko "width $w: $s / $ref"; fi
done
exit $fail

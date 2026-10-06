#!/bin/sh
# Duck Parade touch tests (DESIGN.md "Controls": rs_game.touch_native): scripted fingers through the headless runner.
# A tap starts the title (Mother then waits for her first target) and leaves the game-over panel (after its lock, like
# A); in play a finger sets Mother's target and she hops towards it with the pad's own hops (the same run as the pad's
# presses, hop for hop), along the direct path (water on the way: she falls in); a finger down on Mother stops her;
# while paused the fingers do nothing, not even later; the target is kept in save states. The coordinates are stage
# coordinates (@N: the stage origin stays 0, the rules are those of 320 columns), so the tests run at every width.
#   touch_test.sh <headless binary> <tmp dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/duckparade/LICENSE.
H=$1
T=${2:-/tmp}/duckparade-touch
mkdir -p "$T"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() {   # check <name> <expected regexp> <output>
    if echo "$3" | grep -q "$2"; then ok "$1"; else ko "$1: expected '$2' in: $(echo "$3" | grep 'touch\|state:' | cut -c1-240)"; fi
}
run() { $H --opt music=0 --opt dump=1 "$@" 2>&1; }
num() { echo "$2" | sed -n "s/.*state: .* $1=\([0-9a-f]*\).*/\1/p" | tail -1; }   # the last state line (a restart dumps one)
script() { f="$T/$1.input"; shift; printf "$@" > "$f"; echo "$f"; }
# Mother starts in column 6, row 7: her cell is stage x 96..111, screen y 128..143 (row r: y 16 + 16 r)
START='30 tap-at @160 120\n'

# the title: a tap starts (the finger's release); Mother does not hop yet; a finger only held does not start
out=$(run --frames 40 --input "$(script title "$START")")
check "a tap on the title starts the run (no hop: Mother waits for a target)" "state: st=1 .*lanes=0 .*cols=6," "$out"
out=$(run --frames 90 --input "$(script title_hold '30 touch @160 120\n')")
check "a finger only held on the title does not start" "state: st=0 .*runs=0 " "$out"

# a target: the pad's own hops, one at a time, each starting on the landing frame (the run is the pad's, hop for hop)
same() {    # same <name> <touch lines> <pad lines>: seed 3, 300 frames, the same state (hash) as with the pad
    a=$(run --frames 300 --opt seed=3 --input "$(script "$1-t" "$START$2")")
    b=$(run --frames 300 --opt seed=3 --input "$(script "$1-p" "$START$3")")
    ha=$(num hash "$a"); hb=$(num hash "$b")
    if [ -n "$ha" ] && [ "$ha" = "$hb" ]; then
        ok "$1 (Mother in column $(echo "$a" | sed -n 's/.*cols=\([0-9]*\).*/\1/p'))"
    else ko "$1: touch '$(echo "$a" | grep 'touch\|state:' | cut -c1-200)' vs pad '$(echo "$b" | grep state: | cut -c1-200)'"; fi
}
same "a tap 4 cells ahead: 4 hops forward across the road, as Right x4" '60 tap-at @168 136\n' \
     '60 tap RIGHT\n72 tap RIGHT\n84 tap RIGHT\n96 tap RIGHT\n'
same "a tap up and ahead: the axis with more distance left first (Up, Up, Right, Up)" '60 tap-at @120 88\n' \
     '60 tap UP\n72 tap UP\n84 tap RIGHT\n96 tap UP\n'
out=$(run --frames 300 --opt seed=3 --input "$(script arrive "$START"'60 tap-at @168 136\n')")
check "... she stops on the target" "touch arrived at frame 108 (Mother at col 10)" "$out"

# the direct path: nothing is avoided (seed 1: a river in column 8, a tap beyond it: she hops in and is swept away)
out=$(run --frames 400 --opt seed=1 --input "$(script water "$START"'60 tap-at @168 136\n')")
check "a tap across the water: she hops straight in and is swept away" "player 1 swept away at frame 84" "$out"
check "... game over" "state: st=4 .*runs=1 " "$out"

# a finger down on Mother stops her (a target straight up: she hops up from frame 60; at frame 85 she has just left
# row 5 for row 4: the tap at her place there stops her after that hop)
cam=$(num cam "$(run --frames 85 --opt seed=3 --input "$(script stop0 "$START"'60 tap-at @104 24\n')")")
out=$(run --frames 300 --opt seed=3 --input "$(script stop "$START"'60 tap-at @104 24\n'"85 tap-at @$((104 - cam)) 104\n")")
check "a tap on Mother stops her" "touch stop at frame 85" "$out"
same "... after the hop she was making (Up x3)" '60 tap-at @104 24\n'"85 tap-at @$((104 - cam)) 104\n" \
     '60 tap UP\n72 tap UP\n84 tap UP\n'
same "a pad press cancels the target (the pad takes over: Right, then Up buffered)" '60 tap-at @168 136\n70 tap UP\n' \
     '60 tap RIGHT\n70 tap UP\n'

# dragging: the target follows the finger
out=$(run --frames 200 --opt seed=3 --input "$(script drag "$START"'60 touch @104 24\n62 move @140 40\n64 move @168 136\n70 up\n')")
check "a finger dragged: the target follows it" "touch target col 10 row 7 at frame 64" "$out"

# pause: the fingers do nothing while paused, not even later (the same run as without them)
base=$(run --frames 400 --opt seed=3 --input "$(script pause0 "$START"'60 tap-at @168 136\n66 tap START\n150 tap START\n')")
p1=$(run --frames 400 --opt seed=3 --input "$(script pause1 "$START"'60 tap-at @168 136\n66 tap START\n100 tap-at @104 24\n150 tap START\n')")
p2=$(run --frames 400 --opt seed=3 --input "$(script pause2 "$START"'60 tap-at @168 136\n66 tap START\n100 touch @104 24\n150 tap START\n170 move @104 200\n200 up\n')")
if [ -n "$(num hash "$base")" ] && [ "$(num hash "$base")" = "$(num hash "$p1")" ] && [ "$(num hash "$base")" = "$(num hash "$p2")" ]; then
    ok "paused: a tap, or a finger held and moved after the pause, does nothing (column $(echo "$base" | sed -n 's/.*cols=\([0-9]*\).*/\1/p'))"
else ko "pause: $(num hash "$base") / $(num hash "$p1") / $(num hash "$p2")"; fi

# the target is in the save state: a width change mid-way (a state saved, the game restarted, the state loaded) goes on
w=${RS_WIDTH:-320}; [ "$w" = 320 ] && w2=427 || w2=320
a=$(run --frames 300 --opt seed=3 --input "$(script state "$START"'60 tap-at @200 200\n')")
b=$(run --frames 300 --opt seed=3 --input "$T/state.input" --resize 70:$w2)
if [ -n "$(num hash "$a")" ] && [ "$(num hash "$a")" = "$(num hash "$b")" ] && echo "$b" | grep -q "touch arrived"; then
    ok "the target survives a save state (width $w to $w2 at frame 70: the same run)"
else ko "state: $(num hash "$a") vs $(num hash "$b")"; fi

# the game-over panel: a tap goes back to the title, after the panel's lock (as A); a finger down during it does not
over=$(run --frames 400 --opt seed=1 --input "$T/water.input")
f=$(echo "$over" | sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p')
if [ -n "$f" ]; then
    out=$(run --frames $((f + 70)) --opt seed=1 --input "$(script over "$START"'60 tap-at @168 136\n'"$((f + 40)) tap-at @160 120\n")")
    check "a tap on the game-over panel returns to the title" "state: st=0 .*lanes=0 .*runs=1 " "$out"
    out=$(run --frames $((f + 70)) --opt seed=1 --input "$(script over_lock "$START"'60 tap-at @168 136\n'"$((f + 10)) tap-at @160 120\n$((f + 20)) touch @160 120\n$((f + 50)) up\n")")
    check "... not during the lock, nor a finger that went down during it" "state: st=4 .*runs=1 " "$out"
    out=$(run --frames $((f + 120)) --opt seed=1 --input "$(script over_again "$START"'60 tap-at @168 136\n'"$((f + 40)) tap-at @160 120\n$((f + 80)) tap-at @160 120\n")")
    check "... and a tap there starts the next run" "state: st=1 .*runs=1 " "$out"
else ko "no game over to test the panel with"; fi
exit $fail

#!/bin/sh
# Nutty Branch: the touch controls (rs_game.touch_native; DESIGN.md "Controls"), scripted touches through the headless
# runner: a tap starts on the title, a held half of the screen steers that way (the same run as the pad's Left /
# Right), a swipe up jumps (held: the high jump, lifted at once: the hop), nothing acts while paused (nor is buffered),
# a tap on the game-over panel goes back to the title. Coordinates in % (or @ stage x): the same at every width
# (RS_WIDTH).
#   touch_test.sh <headless binary> <tmp dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE.
H=$1
T=${2:-/tmp}/nuttybranch-touch
mkdir -p "$T"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() {   # check <name> <expected regexp> <output>
    if echo "$3" | grep -q "$2"; then ok "$1"; else ko "$1: expected '$2' in: $(echo "$3" | grep 'state:\|landed\|out at')"; fi
}
same() {    # same <name> <a> <b>: the same lines (non-empty)
    if [ -n "$2" ] && [ "$2" = "$3" ]; then ok "$1"; else ko "$1: '$2' vs '$3'"; fi
}
run() { $H --opt music=0 --opt dump=1 --opt jumplog=1 "$@" 2>&1; }
outs() { grep "state:\|out at\|landed"; }

# the title: a tap starts the run at once (released on frame 30: the same run as A pressed on frame 30)
printf "27 tap-at 50%% 60%%\n" > "$T/tap.input"
out=$(run --frames 60 --input "$T/tap.input")
check "a tap on the title starts the run" "state: st=1 .*s=[1-9]" "$out"
printf "30 tap A\n" > "$T/a.input"
same "... on the same frame as A would" "$(echo "$out" | outs)" "$(run --frames 60 --input "$T/a.input" | outs)"
printf "27 touch 50%% 60%%\n" > "$T/hold.input"
out=$(run --frames 120 --input "$T/hold.input")
check "a finger held on the title (not a tap) does not start" "state: st=0 " "$out"

# steering: a finger held on a half = Left / Right held (the same fall, frame and angle, as the pad's)
printf "27 tap-at 50%% 60%%\n40 touch 25%% 60%%\n" > "$T/left.input"
printf "30 tap A\n40 LEFT\n" > "$T/padleft.input"
l=$(run --frames 300 --input "$T/left.input" --opt seed=3 | outs)
same "holding the left half steers left (the pad's Left: $(echo "$l" | sed -n 's/.*\(angle -*[0-9]*\).*/\1/p'))" \
    "$l" "$(run --frames 300 --input "$T/padleft.input" --opt seed=3 | outs)"
printf "27 tap-at 50%% 60%%\n40 touch 80%% 30%%\n" > "$T/right.input"
printf "30 tap A\n40 RIGHT\n" > "$T/padright.input"
r=$(run --frames 300 --input "$T/right.input" --opt seed=3 | outs)
same "holding the right half steers right (the pad's Right: $(echo "$r" | sed -n 's/.*\(angle -*[0-9]*\).*/\1/p'))" \
    "$r" "$(run --frames 300 --input "$T/padright.input" --opt seed=3 | outs)"
if echo "$l" | grep -q "angle -[1-9]" && echo "$r" | grep -q "angle [1-9]"; then ok "left and right fall on opposite sides"
else ko "left / right: '$l' / '$r'"; fi
printf "27 tap-at 50%% 60%%\n40 T1 touch 20%% 60%%\n40 T2 touch 80%% 60%%\n" > "$T/both.input"
printf "30 tap A\n" > "$T/none.input"
same "both halves held: no steer (Left + Right)" "$(run --frames 300 --input "$T/both.input" --opt seed=3 | outs)" \
    "$(run --frames 300 --input "$T/none.input" --opt seed=3 | outs)"

# the jump: a swipe up presses A; held while the finger stays down (the high jump, as A held), lifted at once: a hop
printf "27 tap-at 50%% 60%%\n60 touch @160 170\n62 move @160 140\n140 up\n" > "$T/jhold.input"
printf "30 tap A\n62 A\n140 -\n" > "$T/padjhold.input"
hi=$(run --frames 200 --input "$T/jhold.input" --opt seed=4 | grep "landed" | head -1)
check "a swipe up jumps, held while the finger stays down" "landed at frame 104: 43 frames in the air, held 14" "$hi"
same "... the pad's held A from the same frame" "$hi" \
    "$(run --frames 200 --input "$T/padjhold.input" --opt seed=4 | grep "landed" | head -1)"
printf "27 tap-at 50%% 60%%\n60 swipe @160 170 0 -40\n" > "$T/jquick.input"
lo=$(run --frames 200 --input "$T/jquick.input" --opt seed=4 | grep "landed" | head -1)
ah=$(echo "$hi" | sed -n 's/.*: \([0-9]*\) frames in the air.*/\1/p')
al=$(echo "$lo" | sed -n 's/.*: \([0-9]*\) frames in the air, held \([0-9]*\).*/\1 \2/p')
if [ -n "$ah" ] && [ -n "$al" ] && [ "${al% *}" -lt "$ah" ] && [ "${al#* }" -lt 14 ]; then
    ok "a quick swipe (lifted at once) is the lower hop: ${al% *} frames in the air (held ${al#* }) vs $ah"
else ko "quick swipe: '$lo' vs '$hi'"; fi
printf "27 tap-at 50%% 60%%\n60 swipe @160 170 0 40\n61 T2 swipe @160 120 40 0\n" > "$T/jother.input"
out=$(run --frames 200 --input "$T/jother.input" --opt seed=4)
if echo "$out" | grep -q "landed"; then ko "a swipe down / sideways jumped"; else ok "a swipe down or sideways does not jump"; fi

# paused (the app's pause button presses Start): touches do nothing, and nothing is kept for after the pause
printf "30 tap A\n50 tap START\n60 swipe @160 170 0 -40\n70 touch 20%% 60%%\n90 up\n100 tap START\n" > "$T/pause.input"
printf "30 tap A\n50 tap START\n100 tap START\n" > "$T/padpause.input"
p=$(run --frames 160 --input "$T/pause.input" --opt seed=4 | outs)
same "paused: a swipe up and a held half do nothing (not buffered either)" "$p" \
    "$(run --frames 160 --input "$T/padpause.input" --opt seed=4 | outs)"

# the game over: a tap goes back to the title (not before the panel's lock), and a tap there starts again
out=$(run --frames 4000 --input "$T/tap.input" --opt seed=2)
check "no more touches: the squirrel falls off, game over" "state: st=3 .*runs=1 " "$out"
f=$(echo "$out" | sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p')
printf "27 tap-at 50%% 60%%\n$((f + 37)) tap-at 30%% 70%%\n" > "$T/back.input"
out=$(run --frames $((f + 50)) --input "$T/back.input" --opt seed=2)
check "a tap on the game-over panel goes back to the title" "state: st=0 .*runs=1 " "$out"
printf "27 tap-at 50%% 60%%\n$((f + 5)) tap-at 30%% 70%%\n" > "$T/early.input"
out=$(run --frames $((f + 30)) --input "$T/early.input" --opt seed=2)
check "... not before the panel's lock (RETRY_LOCK)" "state: st=3 .*runs=1 " "$out"
printf "27 tap-at 50%% 60%%\n$((f + 37)) tap-at 30%% 70%%\n$((f + 60)) tap-at 60%% 40%%\n" > "$T/again.input"
out=$(run --frames $((f + 90)) --input "$T/again.input" --opt seed=2)
check "... and a tap on the title starts the next run" "state: st=1 .*runs=1 .*s=[1-9]" "$out"
rm -f "$T"/*.input
exit $fail

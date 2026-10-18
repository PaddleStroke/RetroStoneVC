#!/bin/sh
# Hamster Havoc: the portrait playfield (a phone held upright, the zoomed screen: DESIGN.md "Tall screen") through the
# headless runner at 1080x2520: the start, the bot's runs (the same twice), the game over, the screens, and a phone
# turned in a run (the app restarts the game from a state at the new size: the other playfield goes back to the title).
#   tall_test.sh <headless binary> <tmp dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/hamsterhavoc/LICENSE.
H=$1
T=${2:-/tmp}/hamsterhavoc-tall
mkdir -p "$T"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if echo "$3" | grep -q "$2"; then ok "$1"; else ko "$1: expected '$2' in: $(echo "$3" | grep 'state:\|tall\|strict')"; fi; }
run() { $H --display 1080x2520 --opt music=0 --opt dump=1 --opt seed=4 "$@" 2>&1; }

out=$(run --frames 120 --opt strict=1)
check "a 1080x2520 phone: the zoomed screen, 440 lines" "tall screen: 320x440" "$out"
if echo "$out" | grep -q "rs strict"; then ko "strict mode: $(echo "$out" | grep 'rs strict')"; else ok "no strict-mode warning"; fi
printf "30 tap A\n" > "$T/start.input"
out=$(run --frames 40 --input "$T/start.input")
check "A starts the portrait playfield: 56 mice" "state: st=1 .*wave=1 phase=1 alive=56 " "$out"
out=$(run --frames 200 --input "$T/start.input")
check "the formation marches from x 92" "state: st=1 .*phase=2 alive=56 fx=[0-9]* " "$out"
[ "$(echo "$out" | sed -n 's/.* fx=\([0-9]*\) .*/\1/p')" != 92 ] || ko "the formation did not move"
a=$(run --frames 3000 --opt bot=1 --opt ready=1 | grep "state:")
b=$(run --frames 3000 --opt bot=1 --opt ready=1 | grep "state:")
if [ -n "$a" ] && [ "$a" = "$b" ]; then ok "the bot's run, the same twice"; else ko "the bot's runs differ"; fi
out=$(run --frames 3000 --opt bot=1 --opt ready=1 --opt botstop=100)
check "one hit: game over" "state: st=3 .*runs=1 " "$out"
for p in 1 2 4; do
    out=$(run --frames 600 --opt bot=$p --opt players=$p --opt ready=1 --png "$T/play-$p.png")
    check "$p player(s) play in portrait" "state: st=1 .*players=$p " "$out"
done
[ -s "$T/play-1.png" ] && ok "the screen renders" || ko "no picture"
# a phone turned in a run: the app restarts from a state at the new size; the other playfield goes back to the title
# (the bot then starts a new run, on the new playfield); another portrait phone goes on with the run
out=$(run --frames 400 --opt bot=1 --opt ready=1 --resize 300:2340x1080)
check "portrait -> landscape in a run: back to the title" "a state of the portrait playfield: back to the title" "$out"
check "... then a run on the landscape playfield" "state: st=1 .*alive=55 fx=52 fy=48 " "$(echo "$out" | grep state: | tail -n 1)"
out=$($H --display 2340x1080 --opt music=0 --opt dump=1 --frames 400 --opt bot=1 --opt ready=1 --resize 300:1080x2340 2>&1)
check "landscape -> portrait in a run: back to the title" "a state of the landscape playfield: back to the title" "$out"
check "... then a run on the portrait playfield" "state: st=1 .*alive=56 fx=92 fy=-44 " "$(echo "$out" | grep state: | tail -n 1)"
out=$(run --frames 400 --opt bot=1 --opt ready=1 --resize 300:1080x2340)
check "portrait -> another portrait phone: the run goes on" "state: st=1 .*phase=2 " "$(echo "$out" | grep state: | tail -n 1)"
if echo "$out" | grep -q "back to the title"; then ko "the run was cut by another portrait phone"; else ok "... not cut"; fi
exit $fail

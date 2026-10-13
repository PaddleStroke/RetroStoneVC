#!/bin/sh
# Hamster Havoc save states: at the title (with players joining), in a run (the formation marching, seeds and crumbs
# in flight, the shelters bitten), the mothership, a 2-player game, 4 players, paused, and on the game-over panel and
# the 4-player results: save, play on, load, play the same frames again: the state and the picture must be the same
# frame by frame, in this process and in a fresh one; bad states are refused (sdk/tests/test_states.c).
#   state_test.sh <hamsterhavoc_test_states> <out dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/hamsterhavoc/LICENSE.
T=$1
O=${2:-/tmp/states}
mkdir -p "$O"
fail=0
scenario() {
    name=$1; n=$2; m=$3; shift 3
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    out=$($T $opts --input "$O/hamsterhavoc-$name.input" --frames "$n" --after "$m" --out "$O/hamsterhavoc-$name" --shot "$O/hamsterhavoc-$name.png" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: saved at frame $n, $m frames replayed the same, bad states refused"
    else echo "  FAIL $name:"; echo "$out" | grep -v "^\[rs\]" | grep "FAIL\|first\|states:"; fail=1; fi
    out=$($T $opts --input "$O/hamsterhavoc-$name.input" --resume "$O/hamsterhavoc-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: a fresh process resumes from it the same"
    else echo "  FAIL $name (fresh process):"; echo "$out" | grep "FAIL\|first"; fail=1; fi
}

printf "150 tap A\n" > "$O/hamsterhavoc-title.input"
scenario title 60 200
# the title's lobby is saved: players join before and after the save, then P1 starts
printf "20 P2 tap A\n50 P3 tap A\n90 P4 tap A\n110 P3 tap B\n150 tap A\n" > "$O/hamsterhavoc-lobby.input"
scenario lobby 60 200
# in a run (the formation marching, seeds and crumbs flying), then the mothership
: > "$O/hamsterhavoc-run.input"
scenario run 700 600 bot=1 ready=1
: > "$O/hamsterhavoc-boss.input"
scenario boss 400 400 bot=1 ready=1 wave=4
# two scripted players moving and firing
printf "20 P2 tap A\n40 tap A\n" > "$O/hamsterhavoc-race.input"
i=140
while [ $i -lt 900 ]; do
    echo "$i A+LEFT" >> "$O/hamsterhavoc-race.input"; echo "$((i + 20)) A+RIGHT" >> "$O/hamsterhavoc-race.input"
    echo "$((i + 7)) P2 A+RIGHT" >> "$O/hamsterhavoc-race.input"; echo "$((i + 31)) P2 A" >> "$O/hamsterhavoc-race.input"
    i=$((i + 43))
done
scenario race 300 400
: > "$O/hamsterhavoc-run4.input"
scenario run4 500 500 bot=4 ready=1
printf "400 tap SELECT\n520 tap SELECT\n" > "$O/hamsterhavoc-paused.input"
scenario paused 450 300 bot=1 ready=1
# the game over: the bot stops dodging from 100 points; saved on the panel (it shows from frame 1126), then back to
# the title and a new run
: > "$O/hamsterhavoc-over.input"
scenario over 1140 400 bot=1 botstop=100 ready=1 botruns=2
# saved on the results (they show from frame 2575; after the game over: the battery save is not part of a state, so a
# NEW BEST shown by the first pass would read BEST after a load), then back to the title and the next run
: > "$O/hamsterhavoc-results4.input"
scenario results4 2590 300 bot=4 botstop=60 ready=1 botruns=2
exit $fail

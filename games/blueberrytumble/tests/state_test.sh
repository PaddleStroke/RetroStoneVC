#!/bin/sh
# Blueberry Tumble save states: at the title, in a run (the bot, deep in the course), a 4-player race, paused, as a
# snowberry, gliding, and on the game-over panel: save, play on, load, play the same frames again: the state and the
# picture must be the same frame by frame, in this process and in a fresh one; bad states are refused
# (sdk/tests/test_states.c).
#   state_test.sh <blueberrytumble_test_states> <out dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/blueberrytumble/LICENSE.
T=$1
O=${2:-/tmp/states}
mkdir -p "$O"
fail=0
scenario() {
    name=$1; n=$2; m=$3; shift 3
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    out=$($T $opts --input "$O/blueberrytumble-$name.input" --frames "$n" --after "$m" --out "$O/blueberrytumble-$name" --shot "$O/blueberrytumble-$name.png" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: saved at frame $n, $m frames replayed the same, bad states refused"
    else echo "  FAIL $name:"; echo "$out" | grep -v "^\[rs\]" | grep "FAIL\|first\|states:"; fail=1; fi
    out=$($T $opts --input "$O/blueberrytumble-$name.input" --resume "$O/blueberrytumble-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: a fresh process resumes from it the same"
    else echo "  FAIL $name (fresh process):"; echo "$out" | grep "FAIL\|first"; fail=1; fi
}

printf "150 tap A\n180 tap A\n" > "$O/blueberrytumble-title.input"
scenario title 60 200
: > "$O/blueberrytumble-run.input"
scenario run 1500 600 bot=1 ready=1 seed=21
: > "$O/blueberrytumble-race.input"
scenario race 400 400 bot=4 players=4 ready=1
printf "400 tap SELECT\n520 tap SELECT\n" > "$O/blueberrytumble-paused.input"
scenario paused 450 300 bot=1 ready=1
: > "$O/blueberrytumble-deep.input"
scenario deep 3000 500 bot=1 ready=1 god=1 seed=3
: > "$O/blueberrytumble-over.input"
scenario over 700 400 bot=1 botstop=30 ready=1 botruns=2
exit $fail

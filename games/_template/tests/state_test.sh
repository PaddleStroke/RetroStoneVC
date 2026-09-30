#!/bin/sh
# @NAME@ save states: at the title, in a run, a 2-player race, paused, and on the game-over panel: save, play on,
# load, play the same frames again: the state and the picture must be the same frame by frame, in this process
# and in a fresh one; bad states are refused (sdk/tests/test_states.c).
#   state_test.sh <@ID@_test_states> <out dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/@ID@/LICENSE.
T=$1
O=${2:-/tmp/states}
mkdir -p "$O"
fail=0
scenario() {
    name=$1; n=$2; m=$3; shift 3
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    out=$($T $opts --input "$O/@ID@-$name.input" --frames "$n" --after "$m" --out "$O/@ID@-$name" --shot "$O/@ID@-$name.png" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: saved at frame $n, $m frames replayed the same, bad states refused"
    else echo "  FAIL $name:"; echo "$out" | grep -v "^\[rs\]" | grep "FAIL\|first\|states:"; fail=1; fi
    out=$($T $opts --input "$O/@ID@-$name.input" --resume "$O/@ID@-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: a fresh process resumes from it the same"
    else echo "  FAIL $name (fresh process):"; echo "$out" | grep "FAIL\|first"; fail=1; fi
}

printf "150 tap A\n180 tap A\n" > "$O/@ID@-title.input"
scenario title 60 200
: > "$O/@ID@-run.input"
scenario run 700 600 bot=1 ready=1
printf "20 P2 tap A\n" > "$O/@ID@-race.input"
scenario race 300 400 bot=2 ready=1
printf "400 tap SELECT\n520 tap SELECT\n" > "$O/@ID@-paused.input"
scenario paused 450 300 bot=1 ready=1
: > "$O/@ID@-over.input"
scenario over 700 400 bot=1 botstop=2 ready=1 botruns=2
exit $fail

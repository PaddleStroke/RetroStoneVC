#!/bin/sh
# Nutty Branch save states: at the title (with players joining), in a run, a 2-player race, 4 players, paused, deep
# in the course (night, fast), on the game-over panel and the 4-player results: save, play on, load, play the same
# frames again: the state and the picture must be the same frame by frame, in this process and in a fresh one; bad
# states are refused (sdk/tests/test_states.c).
#   state_test.sh <nuttybranch_test_states> <out dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE.
T=$1
O=${2:-/tmp/states}
mkdir -p "$O"
fail=0
scenario() {
    name=$1; n=$2; m=$3; shift 3
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    out=$($T $opts --input "$O/nuttybranch-$name.input" --frames "$n" --after "$m" --out "$O/nuttybranch-$name" --shot "$O/nuttybranch-$name.png" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: saved at frame $n, $m frames replayed the same, bad states refused"
    else echo "  FAIL $name:"; echo "$out" | grep -v "^\[rs\]" | grep "FAIL\|first\|states:"; fail=1; fi
    out=$($T $opts --input "$O/nuttybranch-$name.input" --resume "$O/nuttybranch-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: a fresh process resumes from it the same"
    else echo "  FAIL $name (fresh process):"; echo "$out" | grep "FAIL\|first"; fail=1; fi
}

printf "150 tap A\n" > "$O/nuttybranch-title.input"
scenario title 60 200
printf "20 P2 tap A\n50 P3 tap A\n90 P4 tap A\n110 P3 tap B\n150 tap A\n" > "$O/nuttybranch-lobby.input"
scenario lobby 60 200
: > "$O/nuttybranch-run.input"
scenario run 700 600 bot=1 ready=1
printf "20 P2 tap A\n40 tap A\n" > "$O/nuttybranch-race.input"
i=60
while [ $i -lt 900 ]; do
    echo "$i RIGHT" >> "$O/nuttybranch-race.input"; echo "$((i + 5)) -" >> "$O/nuttybranch-race.input"
    echo "$((i + 7)) P2 tap A" >> "$O/nuttybranch-race.input"; echo "$((i + 12)) LEFT" >> "$O/nuttybranch-race.input"
    echo "$((i + 17)) -" >> "$O/nuttybranch-race.input"
    i=$((i + 23))
done
scenario race 300 400
: > "$O/nuttybranch-run4.input"
scenario run4 500 500 bot=4 ready=1
: > "$O/nuttybranch-deep.input"
scenario deep 600 500 bot=1 ready=1 start=1610 seed=5
printf "400 tap SELECT\n520 tap SELECT\n" > "$O/nuttybranch-paused.input"
scenario paused 450 300 bot=1 ready=1
: > "$O/nuttybranch-over.input"
scenario over 1500 400 bot=1 botstop=20 ready=1 botruns=2 seed=3
: > "$O/nuttybranch-results4.input"
scenario results4 1500 300 bot=4 botstop=10 ready=1 botruns=2 seed=3
exit $fail

#!/bin/sh
# Unicorn Hop save states: at the title (with players joining), mid-climb, high up (the sunset, the rainbow), a 2-player
# race, 4 players, paused, and on the game-over panel and the 4-player results: save, play on, load, play the same frames again: the
# state and the picture must be the same frame by frame, in this process and in a fresh one; bad states are refused
# (sdk/tests/test_states.c).
#   state_test.sh <unicornhop_test_states> <out dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/unicornhop/LICENSE.
T=$1
O=${2:-/tmp/states}
mkdir -p "$O"
fail=0
scenario() {
    name=$1; n=$2; m=$3; shift 3
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    out=$($T $opts --input "$O/unicornhop-$name.input" --frames "$n" --after "$m" --out "$O/unicornhop-$name" --shot "$O/unicornhop-$name.png" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: saved at frame $n, $m frames replayed the same, bad states refused"
    else echo "  FAIL $name:"; echo "$out" | grep -v "^\[rs\]" | grep "FAIL\|first\|states:"; fail=1; fi
    out=$($T $opts --input "$O/unicornhop-$name.input" --resume "$O/unicornhop-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: a fresh process resumes from it the same"
    else echo "  FAIL $name (fresh process):"; echo "$out" | grep "FAIL\|first"; fail=1; fi
}

printf "150 tap A\n" > "$O/unicornhop-title.input"
scenario title 60 200
# the title's lobby is saved: players join before and after the save, then P1 starts
printf "20 P2 tap A\n50 P3 tap A\n90 P4 tap A\n110 P3 tap B\n150 tap A\n" > "$O/unicornhop-lobby.input"
scenario lobby 60 200
# mid-climb (saved mid-jump at some point of the 600 frames after), then the sunset sky
: > "$O/unicornhop-run.input"
scenario run 700 600 bot=1 ready=1
: > "$O/unicornhop-high.input"
scenario high 2950 400 bot=1 ready=1 seed=3
printf "20 P2 tap A\n40 tap A\n" > "$O/unicornhop-race.input"
i=60
while [ $i -lt 900 ]; do
    echo "$i tap A" >> "$O/unicornhop-race.input"; echo "$((i + 19)) P2 tap A" >> "$O/unicornhop-race.input"
    i=$((i + 53))
done
scenario race 300 400
: > "$O/unicornhop-run4.input"
scenario run4 500 500 bot=4 ready=1
printf "400 tap SELECT\n520 tap SELECT\n" > "$O/unicornhop-paused.input"
scenario paused 450 300 bot=1 ready=1
: > "$O/unicornhop-over.input"
scenario over 560 400 bot=1 botstop=2 ready=1 botruns=2      # the panel shows from frame 543
# saved on the results (after the game over: the battery save is not part of a state, so a NEW BEST shown
# by the first pass would read BEST after a load), then back to the title and the next run
: > "$O/unicornhop-results4.input"
scenario results4 2330 300 bot=4 botstop=2 ready=1 botruns=2  # the results show from frame 2304
exit $fail

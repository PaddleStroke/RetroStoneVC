#!/bin/sh
# Caterpillar Crawl save states: at the title (with players joining), in a run (crawling, a leaf going down, a treat
# out), a later garden, the butterfly, the arena (2 scripted players, 4 bots, a round's end), paused, and on the
# game-over panel and the results: save, play on, load, play the same frames again: the state and the picture must
# be the same frame by frame, in this process and in a fresh one; bad states are refused (sdk/tests/test_states.c).
#   state_test.sh <caterpillarcrawl_test_states> <out dir> [quick]     (quick: the main scenarios only)
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/caterpillarcrawl/LICENSE.
T=$1
O=${2:-/tmp/states}
QUICK=$3
mkdir -p "$O"
fail=0
scenario() {
    name=$1; n=$2; m=$3; shift 3
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    out=$($T $opts --input "$O/caterpillarcrawl-$name.input" --frames "$n" --after "$m" --out "$O/caterpillarcrawl-$name" --shot "$O/caterpillarcrawl-$name.png" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: saved at frame $n, $m frames replayed the same, bad states refused"
    else echo "  FAIL $name:"; echo "$out" | grep -v "^\[rs\]" | grep "FAIL\|first\|states:"; fail=1; fi
    out=$($T $opts --input "$O/caterpillarcrawl-$name.input" --resume "$O/caterpillarcrawl-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: a fresh process resumes from it the same"
    else echo "  FAIL $name (fresh process):"; echo "$out" | grep "FAIL\|first"; fail=1; fi
}

# the title's lobby is saved: players join before and after the save, then P1 starts
printf "20 P2 tap A\n50 P3 tap A\n90 P4 tap A\n110 P3 tap B\n150 tap A\n" > "$O/caterpillarcrawl-lobby.input"
scenario lobby 60 200
# a run: the bot crawls, eats (the bulges going down), treats come and go
: > "$O/caterpillarcrawl-run.input"
scenario run 700 600 bot=1 ready=1
# 2 scripted players in the arena (turns buffered across the save), then a round's end and the next round
printf "20 P2 tap A\n40 tap A\n" > "$O/caterpillarcrawl-race.input"
i=140
while [ $i -lt 900 ]; do
    echo "$i tap DOWN" >> "$O/caterpillarcrawl-race.input"; echo "$((i + 3)) tap RIGHT" >> "$O/caterpillarcrawl-race.input"
    echo "$((i + 7)) P2 tap UP" >> "$O/caterpillarcrawl-race.input"; echo "$((i + 31)) P2 tap LEFT" >> "$O/caterpillarcrawl-race.input"
    echo "$((i + 20)) tap UP" >> "$O/caterpillarcrawl-race.input"
    i=$((i + 43))
done
scenario race 300 500
[ "$QUICK" = quick ] && exit $fail
printf "150 tap A\n" > "$O/caterpillarcrawl-title.input"
scenario title 60 200
# a later garden (stones), the butterfly (one leaf to go: the cocoon, the flight, GARDEN 2), paused
: > "$O/caterpillarcrawl-garden.input"
scenario garden 500 400 bot=1 ready=1 garden=4
: > "$O/caterpillarcrawl-butterfly.input"
scenario butterfly 120 400 bot=1 ready=1 leaves=11
printf "400 tap SELECT\n520 tap SELECT\n" > "$O/caterpillarcrawl-paused.input"
scenario paused 450 300 bot=1 ready=1
: > "$O/caterpillarcrawl-run4.input"
scenario run4 500 800 bot=4 ready=1
# the game over: the bot crawls into a wall from 8 points; saved on the panel (it shows from frame 1005), then back
# to the title and a new run
: > "$O/caterpillarcrawl-over.input"
scenario over 1020 300 bot=1 botstop=8 ready=1 botruns=2
# saved on the results of a 2-bot match (they show from frame 7159), then back to the title and the next run
: > "$O/caterpillarcrawl-results.input"
scenario results 7170 300 bot=2 ready=1 botruns=2
exit $fail

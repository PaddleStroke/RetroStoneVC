#!/bin/sh
# Pancake Tower save states: at the title, in a run (at several points, the syrup and the ceiling included), paused,
# on the game-over panel, high up (space), and in 2 players: save, play on, load, play the same frames again: the
# state and the picture must be the same frame by frame, in this process and in a fresh one that loads the state
# before its first frame; bad states are refused (sdk/tests/test_states.c).
#   state_test.sh <pancaketower_test_states> <out dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pancaketower/LICENSE.
T=$1
O=${2:-/tmp/states}
mkdir -p "$O"
fail=0
scenario() {
    name=$1; n=$2; m=$3; shift 3
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    [ -f "$O/$name.input" ] || : > "$O/$name.input"
    out=$($T $opts --input "$O/$name.input" --frames "$n" --after "$m" --out "$O/pancaketower-$name" --shot "$O/pancaketower-$name.png" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: saved at frame $n, $m frames replayed the same, bad states refused"
    else echo "  FAIL $name:"; echo "$out" | grep -v "^\[rs\]" | grep "FAIL\|first\|states:"; fail=1; fi
    out=$($T $opts --input "$O/$name.input" --resume "$O/pancaketower-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: a fresh process resumes from it the same"
    else echo "  FAIL $name (fresh process):"; echo "$out" | grep "FAIL\|first"; fail=1; fi
}

# the title (the music plays, the slider slides), then A: ready, A: a drop
printf "150 tap A\n190 tap A\n" > "$O/title.input"
scenario title 60 300
# one player (the bot) at points along a run: early, the first syrup (pancake 5), the ceiling (layer 13), later
rm -f "$O/run.input"
for f in 150 333 517 777 1234 1801; do
    cp /dev/null "$O/run$f.input"
    scenario run$f $f 400 bot=1 ready=1 seed=3 music=1
done
# paused (Start) at the save, resumed after it
printf "400 tap START\n520 tap SELECT\n" > "$O/paused.input"
scenario paused 450 300 bot=1 ready=1 seed=2
# the bot misses at 3 pancakes: the game-over panel, then a new run
scenario over 420 400 bot=1 botstop=3 ready=1 botruns=2 seed=4
# high up: a pre-stacked tower of 110 pancakes (space: the moon, the cow)
scenario space 200 400 bot=1 ready=1 start=110 seed=6
# 2 players: the bots both stack (the syrup splashes), and a human join from ready
scenario versus 600 500 bot=2 players=2 ready=1 seed=5
printf "20 P2 tap A\n40 tap A\n47 P2 tap A\n" > "$O/join.input"
scenario join 30 300 ready=1

# a state of another game (if its tests ran) is refused
for f in "$O"/leadysquid-*.state "$O"/bombermole-*.state; do
    [ -f "$f" ] || continue
    if $T --foreign "$f" 2>&1 | grep -q "all passed"; then echo "  ok   a state of $(basename "$f" | cut -d- -f1) is refused"
    else echo "  FAIL a foreign state was not refused"; fail=1; fi
    break
done
exit $fail

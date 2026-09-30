#!/bin/sh
# Duck Parade save states: at the title, in a run (the bot), in a co-op run, paused, on the game-over panel, and at
# pseudo-random frames of long runs: save, play on, load, play the same frames again: the state and the picture must
# be the same frame by frame, in this process and in a fresh one that loads the state before its first frame; bad
# states are refused (sdk/tests/test_states.c).
#   state_test.sh <duckparade_test_states> <out dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/duckparade/LICENSE.
T=$1
O=${2:-/tmp/states}
mkdir -p "$O"
fail=0
scenario() {
    name=$1; n=$2; m=$3; shift 3
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    [ -f "$O/duckparade-$name.input" ] || : > "$O/duckparade-$name.input"
    out=$($T $opts --input "$O/duckparade-$name.input" --frames "$n" --after "$m" --out "$O/duckparade-$name" --shot "$O/duckparade-$name.png" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: saved at frame $n, $m frames replayed the same, bad states refused"
    else echo "  FAIL $name:"; echo "$out" | grep -v "^\[rs\]" | grep "FAIL\|first\|states:"; fail=1; fi
    out=$($T $opts --input "$O/duckparade-$name.input" --resume "$O/duckparade-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: a fresh process resumes from it the same"
    else echo "  FAIL $name (fresh process):"; echo "$out" | grep "FAIL\|first"; fail=1; fi
}

# the title (the music plays), then A and the first hop
printf "150 tap A\n180 tap RIGHT\n" > "$O/duckparade-title.input"
scenario title 60 200
# the bot far into a run: traffic, rivers, ducklings in the line
scenario run 1500 600 bot=1 ready=1 seed=11
# Mother and Father
printf "20 P2 tap A\n" > "$O/duckparade-coop.input"
scenario coop 900 500 bot=2 ready=1 seed=12
# paused at the save (Start), resumed after it
printf "600 tap START\n760 tap START\n" > "$O/duckparade-paused.input"
scenario paused 700 300 bot=1 ready=1 seed=13
# the bot stops at 20 points: the fox, the game-over panel, then a new run
scenario over 1500 500 bot=1 botstop=20 ready=1 botruns=2 seed=14
# pseudo-random points of long runs (the saved state continues exactly like the run that was not interrupted)
r=12345
for k in 1 2 3 4; do
    r=$(( (r * 1103515245 + 12345) % 2147483648 ))
    f=$(( 300 + r % 5000 ))
    s=$(( 20 + k ))
    scenario "random$k" "$f" 300 bot=1 ready=1 seed=$s
done
# a state of another game (if its tests ran) is refused
for f in "$O"/leadysquid-*.state "$O"/bombermole-*.state; do
    [ -f "$f" ] || continue
    if $T --foreign "$f" 2>&1 | grep -q "all passed"; then echo "  ok   another game's state is refused"
    else echo "  FAIL another game's state was not refused"; fail=1; fi
    break
done
exit $fail

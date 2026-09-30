#!/bin/sh
# Duck Parade: the per-frame cost of long, busy runs (make duckparade-bench).
#   bench.sh <headless binary> <duckparade_bench>
# The bot plays a long run once and its presses are recorded (--opt record=1); the run is then replayed from that
# input script: the same game frame for frame (the state hashes are compared), without the bot's own thinking,
# which is not the game's cost. The headless runner's bench (--bench) gives its view (the host's noise included);
# duckparade_bench renders each frame three times and keeps the cheapest: the frame's own cost, with percentiles.
# The music and the sounds play. The busiest frames: many lanes of traffic, trains, the parades, the judge.
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/duckparade/LICENSE.
H=$1
B=$2
T=${TMPDIR:-/tmp}/dp_bench.$$
mkdir -p "$T"
bench() {   # bench <name> <bots> <frames> <options...>
    name=$1; bots=$2; n=$3; shift 3
    $H --opt dump=1 --opt bot="$bots" --opt record=1 --frames "$n" "$@" > "$T/rec.txt" 2>&1
    sed -n 's/.*press \([0-9]*\) \(P[12]\) \([A-Z]*\).*/\1 \2 tap \3/p' "$T/rec.txt" > "$T/input.txt"
    a=$(grep "state:" "$T/rec.txt" | sed 's/.*hash=\([0-9a-f]*\).*/\1/')
    $H --opt dump=1 --input "$T/input.txt" --frames "$n" --bench 60 "$@" > "$T/bench.txt" 2>&1
    b=$(grep "state:" "$T/bench.txt" | sed 's/.*hash=\([0-9a-f]*\).*/\1/')
    res=$(grep 'state:' "$T/bench.txt" | sed 's/.* score=\([0-9]*\) score2=\([0-9]*\) lanes=\([0-9]*\).*/score \1 (player 2: \2), lanes \3/')
    echo "== $name: $res (replay $( [ "$a" = "$b" ] && echo 'identical' || echo "DIFFERS: $a vs $b"))"
    grep -A6 "^bench:" "$T/bench.txt" | sed 's/^/   /'
    [ -n "$B" ] && echo "   robust: $($B --input "$T/input.txt" --frames "$n" "$@")"
}
bench "Mother Duck, a long run (seed 7)" 1 20000 --opt ready=1 --opt seed=7
bench "Mother and Father (seed 6)" 2 12000 --opt ready=1 --opt seed=6 --opt players=2
rm -rf "$T"

#!/bin/sh
# Beaver Rush save states: at the title, in a run (the bot), in versus, paused, on the game-over panel, on a
# winter night with the dam half built, and at random points of a long run: save, play on, load, play the same
# frames again: the state and the picture must be the same frame by frame, in this process and in a fresh one
# that loads the state before its first frame; bad states are refused (sdk/tests/test_states.c).
#   state_test.sh <beaverrush_test_states> <out dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/beaverrush/LICENSE.
T=$1
O=${2:-/tmp/states}
mkdir -p "$O"
fail=0
scenario() {
    name=$1; n=$2; m=$3; shift 3
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    [ -f "$O/br-$name.input" ] || : > "$O/br-$name.input"
    out=$($T $opts --input "$O/br-$name.input" --frames "$n" --after "$m" --out "$O/beaverrush-$name" --shot "$O/beaverrush-$name.png" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: saved at frame $n, $m frames replayed the same, bad states refused"
    else echo "  FAIL $name:"; echo "$out" | grep -v "^\[rs\]" | grep "FAIL\|first\|states:"; fail=1; fi
    out=$($T $opts --input "$O/br-$name.input" --resume "$O/beaverrush-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: a fresh process resumes from it the same"
    else echo "  FAIL $name (fresh process):"; echo "$out" | grep "FAIL\|first"; fail=1; fi
}

# the title (the music plays), then a gnaw starts the run
printf "150 tap A\n" > "$O/br-title.input"
scenario title 60 200

# one player (the test bot) far into a run: logs in flight, floating to the dam, the woodpecker
rm -f "$O/br-run1p.input"
scenario run1p 700 600 bot=1 ready=1

# versus from the pads: player 2 joins, both gnaw from the script
cat > "$O/br-vs.input" <<'EOF'
20 P2 tap A
40 tap RIGHT
EOF
i=60
while [ $i -lt 900 ]; do
    echo "$i tap LEFT" >> "$O/br-vs.input"
    echo "$((i + 5)) P2 tap B" >> "$O/br-vs.input"
    i=$((i + 13))
done
scenario vs 300 400 seed=4

# two bots in versus (stolen branches after 50 logs)
rm -f "$O/br-vs2.input"
scenario vs2 1000 500 bot=2 players=2

# paused (Start) at the save, resumed after it
printf "400 tap START\n520 tap START\n" > "$O/br-paused.input"
scenario paused 450 300 bot=1 ready=1

# the bot stops at 3 logs: the end of the run, the panel, then a new run
rm -f "$O/br-over.input"
scenario over 520 500 bot=1 botstop=3 ready=1 botruns=2

# a winter night, the dam half built, snow
rm -f "$O/br-winter.input"
scenario winter 400 400 bot=1 ready=1 skip=560

# random points of a long run: a state saved there continues exactly as the run that was never interrupted
for n in 137 611 1453 2291 3317; do
    rm -f "$O/br-at$n.input"
    scenario at$n $n 240 bot=1 ready=1 seed=$((n % 7 + 1))
done

# a state of another game (if its tests ran) is refused
for f in "$O"/leadysquid-*.state "$O"/bombermole-*.state; do
    [ -f "$f" ] || continue
    if $T --foreign "$f" 2>&1 | grep -q "all passed"; then echo "  ok   a state of another game is refused"
    else echo "  FAIL a state of another game was not refused"; fail=1; fi
    break
done
exit $fail

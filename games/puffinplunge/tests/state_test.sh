#!/bin/sh
# Puffin Plunge save states: on the title, a dive (the bot: critters, shots, gems, the effects), the level's shop, a side
# room's gift, paused, a deep zone, the boss, the results: save, play on, load, play the same frames again: the state and
# the picture must be the same frame by frame, in this process and in a fresh one; bad states are refused
# (sdk/tests/test_states.c).
#   state_test.sh <puffinplunge_test_states> <out dir> [quick]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/puffinplunge/LICENSE.
T=$1
O=${2:-/tmp/states}
Q=$3
mkdir -p "$O"
fail=0
scenario() {
    name=$1; n=$2; m=$3; shift 3
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    out=$($T $opts --input "$O/puffinplunge-$name.input" --frames "$n" --after "$m" --out "$O/puffinplunge-$name" --shot "$O/puffinplunge-$name.png" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: saved at frame $n, $m frames replayed the same, bad states refused"
    else echo "  FAIL $name:"; echo "$out" | grep -v "^\[rs\]" | grep "FAIL\|first\|states:"; fail=1; fi
    out=$($T $opts --input "$O/puffinplunge-$name.input" --resume "$O/puffinplunge-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: a fresh process resumes from it the same"
    else echo "  FAIL $name (fresh process):"; echo "$out" | grep "FAIL\|first"; fail=1; fi
}

printf "20 tap RIGHT\n100 tap A\n" > "$O/puffinplunge-title.input"
scenario title 60 200 fakerun=1
: > "$O/puffinplunge-run.input"
scenario run 500 400 bot=1 ready=1 seed=7
printf "10 tap DOWN\n30 tap A\n50 tap DOWN\n60 tap DOWN\n70 tap A\n" > "$O/puffinplunge-shop.input"
scenario shop 20 160 ready=1 seed=7 shop=1 gems=120
printf "250 tap START\n320 tap START\n" > "$O/puffinplunge-paused.input"
scenario paused 280 200 bot=1 ready=1 seed=7
if [ -z "$Q" ]; then
    printf "10 tap A\n30 tap A\n" > "$O/puffinplunge-gift.input"
    scenario gift 20 100 ready=1 seed=7 room=2
    : > "$O/puffinplunge-deep.input"
    scenario deep 400 300 bot=1 ready=1 seed=7 level=10 god=1
    : > "$O/puffinplunge-boss.input"
    scenario boss 1500 400 bot=1 ready=1 seed=7 level=12 god=1
    : > "$O/puffinplunge-over.input"
    scenario over 140 200 ready=1 seed=7 hurtat=100 score=300
fi
exit $fail

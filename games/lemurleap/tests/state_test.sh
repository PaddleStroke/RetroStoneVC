#!/bin/sh
# Lemur Leap save states: on the title (the wardrobe), a climb (the bot: the leaves, the coins, the pests, the seeds),
# paused, a late climb in space, the fall and the results: save, play on, load, play the same frames again: the state
# and the picture must be the same frame by frame, in this process and in a fresh one; bad states are refused
# (sdk/tests/test_states.c).
#   state_test.sh <lemurleap_test_states> <out dir> [quick]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/lemurleap/LICENSE.
T=$1
O=${2:-/tmp/states}
Q=$3
mkdir -p "$O"
fail=0
scenario() {
    name=$1; n=$2; m=$3; shift 3
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    out=$($T $opts --input "$O/lemurleap-$name.input" --frames "$n" --after "$m" --out "$O/lemurleap-$name" --shot "$O/lemurleap-$name.png" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: saved at frame $n, $m frames replayed the same, bad states refused"
    else echo "  FAIL $name:"; echo "$out" | grep -v "^\[rs\]" | grep "FAIL\|first\|states:"; fail=1; fi
    out=$($T $opts --input "$O/lemurleap-$name.input" --resume "$O/lemurleap-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: a fresh process resumes from it the same"
    else echo "  FAIL $name (fresh process):"; echo "$out" | grep "FAIL\|first"; fail=1; fi
}

printf "20 tap RIGHT\n100 tap LEFT\n140 tap X\n" > "$O/lemurleap-title.input"
scenario title 60 200 coins=50
: > "$O/lemurleap-run.input"
scenario run 600 400 bot=1 ready=1 seed=7
printf "250 tap START\n320 tap START\n" > "$O/lemurleap-paused.input"
scenario paused 280 200 bot=1 ready=1 seed=7
if [ -z "$Q" ]; then
    : > "$O/lemurleap-space.input"
    scenario space 400 300 bot=1 ready=1 seed=7 height=1000 god=1
    : > "$O/lemurleap-over.input"
    scenario over 60 200 ready=1 fall=1 seed=7
fi
exit $fail

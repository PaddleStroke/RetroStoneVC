#!/bin/sh
# Prickle Patch save states: on the title (the gardeners), in the shed, a run (the bot: the swarm, the shots, the
# gems, the soft layer), a level-up's choice, paused, a late garden (a boss, every tool), the game over: save, play on,
# load, play the same frames again: the state and the picture must be the same frame by frame, in this process and in
# a fresh one; bad states are refused (sdk/tests/test_states.c).
#   state_test.sh <pricklepatch_test_states> <out dir> [quick]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pricklepatch/LICENSE.
T=$1
O=${2:-/tmp/states}
Q=$3
mkdir -p "$O"
fail=0
scenario() {
    name=$1; n=$2; m=$3; shift 3
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    out=$($T $opts --input "$O/pricklepatch-$name.input" --frames "$n" --after "$m" --out "$O/pricklepatch-$name" --shot "$O/pricklepatch-$name.png" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: saved at frame $n, $m frames replayed the same, bad states refused"
    else echo "  FAIL $name:"; echo "$out" | grep -v "^\[rs\]" | grep "FAIL\|first\|states:"; fail=1; fi
    out=$($T $opts --input "$O/pricklepatch-$name.input" --resume "$O/pricklepatch-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: a fresh process resumes from it the same"
    else echo "  FAIL $name (fresh process):"; echo "$out" | grep "FAIL\|first"; fail=1; fi
}

printf "20 tap RIGHT\n100 tap LEFT\n140 tap A\n" > "$O/pricklepatch-title.input"
scenario title 60 200 coins=50
: > "$O/pricklepatch-run.input"
scenario run 600 400 bot=1 ready=1 seed=7
printf "10 tap DOWN\n30 tap A\n" > "$O/pricklepatch-levelup.input"
scenario levelup 20 120 ready=1 seed=7 levelup=1
printf "250 tap START\n320 tap START\n" > "$O/pricklepatch-paused.input"
scenario paused 280 200 bot=1 ready=1 seed=7
if [ -z "$Q" ]; then
    printf "20 tap DOWN\n40 tap A\n60 tap B\n" > "$O/pricklepatch-shed.input"
    scenario shed 30 120 shed=1 coins=200
    : > "$O/pricklepatch-late.input"
    scenario late 400 300 bot=1 ready=1 seed=7 time=600 god=1
    : > "$O/pricklepatch-over.input"
    scenario over 600 300 ready=1 frail=1 seed=7
fi
exit $fail

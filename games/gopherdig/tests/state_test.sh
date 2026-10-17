#!/bin/sh
# Gopher Dig save states: at the title (players joining), in a run (the bot digging: blocks shaking and falling, chains),
# at a checkpoint, a run played by touch, 2 and 4 players in split screen, paused, and on the game-over panel and the
# 4-player results: save, play on, load, play the same frames again: the state and the picture must be the same frame by
# frame, in this process and in a fresh one; bad states are refused (sdk/tests/test_states.c).
#   state_test.sh <gopherdig_test_states> <out dir> [quick]
# quick (the widescreen passes): the title, a run, the checkpoint and the game over.
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/gopherdig/LICENSE.
T=$1
O=${2:-/tmp/states}
Q=$3
mkdir -p "$O"
fail=0
scenario() {
    name=$1; n=$2; m=$3; shift 3
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    out=$($T $opts --input "$O/gopherdig-$name.input" --frames "$n" --after "$m" --out "$O/gopherdig-$name" --shot "$O/gopherdig-$name.png" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: saved at frame $n, $m frames replayed the same, bad states refused"
    else echo "  FAIL $name:"; echo "$out" | grep -v "^\[rs\]" | grep "FAIL\|first\|states:"; fail=1; fi
    out=$($T $opts --input "$O/gopherdig-$name.input" --resume "$O/gopherdig-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: a fresh process resumes from it the same"
    else echo "  FAIL $name (fresh process):"; echo "$out" | grep "FAIL\|first"; fail=1; fi
}

printf "150 tap A\n" > "$O/gopherdig-title.input"
scenario title 60 200
# in a run: the bot digging, groups shaking, falling, popping
: > "$O/gopherdig-run.input"
scenario run 900 400 bot=1 ready=1 seed=7
# the first checkpoint (100 m): saved while the well stands still for the celebration
: > "$O/gopherdig-check.input"
scenario check 940 500 bot=1 ready=1 seed=7
# the game over: the bot gives up from 20 m (its run over at frame 596); saved on the panel, then back to the title and a
# new run (the battery save is not part of a state)
: > "$O/gopherdig-over.input"
scenario over 616 400 bot=1 botstop=20 ready=1 botruns=2 seed=5
[ -n "$Q" ] && exit $fail

printf "20 P2 tap A\n50 P3 tap A\n90 P4 tap A\n110 P3 tap B\n150 tap A\n" > "$O/gopherdig-lobby.input"
scenario lobby 60 200
# played by touch: a finger held on the block under the gopher (drilling again and again), then walking
printf "20 tap-at 50%% 30%%\n90 touch @160 100\n200 up\n230 tap-at @296 120\n300 tap-at @150 220\n" > "$O/gopherdig-touch.input"
scenario touch 150 250
: > "$O/gopherdig-run2.input"
scenario run2 700 400 bot=2 ready=1
: > "$O/gopherdig-run4.input"
scenario run4 700 400 bot=4 ready=1
printf "400 tap SELECT\n520 tap SELECT\n" > "$O/gopherdig-paused.input"
scenario paused 450 300 bot=1 ready=1
# the 4-player results: the bots give up from 10, 30, 50, 70 m (the run over at frame 1159): saved on the panel
: > "$O/gopherdig-results4.input"
scenario results4 1180 300 bot=4 botstop=10 ready=1 botruns=2 seed=5
exit $fail

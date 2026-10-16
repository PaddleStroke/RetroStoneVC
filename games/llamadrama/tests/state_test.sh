#!/bin/sh
# Llama Drama save states: at the title (with players joining), in a run (hornets in their patterns, gobs, bonked
# hornets falling), a busy night sky, 2 players with P1 aiming by touch, 4 players, paused, and on the game-over panel
# and the 4-player results: save, play on, load, play the same frames again: the state and the picture must be the
# same frame by frame, in this process and in a fresh one; bad states are refused (sdk/tests/test_states.c).
#   state_test.sh <llamadrama_test_states> <out dir> [quick]
# quick (the widescreen passes): the title, a run, the busy sky and the game over.
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/llamadrama/LICENSE.
T=$1
O=${2:-/tmp/states}
Q=$3
mkdir -p "$O"
fail=0
scenario() {
    name=$1; n=$2; m=$3; shift 3
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    out=$($T $opts --input "$O/llamadrama-$name.input" --frames "$n" --after "$m" --out "$O/llamadrama-$name" --shot "$O/llamadrama-$name.png" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: saved at frame $n, $m frames replayed the same, bad states refused"
    else echo "  FAIL $name:"; echo "$out" | grep -v "^\[rs\]" | grep "FAIL\|first\|states:"; fail=1; fi
    out=$($T $opts --input "$O/llamadrama-$name.input" --resume "$O/llamadrama-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: a fresh process resumes from it the same"
    else echo "  FAIL $name (fresh process):"; echo "$out" | grep "FAIL\|first"; fail=1; fi
}

printf "150 tap A\n" > "$O/llamadrama-title.input"
scenario title 60 200
# in a run (hornets gliding in, waving, looping, gobs in the air, a bonked hornet falling)
: > "$O/llamadrama-run.input"
scenario run 1300 500 bot=1 ready=1
: > "$O/llamadrama-busy.input"
scenario busy 900 400 bot=1 ready=1 score=900
# the game over: the bot stops at 100 points; saved on the panel, then back to the title and a new run (the battery
# save is not part of a state, so the panel's NEW BEST is not shown on the replay: saved after it)
: > "$O/llamadrama-over.input"
scenario over 1800 400 bot=1 botstop=100 ready=1 botruns=2 seed=4
[ -n "$Q" ] && exit $fail

# the title's lobby is saved: players join before and after the save, then P1 starts
printf "20 P2 tap A\n50 P3 tap A\n90 P4 tap A\n110 P3 tap B\n150 tap A\n" > "$O/llamadrama-lobby.input"
scenario lobby 60 200
# two players: P1 aims by touch (taps and a finger held, moving), P2 on the pad
printf "20 P2 tap A\n40 tap A\n" > "$O/llamadrama-touch.input"
i=100
while [ $i -lt 700 ]; do
    [ $((i % 37)) = 0 ] && echo "$i tap-at @$((200 + (i % 90))) $((40 + (i % 140)))" >> "$O/llamadrama-touch.input"
    [ $((i % 91)) = 5 ] && echo "$i touch @240 100" >> "$O/llamadrama-touch.input"
    [ $((i % 91)) = 8 ] && echo "$i move @250 $((60 + (i % 100)))" >> "$O/llamadrama-touch.input"
    [ $((i % 91)) = 20 ] && echo "$i up" >> "$O/llamadrama-touch.input"
    [ $((i % 43)) = 0 ] && echo "$i P2 UP+A" >> "$O/llamadrama-touch.input"
    [ $((i % 43)) = 20 ] && echo "$i P2 DOWN" >> "$O/llamadrama-touch.input"
    i=$((i + 1))
done
scenario touch 300 300
: > "$O/llamadrama-run4.input"
scenario run4 900 400 bot=4 ready=1 score=300
printf "400 tap SELECT\n520 tap SELECT\n" > "$O/llamadrama-paused.input"
scenario paused 450 300 bot=1 ready=1
: > "$O/llamadrama-results4.input"
scenario results4 2310 300 bot=4 botstop=40 ready=1 botruns=2 seed=4
exit $fail

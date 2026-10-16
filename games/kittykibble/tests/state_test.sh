#!/bin/sh
# Kitty Kibble save states: at the title (with players joining), in a run (the vacuums out, kibble eaten), yarn time,
# another room, a 2-player game, 4 players, paused, a finger swiping, and on the game-over panel and the 4-player
# results: save, play on, load, play the same frames again: the state and the picture must be the same frame by frame,
# in this process and in a fresh one; bad states are refused (sdk/tests/test_states.c).
#   state_test.sh <kittykibble_test_states> <out dir> [quick]
# quick (the widescreen passes): the title, a run, the game over.
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/kittykibble/LICENSE.
T=$1
O=${2:-/tmp/states}
Q=$3
mkdir -p "$O"
fail=0
scenario() {
    name=$1; n=$2; m=$3; shift 3
    opts="--opt sound=0"
    for o in "$@"; do opts="$opts --opt $o"; done
    out=$($T $opts --input "$O/kittykibble-$name.input" --frames "$n" --after "$m" --out "$O/kittykibble-$name" --shot "$O/kittykibble-$name.png" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: saved at frame $n, $m frames replayed the same, bad states refused"
    else echo "  FAIL $name:"; echo "$out" | grep -v "^\[rs\]" | grep "FAIL\|first\|states:"; fail=1; fi
    out=$($T $opts --input "$O/kittykibble-$name.input" --resume "$O/kittykibble-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: a fresh process resumes from it the same"
    else echo "  FAIL $name (fresh process):"; echo "$out" | grep "FAIL\|first"; fail=1; fi
}

printf "150 tap A\n" > "$O/kittykibble-title.input"
scenario title 60 200
# in a run: the vacuums out of the dock, kibble eaten, the fish treat
: > "$O/kittykibble-run.input"
scenario run 900 500 bot=1 ready=1
# the game over: the bot runs into a vacuum from 50 points; saved on the panel (from frame 847), then back to the title and a new run
: > "$O/kittykibble-over.input"
scenario over 860 300 bot=1 botstop=50 ready=1 botruns=2 seed=4
[ -n "$Q" ] && exit $fail
# the title's lobby is saved: players join before and after the save, then P1 starts
printf "20 P2 tap A\n50 P3 tap A\n90 P4 tap A\n110 P3 tap B\n150 tap A\n" > "$O/kittykibble-lobby.input"
scenario lobby 60 200
# yarn time (scared vacuums, a catch), another room, 4 kittens
: > "$O/kittykibble-yarn.input"
scenario yarn 260 300 bot=1 ready=1 seed=3
: > "$O/kittykibble-room.input"
scenario room 400 400 bot=1 ready=1 room=4
: > "$O/kittykibble-run4.input"
scenario run4 500 400 bot=4 botskill=2 ready=1
# two scripted players steering; a finger swiping on through the save
printf "20 P2 tap A\n40 tap A\n" > "$O/kittykibble-race.input"
i=180
while [ $i -lt 900 ]; do
    echo "$i UP" >> "$O/kittykibble-race.input"; echo "$((i + 20)) LEFT" >> "$O/kittykibble-race.input"
    echo "$((i + 7)) P2 DOWN" >> "$O/kittykibble-race.input"; echo "$((i + 31)) P2 RIGHT" >> "$O/kittykibble-race.input"
    i=$((i + 43))
done
scenario race 300 400
printf "40 tap A\n200 touch 50%% 60%%\n204 move 50%% 50%%\n240 move 60%% 50%%\n300 move 60%% 70%%\n320 up\n" \
    > "$O/kittykibble-touch.input"
scenario touch 220 200
printf "400 tap SELECT\n520 tap SELECT\n" > "$O/kittykibble-paused.input"
scenario paused 450 300 bot=1 ready=1
# saved on the 4-player results (from frame 4034), then back to the title and the next run
: > "$O/kittykibble-results4.input"
scenario results4 4050 300 bot=4 botstop=60 ready=1 botruns=2 seed=5
exit $fail

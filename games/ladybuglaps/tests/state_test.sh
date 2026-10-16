#!/bin/sh
# Ladybug Laps save states: at the title (the bots racing behind it, players joining), during the start lights, in a
# race (the bots, bumps, the left-behind rule), a race driven by touch, 4 players on one screen, paused, the results and
# the game-over panel: save, play on, load, play the same frames again: the state and the picture must be the same frame
# by frame, in this process and in a fresh one; bad states are refused (sdk/tests/test_states.c).
#   state_test.sh <ladybuglaps_test_states> <out dir> [quick]
# quick (the widescreen passes): the title, a race and the game over.
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/ladybuglaps/LICENSE.
T=$1
O=${2:-/tmp/states}
Q=$3
mkdir -p "$O"
fail=0
scenario() {
    name=$1; n=$2; m=$3; shift 3
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    out=$($T $opts --input "$O/ladybuglaps-$name.input" --frames "$n" --after "$m" --out "$O/ladybuglaps-$name" --shot "$O/ladybuglaps-$name.png" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: saved at frame $n, $m frames replayed the same, bad states refused"
    else echo "  FAIL $name:"; echo "$out" | grep -v "^\[rs\]" | grep "FAIL\|first\|states:"; fail=1; fi
    out=$($T $opts --input "$O/ladybuglaps-$name.input" --resume "$O/ladybuglaps-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: a fresh process resumes from it the same"
    else echo "  FAIL $name (fresh process):"; echo "$out" | grep "FAIL\|first"; fail=1; fi
}

printf "150 tap A\n" > "$O/ladybuglaps-title.input"
scenario title 60 200
# a race: the player bot among the bots (saved mid-race, the leader pulling away)
: > "$O/ladybuglaps-run.input"
scenario run 1300 400 bot=1 ready=1
# the game over: the player bot does not drive; saved on the results, then the panel
: > "$O/ladybuglaps-over.input"
scenario over 5900 400 bot=1 botout=1 ready=1 seed=3
[ -n "$Q" ] && exit $fail

# the title's lobby is saved: players join before and after the save, then P1 starts
printf "20 P2 tap A\n50 P3 tap A\n90 P4 tap A\n110 P3 tap B\n150 tap A\n" > "$O/ladybuglaps-lobby.input"
scenario lobby 60 200
: > "$O/ladybuglaps-lights.input"
scenario lights 60 200 bot=1 ready=1
# driven by touch: a stick dragged around, BRAKE (the stick's slot and origin are in the state)
printf "20 tap-at 50%% 30%%\n150 touch 60 150\n" > "$O/ladybuglaps-touch.input"
i=151
while [ $i -lt 700 ]; do
    echo "$i move $((60 + (i % 40) - 20)) $((150 + (i % 30) - 15))" >> "$O/ladybuglaps-touch.input"
    i=$((i + 1))
done
echo "500 T2 touch 286 206" >> "$O/ladybuglaps-touch.input"
echo "540 T2 up" >> "$O/ladybuglaps-touch.input"
scenario touch 400 250
: > "$O/ladybuglaps-run4.input"
scenario run4 1500 400 bot=4 ready=1 track=3
printf "400 tap SELECT\n520 tap SELECT\n" > "$O/ladybuglaps-paused.input"
scenario paused 450 300 bot=1 ready=1
: > "$O/ladybuglaps-patio.input"
scenario patio 1500 400 bot=1 ready=1 track=4 seed=5
exit $fail

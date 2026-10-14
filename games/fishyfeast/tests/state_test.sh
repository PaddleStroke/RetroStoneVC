#!/bin/sh
# Fishy Feast save states: at the title (the pond going on, players joining), in a run (bots, plankton, pellets), a fish
# in pieces merging, a big fish zoomed out, a run swum by touch, 4 players in split screen, paused, and on the
# game-over panel and the 4-player results: save, play on, load, play the same frames again: the state and the picture
# must be the same frame by frame, in this process and in a fresh one; bad states are refused (sdk/tests/test_states.c).
#   state_test.sh <fishyfeast_test_states> <out dir> [quick]
# quick (the widescreen passes): the title, a run, the pieces and the game over.
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/fishyfeast/LICENSE.
T=$1
O=${2:-/tmp/states}
Q=$3
mkdir -p "$O"
fail=0
scenario() {
    name=$1; n=$2; m=$3; shift 3
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    out=$($T $opts --input "$O/fishyfeast-$name.input" --frames "$n" --after "$m" --out "$O/fishyfeast-$name" --shot "$O/fishyfeast-$name.png" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: saved at frame $n, $m frames replayed the same, bad states refused"
    else echo "  FAIL $name:"; echo "$out" | grep -v "^\[rs\]" | grep "FAIL\|first\|states:"; fail=1; fi
    out=$($T $opts --input "$O/fishyfeast-$name.input" --resume "$O/fishyfeast-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: a fresh process resumes from it the same"
    else echo "  FAIL $name (fresh process):"; echo "$out" | grep "FAIL\|first"; fail=1; fi
}

printf "150 tap A\n" > "$O/fishyfeast-title.input"
scenario title 60 200
# in a run: the bot grazing among the bots (and splitting, ejecting now and then)
: > "$O/fishyfeast-run.input"
scenario run 1300 400 bot=1 ready=1
# a fish in pieces: split twice, saved while they swim apart, merging after
printf "40 tap A\n41 RIGHT\n60 tap A\n90 tap B\n" > "$O/fishyfeast-pieces.input"
scenario pieces 120 300 ready=1 mass=400
# the game over: the bot gives up from mass 20; saved on the panel, then back to the title and a new run (the
# battery save is not part of a state, so the panel's NEW BEST is not shown on the replay: saved after it)
: > "$O/fishyfeast-over.input"
scenario over 2400 400 bot=1 botstop=20 ready=1 botruns=2
[ -n "$Q" ] && exit $fail

# the title's lobby is saved: players join before and after the save, then P1 starts
printf "20 P2 tap A\n50 P3 tap A\n90 P4 tap A\n110 P3 tap B\n150 tap A\n" > "$O/fishyfeast-lobby.input"
scenario lobby 60 200
: > "$O/fishyfeast-big.input"
scenario big 400 300 bot=1 ready=1 mass=1500
# swum by touch: a stick dragged around, the SPLIT button (the stick's slot and origin are in the state)
printf "20 tap-at 50%% 30%%\n60 touch 60 120\n" > "$O/fishyfeast-touch.input"
i=61
while [ $i -lt 600 ]; do
    echo "$i move $((60 + (i % 40) - 20)) $((120 + (i % 30) - 15))" >> "$O/fishyfeast-touch.input"
    i=$((i + 1))
done
scenario touch 300 250 mass=200
: > "$O/fishyfeast-run4.input"
scenario run4 700 400 bot=4 ready=1 mass=60
printf "400 tap SELECT\n520 tap SELECT\n" > "$O/fishyfeast-paused.input"
scenario paused 450 300 bot=1 ready=1
: > "$O/fishyfeast-results4.input"
scenario results4 3000 300 bot=4 botstop=20 ready=1 botruns=2
exit $fail

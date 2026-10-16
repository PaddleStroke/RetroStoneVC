#!/bin/sh
# Seal Shove save states: at the title (players joining), in a match (the bots, the floe cracking and chunks drifting
# off, a fish), a match played by touch, 4 players, paused, and on the game-over panel and the 4-player results:
# save, play on, load, play the same frames again: the state and the picture must be the same frame by frame, in this
# process and in a fresh one; bad states are refused (sdk/tests/test_states.c).
#   state_test.sh <sealshove_test_states> <out dir> [quick]
# quick (the widescreen passes): the title, a match and the game over.
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/sealshove/LICENSE.
T=$1
O=${2:-/tmp/states}
Q=$3
mkdir -p "$O"
fail=0
scenario() {
    name=$1; n=$2; m=$3; shift 3
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    out=$($T $opts --input "$O/sealshove-$name.input" --frames "$n" --after "$m" --out "$O/sealshove-$name" --shot "$O/sealshove-$name.png" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: saved at frame $n, $m frames replayed the same, bad states refused"
    else echo "  FAIL $name:"; echo "$out" | grep -v "^\[rs\]" | grep "FAIL\|first\|states:"; fail=1; fi
    out=$($T $opts --input "$O/sealshove-$name.input" --resume "$O/sealshove-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: a fresh process resumes from it the same"
    else echo "  FAIL $name (fresh process):"; echo "$out" | grep "FAIL\|first"; fail=1; fi
}

printf "150 tap A\n" > "$O/sealshove-title.input"
scenario title 60 200
# in a match: the bot shoving the bots, the floe cracking (saved while a chunk drifts off)
: > "$O/sealshove-run.input"
scenario run 700 600 bot=1 ready=1 seed=3
# the game over: player 1 walks off every round; saved on the panel, then back to the title and a new match
: > "$O/sealshove-over.input"
scenario over 5560 400 bot=1 botquit=1 ready=1 botruns=2 seed=3
[ -n "$Q" ] && exit $fail

# the title's lobby is saved: players join before and after the save, then P1 starts
printf "20 P2 tap A\n50 P3 tap A\n90 P4 tap A\n110 P3 tap B\n150 tap A\n" > "$O/sealshove-lobby.input"
scenario lobby 60 200
# played by touch: a stick dragged around, the DASH button (the stick's slot and origin are in the state)
printf "20 tap-at 50%% 30%%\n120 touch 60 120\n" > "$O/sealshove-touch.input"
i=121
while [ $i -lt 700 ]; do
    echo "$i move $((60 + (i % 40) - 20)) $((120 + (i % 30) - 15))" >> "$O/sealshove-touch.input"
    [ $((i % 90)) = 0 ] && echo "$i T2 tap-at 284 202" >> "$O/sealshove-touch.input"
    i=$((i + 1))
done
scenario touch 300 300
: > "$O/sealshove-run4.input"
scenario run4 900 500 bot=4 ready=1
printf "400 tap SELECT\n520 tap SELECT\n" > "$O/sealshove-paused.input"
scenario paused 450 300 bot=1 ready=1
: > "$O/sealshove-results4.input"
scenario results4 7070 400 bot=4 ready=1 botruns=2 seed=5
exit $fail

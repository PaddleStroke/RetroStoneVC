#!/bin/sh
# Jelly Jam save states: at the title (players joining), in a duel (the bot against chef 1: pairs falling, chains,
# rain), a duel's banner, the game over, a run played by touch, 4 players, paused, and the 4-player results: save, play
# on, load, play the same frames again: the state and the picture must be the same frame by frame, in this process and
# in a fresh one; bad states are refused (sdk/tests/test_states.c).
#   state_test.sh <jellyjam_test_states> <out dir> [quick]
# quick (the widescreen passes): the title, a duel and the game over.
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/jellyjam/LICENSE.
T=$1
O=${2:-/tmp/states}
Q=$3
mkdir -p "$O"
fail=0
scenario() {
    name=$1; n=$2; m=$3; shift 3
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    out=$($T $opts --input "$O/jellyjam-$name.input" --frames "$n" --after "$m" --out "$O/jellyjam-$name" --shot "$O/jellyjam-$name.png" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: saved at frame $n, $m frames replayed the same, bad states refused"
    else echo "  FAIL $name:"; echo "$out" | grep -v "^\[rs\]" | grep "FAIL\|first\|states:"; fail=1; fi
    out=$($T $opts --input "$O/jellyjam-$name.input" --resume "$O/jellyjam-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: a fresh process resumes from it the same"
    else echo "  FAIL $name (fresh process):"; echo "$out" | grep "FAIL\|first"; fail=1; fi
}

printf "150 tap A\n" > "$O/jellyjam-title.input"
scenario title 60 200
# a duel: the bot against chef 1 (saved while jellies melt and fall)
: > "$O/jellyjam-run.input"
scenario run 1500 400 bot=1 ready=1
# the game over: the bot gives up after 600 frames; saved as its field spills, the panel and back to the title after
: > "$O/jellyjam-over.input"
scenario over 1500 500 bot=1 botstop=600 ready=1 botruns=2
[ -n "$Q" ] && exit $fail

# the title's lobby is saved: players join before and after the save, then P1 starts
printf "20 P2 tap A\n50 P3 tap A\n90 P4 tap A\n110 P3 tap B\n150 tap A\n" > "$O/jellyjam-lobby.input"
scenario lobby 60 200
: > "$O/jellyjam-banner.input"
scenario banner 60 120 ready=1 level=3
# played by touch: drags, taps and swipes (the finger's slot and anchor are in the state)
printf "20 tap-at 50%% 30%%\n" > "$O/jellyjam-touch.input"
i=150
while [ $i -lt 900 ]; do
    echo "$i touch @$((40 + (i * 7) % 80)) 120" >> "$O/jellyjam-touch.input"
    echo "$((i + 3)) move @$((60 + (i * 13) % 90)) 120" >> "$O/jellyjam-touch.input"
    echo "$((i + 6)) up" >> "$O/jellyjam-touch.input"
    echo "$((i + 12)) tap-at @160 120" >> "$O/jellyjam-touch.input"
    echo "$((i + 20)) swipe @160 100 0 40" >> "$O/jellyjam-touch.input"
    i=$((i + 37))
done
scenario touch 400 300
: > "$O/jellyjam-run4.input"
scenario run4 1200 400 bot=4 ready=1
printf "400 tap SELECT\n520 tap SELECT\n" > "$O/jellyjam-paused.input"
scenario paused 450 300 bot=1 ready=1
: > "$O/jellyjam-results4.input"
scenario results4 6000 300 bot=4 botstop=300 ready=1 botruns=2
exit $fail

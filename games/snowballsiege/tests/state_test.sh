#!/bin/sh
# Snowball Siege save states: at the title (with players joining), a turn being aimed, a throw about to carve the
# field (the terrain bitmap and its tiles are in the state), the computer thinking, a slingshot pulled by touch,
# 4 players, paused, the game over and the 4-player results: save, play on, load, play the same frames again: the
# state and the picture must be the same frame by frame, in this process and in a fresh one; bad states are refused
# (sdk/tests/test_states.c).
#   state_test.sh <snowballsiege_test_states> <out dir> [quick]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/snowballsiege/LICENSE.
T=$1
O=${2:-/tmp/states}
Q=$3
mkdir -p "$O"
fail=0
scenario() {
    name=$1; n=$2; m=$3; shift 3
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    out=$($T $opts --input "$O/snowballsiege-$name.input" --frames "$n" --after "$m" --out "$O/snowballsiege-$name" --shot "$O/snowballsiege-$name.png" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: saved at frame $n, $m frames replayed the same, bad states refused"
    else echo "  FAIL $name:"; echo "$out" | grep -v "^\[rs\]" | grep "FAIL\|first\|states:"; fail=1; fi
    out=$($T $opts --input "$O/snowballsiege-$name.input" --resume "$O/snowballsiege-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: a fresh process resumes from it the same"
    else echo "  FAIL $name (fresh process):"; echo "$out" | grep "FAIL\|first"; fail=1; fi
}

printf "150 tap A\n" > "$O/snowballsiege-title.input"
scenario title 60 200
# the title's lobby is saved: players join before and after the save, then P1 starts
printf "20 P2 tap A\n50 P3 tap A\n90 P4 tap A\n110 P3 tap B\n150 tap A\n" > "$O/snowballsiege-lobby.input"
scenario lobby 60 200
# the bot: saved while the computer searches its shot, then several turns with splats carving the field
: > "$O/snowballsiege-run.input"
scenario run 470 700 bot=1 ready=1 seed=5
if [ -z "$Q" ]; then
    # a turn on the pad: aiming, walking, charging; saved mid-charge, the throw carves after the load
    printf "30 tap A\n40 UP\n56 -\n60 RIGHT\n90 -\n100 A\n150 -\n" > "$O/snowballsiege-pad.input"
    scenario pad 120 500 seed=3
    # the slingshot pulled by touch (the fingers' roles are in the state; a load forgets the fingers themselves)
    printf "20 tap-at 50%% 30%%\n60 touch @126 186\n61 move @96 210\n90 move @80 220\n" > "$O/snowballsiege-sling.input"
    scenario sling 75 120 seed=5 ready=0
    : > "$O/snowballsiege-run4.input"
    scenario run4 600 600 bot=4 ready=1 seed=5
    printf "400 tap SELECT\n520 tap SELECT\n" > "$O/snowballsiege-paused.input"
    scenario paused 450 300 bot=1 ready=1 seed=5
    : > "$O/snowballsiege-over.input"
    scenario over 2050 300 bot=1 botstop=1 ready=1 seed=5 botruns=2
    : > "$O/snowballsiege-results4.input"
    scenario results4 2640 300 bot=4 ready=1 seed=5 botruns=2
fi
exit $fail

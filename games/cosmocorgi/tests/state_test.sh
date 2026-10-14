#!/bin/sh
# Cosmo Corgi save states: at the title (with players joining), in a run (robots, shots, capsules, a puppy pod), the
# asteroid belt's walls and turrets, a boss, a 2-player game dragged by touch, 4 players, paused, and on the game-over
# panel and the 4-player results: save, play on, load, play the same frames again: the state and the picture must be
# the same frame by frame, in this process and in a fresh one; bad states are refused (sdk/tests/test_states.c).
#   state_test.sh <cosmocorgi_test_states> <out dir> [quick]
# quick (the widescreen passes): the title, a run, the boss and the game over.
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/cosmocorgi/LICENSE.
T=$1
O=${2:-/tmp/states}
Q=$3
mkdir -p "$O"
fail=0
scenario() {
    name=$1; n=$2; m=$3; shift 3
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    out=$($T $opts --input "$O/cosmocorgi-$name.input" --frames "$n" --after "$m" --out "$O/cosmocorgi-$name" --shot "$O/cosmocorgi-$name.png" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: saved at frame $n, $m frames replayed the same, bad states refused"
    else echo "  FAIL $name:"; echo "$out" | grep -v "^\[rs\]" | grep "FAIL\|first\|states:"; fail=1; fi
    out=$($T $opts --input "$O/cosmocorgi-$name.input" --resume "$O/cosmocorgi-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: a fresh process resumes from it the same"
    else echo "  FAIL $name (fresh process):"; echo "$out" | grep "FAIL\|first"; fail=1; fi
}

printf "150 tap A\n" > "$O/cosmocorgi-title.input"
scenario title 60 200
# in a run (robots gliding in and looping, shots, a capsule, a pod), then a boss
: > "$O/cosmocorgi-run.input"
scenario run 1300 500 bot=1 ready=1
: > "$O/cosmocorgi-boss.input"
scenario boss 300 400 bot=1 ready=1 section=3 boss=1 power=3
# the game over: the bot flies into the robots from 100 points; saved on the panel, then back to the title and a new
# run (the battery save is not part of a state, so the panel's NEW BEST is not shown on the replay: saved after it)
: > "$O/cosmocorgi-over.input"
scenario over 1360 400 bot=1 botstop=100 ready=1 botruns=2
[ -n "$Q" ] && exit $fail

# the title's lobby is saved: players join before and after the save, then P1 starts
printf "20 P2 tap A\n50 P3 tap A\n90 P4 tap A\n110 P3 tap B\n150 tap A\n" > "$O/cosmocorgi-lobby.input"
scenario lobby 60 200
: > "$O/cosmocorgi-walls.input"
scenario walls 900 400 bot=1 ready=1 section=1
# two players: P1 dragged by a finger (the drag's last point is in the state), P2 on the pad
printf "20 P2 tap A\n40 tap A\n100 touch 150 150\n" > "$O/cosmocorgi-race.input"
i=101
while [ $i -lt 700 ]; do
    echo "$i move $((150 + (i % 40) - 20)) $((150 - (i % 30)))" >> "$O/cosmocorgi-race.input"
    [ $((i % 43)) = 0 ] && echo "$i P2 UP+RIGHT" >> "$O/cosmocorgi-race.input"
    [ $((i % 43)) = 20 ] && echo "$i P2 DOWN" >> "$O/cosmocorgi-race.input"
    i=$((i + 1))
done
scenario race 300 300
: > "$O/cosmocorgi-run4.input"
scenario run4 700 400 bot=4 ready=1 power=1
printf "400 tap SELECT\n520 tap SELECT\n" > "$O/cosmocorgi-paused.input"
scenario paused 450 300 bot=1 ready=1
: > "$O/cosmocorgi-results4.input"
scenario results4 3010 300 bot=4 botstop=60 ready=1 botruns=2
exit $fail

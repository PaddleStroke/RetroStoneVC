#!/bin/sh
# Gecko Golf save states: at the title (players joining), in a run (the bot aiming, charging, the ball flying, holing
# out, the camera's pan to the next hole), a shot charged and played on the pad, a slingshot pulled on the touch screen,
# a later biome, 4 players taking turns, paused, and on the game-over panel and the 4-player results: save, play on,
# load, play the same frames again: the state and the picture must be the same frame by frame, in this process and in a
# fresh one; bad states are refused (sdk/tests/test_states.c).
#   state_test.sh <geckogolf_test_states> <out dir> [quick]
# quick (the widescreen passes): the title, a run, a pan and the game over.
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/geckogolf/LICENSE.
T=$1
O=${2:-/tmp/states}
Q=$3
mkdir -p "$O"
fail=0
H=${T%_test_states}_headless          # the headless runner: when a run ends
over_at() { $H --frames 30000 --opt music=0 "$@" 2>&1 | sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' | head -1; }
scenario() {
    name=$1; n=$2; m=$3; shift 3
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    out=$($T $opts --input "$O/geckogolf-$name.input" --frames "$n" --after "$m" --out "$O/geckogolf-$name" --shot "$O/geckogolf-$name.png" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: saved at frame $n, $m frames replayed the same, bad states refused"
    else echo "  FAIL $name:"; echo "$out" | grep -v "^\[rs\]" | grep "FAIL\|first\|states:"; fail=1; fi
    out=$($T $opts --input "$O/geckogolf-$name.input" --resume "$O/geckogolf-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: a fresh process resumes from it the same"
    else echo "  FAIL $name (fresh process):"; echo "$out" | grep "FAIL\|first"; fail=1; fi
}

printf "150 tap A\n" > "$O/geckogolf-title.input"
scenario title 60 200
# in a run: the bot aims, shoots, the ball flies and rolls (saved in flight), holes out, the camera pans
: > "$O/geckogolf-run.input"
scenario run 150 500 bot=1 ready=1
# saved during the pan to the next hole (the course's tiles drawn again as the camera moves)
: > "$O/geckogolf-pan.input"
f=$($H --frames 2000 --opt music=0 --opt bot=1 --opt ready=1 --opt seed=4 2>&1 | sed -n 's/.*hole 2 at frame \([0-9]*\).*/\1/p')
scenario pan $((${f:-400} + 30)) 200 bot=1 ready=1 seed=4
# the game over: the bot gives up after its first hole; saved on the panel, then back to the title and a new run (the
# battery save is not part of a state, so the panel's NEW BEST is not shown on the replay: saved after it)
: > "$O/geckogolf-over.input"
f=$(over_at --opt bot=1 --opt botstop=1 --opt strokes=2 --opt ready=1 --opt seed=4)
scenario over $((${f:-700} + 30)) 400 bot=1 botstop=1 strokes=2 ready=1 botruns=2 seed=4
[ -n "$Q" ] && exit $fail

# the title's lobby is saved: players join before and after the save, then P1 starts
printf "20 P2 tap A\n50 P3 tap A\n90 P4 tap A\n110 P3 tap B\n150 tap A\n" > "$O/geckogolf-lobby.input"
scenario lobby 60 200
# on the pad: the aim turned, saved while the gauge fills, let go after the load
printf "20 tap A\n30 LEFT\n60 -\n70 A\n140 -\n" > "$O/geckogolf-charge.input"
scenario charge 100 300
# the slingshot: a finger pulled back (its slot and where it went down are in the state), let go after the load
printf "20 tap-at 50%% 30%%\n60 touch 200 100\n61 move 180 110\n70 move 150 130\n130 up\n" > "$O/geckogolf-touch.input"
scenario touch 100 300
: > "$O/geckogolf-snow.input"
scenario snow 300 400 bot=1 ready=1 hole=14
: > "$O/geckogolf-run4.input"
scenario run4 700 500 bot=4 ready=1
printf "200 tap SELECT\n320 tap SELECT\n" > "$O/geckogolf-paused.input"
scenario paused 250 300 bot=1 ready=1
: > "$O/geckogolf-results4.input"
f=$(over_at --opt bot=4 --opt botstop=1 --opt strokes=2 --opt ready=1 --opt seed=5)
scenario results4 $((${f:-4000} + 30)) 300 bot=4 botstop=1 strokes=2 ready=1 botruns=2 seed=5
exit $fail

#!/bin/sh
# Goat Gorge save states: on the title (choosing a chapter), climbing (the bot: dashes, springs, crumbling blocks,
# platforms), a death and the room again, a room's change (the fade), paused, and on the results: save, play on, load,
# play the same frames again: the state and the picture must be the same frame by frame, in this process and in a
# fresh one; bad states are refused (sdk/tests/test_states.c).
#   state_test.sh <goatgorge_test_states> <out dir> [quick]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/goatgorge/LICENSE.
T=$1
O=${2:-/tmp/states}
Q=$3
mkdir -p "$O"
fail=0
scenario() {
    name=$1; n=$2; m=$3; shift 3
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    out=$($T $opts --input "$O/goatgorge-$name.input" --frames "$n" --after "$m" --out "$O/goatgorge-$name" --shot "$O/goatgorge-$name.png" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: saved at frame $n, $m frames replayed the same, bad states refused"
    else echo "  FAIL $name:"; echo "$out" | grep -v "^\[rs\]" | grep "FAIL\|first\|states:"; fail=1; fi
    out=$($T $opts --input "$O/goatgorge-$name.input" --resume "$O/goatgorge-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: a fresh process resumes from it the same"
    else echo "  FAIL $name (fresh process):"; echo "$out" | grep "FAIL\|first"; fail=1; fi
}

printf "20 tap RIGHT\n100 tap A\n" > "$O/goatgorge-title.input"
scenario title 60 200 unlock=1
: > "$O/goatgorge-run.input"
scenario run 300 400 bot=1 ready=1
printf "0 RIGHT\n" > "$O/goatgorge-death.input"
scenario death 40 200 ready=1 room=4
printf "150 tap START\n220 tap START\n" > "$O/goatgorge-paused.input"
scenario paused 180 200 bot=1 ready=1
if [ -z "$Q" ]; then
    : > "$O/goatgorge-fade.input"
    scenario fade 75 120 bot=1 ready=1
    : > "$O/goatgorge-results.input"
    scenario results 2300 200 bot=1
    has() { grep -q "^$1 2 " games/goatgorge/levels/solutions.txt 2>/dev/null; }
    : > "$O/goatgorge-crumble.input"
    has 2 && scenario crumble 120 400 bot=1 ready=1 chapter=2 unlock=1 room=2
    : > "$O/goatgorge-platforms.input"
    has 3 && scenario platforms 150 400 bot=1 ready=1 chapter=3 unlock=1 room=2
    : > "$O/goatgorge-wind.input"
    has 4 && scenario wind 150 400 bot=1 ready=1 chapter=4 unlock=1 room=2
fi
exit $fail

#!/bin/sh
# Pogo Mamie save states: at the title, in a 1-player run (the bot, each district, at night), in a 2-player race,
# paused, during the fall, on the game-over panel and at random points of a run: save, play on, load, play the
# same frames again: the state and the picture must be the same frame by frame, in this process and in a fresh
# one that loads the state before its first frame; bad states are refused (sdk/tests/test_states.c).
#   state_test.sh <pogomamie_test_states> <out dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pogomamie/LICENSE.
T=$1
O=${2:-/tmp/states}
mkdir -p "$O"
fail=0
scenario() {
    name=$1; n=$2; m=$3; shift 3
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    [ -f "$O/pm-$name.input" ] || : > "$O/pm-$name.input"
    out=$($T $opts --input "$O/pm-$name.input" --frames "$n" --after "$m" --out "$O/pogomamie-$name" --shot "$O/pogomamie-$name.png" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: saved at frame $n, $m frames replayed the same, bad states refused"
    else echo "  FAIL $name:"; echo "$out" | grep -v "^\[rs\]" | grep "FAIL\|first\|states:"; fail=1; fi
    out=$($T $opts --input "$O/pm-$name.input" --resume "$O/pogomamie-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: a fresh process resumes from it the same"
    else echo "  FAIL $name (fresh process):"; echo "$out" | grep "FAIL\|first"; fail=1; fi
}

# (skip=M: a run that starts M metres along never writes the battery save: a best distance written between the save
# and the load would change the game-over panel, NEW BEST / BEST, since the battery save is not part of a state)
# the title (the music plays, the cat taunts), then A, get ready, A: the run starts
printf "150 tap A\n200 tap A\n" > "$O/pm-title.input"
scenario title 60 300

# one player (the bot) in each district and at night
scenario run1p 700 600 bot=1 ready=1 skip=4
scenario seine 900 500 bot=1 ready=1 skip=520 seed=21
scenario haussmann 900 500 bot=1 ready=1 skip=1030 seed=22
scenario eiffel 900 500 bot=1 ready=1 skip=1540 seed=23
scenario night 900 500 bot=1 ready=1 skip=2100 seed=24

# a 2-player race: Papi joins, both steer and hold A from the script
cat > "$O/pm-race.input" <<'EOF'
20 P2 tap A
40 tap A
60 RIGHT
70 P2 RIGHT
EOF
i=100
while [ $i -lt 900 ]; do
    echo "$i RIGHT+A" >> "$O/pm-race.input"
    echo "$((i + 20)) RIGHT" >> "$O/pm-race.input"
    echo "$((i + 9)) P2 RIGHT+A" >> "$O/pm-race.input"
    echo "$((i + 31)) P2 RIGHT" >> "$O/pm-race.input"
    i=$((i + 47))
done
scenario race 300 400 skip=4

# two bots racing
scenario run2p 500 500 bot=2 players=2 ready=1 skip=4

# paused (Start) at the save, resumed after it
printf "400 tap START\n520 tap START\n" > "$O/pm-paused.input"
scenario paused 450 300 bot=1 ready=1 skip=4

# the fall (the bot gives up at 60 m), the café awning, the game-over panel, then a new run (skip: a best distance
# written to the battery save between the save and the load would change the panel: "NEW BEST" / "BEST")
f=$($(dirname "$T")/pogomamie_headless --frames 4000 --opt music=0 --opt bot=1 --opt botstop=60 --opt ready=1 --opt seed=31 \
    --opt skip=8 2>&1 | sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p')
if [ -n "$f" ]; then
    scenario falling $((f - 90)) 200 bot=1 botstop=60 ready=1 seed=31 skip=8
    scenario over $((f + 20)) 400 bot=1 botstop=60 ready=1 seed=31 botruns=2 skip=8
else
    echo "  FAIL no fall to save in"; fail=1
fi

# random points of a long bot run: a state saved there continues exactly like the uninterrupted run
for k in 1 2 3 4; do
    n=$(( (k * 7919 + 1237) % 5000 + 200 ))
    scenario random$k $n 300 bot=1 ready=1 seed=$((40 + k)) skip=4
done

# a state of another game (if its tests ran) is refused
for f in "$O"/leadysquid-*.state "$O"/bombermole-*.state; do
    [ -f "$f" ] || continue
    if $T --foreign "$f" 2>&1 | grep -q "all passed"; then echo "  ok   another game's state is refused ($(basename "$f"))"
    else echo "  FAIL another game's state was not refused"; fail=1; fi
    break
done
printf "20 P2 tap A\n25 P3 tap A\n30 P4 tap A\n40 P3 tap B\n120 tap A\n" > "$O/pm-join4.input"
scenario join4 60 200 seed=5
scenario four 600 400 bot=4 players=4 ready=1 seed=5
scenario four-over 3000 300 bot=4 players=4 ready=1 seed=5 botstop=3
scenario two-over 1200 300 bot=2 players=2 ready=1 seed=5 botstop=3 skip=4
exit $fail

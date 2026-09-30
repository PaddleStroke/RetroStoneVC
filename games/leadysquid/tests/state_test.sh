#!/bin/sh
# Leady Squid save states: at the title, in a 1-player run, a 2-player race, paused, and on the game-over panel,
# save, play on, load, play the same frames again: the state and the picture must be the same frame by frame,
# in this process and in a fresh one that loads the state before its first frame; bad states are refused
# (sdk/tests/test_states.c).
#   state_test.sh <leadysquid_test_states> <out dir>
T=$1
O=${2:-/tmp/states}
mkdir -p "$O"
fail=0
scenario() {
    name=$1; n=$2; m=$3; shift 3
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    out=$($T $opts --input "$O/$name.input" --frames "$n" --after "$m" --out "$O/leadysquid-$name" --shot "$O/leadysquid-$name.png" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: saved at frame $n, $m frames replayed the same, bad states refused"
    else echo "  FAIL $name:"; echo "$out" | grep -v "^\[rs\]" | grep "FAIL\|first\|states:"; fail=1; fi
    out=$($T $opts --input "$O/$name.input" --resume "$O/leadysquid-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: a fresh process resumes from it the same"
    else echo "  FAIL $name (fresh process):"; echo "$out" | grep "FAIL\|first"; fail=1; fi
}

# the title (the music plays), then a flap starts the run
cat > "$O/title.input" <<'EOF'
150 tap A
EOF
scenario title 60 200

# one player (the test bot) far into a run
: > "$O/run1p.input"
scenario run1p 700 600 bot=1 ready=1

# a 2-player race: player 2 joins, both flap from the script
cat > "$O/race.input" <<'EOF'
20 P2 tap A
40 tap A
EOF
i=60
while [ $i -lt 900 ]; do
    echo "$i tap A" >> "$O/race.input"
    echo "$((i + 7)) P2 tap A" >> "$O/race.input"
    i=$((i + 19))
done
scenario race 300 400

# two bots racing
: > "$O/run2p.input"
scenario run2p 500 500 bot=2 players=2 ready=1

# paused (Select) at the save, resumed after it
cat > "$O/paused.input" <<'EOF'
400 tap SELECT
520 tap SELECT
EOF
scenario paused 450 300 bot=1 ready=1

# the bot stops at 2 points: the game-over panel, then a new run
: > "$O/over.input"
scenario over 700 400 bot=1 botstop=2 ready=1 botruns=2

# a state of Bomber Mole (if its tests ran) is refused
for f in "$O"/bombermole-*.state; do
    [ -f "$f" ] || continue
    if $T --foreign "$f" 2>&1 | grep -q "all passed"; then echo "  ok   a Bomber Mole state is refused"
    else echo "  FAIL a Bomber Mole state was not refused"; fail=1; fi
    break
done
exit $fail

#!/bin/sh
# Bomber Mole save states: at several points of the game (menus, a level, bombs and enemies, the pause menu, a
# depth slide, the boss, the lamp at night, co-op split screen, battle), save, play on, load, play the same frames
# again: the state and the picture must be the same frame by frame, in this process and in a fresh one that
# loads the state before its first frame; bad states are refused (sdk/tests/test_states.c).
#   state_test.sh <bombermole_test_states> <out dir>
T=$1
O=${2:-/tmp/states}
mkdir -p "$O"
fail=0
# scenario <name> <frames before the save> <frames after> <options...>; the input script is $O/<name>.input
scenario() {
    name=$1; n=$2; m=$3; shift 3
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    out=$($T $opts --input "$O/$name.input" --frames "$n" --after "$m" --out "$O/bombermole-$name" --shot "$O/bombermole-$name.png" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: saved at frame $n, $m frames replayed the same, bad states refused"
    else echo "  FAIL $name:"; echo "$out" | grep -v "^\[rs\]" | grep "FAIL\|first\|states:"; echo "$out" | grep "state:" | head -5; fail=1; fi
    out=$($T $opts --input "$O/$name.input" --resume "$O/bombermole-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then echo "  ok   $name: a fresh process resumes from it the same"
    else echo "  FAIL $name (fresh process):"; echo "$out" | grep "FAIL\|first\|state:"; fail=1; fi
}

# the title menu: the cursor moves, the options screen opens and closes (the music plays)
cat > "$O/title.input" <<'EOF'
40 tap DOWN
80 tap DOWN
130 tap A
170 tap DOWN
200 tap DOWN
240 tap B
280 tap UP
EOF
scenario title 100 250

# the season and level select (the save lands on the level select)
cat > "$O/select.input" <<'EOF'
30 tap A
70 tap RIGHT
100 tap A
140 tap RIGHT
170 tap DOWN
220 tap LEFT
EOF
scenario select 150 150

# spring 1: the start box, walking, digging, a bomb (its fuse runs through the save)
cat > "$O/level.input" <<'EOF'
30 tap A
60 RIGHT
100 -
105 tap B
110 LEFT
150 -
170 DOWN
200 RIGHT
260 -
265 tap B
270 UP
330 -
EOF
scenario level 210 240 level=spring-1

# spring 5 with the test bot: bombs, blasts and enemies chasing it
: > "$O/bombs.input"
scenario bombs 400 400 level=spring-5 nointro=1 bot=1

# a chain of bombs going off (debug scene), enemies around
: > "$O/chain.input"
scenario chain 50 200 level=spring-5 nointro=1 scene=chain

# the pause menu: saved while paused, then resumed
cat > "$O/pause.input" <<'EOF'
40 RIGHT
70 -
80 tap START
100 tap RIGHT
120 tap LEFT
160 tap B
170 LEFT
230 -
EOF
scenario pause 140 150 level=spring-2 nointro=1

# a depth slide (debug: 30 frames into the level): saved half-way through it
: > "$O/slide.input"
scenario slide 50 90 level=spring-1 nointro=1 transition=1

# a boss (summer 8) with the bot, and a night level (the helmet lamp, raster windows)
: > "$O/boss.input"
scenario boss 500 300 level=summer-8 nointro=1 bot=1
: > "$O/night.input"
scenario night 300 200 level=winter-5 nointro=1 bot=1

# co-op, 2 players, split screen (viewports, cameras, the shared goal)
cat > "$O/coop.input" <<'EOF'
20 P1 RIGHT
60 P1 -
65 P1 tap B
70 P1 LEFT
20 P2 DOWN
50 P2 -
80 P2 RIGHT
120 P2 -
130 P1 DOWN
170 P1 -
EOF
scenario coop 100 200 mp=coop players=2 level=spring-3 nointro=1

# battle: a human and a CPU mid-round, and 4 CPUs later in a round
cat > "$O/battle.input" <<'EOF'
100 P1 RIGHT
140 P1 -
145 P1 tap B
150 P1 LEFT
190 P1 -
300 P1 DOWN
340 P1 -
345 P1 tap B
350 P1 UP
400 P1 -
EOF
scenario battle 320 400 mp=battle players=2 cpus=1
: > "$O/battle4.input"
scenario battle4 900 500 mp=battle players=4 cpus=4 arena=pumpkin-fort

# the battle join screen: P2 joins after the save
cat > "$O/join.input" <<'EOF'
80 P2 tap A
110 P2 tap RIGHT
EOF
scenario join 60 120 screen=join

# a state of Leady Squid (if its tests ran) is refused
for f in "$O"/leadysquid-*.state; do
    [ -f "$f" ] || continue
    if $T --foreign "$f" 2>&1 | grep -q "all passed"; then echo "  ok   a Leady Squid state is refused"
    else echo "  FAIL a Leady Squid state was not refused"; fail=1; fi
    break
done
exit $fail

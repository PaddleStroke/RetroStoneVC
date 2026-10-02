#!/bin/sh
# Pancake Tower smoke tests: scripted runs through the headless runner (flow, drop, cut, miss, pause, retry, save RAM,
# player 2), no strict-mode warning, and determinism (a scripted run and a bot run, twice: same state and picture).
#   smoke_test.sh <headless binary> <tmp dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pancaketower/LICENSE.
H=$1
T=${2:-/tmp}/pancaketower-tests
mkdir -p "$T"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() {   # check <name> <expected regexp> <output>
    if echo "$3" | grep -q "$2"; then ok "$1"; else ko "$1: expected '$2' in: $(echo "$3" | grep 'state:\|strict')"; fi
}
run() { $H --opt music=0 --opt dump=1 "$@" 2>&1; }

out=$(run --frames 300 --opt strict=1)
check "title: waits for A" "state: st=0 .*height=0 score=0 .*runs=0 " "$out"
if echo "$out" | grep -q "rs strict"; then ko "strict mode warnings on the title: $(echo "$out" | grep 'rs strict')"; else ok "no strict-mode warning on the title"; fi
out=$(run --frames 4000 --opt strict=1 --opt bot=1 --opt seed=2)
if echo "$out" | grep -q "rs strict"; then ko "strict mode warnings in a run: $(echo "$out" | grep 'rs strict')"; else ok "no strict-mode warning in a long run (VRAM, sprites, voices, samples)"; fi
out=$(run --frames 3000 --opt strict=1 --opt bot=2 --opt players=2 --opt ready=1 --opt seed=2)
if echo "$out" | grep -q "rs strict"; then ko "strict mode warnings in 2 players: $(echo "$out" | grep 'rs strict')"; else ok "no strict-mode warning in 2 players"; fi
out=$(run --frames 2500 --opt strict=1 --opt bot=4 --opt players=4 --opt ready=1 --opt seed=5 --opt start=30)
if echo "$out" | grep -q "rs strict"; then ko "strict mode warnings in 4 players: $(echo "$out" | grep 'rs strict')"; else ok "no strict-mode warning in 4 players through the roof"; fi
check "four towers grow" "players=4 height=[4-9][0-9]* .*height2=[4-9][0-9]* " "$out"

# A on the title: ready; A again at frame 60 drops the pancake 64 px right of the tower: cut to 32 px
printf "30 tap A\n60 tap A\n" > "$T/start.input"
out=$(run --frames 80 --input "$T/start.input")
check "A on the title: ready, A again drops: the run starts" "state: st=2 .*height=1 " "$out"
out=$(run --frames 3000 --input "$T/start.input")
check "no more presses: the next slider slides on forever (no game over by itself)" "state: st=2 .*height=1 .*ts=0," "$out"

: > "$T/rhythm.input"
echo "30 tap A" >> "$T/rhythm.input"
i=60; while [ $i -lt 1500 ]; do echo "$i tap A" >> "$T/rhythm.input"; i=$((i + 29)); done
out=$(run --frames 1500 --input "$T/rhythm.input")
check "a blind rhythm misses a pancake" "player 1 missed at frame" "$out"
f=$(echo "$out" | sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' | head -1)
out=$(run --frames $((${f:-0} + 10)) --input "$T/rhythm.input")
check "... and the game-over panel shows" "state: st=3 .*runs=1 " "$out"

printf "30 tap A\n60 tap A\n65 tap A\n" > "$T/miss.input"
out=$(run --frames 300 --input "$T/miss.input")
check "a pancake dropped far from the tower: a miss, game over" "state: st=3 .*height=1 .*runs=1 " "$out"
g=$(echo "$out" | sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p')
printf "30 tap A\n60 tap A\n65 tap A\n$((g + 20)) tap A\n$((g + 50)) tap A\n" > "$T/retry.input"
out=$(run --frames $((g + 30)) --input "$T/retry.input")
check "the panel ignores the button for $((36)) frames" "state: st=3 " "$out"
out=$(run --frames $((g + 60)) --input "$T/retry.input")
check "game over: one press returns to the title" "state: st=0 .*height=0 .*runs=1 " "$out"

printf "30 tap A\n60 tap A\n100 tap START\n" > "$T/pause.input"
a=$(run --frames 130 --input "$T/pause.input" | sed -n 's/.*sx=\([-0-9]*\) paused=\([0-9]\).*/\1 \2/p')
b=$(run --frames 400 --input "$T/pause.input" | sed -n 's/.*sx=\([-0-9]*\) paused=\([0-9]\).*/\1 \2/p')
if [ "$a" = "$b" ] && [ "${a#* }" = "1" ]; then ok "Start pauses (the slider is frozen at ${a% *})"; else ko "pause: '$a' vs '$b'"; fi
printf "30 tap A\n60 tap A\n100 tap START\n200 tap SELECT\n" > "$T/pause2.input"
out=$(run --frames 300 --input "$T/pause2.input")
check "Select resumes" "paused=0 " "$out"

rm -f "$T/save.srm"
run --frames 3000 --opt bot=1 --opt seed=3 --opt botstop=12 --sram "$T/save.srm" > "$T/save.log"
best=$(sed -n 's/.*state: .* best=\([0-9]*\) .*/\1/p' "$T/save.log")
out=$(run --frames 10 --sram "$T/save.srm")
if [ "${best:-0}" -gt 12 ]; then check "save RAM: the best score ($best) persists" "best=$best runs=0" "$out"
else ko "the bot run for the save test scored only $best"; fi

printf "20 P2 tap A\n" > "$T/join.input"
out=$(run --frames 60 --input "$T/join.input")
check "player 2 joins on the title" "players=2 " "$out"
out=$(run --frames 2500 --opt bot=2 --opt seed=6 --opt players=2 --opt ready=1)
check "versus: both towers grow" "players=2 height=[1-9][0-9]* score=[0-9]* height2=[1-9][0-9]* " "$out"

# determinism: the same inputs, the same run (state hash and picture), twice
for k in 1 2; do
    run --frames 1500 --input "$T/rhythm.input" --png "$T/det$k.png" | grep "state:" > "$T/det$k.txt"
    run --frames 2500 --opt bot=1 --opt seed=4 --png "$T/bot$k.png" | grep "state:" > "$T/bot$k.txt"
    run --frames 2000 --opt bot=2 --opt players=2 --opt ready=1 --opt seed=5 --png "$T/vs$k.png" | grep "state:" > "$T/vs$k.txt"
done
if cmp -s "$T/det1.txt" "$T/det2.txt" && cmp -s "$T/det1.png" "$T/det2.png"; then ok "determinism: a scripted run twice, same state and picture"
else ko "determinism (script)"; fi
if cmp -s "$T/bot1.txt" "$T/bot2.txt" && cmp -s "$T/bot1.png" "$T/bot2.png"; then ok "determinism: a bot run twice, same state and picture"
else ko "determinism (bot)"; fi
if cmp -s "$T/vs1.txt" "$T/vs2.txt" && cmp -s "$T/vs1.png" "$T/vs2.png"; then ok "determinism: a 2-player run twice, same state and picture"
else ko "determinism (versus)"; fi
exit $fail

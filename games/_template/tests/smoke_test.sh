#!/bin/sh
# @NAME@ smoke tests: scripted runs through the headless runner, the bot hook and determinism.
#   smoke_test.sh <headless binary> <tmp dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/@ID@/LICENSE.
H=$1
T=${2:-/tmp}/@ID@-tests
mkdir -p "$T"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() {   # check <name> <expected regexp> <output>
    if echo "$3" | grep -q "$2"; then ok "$1"; else ko "$1: expected '$2' in: $(echo "$3" | grep 'state:\|strict')"; fi
}
run() { $H --opt music=0 --opt dump=1 "$@" 2>&1; }

out=$(run --frames 300 --opt strict=1)
check "title: nothing happens without input" "state: st=0 .*score=0 .*runs=0 " "$out"
if echo "$out" | grep -q "rs strict"; then ko "strict mode warnings: $(echo "$out" | grep 'rs strict')"; else ok "no strict-mode warning (VRAM, sprites, voices...)"; fi

printf "30 tap A\n60 tap A\n" > "$T/start.input"
out=$(run --frames 80 --input "$T/start.input")
check "A on the title: get ready, A again: the run starts" "state: st=2 " "$out"
out=$(run --frames 900 --input "$T/start.input")
check "no more input: the hero hits a crate, game over" "state: st=4 .*runs=1 " "$out"

printf "30 tap A\n60 tap A\n860 tap A\n" > "$T/retry.input"
out=$(run --frames 880 --input "$T/retry.input")
check "retry: one button back to get ready" "state: st=1 .*runs=1 " "$out"

printf "30 tap A\n60 tap A\n90 tap SELECT\n" > "$T/pause.input"
a=$(run --frames 120 --input "$T/pause.input" | sed -n 's/.*scroll=\([0-9]*\) paused=\([0-9]\).*/\1 \2/p')
b=$(run --frames 300 --input "$T/pause.input" | sed -n 's/.*scroll=\([0-9]*\) paused=\([0-9]\).*/\1 \2/p')
if [ "$a" = "$b" ] && [ "${a#* }" = "1" ]; then ok "Select pauses (the world is frozen: scroll ${a% *})"; else ko "pause: '$a' vs '$b'"; fi

rm -f "$T/save.srm"
run --frames 4000 --opt bot=1 --opt seed=8 --opt botstop=7 --sram "$T/save.srm" > /dev/null
out=$(run --frames 10 --sram "$T/save.srm")
check "save RAM: the best score (7) persists" "best=7 runs=0" "$out"

printf "20 P2 tap A\n" > "$T/join.input"
out=$(run --frames 60 --opt ready=1 --input "$T/join.input")
check "player 2 joins from get ready" "players=2 " "$out"
out=$(run --frames 2500 --opt bot=2 --opt seed=6 --opt players=2)
check "2 players: both heroes run and score" "players=2 score=[1-9][0-9]* score2=[1-9][0-9]* " "$out"

# the bot hook (bot_decide in main.c): it must reach 20 with the default settings
out=$(run --frames 4000 --opt bot=1)
s=$(echo "$out" | sed -n 's/.*state: .* score=\([0-9]*\) .*/\1/p')
if [ "${s:-0}" -ge 20 ]; then ok "the bot reaches $s (>= 20)"; else ko "the bot only reached $s: $(echo "$out" | grep state)"; fi

# determinism: the same inputs, the same run (state hash and picture), twice
: > "$T/rhythm.input"
i=30; while [ $i -lt 1500 ]; do echo "$i tap A" >> "$T/rhythm.input"; i=$((i + 23)); done
for k in 1 2; do
    run --frames 1500 --input "$T/rhythm.input" --opt seed=3 --png "$T/det$k.png" | grep "state:" > "$T/det$k.txt"
    run --frames 2000 --opt bot=1 --png "$T/bot$k.png" | grep "state:" > "$T/bot$k.txt"
done
if cmp -s "$T/det1.txt" "$T/det2.txt" && cmp -s "$T/det1.png" "$T/det2.png"; then ok "determinism: a scripted run twice, same state and picture"
else ko "determinism (script)"; fi
if cmp -s "$T/bot1.txt" "$T/bot2.txt" && cmp -s "$T/bot1.png" "$T/bot2.png"; then ok "determinism: a bot run twice, same state and picture"
else ko "determinism (bot)"; fi
exit $fail

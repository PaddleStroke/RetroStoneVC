#!/bin/sh
# Leady Squid smoke tests: scripted runs through the headless runner, the bot and determinism.
#   smoke_test.sh <headless binary> <tmp dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/leadysquid/LICENSE.
H=$1
T=${2:-/tmp}/leadysquid-tests
mkdir -p "$T"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() {   # check <name> <expected regexp> <output>
    if echo "$3" | grep -q "$2"; then ok "$1"; else ko "$1: expected '$2' in: $(echo "$3" | grep 'state:\|strict')"; fi
}
run() { $H --opt music=0 --opt dump=1 "$@" 2>&1; }

out=$(run --frames 300 --opt strict=1)
check "title: nothing happens without input" "state: st=0 .*score=0 .*scroll=0 " "$out"
if echo "$out" | grep -q "rs strict"; then ko "strict mode warnings: $(echo "$out" | grep 'rs strict')"; else ok "no strict-mode warning (VRAM, sprites, voices...)"; fi

printf "30 tap A\n" > "$T/one.input"
out=$(run --frames 400 --input "$T/one.input")
check "one flap, then nothing: sinks to the seabed, game over with 0" "state: st=4 .*score=0 .*runs=1 " "$out"

: > "$T/rhythm.input"
i=30; while [ $i -lt 1500 ]; do echo "$i tap A" >> "$T/rhythm.input"; i=$((i + 17)); done
out=$(run --frames 1500 --input "$T/rhythm.input" --opt seed=3)
check "a blind rhythm hits an obstacle" "player 1 hit at frame" "$out"
check "... and the panel shows" "state: st=4 " "$out"

printf "30 tap A\n500 tap B\n560 tap START\n" > "$T/retry.input"
out=$(run --frames 540 --input "$T/retry.input")
check "retry: one button back to get ready" "state: st=1 .*runs=1 " "$out"
out=$(run --frames 600 --input "$T/retry.input")
check "... and the next flap swims" "state: st=2 .*runs=1 " "$out"

printf "450 tap SELECT\n" > "$T/pause.input"
a=$(run --frames 500 --opt bot=1 --opt seed=4 --input "$T/pause.input" | sed -n 's/.*scroll=\([0-9]*\).*paused=\([0-9]\).*/\1 \2/p')
b=$(run --frames 700 --opt bot=1 --opt seed=4 --input "$T/pause.input" | sed -n 's/.*scroll=\([0-9]*\).*paused=\([0-9]\).*/\1 \2/p')
if [ "$a" = "$b" ] && [ "${a#* }" = "1" ]; then ok "Select pauses (the world is frozen: scroll ${a% *})"; else ko "pause: '$a' vs '$b'"; fi
printf "450 tap SELECT\n520 tap SELECT\n" > "$T/pause2.input"
out=$(run --frames 700 --opt bot=1 --opt seed=4 --input "$T/pause2.input")
check "Select again resumes" "paused=0 " "$out"

rm -f "$T/save.srm"
run --frames 3000 --opt bot=1 --opt seed=8 --opt botstop=12 --sram "$T/save.srm" > /dev/null
out=$(run --frames 10 --sram "$T/save.srm")
check "save RAM: the best score (12) persists" "best=12 runs=0" "$out"

printf "20 P2 tap A\n" > "$T/join.input"
out=$(run --frames 60 --opt ready=1 --input "$T/join.input")
check "player 2 joins from get ready (race mode)" "players=2 " "$out"
out=$(run --frames 2500 --opt bot=2 --opt seed=6 --opt players=2)
check "race: both squids swim and score" "players=2 score=[1-9][0-9]* score2=[1-9][0-9]* " "$out"

# the bot plays from the screen only (OAM): it must reach 30 with the default settings
out=$(run --frames 5000 --opt bot=1)
s=$(echo "$out" | sed -n 's/.*state: .* score=\([0-9]*\) .*/\1/p')
if [ "${s:-0}" -ge 30 ]; then ok "the bot reaches $s (>= 30) with the default settings"; else ko "the bot only reached $s: $(echo "$out" | grep state)"; fi
all=""
for seed in 1 2 3 4 5 6 7 8 9 10; do
    s=$(run --frames 3000 --opt bot=1 --opt seed=$seed | sed -n 's/.*state: .* score=\([0-9]*\) .*/\1/p')
    all="$all $s"
    [ "${s:-0}" -ge 30 ] || { ko "the bot only reached $s on seed $seed"; }
done
ok "bot scores in 3000 frames (50 s), seeds 1-10:$all"

# determinism: the same inputs, the same run (state hash and picture), twice
for k in 1 2; do
    run --frames 1500 --input "$T/rhythm.input" --opt seed=3 --png "$T/det$k.png" | grep "state:" > "$T/det$k.txt"
    run --frames 2000 --opt bot=1 --png "$T/bot$k.png" | grep "state:" > "$T/bot$k.txt"
done
if cmp -s "$T/det1.txt" "$T/det2.txt" && cmp -s "$T/det1.png" "$T/det2.png"; then ok "determinism: a scripted run twice, same state and picture"
else ko "determinism (script)"; fi
if cmp -s "$T/bot1.txt" "$T/bot2.txt" && cmp -s "$T/bot1.png" "$T/bot2.png"; then ok "determinism: a bot run twice, same state and picture"
else ko "determinism (bot)"; fi
exit $fail

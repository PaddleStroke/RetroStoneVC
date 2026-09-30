#!/bin/sh
# Beaver Rush smoke tests: scripted runs through the headless runner, the bot (10 seeds), versus and determinism.
#   smoke_test.sh <headless binary> <tmp dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/beaverrush/LICENSE.
H=$1
T=${2:-/tmp}/beaverrush-tests
mkdir -p "$T"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() {   # check <name> <expected regexp> <output>
    if echo "$3" | grep -q "$2"; then ok "$1"; else ko "$1: expected '$2' in: $(echo "$3" | grep 'state:\|strict')"; fi
}
run() { $H --opt music=0 --opt dump=1 "$@" 2>&1; }
field() { echo "$2" | sed -n "s/.*state: .* $1=\([-0-9a-f]*\).*/\1/p"; }

out=$(run --frames 300 --opt strict=1)
check "title: nothing happens without input" "state: st=0 .*score=0 .*logs=0 bar=1000 " "$out"
out=$(run --frames 2400 --opt strict=1 --opt bot=1 --opt seed=5 --opt skip=560 --opt music=1)
if echo "$out" | grep -q "rs strict"; then ko "strict mode warnings: $(echo "$out" | grep 'rs strict' | head -3)"
else ok "no strict-mode warning in a winter night run with the music (VRAM, sprites per line, voices...)"; fi
out=$(run --frames 900 --opt strict=1 --opt bot=2 --opt players=2 --opt seed=5 --opt skip=560 --opt music=1)
if echo "$out" | grep -q "rs strict"; then ko "strict mode warnings (versus): $(echo "$out" | grep 'rs strict' | head -3)"
else ok "no strict-mode warning in versus"; fi

# one gnaw, then nothing: the bar runs out (6 s at level 0)
printf "30 tap RIGHT\n" > "$T/one.input"
out=$(run --frames 500 --opt ready=1 --input "$T/one.input")
check "one gnaw, then nothing: out of breath, game over" "state: st=4 .*runs=1 .*logs=1 " "$out"
f=$(echo "$out" | sed -n 's/.*out of breath at frame \([0-9]*\).*/\1/p')
if [ -n "$f" ] && [ "$f" -ge 385 ] && [ "$f" -le 395 ]; then ok "... after 6 s ($((f - 30)) frames)"; else ko "out of breath at frame '$f'"; fi

# always the same side: a branch comes down on the beaver's head
: > "$T/left.input"
i=20; while [ $i -lt 1200 ]; do echo "$i tap LEFT" >> "$T/left.input"; i=$((i + 10)); done
out=$(run --frames 1200 --opt ready=1 --opt seed=3 --input "$T/left.input")
check "gnawing on one side only: bonked by a branch" "player 1 bonked" "$out"
check "... the panel shows" "state: st=4 " "$out"
# B and A gnaw too (B = left, A = right)
printf "30 tap B\n45 tap A\n60 tap A\n" > "$T/ba.input"
out=$(run --frames 80 --opt ready=1 --opt seed=3 --input "$T/ba.input")
check "B and A gnaw (left, right)" "state: st=2 .*side=2 logs=3 " "$out"

# retry: one button back to get ready, the next gnaw plays
printf "30 tap A\n480 tap A\n" > "$T/retry.input"
out=$(run --frames 470 --opt ready=1 --input "$T/retry.input")
check "the panel after the run" "state: st=4 .*runs=1 " "$out"
out=$(run --frames 485 --opt ready=1 --input "$T/retry.input")
check "retry: one press, back to get ready" "state: st=1 .*runs=1 .*logs=0 " "$out"
printf "30 tap A\n480 tap A\n500 tap B\n" > "$T/retry2.input"
out=$(run --frames 520 --opt ready=1 --input "$T/retry2.input")
check "... and the next press gnaws" "state: st=2 .*logs=1 " "$out"

# pause: Start freezes the run; Start again resumes
printf "450 tap START\n" > "$T/pause.input"
a=$(run --frames 500 --opt bot=1 --opt seed=4 --opt ready=1 --input "$T/pause.input" | sed -n 's/.*logs=\([0-9]*\) bar=\([0-9]*\) paused=\([0-9]\).*/\1 \2 \3/p')
b=$(run --frames 700 --opt bot=1 --opt seed=4 --opt ready=1 --input "$T/pause.input" | sed -n 's/.*logs=\([0-9]*\) bar=\([0-9]*\) paused=\([0-9]\).*/\1 \2 \3/p')
if [ "$a" = "$b" ] && [ "${a##* }" = "1" ]; then ok "Start pauses (the run is frozen: logs, bar $a)"; else ko "pause: '$a' vs '$b'"; fi
printf "450 tap START\n520 tap START\n" > "$T/pause2.input"
out=$(run --frames 700 --opt bot=1 --opt seed=4 --opt ready=1 --input "$T/pause2.input")
check "Start again resumes" "paused=0 " "$out"

# save RAM: the best score persists
rm -f "$T/save.srm"
run --frames 3000 --opt bot=1 --opt seed=8 --opt botstop=12 --opt ready=1 --sram "$T/save.srm" > /dev/null
out=$(run --frames 10 --sram "$T/save.srm")
check "save RAM: the best score (12) persists" "best=12 runs=0" "$out"

# versus: player 2 joins; both race; the last beaver standing wins; milestones send branches
printf "20 P2 tap A\n" > "$T/join.input"
out=$(run --frames 60 --opt ready=1 --input "$T/join.input")
check "player 2 joins from get ready (versus)" "players=2 " "$out"
out=$(run --frames 8000 --opt bot=2 --opt players=2 --opt seed=6)
check "versus: both gnaw and a winner is named" "state: st=4 players=2 score=[1-9][0-9]* score2=[1-9][0-9]* .*winner=[01] " "$out"
check "versus: a milestone sends a branch to the rival" "sends a branch to player" "$out"
printf "30 tap A\n" > "$T/vs1.input"
out=$(run --frames 600 --opt players=2 --opt seed=6 --input "$T/vs1.input")
check "versus: the idle rival runs out of breath too, the timers race" "state: st=4 players=2 .*winner=" "$out"

# the music speeds up with the drain (the tempo of the next 8 bars)
out=$(run --frames 3600 --opt bot=1 --opt seed=2 --opt ready=1 --opt music=1)
t=$(field tempo "$out")
if [ "${t:-0}" -ge 2 ]; then ok "the music plays faster at level $(field level "$out") (tempo $t of 0..3)"; else ko "tempo '$t'"; fi

# the bot plays from the screen only (the BG2 map and OAM): over 10 seeds it must average 300 logs or more
all="" sum=0 min=100000 max=0
for seed in 1 2 3 4 5 6 7 8 9 10; do
    out=$(run --frames 20000 --opt bot=1 --opt ready=1 --opt sound=0 --opt seed=$seed)
    s=$(field score "$out")
    if echo "$out" | grep -q "bonked"; then ko "the bot was bonked on seed $seed (an unfair trunk?)"; fi
    all="$all $s"; sum=$((sum + ${s:-0}))
    [ "${s:-0}" -lt $min ] && min=${s:-0}
    [ "${s:-0}" -gt $max ] && max=${s:-0}
done
avg=$((sum / 10))
if [ $avg -ge 300 ]; then ok "the bot averages $avg logs (min $min, max $max) over seeds 1-10:$all"
else ko "the bot only averages $avg:$all"; fi

# determinism: the same inputs, the same run (state hash and picture), twice
for k in 1 2; do
    run --frames 1200 --opt ready=1 --opt seed=3 --input "$T/left.input" --png "$T/det$k.png" | grep "state:" > "$T/det$k.txt"
    run --frames 2500 --opt bot=1 --png "$T/bot$k.png" | grep "state:" > "$T/bot$k.txt"
    run --frames 1500 --opt bot=2 --opt players=2 --png "$T/vs$k.png" | grep "state:" > "$T/vs$k.txt"
done
if cmp -s "$T/det1.txt" "$T/det2.txt" && cmp -s "$T/det1.png" "$T/det2.png"; then ok "determinism: a scripted run twice, same state and picture"
else ko "determinism (script)"; fi
if cmp -s "$T/bot1.txt" "$T/bot2.txt" && cmp -s "$T/bot1.png" "$T/bot2.png"; then ok "determinism: a bot run twice, same state and picture"
else ko "determinism (bot)"; fi
if cmp -s "$T/vs1.txt" "$T/vs2.txt" && cmp -s "$T/vs1.png" "$T/vs2.png"; then ok "determinism: a versus run twice, same state and picture"
else ko "determinism (versus)"; fi
exit $fail

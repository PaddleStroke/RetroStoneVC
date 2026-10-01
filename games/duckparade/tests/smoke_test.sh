#!/bin/sh
# Duck Parade smoke tests: scripted runs through the headless runner, the bot and determinism.
#   smoke_test.sh <headless binary> <tmp dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/duckparade/LICENSE.
H=$1
T=${2:-/tmp}/duckparade-tests
mkdir -p "$T"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() {   # check <name> <expected regexp> <output>
    if echo "$3" | grep -q "$2"; then ok "$1"; else ko "$1: expected '$2' in: $(echo "$3" | grep 'state:\|strict' | cut -c1-240)"; fi
}
run() { $H --opt music=0 --opt dump=1 "$@" 2>&1; }

out=$(run --frames 300 --opt strict=1)
check "title: nothing happens without input" "state: st=0 .*score=0 .*runs=0 " "$out"
if echo "$out" | grep -q "rs strict"; then ko "strict mode warnings: $(echo "$out" | grep 'rs strict')"; else ok "no strict-mode warning on the title (VRAM, sprites, voices...)"; fi

printf "30 tap A\n" > "$T/start.input"
out=$(run --frames 80 --input "$T/start.input")
check "A on the title starts the run (the press is the first hop)" "state: st=2 .*lanes=1 " "$out"
out=$(run --frames 900 --input "$T/start.input")
check "no more hops: the fox comes" "caught by the fox" "$out"
check "... and the game-over panel shows" "state: st=4 .*runs=1 " "$out"

: > "$T/rhythm.input"
echo "30 tap A" >> "$T/rhythm.input"
i=60; while [ $i -lt 2400 ]; do echo "$i tap RIGHT" >> "$T/rhythm.input"; i=$((i + 13)); done
out=$(run --frames 2400 --input "$T/rhythm.input" --opt seed=3)
check "hopping blindly forward: hit, swept away or caught" "hit at frame\|swept away\|caught by" "$out"
check "... game over (an arrow on the panel retries at once)" "runs=[1-9]" "$out"

printf "30 tap A\n800 tap A\n" > "$T/retry.input"
out=$(run --frames 820 --input "$T/retry.input")
check "retry: one press, straight into play (the press is the first hop)" "state: st=2 .*lanes=1 .*runs=1 " "$out"

printf "30 tap A\n120 tap START\n" > "$T/pause.input"
a=$(run --frames 150 --input "$T/pause.input" | sed -n 's/.*cam=\([0-9]*\) paused=\([0-9]\).*/\1 \2/p')
b=$(run --frames 400 --input "$T/pause.input" | sed -n 's/.*cam=\([0-9]*\) paused=\([0-9]\).*/\1 \2/p')
if [ "$a" = "$b" ] && [ "${a#* }" = "1" ]; then ok "Start pauses (the world is frozen: camera ${a% *})"; else ko "pause: '$a' vs '$b'"; fi
printf "30 tap A\n120 tap START\n200 tap START\n" > "$T/pause2.input"
out=$(run --frames 400 --input "$T/pause2.input")
check "Start again resumes" "paused=0 " "$out"

rm -f "$T/save.srm"
out=$(run --frames 6000 --opt bot=1 --opt seed=8 --opt botstop=40 --sram "$T/save.srm")
s=$(echo "$out" | sed -n 's/.*run 1 over at frame [0-9]*: score \([0-9]*\).*/\1/p')
out=$(run --frames 10 --sram "$T/save.srm")
check "save RAM: the best score ($s) persists" "best=$s runs=0" "$out"

printf "20 P2 tap A\n" > "$T/join.input"
out=$(run --frames 60 --input "$T/join.input")
check "Father Duck joins on the title" "st=0 .*lobby=2 " "$out"
out=$(run --frames 3000 --opt bot=2 --opt seed=6 --opt players=2 --opt ready=1)
check "co-op: both parents hop and score" "players=2 score=[1-9][0-9]* score2=[1-9][0-9]* " "$out"

# four parents: pads 2-4 join on the title, P3 leaves with B and joins again, P1 starts with an arrow (its first hop)
printf "20 P2 tap A\n25 P3 tap A\n30 P4 tap A\n40 P3 tap B\n50 P3 tap A\n70 tap RIGHT\n" > "$T/join4.input"
out=$(run --frames 60 --input "$T/join4.input" --opt strict=1)
check "4 parents: join, leave with B, join again" "st=0 .*lobby=4 " "$out"
out=$(run --frames 400 --input "$T/join4.input" --opt strict=1)
check "4 parents: P1's arrow starts the run and hops" "st=2 players=4 .*cols=7,6,6,6 " "$out"
out=$(run --frames 4000 --opt bot=4 --opt seed=8 --opt strict=1)
check "4 bots play (the family score grows)" "players=4 .*family=[1-9][0-9]* " "$out"
if echo "$out" | grep -q "rs strict"; then ko "strict mode, 4 parents: $(echo "$out" | grep 'rs strict' | head -3)"; else ok "no strict-mode warning with 4 parents (the sprite budget)"; fi

# the bot plays from the screen only (OAM, the BG3 map, the scroll register): 150 lanes on average over 10 seeds
F=${BOT_FRAMES:-36000}
for seed in 1 2 3 4 5 6 7 8 9 10; do
    run --frames $F --opt bot=1 --opt seed=$seed --opt sound=0 > "$T/bot$seed.txt" &
done
wait
all=""; sum=0; min=100000; max=0
for seed in 1 2 3 4 5 6 7 8 9 10; do
    l=$(sed -n 's/.*state: .* lanes=\([0-9]*\) .*/\1/p' "$T/bot$seed.txt")
    how=$(grep -o "hit at\|swept away\|caught by the fox" "$T/bot$seed.txt" | head -1)
    [ -n "$how" ] || how="alive"
    all="$all $l"
    sum=$((sum + ${l:-0}))
    [ "${l:-0}" -lt $min ] && min=${l:-0}
    [ "${l:-0}" -gt $max ] && max=${l:-0}
    echo "       seed $seed: $l lanes ($how)"
done
avg=$((sum / 10))
if [ $avg -ge 150 ]; then ok "the bot crosses $avg lanes on average (>= 150; min $min, max $max:$all)"
else ko "the bot only crosses $avg lanes on average:$all"; fi

# determinism: the same inputs, the same run (state hash and picture), twice
for k in 1 2; do
    run --frames 2400 --input "$T/rhythm.input" --opt seed=3 --png "$T/det$k.png" | grep "state:" > "$T/det$k.txt"
    run --frames 3000 --opt bot=1 --opt seed=5 --png "$T/botd$k.png" | grep "state:" > "$T/botd$k.txt"
done
if cmp -s "$T/det1.txt" "$T/det2.txt" && cmp -s "$T/det1.png" "$T/det2.png"; then ok "determinism: a scripted run twice, same state and picture"
else ko "determinism (script)"; fi
if cmp -s "$T/botd1.txt" "$T/botd2.txt" && cmp -s "$T/botd1.png" "$T/botd2.png"; then ok "determinism: a bot run twice, same state and picture"
else ko "determinism (bot)"; fi
for k in 1 2; do run --frames 2000 --opt bot=4 --opt seed=9 --png "$T/bot4d$k.png" | grep "state:" > "$T/bot4d$k.txt"; done
if cmp -s "$T/bot4d1.txt" "$T/bot4d2.txt" && cmp -s "$T/bot4d1.png" "$T/bot4d2.png"; then ok "determinism: 4 bots twice, same state and picture"
else ko "determinism (4 bots)"; fi
exit $fail

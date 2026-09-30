#!/bin/sh
# Blueberry Tumble smoke tests: scripted runs through the headless runner (the flow, pause, save RAM, player 2,
# strict mode), determinism, and the screen-reading bot over 10 seeds.
#   smoke_test.sh <headless binary> <tmp dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/blueberrytumble/LICENSE.
H=$1
T=${2:-/tmp}/blueberrytumble-tests
BOT_TARGET=${BOT_TARGET:-800}       # metres: the bot's average over 10 seeds must reach this
mkdir -p "$T"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() {   # check <name> <expected regexp> <output>
    if echo "$3" | grep -q "$2"; then ok "$1"; else ko "$1: expected '$2' in: $(echo "$3" | grep 'state:\|strict')"; fi
}
run() { $H --opt music=0 --opt sound=0 --opt dump=1 "$@" 2>&1; }

out=$(run --frames 300 --opt strict=1)
check "title: nothing happens without input" "state: st=0 .*m=0 .*runs=0 " "$out"
if echo "$out" | grep -q "rs strict"; then ko "strict mode on the title: $(echo "$out" | grep 'rs strict')"; else ok "no strict-mode warning on the title (VRAM, maps)"; fi
out=$(run --frames 2400 --opt strict=1 --opt bot=1 --opt seed=4 --opt ready=1)
if echo "$out" | grep -q "rs strict"; then ko "strict mode in a run: $(echo "$out" | grep 'rs strict')"; else ok "no strict-mode warning in a run (sprites per line, voices, VRAM)"; fi

printf "30 tap A\n60 tap A\n" > "$T/start.input"
out=$(run --frames 80 --input "$T/start.input")
check "A on the title: get ready; A again: the run starts" "state: st=2 " "$out"
out=$(run --frames 900 --input "$T/start.input")
check "no more input: the berry splats, game over, one attempt" "state: st=4 .*runs=1 " "$out"
check "  ... and the first thing it meets kills it early (< 80 m)" "state: st=4 .*m=[1-7]\?[0-9] " "$out"

printf "30 tap A\n60 tap A\n860 tap A\n" > "$T/retry.input"
out=$(run --frames 880 --input "$T/retry.input")
check "retry: one button back to get ready" "state: st=1 .*runs=1 " "$out"

printf "30 tap A\n60 tap A\n90 tap SELECT\n" > "$T/pause.input"
a=$(run --frames 120 --input "$T/pause.input" | sed -n 's/.* f=\([0-9]*\) paused=\([0-9]\).*/\1 \2/p')
b=$(run --frames 300 --input "$T/pause.input" | sed -n 's/.* f=\([0-9]*\) paused=\([0-9]\).*/\1 \2/p')
if [ "$a" = "$b" ] && [ "${a#* }" = "1" ]; then ok "Select pauses (the course is frozen at frame ${a% *})"; else ko "pause: '$a' vs '$b'"; fi

rm -f "$T/save.srm"
out=$(run --frames 3000 --opt bot=1 --opt seed=8 --opt botstop=60 --opt ready=1 --sram "$T/save.srm")
best=$(echo "$out" | sed -n 's/.*state: .* best=\([0-9]*\) .*/\1/p')
out=$(run --frames 10 --sram "$T/save.srm")
if [ -n "$best" ] && [ "$best" -ge 60 ] && echo "$out" | grep -q "best=$best runs=1 "; then ok "save RAM: the best score ($best) and the attempts persist"
else ko "save RAM: best '$best': $(echo "$out" | grep state)"; fi

printf "20 P2 tap A\n" > "$T/join.input"
out=$(run --frames 60 --opt ready=1 --input "$T/join.input")
check "player 2 joins from get ready" "players=2 " "$out"
out=$(run --frames 2500 --opt bot=2 --opt seed=6 --opt players=2 --opt ready=1)
check "2 players: both berries roll on (the raspberry too)" "players=2 m=[1-9][0-9]* m2=[1-9][0-9]* " "$out"

# determinism: the same inputs, the same run (state hash and picture), twice
: > "$T/rhythm.input"
i=30; while [ $i -lt 1500 ]; do echo "$i tap A" >> "$T/rhythm.input"; i=$((i + 23)); done
for k in 1 2; do
    run --frames 1500 --input "$T/rhythm.input" --opt seed=3 --png "$T/det$k.png" | grep "state:" > "$T/det$k.txt"
    run --frames 2000 --opt bot=1 --opt ready=1 --png "$T/bot$k.png" | grep "state:" > "$T/bot$k.txt"
done
if cmp -s "$T/det1.txt" "$T/det2.txt" && cmp -s "$T/det1.png" "$T/det2.png"; then ok "determinism: a scripted run twice, same state and picture"
else ko "determinism (script)"; fi
if cmp -s "$T/bot1.txt" "$T/bot2.txt" && cmp -s "$T/bot1.png" "$T/bot2.png"; then ok "determinism: a bot run twice, same state and picture"
else ko "determinism (bot)"; fi

# the bot: it plays from the screen only (src/bot.c); its average over 10 seeds must reach BOT_TARGET metres
sum=0; list=""; lo=999999; hi=0
for s in 1 2 3 4 5 6 7 8 9 10; do
    m=$(run --frames 40000 --opt bot=1 --opt seed=$s --opt ready=1 | sed -n 's/.*run 1 over at frame [0-9]*: \([0-9]*\) m.*/\1/p')
    m=${m:-40000}
    sum=$((sum + m)); list="$list $m"
    [ "$m" -lt "$lo" ] && lo=$m
    [ "$m" -gt "$hi" ] && hi=$m
done
avg=$((sum / 10))
echo "  bot distances over 10 seeds (m):$list"
if [ "$avg" -ge "$BOT_TARGET" ]; then ok "the bot averages $avg m (min $lo, max $hi; target $BOT_TARGET)"
else ko "the bot averages only $avg m (target $BOT_TARGET)"; fi
exit $fail

#!/bin/sh
# Llama Drama smoke tests: scripted runs through the headless runner (the title with players joining, the start,
# aiming and spitting, the reload, the game over, back to the title, pause, save RAM, 1-4 players, a busy sky), the bot
# hook and determinism.
#   smoke_test.sh <headless binary> <tmp dir> [quick]
# quick (the widescreen passes): the flow and 4 players only; the bot's long run and determinism run once, at 320.
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/llamadrama/LICENSE.
H=$1
T=${2:-/tmp}/llamadrama-tests
Q=$3
mkdir -p "$T"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() {   # check <name> <expected regexp> <output>
    if echo "$3" | grep -q "$2"; then ok "$1"; else ko "$1: expected '$2' in: $(echo "$3" | grep 'state:\|strict')"; fi
}
nostrict() { if echo "$2" | grep -q "rs strict"; then ko "$1: strict mode warnings: $(echo "$2" | grep 'rs strict')"; else ok "$1: no strict-mode warning"; fi; }
run() { $H --opt music=0 --opt dump=1 --opt seed=4 "$@" 2>&1; }
over_frame() { echo "$1" | sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p'; }
field() { echo "$1" | grep "state:" | sed -n "s/.* $2=\([^ ]*\).*/\1/p"; }

# screens: st=0 the title, 1 play, 2 the last llama's bonk, 3 the game-over panel (no "get ready"); llamas (hs): 1
# playing, 2 stung, 3 out
out=$(run --frames 300 --opt strict=1)
check "title: nothing happens without input" "state: st=0 .*score=0 .*runs=0 " "$out"
nostrict "the title" "$out"

printf "30 tap A\n" > "$T/start.input"
out=$(run --frames 40 --input "$T/start.input")
check "A on the title starts the run at once, and spits" "state: st=1 .* aim=14 spits=1 " "$out"
printf "30 tap DOWN\n" > "$T/down.input"
out=$(run --frames 40 --input "$T/down.input")
check "Down starts it too (an extra start input), no spit" "state: st=1 .* spits=0 " "$out"

# UP/DOWN aim: 2 units a frame for 8 frames, then 5; between -100 and 156 (1024 to the turn)
printf "30 tap START\n60 UP\n70 -\n" > "$T/up.input"
out=$(run --frames 80 --input "$T/up.input")
check "Up held 10 frames: the aim rises 2 x 8 + 5 x 2 (14 -> 40)" "state: st=1 .* aim=40 " "$out"
printf "30 tap DOWN\n60 UP\n200 -\n" > "$T/upmax.input"
out=$(run --frames 210 --input "$T/upmax.input")
check "...up to 55 degrees at most" "state: st=1 .* aim=156 " "$out"
printf "30 tap DOWN\n60 DOWN\n200 -\n" > "$T/downmax.input"
out=$(run --frames 210 --input "$T/downmax.input")
check "...down to -35 degrees at least (the grass)" "state: st=1 .* aim=-100 " "$out"
# A mashed 15 times a second for 2 s: the reload (12 frames) and 3 gobs in the air at most
: > "$T/mash.input"
echo "30 tap A" >> "$T/mash.input"
i=40; while [ $i -lt 160 ]; do echo "$i tap A" >> "$T/mash.input"; i=$((i + 4)); done
out=$(run --frames 160 --input "$T/mash.input")
s=$(field "$out" spits)
if [ "${s:-0}" -ge 6 ] && [ "${s:-0}" -le 11 ]; then ok "A mashed for 2 s: $s spits (the reload: no wall of spit)"
else ko "mashing: $(echo "$out" | grep state:)"; fi

# the game over: the bot stops playing at 100 points (botstop): a hornet stings it
out=$(run --frames 6000 --opt bot=1 --opt botstop=100)
check "one sting: game over" "state: st=3 .*runs=1 hs=3 " "$out"
f=$(over_frame "$out")
[ -n "$f" ] || { ko "the bot run did not end"; f=3000; }
printf "%d tap A\n" $((f + 50)) > "$T/retry.input"
out=$(run --frames $((f + 60)) --opt bot=1 --opt botstop=100 --input "$T/retry.input")
check "game over: one press goes back to the title (the only menu)" "state: st=0 .*runs=1 " "$out"
printf "%d tap A\n" $((f + 10)) > "$T/early.input"
out=$(run --frames $((f + 20)) --opt bot=1 --opt botstop=100 --input "$T/early.input")
check "...not before the panel's lock" "state: st=3 .*runs=1 " "$out"
printf "%d tap A\n%d tap A\n" $((f + 50)) $((f + 70)) > "$T/again.input"
out=$(run --frames $((f + 80)) --opt bot=1 --opt botstop=100 --input "$T/again.input")
check "...and A on the title starts the next run" "state: st=1 .*runs=1 " "$out"

printf "30 tap A\n200 tap SELECT\n" > "$T/pause.input"
a=$(run --frames 260 --input "$T/pause.input" | sed -n 's/.*\(aim=.* hn=[0-9]*\) paused=\([0-9]\) hash=\([0-9a-f]*\).*/\1 \2 \3/p')
b=$(run --frames 400 --input "$T/pause.input" | sed -n 's/.*\(aim=.* hn=[0-9]*\) paused=\([0-9]\) hash=\([0-9a-f]*\).*/\1 \2 \3/p')
if [ -n "$a" ] && [ "$a" = "$b" ] && echo "$a" | grep -q " 1 "; then ok "Select pauses (the world is frozen)"
else ko "pause: '$a' vs '$b'"; fi

# players 2-4 join on the title (A on their pad), leave with B; only P1 starts
printf "20 P2 tap A\n25 P3 tap A\n30 P4 tap A\n40 P2 tap B\n60 P2 tap A\n70 P2 tap DOWN\n" > "$T/join.input"
out=$(run --frames 80 --input "$T/join.input")
check "players join and leave on the title; only P1 starts" "state: st=0 players=4 " "$out"
out=$(run --frames 2500 --opt bot=4 --opt strict=1)
check "4 players (4 bots): all four pop hornets" "players=4 .*scores=[1-9][0-9]*,[1-9][0-9]*,[1-9][0-9]*,[1-9][0-9]*" "$out"
nostrict "4 bots" "$out"
out=$(run --frames 1500 --opt bot=4 --opt ready=1 --opt score=900 --opt strict=1)
check "a busy night sky: 4 llamas, the ramp at its height" "state: st=[12] players=4 .*sky=[1-9]" "$out"
nostrict "the busy night with 4 llamas" "$out"

[ -n "$Q" ] && exit $fail

rm -f "$T/save.srm"
out=$(run --frames 6000 --opt bot=1 --opt botstop=100 --sram "$T/save.srm")
s=$(field "$out" score)
out=$(run --frames 10 --sram "$T/save.srm")
check "save RAM: the best score ($s) persists" "best=${s:-x} runs=0" "$out"

# the bot hook (bot_decide): it plays well: a minute without a sting, the sky turning to the sunset
out=$(run --frames 3800 --opt bot=1 --opt seed=7)
check "the bot plays a minute without a sting" "state: st=1 .*hs=1 " "$out"

# determinism: the same inputs, the same run (state hash and picture), twice; also 4 human-like bots
: > "$T/rhythm.input"
echo "30 tap A" >> "$T/rhythm.input"
i=60; while [ $i -lt 1500 ]; do echo "$i UP+A" >> "$T/rhythm.input"; echo "$((i + 11)) DOWN" >> "$T/rhythm.input"; echo "$((i + 25)) A" >> "$T/rhythm.input"; i=$((i + 37)); done
for k in 1 2; do
    run --frames 1500 --input "$T/rhythm.input" --png "$T/det$k.png" | grep "state:" > "$T/det$k.txt"
    run --frames 2000 --opt bot=4 --opt botskill=3 --png "$T/bot4p$k.png" | grep "state:" > "$T/bot4p$k.txt"
done
if cmp -s "$T/det1.txt" "$T/det2.txt" && cmp -s "$T/det1.png" "$T/det2.png"; then ok "determinism: a scripted run twice, same state and picture"
else ko "determinism (script)"; fi
if cmp -s "$T/bot4p1.txt" "$T/bot4p2.txt" && cmp -s "$T/bot4p1.png" "$T/bot4p2.png"; then ok "determinism: 4 human-like bots twice, same state and picture"
else ko "determinism (4 bots)"; fi
exit $fail

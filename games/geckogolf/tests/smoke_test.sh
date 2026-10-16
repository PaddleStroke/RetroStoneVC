#!/bin/sh
# Gecko Golf smoke tests: scripted runs through the headless runner (the title with players joining, the start, the
# aim on the d-pad, the power gauge and the shot, B and a tap of A are no shot, the game over, back to the title,
# pause, save RAM, 1-4 players), the bot hook and determinism.
#   smoke_test.sh <headless binary> <tmp dir> [quick]
# quick (the widescreen passes): the flow and 4 players only; the bot's runs and determinism run once, at 320.
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/geckogolf/LICENSE.
H=$1
T=${2:-/tmp}/geckogolf-tests
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

# screens: st=0 the title (the first hole behind it), 1 play, 2 the last player out, 3 the game-over panel
out=$(run --frames 300 --opt strict=1)
check "title: nothing happens without input" "state: st=0 .*score=0 strokes=12 .*runs=0 " "$out"
nostrict "the title" "$out"

printf "30 tap A\n" > "$T/start.input"
out=$(run --frames 60 --input "$T/start.input")
check "A on the title starts the run (the press is no shot)" "state: st=1 .*hole=0 phase=0 turn=0 shots=0 aim=90 " "$out"
printf "30 tap DOWN\n" > "$T/down.input"
out=$(run --frames 40 --input "$T/down.input")
check "the d-pad starts it too (an extra start input)" "state: st=1 " "$out"

# the aim: Left turns it towards the left, Up towards the sky, Down towards the ground; held: faster
printf "30 tap A\n40 tap LEFT\n" > "$T/aim1.input"
out=$(run --frames 60 --input "$T/aim1.input")
check "a tap of Left: half a degree" "state: st=1 .* aim=91 " "$out"
printf "30 tap A\n40 LEFT\n100 -\n" > "$T/aim2.input"
a=$(field "$(run --frames 110 --input "$T/aim2.input")" aim)
[ "${a:-0}" -gt 120 ] && ok "Left held a second: the aim turns faster ($a)" || ko "Left held: aim $a"
printf "30 tap A\n40 UP\n240 -\n" > "$T/aim3.input"
a=$(field "$(run --frames 250 --input "$T/aim3.input")" aim)
[ "$a" = 180 ] && ok "Up held: straight up, no further" || ko "Up held: aim $a"
printf "30 tap A\n40 DOWN\n240 -\n" > "$T/aim4.input"
a=$(field "$(run --frames 250 --input "$T/aim4.input")" aim)
[ "$a" = 8 ] && ok "Down held: down to the ground (4 degrees)" || ko "Down held: aim $a"

# the shot: A held fills the gauge, let go: the shot (a stroke of the budget)
printf "30 tap A\n40 A\n80 -\n" > "$T/shot.input"
out=$(run --frames 70 --input "$T/shot.input")
check "A held: the gauge fills" "state: st=1 .*phase=1 .*gauge=[3-6][0-9] " "$out"
out=$(run --frames 82 --input "$T/shot.input")
check "let go: the shot (12 -> 11 strokes)" "state: st=1 .*strokes=11 .*phase=2 .*shots=1 " "$out"
out=$(run --frames 600 --input "$T/shot.input")
check "...the ball comes to rest, the next shot" "state: st=1 .*shots=1 .*ball=0 " "$out"
printf "30 tap A\n40 tap A\n" > "$T/tap.input"
out=$(run --frames 80 --input "$T/tap.input")
check "a tap of A is no shot" "state: st=1 .*strokes=12 .*phase=0 .*shots=0 " "$out"
printf "30 tap A\n40 A\n70 A+B\n72 -\n" > "$T/cancel.input"
out=$(run --frames 100 --input "$T/cancel.input")
check "B while charging: no shot" "state: st=1 .*strokes=12 .*shots=0 " "$out"

# the game over: the bot gives up after its first hole (botstop) with 2 strokes: out of bounds until out
B="--opt bot=1 --opt botstop=1 --opt strokes=2"
out=$(run --frames 3000 $B)
check "out of strokes: game over" "state: st=3 .*runs=1 .*out=1" "$out"
f=$(over_frame "$out")
[ -n "$f" ] || { ko "the bot run did not end"; f=1500; }
printf "%d tap A\n" $((f + 50)) > "$T/retry.input"
out=$(run --frames $((f + 60)) $B --input "$T/retry.input")
check "game over: one press goes back to the title (the only menu)" "state: st=0 .*runs=1 " "$out"
printf "%d tap A\n" $((f + 10)) > "$T/early.input"
out=$(run --frames $((f + 20)) $B --input "$T/early.input")
check "...not before the panel's lock" "state: st=3 .*runs=1 " "$out"
printf "%d tap SELECT\n" $((f + 50)) > "$T/back.input"
out=$(run --frames $((f + 60)) $B --input "$T/back.input")
check "Select on the panel: back to the title too" "state: st=0 .*runs=1 " "$out"

printf "30 tap A\n40 A\n80 -\n100 tap SELECT\n" > "$T/pause.input"
a=$(run --frames 160 --input "$T/pause.input" | sed -n 's/.* x=\([0-9-]*\) y=\([0-9-]*\) .* paused=\([0-9]\).* hash=\([0-9a-f]*\).*/\1 \2 \3 \4/p')
b=$(run --frames 400 --input "$T/pause.input" | sed -n 's/.* x=\([0-9-]*\) y=\([0-9-]*\) .* paused=\([0-9]\).* hash=\([0-9a-f]*\).*/\1 \2 \3 \4/p')
if [ -n "$a" ] && [ "$a" = "$b" ] && echo "$a" | grep -q " 1 [0-9a-f]*$"; then ok "Select pauses (the ball in flight frozen)"
else ko "pause: '$a' vs '$b'"; fi

# players 2-4 join on the title (A on their pad), leave with B; only P1 starts
printf "20 P2 tap A\n25 P3 tap A\n30 P4 tap A\n40 P2 tap B\n60 P2 tap A\n70 P2 tap DOWN\n" > "$T/join.input"
out=$(run --frames 80 --input "$T/join.input")
check "players join and leave on the title; only P1 starts" "state: st=0 players=4 " "$out"
# 2 players on the pads: P1 shoots, then P2 (the farther from the cup) on its own pad
printf "20 P2 tap A\n40 tap A\n50 A\n70 -\n" > "$T/two.input"
out=$(run --frames 400 --input "$T/two.input")
check "2 players: after P1's shot, P2's turn" "state: st=1 players=2 .*turn=1 shots=1 " "$out"
printf "20 P2 tap A\n40 tap A\n50 A\n70 -\n420 P2 A\n440 P2 -\n" > "$T/two2.input"
out=$(run --frames 450 --input "$T/two2.input")
check "...and P2 shoots on pad 2" "state: st=1 players=2 .*shots=2 " "$out"
out=$(run --frames 3000 --opt bot=4 --opt strict=1)
check "4 players (4 bots): all four hole out" "players=4 .*scores=[1-9][0-9]*,[1-9][0-9]*,[1-9][0-9]*,[1-9][0-9]* " "$out"
nostrict "4 bots" "$out"
out=$(run --frames 600 --opt bot=1 --opt ready=1 --opt hole=20 --opt strict=1)
check "a later hole (another biome)" "state: st=1 .*hole=2[0-9] " "$out"
nostrict "hole 20" "$out"

[ -n "$Q" ] && exit $fail

rm -f "$T/save.srm"
out=$(run --frames 3000 --opt bot=1 --opt botstop=2 --opt strokes=2 --sram "$T/save.srm")
s=$(field "$out" score)
out=$(run --frames 10 --sram "$T/save.srm")
check "save RAM: the best ($s holes) persists" "best=${s:-x} runs=0" "$out"

# the bot hook (bot_plan_shot): it plays well: 6 holes in 4000 frames, still playing
out=$(run --frames 4000 --opt bot=1 --opt seed=7)
s=$(field "$out" score)
[ "${s:-0}" -ge 6 ] && ok "the bot plays: $s holes" || ko "the bot's holes: $s"
check "...and is still in" "state: st=1 .*out=0" "$out"

# determinism: the same inputs, the same run (state hash and picture), twice; also 4 human-like bots
: > "$T/rhythm.input"
echo "30 tap A" >> "$T/rhythm.input"
i=40; while [ $i -lt 1500 ]; do echo "$i LEFT" >> "$T/rhythm.input"; echo "$((i + 9)) A" >> "$T/rhythm.input"
    echo "$((i + 50)) -" >> "$T/rhythm.input"; i=$((i + 260)); done
for k in 1 2; do
    run --frames 1500 --input "$T/rhythm.input" --png "$T/det$k.png" | grep "state:" > "$T/det$k.txt"
    run --frames 2500 --opt bot=4 --opt botskill=5 --png "$T/bot4p$k.png" | grep "state:" > "$T/bot4p$k.txt"
done
if cmp -s "$T/det1.txt" "$T/det2.txt" && cmp -s "$T/det1.png" "$T/det2.png"; then ok "determinism: a scripted run twice, same state and picture"
else ko "determinism (script)"; fi
if cmp -s "$T/bot4p1.txt" "$T/bot4p2.txt" && cmp -s "$T/bot4p1.png" "$T/bot4p2.png"; then ok "determinism: 4 human-like bots twice, same state and picture"
else ko "determinism (4 bots)"; fi
exit $fail

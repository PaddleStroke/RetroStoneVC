#!/bin/sh
# Gopher Dig smoke tests: scripted runs through the headless runner (the title with players joining, the start, walking
# and drilling on the pad, the game over, back to the title, pause, save RAM, 1-4 players), the bot hook and determinism.
#   smoke_test.sh <headless binary> <tmp dir> [quick]
# quick (the widescreen passes): the flow and 4 players only; the bot's runs and determinism run once, at 320.
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/gopherdig/LICENSE.
H=$1
T=${2:-/tmp}/gopherdig-tests
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

# screens: st=0 the title, 1 play, 2 the last gopher out, 3 the game-over panel
out=$(run --frames 300 --opt strict=1)
check "title: nothing happens without input" "state: st=0 .*score=0 .*runs=0 " "$out"
nostrict "the title" "$out"

printf "30 tap A\n" > "$T/start.input"
out=$(run --frames 40 --input "$T/start.input")
check "A on the title starts the run at once" "state: st=1 .*col=4 row=-1 air=" "$out"
printf "30 tap DOWN\n" > "$T/down.input"
out=$(run --frames 40 --input "$T/down.input")
check "Down starts it too (an extra start input)" "state: st=1 " "$out"

# the pad: Right walks, Down + A digs down (the gopher falls into the hole)
printf "30 tap A\n40 RIGHT\n120 -\n" > "$T/walk.input"
out=$(run --frames 130 --input "$T/walk.input")
check "Right walks to the well's side" "state: st=1 .*col=8 row=-1 " "$out"
printf "30 tap A\n40 DOWN\n42 DOWN+A\n44 DOWN\n46 -\n" > "$T/dig.input"
out=$(run --frames 90 --input "$T/dig.input")
check "Down + A drills down, the gopher drops into the hole" "state: st=1 .*row=[0-9][0-9]* .*pops=[1-9]" "$out"
printf "30 tap A\n40 DOWN\n42 -\n50 tap A\n70 tap A\n" > "$T/again.input"
out=$(run --frames 110 --input "$T/again.input")
d=$(field "$out" row)
[ "${d:-0}" -ge 1 ] && ok "after Down, A alone keeps drilling down (row $d)" || ko "A after Down: $out"

# the game over: the bot stands still from 20 m (botstop) until the air runs out or a block falls on it
out=$(run --frames 12000 --opt bot=1 --opt botstop=20)
check "game over" "state: st=3 .*runs=1 out=[12] " "$out"
f=$(over_frame "$out")
[ -n "$f" ] || { ko "the bot run did not end"; f=6000; }
printf "%d tap A\n" $((f + 50)) > "$T/retry.input"
out=$(run --frames $((f + 60)) --opt bot=1 --opt botstop=20 --input "$T/retry.input")
check "game over: one press goes back to the title (the only menu)" "state: st=0 .*runs=1 " "$out"
printf "%d tap A\n" $((f + 10)) > "$T/early.input"
out=$(run --frames $((f + 20)) --opt bot=1 --opt botstop=20 --input "$T/early.input")
check "...not before the panel's lock" "state: st=3 .*runs=1 " "$out"
printf "%d tap A\n%d tap A\n" $((f + 50)) $((f + 70)) > "$T/next.input"
out=$(run --frames $((f + 80)) --opt bot=1 --opt botstop=20 --input "$T/next.input")
check "...and A on the title starts the next run" "state: st=1 .*runs=1 .*row=-1 " "$out"

printf "30 tap A\n40 DOWN\n42 DOWN+A\n44 -\n100 tap SELECT\n" > "$T/pause.input"
a=$(run --frames 160 --input "$T/pause.input" | sed -n 's/.* air=\([0-9]*\) .* t=\([0-9]*\) paused=\([0-9]\).* hash=\([0-9a-f]*\).*/\1 \2 \3 \4/p')
b=$(run --frames 400 --input "$T/pause.input" | sed -n 's/.* air=\([0-9]*\) .* t=\([0-9]*\) paused=\([0-9]\).* hash=\([0-9a-f]*\).*/\1 \2 \3 \4/p')
if [ -n "$a" ] && [ "$a" = "$b" ] && echo "$a" | grep -q " 1 [0-9a-f]*$"; then ok "Select pauses (the well and the air stand still)"
else ko "pause: '$a' vs '$b'"; fi

# players 2-4 join on the title (A on their pad), leave with B; only P1 starts
printf "20 P2 tap A\n25 P3 tap A\n30 P4 tap A\n40 P2 tap B\n60 P2 tap A\n70 P2 tap DOWN\n" > "$T/join.input"
out=$(run --frames 80 --input "$T/join.input")
check "players join and leave on the title; only P1 starts" "state: st=0 players=4 " "$out"
out=$(run --frames 2400 --opt bot=4 --opt strict=1)
check "4 players (4 bots) in quarters: all four dig" "players=4 .*scores=[1-9][0-9]*,[1-9][0-9]*,[1-9][0-9]*,[1-9][0-9]*$" "$out"
nostrict "4 bots in 4 views" "$out"
out=$(run --frames 1500 --opt bot=3 --opt strict=1)
check "3 players: the 4th quarter is the race" "players=3 .*scores=[1-9][0-9]*,[1-9][0-9]*,[1-9][0-9]*$" "$out"
nostrict "3 views and the race" "$out"
out=$(run --frames 1500 --opt bot=2 --opt strict=1)
check "2 players side by side" "players=2 .*scores=[1-9][0-9]*,[1-9][0-9]*$" "$out"
nostrict "2 views" "$out"

[ -n "$Q" ] && exit $fail

rm -f "$T/save.srm"
out=$(run --frames 12000 --opt bot=1 --opt botstop=20 --sram "$T/save.srm")
s=$(field "$out" score)
out=$(run --frames 10 --sram "$T/save.srm")
check "save RAM: the best depth ($s) persists" "best=${s:-x} runs=0" "$out"

# the bot hook (bot_think): it digs well: past the first stratum in a minute, a checkpoint on the way
out=$(run --frames 3600 --opt bot=1 --opt seed=7 --opt strict=1)
s=$(field "$out" score)
[ "${s:-0}" -ge 100 ] && ok "the bot digs $s m in a minute" || ko "the bot's depth: $s"
echo "$out" | grep -q "reached 100 m" && ok "a checkpoint at 100 m" || ko "no checkpoint: $(echo "$out" | grep state)"
nostrict "a deep run" "$out"

# determinism: the same inputs, the same run (state hash and picture), twice; also 4 human-like bots
: > "$T/rhythm.input"
echo "30 tap A" >> "$T/rhythm.input"
i=40; while [ $i -lt 1500 ]; do echo "$i DOWN+A" >> "$T/rhythm.input"; echo "$((i + 3)) -" >> "$T/rhythm.input"
    echo "$((i + 20)) RIGHT" >> "$T/rhythm.input"; echo "$((i + 30)) -" >> "$T/rhythm.input"; i=$((i + 47)); done
for k in 1 2; do
    run --frames 1500 --input "$T/rhythm.input" --png "$T/det$k.png" | grep "state:" > "$T/det$k.txt"
    run --frames 2000 --opt bot=4 --opt botskill=3 --png "$T/bot4p$k.png" | grep "state:" > "$T/bot4p$k.txt"
done
if cmp -s "$T/det1.txt" "$T/det2.txt" && cmp -s "$T/det1.png" "$T/det2.png"; then ok "determinism: a scripted run twice, same state and picture"
else ko "determinism (script)"; fi
if cmp -s "$T/bot4p1.txt" "$T/bot4p2.txt" && cmp -s "$T/bot4p1.png" "$T/bot4p2.png"; then ok "determinism: 4 human-like bots twice, same state and picture"
else ko "determinism (4 bots)"; fi
exit $fail

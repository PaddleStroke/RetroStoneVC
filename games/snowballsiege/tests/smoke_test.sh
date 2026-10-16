#!/bin/sh
# Snowball Siege smoke tests: scripted runs through the headless runner (the title with players joining, a turn played
# on the pad: aim, walk, switch, charge and throw; the timer; the computer's turn; the game over and back to the
# title; pause; save RAM; 2-4 players in hot seat), the bot hook and determinism.
#   smoke_test.sh <headless binary> <tmp dir> [quick]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/snowballsiege/LICENSE.
H=$1
T=${2:-/tmp}/snowballsiege-tests
Q=$3
mkdir -p "$T"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() {   # check <name> <expected regexp> <output>
    if echo "$3" | grep -q "$2"; then ok "$1"; else ko "$1: expected '$2' in: $(echo "$3" | grep 'state:\|strict')"; fi
}
run() { $H --opt music=0 --opt dump=1 --opt seed=3 "$@" 2>&1; }
field() { echo "$1" | grep "state:" | tail -1 | sed -n "s/.* $2=\([^ ]*\).*/\1/p"; }

# screens: st=0 the title, 1 play, 2 the last critter out, 3 the game-over panel or the results
out=$(run --frames 300 --opt strict=1)
check "title: nothing happens without input" "state: st=0 .*turns=0 " "$out"
if echo "$out" | grep -q "rs strict"; then ko "strict mode warnings: $(echo "$out" | grep 'rs strict')"; else ok "no strict-mode warning (VRAM, sprites, voices, samples...)"; fi

printf "30 tap A\n" > "$T/start.input"
out=$(run --frames 40 --input "$T/start.input")
check "A on the title starts match 1: the player's turn" "state: st=1 .*level=1 phase=1 turns=1 side=0 " "$out"
printf "30 tap START\n" > "$T/startb.input"
out=$(run --frames 40 --input "$T/startb.input")
check "Start starts it too" "state: st=1 " "$out"

# a turn on the pad: Up aims (a degree every 2 frames), a tap Left only turns round, Right held walks 40 px at most,
# B switches the throwable, A held charges, let go: the throw
printf "30 tap A\n40 UP\n60 -\n" > "$T/aim.input"
out=$(run --frames 70 --input "$T/aim.input")
check "Up held 20 frames: 10 degrees higher (55)" "angle=55 " "$out"
printf "30 tap A\n40 LEFT\n42 -\n" > "$T/turn.input"
out=$(run --frames 60 --input "$T/turn.input")
check "a tap the other way turns round, no step" "facing=-1 .*moved=0 " "$out"
printf "30 tap A\n40 RIGHT\n400 -\n" > "$T/walk.input"
out=$(run --frames 410 --input "$T/walk.input")
check "Right held: it walks up to its fort's wall (15 px)" "facing=1 .*moved=15 " "$out"
printf "30 tap A\n40 tap B\n" > "$T/switch.input"
out=$(run --frames 50 --input "$T/switch.input")
check "B: the big snowball" "weapon=1 " "$out"
printf "30 tap A\n40 A\n80 -\n" > "$T/charge.input"
out=$(run --frames 78 --input "$T/charge.input")
check "A held: the gauge rises (150 + 12 a frame)" "phase=1 .*power=5[0-9][0-9] " "$out"
s0=$(field "$(run --frames 70 --input "$T/charge.input")" solid)
out=$(run --frames 300 --input "$T/charge.input")
s1=$(field "$out" solid)
check "let go: thrown; then the computer's turn" "state: st=1 .*turns=2 side=1 " "$out"
if [ "${s1:-0}" -lt "${s0:-0}" ]; then ok "the splat carved the snow ($s0 -> $s1 px)"; else ko "no carving: $s0 -> $s1"; fi
out=$(run --frames 1400 --input "$T/start.input")
check "no throw in 20 s: TIME UP, the computer's turn" "state: st=1 .*turns=2 side=1 " "$out"

printf "30 tap A\n90 tap SELECT\n" > "$T/pause.input"
a=$(field "$(run --frames 120 --input "$T/pause.input")" hash)
b=$(field "$(run --frames 400 --input "$T/pause.input")" hash)
p=$(field "$(run --frames 400 --input "$T/pause.input")" paused)
if [ "$a" = "$b" ] && [ "$p" = 1 ]; then ok "Select pauses (the timer and the field are frozen)"; else ko "pause: $a vs $b ($p)"; fi

# the game over (the bot gives up after a match won) and back to the title
rm -f "$T/save.srm"
out=$(run --frames 6000 --opt bot=1 --opt seed=5 --opt botstop=1 --sram "$T/save.srm")
check "1 player: a match won, the next lost: game over" "state: st=3 .*wins=1 level=2 .*runs=1 " "$out"
over=$(echo "$out" | sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p')
best=$(field "$out" score)
out=$(run --frames 10 --sram "$T/save.srm")
check "save RAM: the best score ($best) persists" "best=$best runs=0" "$out"
if [ -n "$over" ]; then
    printf "$((over + 50)) tap A\n" > "$T/back.input"
    out=$(run --frames $((over + 60)) --opt bot=1 --opt seed=5 --opt botstop=1 --input "$T/back.input")
    check "game over: one press goes back to the title (the only menu)" "state: st=0 .*runs=1 " "$out"
    printf "$((over + 10)) tap A\n" > "$T/early.input"
    out=$(run --frames $((over + 30)) --opt bot=1 --opt seed=5 --opt botstop=1 --input "$T/early.input")
    check "... not before the panel's lock" "state: st=3 " "$out"
else
    ko "the bot run did not end"
fi

# players 2-4 join on the title (A on their pad), leave with B; only P1 starts
printf "20 P2 tap A\n" > "$T/join.input"
out=$(run --frames 60 --input "$T/join.input")
check "player 2 joins on the title" "state: st=0 players=2 " "$out"
printf "20 P2 tap A\n40 P2 tap A\n" > "$T/p2start.input"
out=$(run --frames 80 --input "$T/p2start.input")
check "... and does not start (P1 starts)" "state: st=0 players=2 " "$out"
printf "20 P2 tap A\n25 P3 tap A\n30 P4 tap A\n40 P2 tap B\n" > "$T/leave.input"
out=$(run --frames 60 --input "$T/leave.input")
check "4 join, B leaves (4 -> 3 players)" "state: st=0 players=3 " "$out"

# hot seat: P1 throws a short one, then P2's pad aims on P2's turn (P1's pad does nothing then)
printf "20 P2 tap A\n40 tap A\n60 tap A\n" > "$T/hot.input"
out=$(run --frames 400 --input "$T/hot.input")
t2=$(field "$out" turns)
printf "20 P2 tap A\n40 tap A\n60 tap A\n400 P2 UP\n420 P2 -\n400 UP\n420 -\n" > "$T/hot2.input"
out=$(run --frames 430 --input "$T/hot2.input")
check "2 players: P2's turn after P1's throw; P2's pad aims, P1's does not" "state: st=1 players=2 .*side=1 .*angle=55 " "$out"
[ "$t2" = 2 ] || ko "P2's turn had not started at frame 400 (turns $t2)"

if [ -z "$Q" ]; then
    out=$(run --frames 12000 --opt bot=4 --opt seed=6 --opt strict=1)
    check "4 players (4 bots): a match to the end, the results" "state: st=3 players=4 .*runs=1 " "$out"
    if echo "$out" | grep -q "rs strict"; then ko "4 players: strict mode warnings: $(echo "$out" | grep 'rs strict')"; else ok "4 players: no strict-mode warning"; fi
    out=$(run --frames 6000 --opt bot=1 --opt seed=8)
    w=$(field "$out" wins)
    if [ "${w:-0}" -ge 2 ]; then ok "the bot (no error) wins match after match ($w)"; else ko "the bot only won $w: $out"; fi
fi

# determinism: the same inputs, the same run (state hash and picture), twice; also the bots, 1 and 4
: > "$T/rhythm.input"
i=30; while [ $i -lt 1800 ]; do echo "$i A" >> "$T/rhythm.input"; echo "$((i + 40)) -" >> "$T/rhythm.input"; i=$((i + 97)); done
for k in 1 2; do
    run --frames 1800 --input "$T/rhythm.input" --png "$T/det$k.png" | grep "state:" > "$T/det$k.txt"
    run --frames 2000 --opt bot=1 --png "$T/bot$k.png" | grep "state:" > "$T/bot$k.txt"
    run --frames 2000 --opt bot=4 --png "$T/bot4p$k.png" | grep "state:" > "$T/bot4p$k.txt"
done
if cmp -s "$T/det1.txt" "$T/det2.txt" && cmp -s "$T/det1.png" "$T/det2.png"; then ok "determinism: a scripted run twice, same state and picture"
else ko "determinism (script)"; fi
if cmp -s "$T/bot1.txt" "$T/bot2.txt" && cmp -s "$T/bot1.png" "$T/bot2.png"; then ok "determinism: a bot run twice, same state and picture"
else ko "determinism (bot)"; fi
if cmp -s "$T/bot4p1.txt" "$T/bot4p2.txt" && cmp -s "$T/bot4p1.png" "$T/bot4p2.png"; then ok "determinism: 4 bots twice, same state and picture"
else ko "determinism (4 bots)"; fi
exit $fail

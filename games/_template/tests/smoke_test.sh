#!/bin/sh
# @NAME@ smoke tests: scripted runs through the headless runner (the title with players joining, play, game over,
# retry, pause, save RAM, 1-4 players), the bot hook and determinism.
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

# screens: st=0 the title, 1 play, 2 the last hero falling, 3 the game-over panel (no "get ready")
out=$(run --frames 300 --opt strict=1)
check "title: nothing happens without input" "state: st=0 .*score=0 .*runs=0 " "$out"
if echo "$out" | grep -q "rs strict"; then ko "strict mode warnings: $(echo "$out" | grep 'rs strict')"; else ok "no strict-mode warning (VRAM, sprites, voices...)"; fi

printf "30 tap A\n" > "$T/start.input"
out=$(run --frames 40 --input "$T/start.input")
check "A on the title starts the run at once (the press is the first jump)" "state: st=1 .*scroll=[1-9]" "$out"
printf "30 tap UP\n" > "$T/up.input"
out=$(run --frames 40 --input "$T/up.input")
check "Up starts it too (a start input)" "state: st=1 " "$out"
out=$(run --frames 900 --input "$T/start.input")
check "no more input: the hero hits a crate, game over" "state: st=3 .*runs=1 " "$out"

printf "30 tap A\n860 tap A\n" > "$T/retry.input"
out=$(run --frames 870 --input "$T/retry.input")
check "retry: one press, straight back into play" "state: st=1 .*runs=1 .*scroll=[1-9]" "$out"
printf "30 tap A\n860 tap SELECT\n" > "$T/back.input"
out=$(run --frames 870 --input "$T/back.input")
check "Select on the panel: back to the title" "state: st=0 .*runs=1 " "$out"

printf "30 tap A\n90 tap SELECT\n" > "$T/pause.input"
a=$(run --frames 120 --input "$T/pause.input" | sed -n 's/.*scroll=\([0-9]*\) paused=\([0-9]\).*/\1 \2/p')
b=$(run --frames 300 --input "$T/pause.input" | sed -n 's/.*scroll=\([0-9]*\) paused=\([0-9]\).*/\1 \2/p')
if [ "$a" = "$b" ] && [ "${a#* }" = "1" ]; then ok "Select pauses (the world is frozen: scroll ${a% *})"; else ko "pause: '$a' vs '$b'"; fi

rm -f "$T/save.srm"
run --frames 4000 --opt bot=1 --opt seed=8 --opt botstop=7 --sram "$T/save.srm" > /dev/null
out=$(run --frames 10 --sram "$T/save.srm")
check "save RAM: the best score (7) persists" "best=7 runs=0" "$out"

# players 2-4 join on the title (A on their pad), leave with B; only P1 starts
printf "20 P2 tap A\n" > "$T/join.input"
out=$(run --frames 60 --input "$T/join.input")
check "player 2 joins on the title" "state: st=0 players=2 " "$out"
printf "20 P2 tap A\n40 P2 tap A\n60 P2 tap UP\n" > "$T/p2start.input"
out=$(run --frames 80 --input "$T/p2start.input")
check "... and does not start the run (P1 starts)" "state: st=0 players=2 " "$out"
printf "20 P2 tap A\n25 P3 tap A\n30 P4 tap A\n" > "$T/join4.input"
out=$(run --frames 60 --input "$T/join4.input")
check "4 players join on the title" "state: st=0 players=4 " "$out"
printf "20 P2 tap A\n25 P3 tap A\n30 P4 tap A\n40 P2 tap B\n" > "$T/leave.input"
out=$(run --frames 60 --input "$T/leave.input")
check "B leaves (4 -> 3 players)" "state: st=0 players=3 " "$out"
printf "20 P4 tap A\n" > "$T/pad4.input"
out=$(run --frames 60 --input "$T/pad4.input")
check "any free pad joins as the next player (pad 4 -> P2)" "state: st=0 players=2 " "$out"

# 4 scripted pads: join on the title, P1 starts, everyone jumps to a rhythm
printf "20 P2 tap A\n25 P3 tap A\n30 P4 tap A\n60 tap A\n" > "$T/play4.input"
i=80; while [ $i -lt 1400 ]; do
    echo "$i tap A" >> "$T/play4.input"; echo "$((i + 5)) P2 tap A" >> "$T/play4.input"
    echo "$((i + 9)) P3 tap A" >> "$T/play4.input"; echo "$((i + 13)) P4 tap A" >> "$T/play4.input"
    i=$((i + 29))
done
out=$(run --frames 1400 --input "$T/play4.input" --opt seed=3 --opt strict=1)
check "4 scripted pads join and play" "state: st=[123] players=4 " "$out"
if echo "$out" | grep -q "rs strict"; then ko "4 players: strict mode warnings: $(echo "$out" | grep 'rs strict')"; else ok "4 players: no strict-mode warning"; fi

out=$(run --frames 2500 --opt bot=2 --opt seed=6 --opt players=2)
check "2 players: both heroes run and score" "players=2 score=[1-9][0-9]* score2=[1-9][0-9]* " "$out"
out=$(run --frames 2500 --opt bot=4 --opt seed=6)
check "4 players (4 bots): all four run and score" "players=4 .*scores=[1-9][0-9]*,[1-9][0-9]*,[1-9][0-9]*,[1-9][0-9]*$" "$out"

# the bot hook (bot_decide in main.c): it must reach 20 with the default settings
out=$(run --frames 4000 --opt bot=1)
s=$(echo "$out" | sed -n 's/.*state: .* score=\([0-9]*\) .*/\1/p')
if [ "${s:-0}" -ge 20 ]; then ok "the bot reaches $s (>= 20)"; else ko "the bot only reached $s: $(echo "$out" | grep state)"; fi

# determinism: the same inputs, the same run (state hash and picture), twice; also with 4 players
: > "$T/rhythm.input"
i=30; while [ $i -lt 1500 ]; do echo "$i tap A" >> "$T/rhythm.input"; i=$((i + 23)); done
for k in 1 2; do
    run --frames 1500 --input "$T/rhythm.input" --opt seed=3 --png "$T/det$k.png" | grep "state:" > "$T/det$k.txt"
    run --frames 2000 --opt bot=1 --png "$T/bot$k.png" | grep "state:" > "$T/bot$k.txt"
    run --frames 1400 --input "$T/play4.input" --opt seed=3 --png "$T/det4p$k.png" | grep "state:" > "$T/det4p$k.txt"
    run --frames 2000 --opt bot=4 --opt botruns=2 --png "$T/bot4p$k.png" | grep "state:" > "$T/bot4p$k.txt"
done
if cmp -s "$T/det1.txt" "$T/det2.txt" && cmp -s "$T/det1.png" "$T/det2.png"; then ok "determinism: a scripted run twice, same state and picture"
else ko "determinism (script)"; fi
if cmp -s "$T/bot1.txt" "$T/bot2.txt" && cmp -s "$T/bot1.png" "$T/bot2.png"; then ok "determinism: a bot run twice, same state and picture"
else ko "determinism (bot)"; fi
if cmp -s "$T/det4p1.txt" "$T/det4p2.txt" && cmp -s "$T/det4p1.png" "$T/det4p2.png"; then ok "determinism: 4 scripted players twice, same state and picture"
else ko "determinism (4 scripted players)"; fi
if cmp -s "$T/bot4p1.txt" "$T/bot4p2.txt" && cmp -s "$T/bot4p1.png" "$T/bot4p2.png"; then ok "determinism: 4 bots twice, same state and picture"
else ko "determinism (4 bots)"; fi
exit $fail

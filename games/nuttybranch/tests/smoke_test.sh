#!/bin/sh
# Nutty Branch smoke tests: scripted runs through the headless runner (the title with players joining, play, game
# over, back to the title, pause, save RAM, 1-4 players) and determinism.
#   smoke_test.sh <headless binary> <tmp dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE.
H=$1
T=${2:-/tmp}/nuttybranch-tests
mkdir -p "$T"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() {   # check <name> <expected regexp> <output>
    if echo "$3" | grep -q "$2"; then ok "$1"; else ko "$1: expected '$2' in: $(echo "$3" | grep 'state:\|strict')"; fi
}
run() { $H --opt music=0 --opt dump=1 "$@" 2>&1; }

# screens: st=0 the title, 1 play, 2 the last squirrel falling, 3 the game-over panel (no "get ready")
out=$(run --frames 300 --opt strict=1)
check "title: nothing happens without input" "state: st=0 .*score=0 .*runs=0 " "$out"
if echo "$out" | grep -q "rs strict"; then ko "strict mode warnings: $(echo "$out" | grep 'rs strict')"; else ok "no strict-mode warning (VRAM, sprites, voices, samples...)"; fi

printf "30 tap A\n" > "$T/start.input"
out=$(run --frames 60 --input "$T/start.input")
check "A on the title starts the run at once" "state: st=1 .*s=[1-9]" "$out"
printf "30 tap LEFT\n" > "$T/left.input"
out=$(run --frames 60 --input "$T/left.input")
check "Left starts it too (an extra start input)" "state: st=1 " "$out"
printf "30 tap A\n" > "$T/start.input"
out=$(run --frames 4000 --input "$T/start.input" --opt seed=2)
check "no more input: the squirrel falls off, game over" "state: st=3 .*runs=1 " "$out"
f=$(echo "$out" | sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p')
printf "30 tap A\n$((f + 40)) tap A\n" > "$T/retry.input"
out=$(run --frames $((f + 50)) --input "$T/retry.input" --opt seed=2)
check "game over: one press goes back to the title (the only menu)" "state: st=0 .*runs=1 " "$out"
printf "30 tap A\n$((f + 40)) tap SELECT\n" > "$T/back.input"
out=$(run --frames $((f + 50)) --input "$T/back.input" --opt seed=2)
check "Select on the panel: back to the title too" "state: st=0 .*runs=1 " "$out"
printf "30 tap A\n$((f + 40)) tap A\n$((f + 60)) tap A\n" > "$T/again.input"
out=$(run --frames $((f + 90)) --input "$T/again.input" --opt seed=2)
check "...and A on the title starts the next run" "state: st=1 .*runs=1 .*s=[1-9]" "$out"

printf "30 tap A\n90 tap SELECT\n" > "$T/pause.input"
a=$(run --frames 120 --input "$T/pause.input" | sed -n 's/.* s=\([0-9]*\) paused=\([0-9]\).*/\1 \2/p')
b=$(run --frames 300 --input "$T/pause.input" | sed -n 's/.* s=\([0-9]*\) paused=\([0-9]\).*/\1 \2/p')
if [ "$a" = "$b" ] && [ "${a#* }" = "1" ]; then ok "Select pauses (the world is frozen: s ${a% *})"; else ko "pause: '$a' vs '$b'"; fi

# the jump: A hops (the squirrel leaves the bark), the steer moves it round the branch
printf "30 tap A\n100 A\n" > "$T/jump.input"
out=$(run --frames 104 --input "$T/jump.input" --opt seed=4)
check "A jumps" "state: st=1 " "$out"

rm -f "$T/save.srm"
out=$(run --frames 6000 --opt bot=1 --opt seed=8 --opt botstop=60 --sram "$T/save.srm")
d=$(echo "$out" | sed -n 's/.*state: .* dist=\([0-9]*\) .*/\1/p')
out=$(run --frames 10 --sram "$T/save.srm")
check "save RAM: the best distance ($d m) persists" "best=$d bestscore=[0-9]* runs=0" "$out"

# players 2-4 join on the title (A on their pad), leave with B; only P1 starts
printf "20 P2 tap A\n" > "$T/join.input"
out=$(run --frames 60 --input "$T/join.input")
check "player 2 joins on the title" "state: st=0 players=2 " "$out"
printf "20 P2 tap A\n40 P2 tap A\n60 P2 tap LEFT\n" > "$T/p2start.input"
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

# 4 scripted pads: join on the title, P1 starts, everyone steers and jumps to a rhythm
printf "20 P2 tap A\n25 P3 tap A\n30 P4 tap A\n60 tap A\n" > "$T/play4.input"
i=80; while [ $i -lt 1400 ]; do
    echo "$i tap A" >> "$T/play4.input"; echo "$((i + 5)) P2 tap A" >> "$T/play4.input"
    echo "$((i + 9)) P3 tap RIGHT" >> "$T/play4.input"; echo "$((i + 13)) P4 tap LEFT" >> "$T/play4.input"
    i=$((i + 29))
done
out=$(run --frames 1400 --input "$T/play4.input" --opt seed=3 --opt strict=1)
check "4 scripted pads join and play" "state: st=[123] players=4 " "$out"
if echo "$out" | grep -q "rs strict"; then ko "4 players: strict mode warnings: $(echo "$out" | grep 'rs strict')"; else ok "4 players: no strict-mode warning"; fi

out=$(run --frames 2500 --opt bot=2 --opt seed=6 --opt players=2)
check "2 players: both squirrels run" "players=2 .*dists=[1-9][0-9]*,[1-9][0-9]* " "$out"
out=$(run --frames 2500 --opt bot=4 --opt seed=6 --opt strict=1)
check "4 players (4 bots): all four run" "players=4 .*dists=[1-9][0-9]*,[1-9][0-9]*,[1-9][0-9]*,[1-9][0-9]* " "$out"
if echo "$out" | grep -q "rs strict"; then ko "4 bots: strict mode warnings: $(echo "$out" | grep 'rs strict')"; else ok "4 bots: no strict-mode warning"; fi

# determinism: the same inputs, the same run (state hash and picture), twice; also with 4 players
: > "$T/rhythm.input"
echo "30 tap A" >> "$T/rhythm.input"
i=40; while [ $i -lt 1500 ]; do
    echo "$i RIGHT" >> "$T/rhythm.input"; echo "$((i + 6)) -" >> "$T/rhythm.input"
    echo "$((i + 20)) LEFT" >> "$T/rhythm.input"; echo "$((i + 27)) -" >> "$T/rhythm.input"
    echo "$((i + 33)) tap A" >> "$T/rhythm.input"; i=$((i + 41))
done
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

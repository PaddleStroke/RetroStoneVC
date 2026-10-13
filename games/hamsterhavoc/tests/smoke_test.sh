#!/bin/sh
# Hamster Havoc smoke tests: scripted runs through the headless runner (the title with players joining, the start, the
# intro, moving and firing, the game over, back to the title, pause, save RAM, 1-4 players), the bot hook and
# determinism.
#   smoke_test.sh <headless binary> <tmp dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/hamsterhavoc/LICENSE.
H=$1
T=${2:-/tmp}/hamsterhavoc-tests
mkdir -p "$T"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() {   # check <name> <expected regexp> <output>
    if echo "$3" | grep -q "$2"; then ok "$1"; else ko "$1: expected '$2' in: $(echo "$3" | grep 'state:\|strict')"; fi
}
run() { $H --opt music=0 --opt dump=1 --opt seed=4 "$@" 2>&1; }
over_frame() { echo "$1" | sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p'; }
field() { echo "$1" | grep "state:" | sed -n "s/.* $2=\([^ ]*\).*/\1/p"; }

# screens: st=0 the title, 1 play, 2 the last ship popping, 3 the game-over panel (no "get ready")
# phases: 0 ready, 1 the wave's intro, 2 play, 3 the pause after a wave, 4 over; ships (hs): 1 play, 2 pop, 4 down
out=$(run --frames 300 --opt strict=1)
check "title: nothing happens without input" "state: st=0 .*score=0 .*runs=0 " "$out"
if echo "$out" | grep -q "rs strict"; then ko "strict mode warnings: $(echo "$out" | grep 'rs strict')"; else ok "no strict-mode warning (VRAM, sprites, voices...)"; fi

printf "30 tap A\n" > "$T/start.input"
out=$(run --frames 40 --input "$T/start.input")
check "A on the title starts the run at once: WAVE 1, the mice fly in" "state: st=1 .*wave=1 phase=1 alive=55 " "$out"
printf "30 tap LEFT\n" > "$T/left.input"
out=$(run --frames 40 --input "$T/left.input")
check "Left starts it too (an extra start input)" "state: st=1 .*phase=1 " "$out"
out=$(run --frames 400 --input "$T/start.input")
check "after the intro the formation marches" "state: st=1 .*phase=2 alive=55 fx=[1-9][0-9]* " "$out"
[ "$(field "$out" fx)" != 52 ] || ko "the formation did not move"

# the d-pad moves, A held fires (one seed on screen at a time)
printf "30 tap A\n130 RIGHT\n170 -\n" > "$T/move.input"
out=$(run --frames 180 --input "$T/move.input")
check "Right held 40 frames: 60 px (1.5 px a frame)" "state: st=1 .* x=220 shots=0 " "$out"
printf "30 tap A\n130 A\n260 -\n" > "$T/fire.input"
out=$(run --frames 270 --input "$T/fire.input")
s=$(field "$out" shots)
if [ "${s:-0}" -ge 4 ] && [ "${s:-0}" -le 10 ] && [ "$(field "$out" score)" -gt 0 ]; then
    ok "A held fires as each seed is gone ($s seeds in 130 frames) and pops mice (score $(field "$out" score))"
else ko "firing: $(echo "$out" | grep state:)"; fi

# the game over: the bot stops dodging from 100 points (botstop), its three ships pop
out=$(run --frames 2000 --opt bot=1 --opt botstop=100)
check "three ships lost: game over" "state: st=3 .*runs=1 .*hs=4 lives=0 " "$out"
f=$(over_frame "$out")
[ -n "$f" ] || { ko "the bot run did not end"; f=1500; }
printf "%d tap A\n" $((f + 50)) > "$T/retry.input"
out=$(run --frames $((f + 60)) --opt bot=1 --opt botstop=100 --input "$T/retry.input")
check "game over: one press goes back to the title (the only menu)" "state: st=0 .*runs=1 " "$out"
printf "%d tap SELECT\n" $((f + 50)) > "$T/back.input"
out=$(run --frames $((f + 60)) --opt bot=1 --opt botstop=100 --input "$T/back.input")
check "Select on the panel: back to the title too" "state: st=0 .*runs=1 " "$out"
printf "%d tap A\n" $((f + 10)) > "$T/early.input"
out=$(run --frames $((f + 20)) --opt bot=1 --opt botstop=100 --input "$T/early.input")
check "...not before the panel's lock" "state: st=3 .*runs=1 " "$out"
printf "%d tap A\n%d tap A\n" $((f + 50)) $((f + 70)) > "$T/again.input"
out=$(run --frames $((f + 80)) --opt bot=1 --opt botstop=100 --input "$T/again.input")
check "...and A on the title starts the next run" "state: st=1 .*runs=1 .*wave=1 phase=1 " "$out"

printf "30 tap A\n200 tap SELECT\n" > "$T/pause.input"
a=$(run --frames 260 --input "$T/pause.input" | sed -n 's/.*\(fx=.* x=[0-9]*\).* paused=\([0-9]\) hash=\([0-9a-f]*\).*/\1 \2 \3/p')
b=$(run --frames 400 --input "$T/pause.input" | sed -n 's/.*\(fx=.* x=[0-9]*\).* paused=\([0-9]\) hash=\([0-9a-f]*\).*/\1 \2 \3/p')
if [ -n "$a" ] && [ "$a" = "$b" ] && echo "$a" | grep -q " 1 "; then ok "Select pauses (the world is frozen: $a)"
else ko "pause: '$a' vs '$b'"; fi

rm -f "$T/save.srm"
out=$(run --frames 2000 --opt bot=1 --opt botstop=100 --sram "$T/save.srm")
s=$(field "$out" score)
out=$(run --frames 10 --sram "$T/save.srm")
check "save RAM: the best score ($s) persists" "best=${s:-x} runs=0" "$out"

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

# 4 scripted pads: join on the title, P1 starts, everyone moves and fires to a rhythm
printf "20 P2 tap A\n25 P3 tap A\n30 P4 tap A\n60 tap A\n" > "$T/play4.input"
i=150; while [ $i -lt 1400 ]; do
    echo "$i A+LEFT" >> "$T/play4.input"; echo "$((i + 30)) A+RIGHT" >> "$T/play4.input"
    echo "$((i + 7)) P2 A+RIGHT" >> "$T/play4.input"; echo "$((i + 40)) P2 A" >> "$T/play4.input"
    echo "$((i + 13)) P3 A" >> "$T/play4.input"; echo "$((i + 50)) P3 LEFT" >> "$T/play4.input"
    echo "$((i + 21)) P4 A+LEFT" >> "$T/play4.input"; echo "$((i + 33)) P4 A+RIGHT" >> "$T/play4.input"
    i=$((i + 61))
done
out=$(run --frames 1400 --input "$T/play4.input" --opt strict=1)
check "4 scripted pads join and play" "state: st=[123] players=4 .*scores=[1-9][0-9]*,[1-9][0-9]*,[1-9][0-9]*,[1-9][0-9]*" "$out"
if echo "$out" | grep -q "rs strict"; then ko "4 players: strict mode warnings: $(echo "$out" | grep 'rs strict')"; else ok "4 players: no strict-mode warning"; fi

out=$(run --frames 2500 --opt bot=4 --opt strict=1)
check "4 players (4 bots): all four score" "players=4 .*scores=[1-9][0-9]*,[1-9][0-9]*,[1-9][0-9]*,[1-9][0-9]*" "$out"
if echo "$out" | grep -q "rs strict"; then ko "4 bots: strict mode warnings: $(echo "$out" | grep 'rs strict')"; else ok "4 bots: no strict-mode warning"; fi

out=$(run --frames 400 --opt bot=4 --opt ready=1 --opt wave=4 --opt strict=1)
check "the mothership: 4 hamsters fight it" "state: st=1 players=4 .*wave=4 phase=2 " "$out"
if echo "$out" | grep -q "rs strict"; then ko "the mothership: strict mode warnings: $(echo "$out" | grep 'rs strict')"; else ok "the mothership: no strict-mode warning"; fi

# the bot hook (bot_decide): it plays well: wave 3 in 4000 frames without losing a ship
out=$(run --frames 4000 --opt bot=1 --opt seed=7)
w=$(field "$out" wave)
if [ "${w:-0}" -ge 3 ] && echo "$out" | grep -q "lives=[3-9] "; then ok "the bot reaches wave $w without losing a ship"
else ko "the bot: $(echo "$out" | grep state:)"; fi

# determinism: the same inputs, the same run (state hash and picture), twice; also with 4 players
: > "$T/rhythm.input"
echo "30 tap A" >> "$T/rhythm.input"
i=130; while [ $i -lt 1500 ]; do echo "$i A+RIGHT" >> "$T/rhythm.input"; echo "$((i + 23)) A+LEFT" >> "$T/rhythm.input"; i=$((i + 47)); done
for k in 1 2; do
    run --frames 1500 --input "$T/rhythm.input" --png "$T/det$k.png" | grep "state:" > "$T/det$k.txt"
    run --frames 2000 --opt bot=1 --png "$T/bot$k.png" | grep "state:" > "$T/bot$k.txt"
    run --frames 1400 --input "$T/play4.input" --png "$T/det4p$k.png" | grep "state:" > "$T/det4p$k.txt"
    run --frames 2000 --opt bot=4 --opt botskill=3 --png "$T/bot4p$k.png" | grep "state:" > "$T/bot4p$k.txt"
done
if cmp -s "$T/det1.txt" "$T/det2.txt" && cmp -s "$T/det1.png" "$T/det2.png"; then ok "determinism: a scripted run twice, same state and picture"
else ko "determinism (script)"; fi
if cmp -s "$T/bot1.txt" "$T/bot2.txt" && cmp -s "$T/bot1.png" "$T/bot2.png"; then ok "determinism: a bot run twice, same state and picture"
else ko "determinism (bot)"; fi
if cmp -s "$T/det4p1.txt" "$T/det4p2.txt" && cmp -s "$T/det4p1.png" "$T/det4p2.png"; then ok "determinism: 4 scripted players twice, same state and picture"
else ko "determinism (4 scripted players)"; fi
if cmp -s "$T/bot4p1.txt" "$T/bot4p2.txt" && cmp -s "$T/bot4p1.png" "$T/bot4p2.png"; then ok "determinism: 4 human-like bots twice, same state and picture"
else ko "determinism (4 bots)"; fi
exit $fail

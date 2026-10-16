#!/bin/sh
# Kitty Kibble smoke tests: scripted runs through the headless runner (the title with players joining, the start, the
# room's intro, turning with the d-pad, eating, the game over, back to the title, pause, save RAM, 1-4 players), the
# bot hook and determinism.
#   smoke_test.sh <headless binary> <tmp dir> [quick]
# quick (the widescreen passes): the flow and 4 players only; the bot's long runs and determinism run once, at 320.
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/kittykibble/LICENSE.
H=$1
T=${2:-/tmp}/kittykibble-tests
Q=$3
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
strict() { if echo "$2" | grep -q "rs strict"; then ko "$1: strict mode warnings: $(echo "$2" | grep 'rs strict')"
           else ok "$1: no strict-mode warning (VRAM, sprites, voices...)"; fi; }

# screens: st=0 the title, 1 play, 2 the last kitten caught, 3 the game-over panel (no "get ready")
# phases: 0 ready, 1 the room's intro, 2 play, 3 the room clean; kittens (ks): 0 play, 1 caught, 2 out
out=$(run --frames 300 --opt strict=1)
check "title: nothing happens without input" "state: st=0 .*score=0 .*runs=0 " "$out"
strict "the title" "$out"

printf "30 tap A\n" > "$T/start.input"
out=$(run --frames 40 --input "$T/start.input")
check "A on the title starts the run at once: room 1's intro" "state: st=1 .*room=1 phase=1 " "$out"
printf "30 tap LEFT\n" > "$T/left.input"
out=$(run --frames 40 --input "$T/left.input")
check "the d-pad starts it too" "state: st=1 .*phase=1 " "$out"
out=$(run --frames 200 --input "$T/start.input" --opt strict=1)
check "after the intro the kitten runs left, eating" "state: st=1 .*phase=2 .*dir=2 .*scores=[1-9]" "$out"
strict "play" "$out"

# the d-pad: Up held from the start: the kitten turns up at the first junction (row 16 of the kitchen: column 10)
printf "30 tap A\n100 UP\n200 -\n" > "$T/up.input"
out=$(run --frames 200 --input "$T/up.input")
y=$(field "$out" y)
if [ "$(field "$out" dir)" = 3 ] && [ "${y:-999}" -lt 132 ]; then ok "Up held: the kitten turns up at the junction (y $y)"
else ko "turning up: $(echo "$out" | grep state:)"; fi
# a turn tapped just before the junction (the buffer)
printf "30 tap A\n158 tap UP\n" > "$T/buffer.input"
out=$(run --frames 200 --input "$T/buffer.input")
y=$(field "$out" y)
if [ "$(field "$out" dir)" = 3 ] && [ "${y:-999}" -lt 132 ]; then ok "a turn tapped before a junction is taken there (y $y)"
else ko "the turn buffer: $(echo "$out" | grep state:)"; fi

# the game over: the bot runs into a vacuum from 50 points (botstop): one catch and the run is over
out=$(run --frames 3000 --opt bot=1 --opt botstop=50)
check "one catch: game over" "state: st=3 .*runs=1 ks=2 " "$out"
f=$(over_frame "$out")
[ -n "$f" ] || { ko "the bot run did not end"; f=2000; }
printf "%d tap A\n" $((f + 50)) > "$T/retry.input"
out=$(run --frames $((f + 60)) --opt bot=1 --opt botstop=50 --input "$T/retry.input")
check "game over: one press goes back to the title (the only menu)" "state: st=0 .*runs=1 " "$out"
printf "%d tap SELECT\n" $((f + 50)) > "$T/back.input"
out=$(run --frames $((f + 60)) --opt bot=1 --opt botstop=50 --input "$T/back.input")
check "Select on the panel: back to the title too" "state: st=0 .*runs=1 " "$out"
printf "%d tap A\n" $((f + 10)) > "$T/early.input"
out=$(run --frames $((f + 20)) --opt bot=1 --opt botstop=50 --input "$T/early.input")
check "...not before the panel's lock" "state: st=3 .*runs=1 " "$out"
printf "%d tap A\n%d tap A\n" $((f + 50)) $((f + 70)) > "$T/again.input"
out=$(run --frames $((f + 80)) --opt bot=1 --opt botstop=50 --input "$T/again.input")
check "...and A on the title starts the next run" "state: st=1 .*runs=1 .*room=1 phase=1 " "$out"

printf "30 tap A\n250 tap SELECT\n" > "$T/pause.input"
a=$(run --frames 300 --input "$T/pause.input" | sed -n 's/.*\(left=.*\) paused=\([0-9]\) hash=\([0-9a-f]*\).*/\1 \2 \3/p')
b=$(run --frames 450 --input "$T/pause.input" | sed -n 's/.*\(left=.*\) paused=\([0-9]\) hash=\([0-9a-f]*\).*/\1 \2 \3/p')
if [ -n "$a" ] && [ "$a" = "$b" ] && echo "$a" | grep -q " 1 "; then ok "Select pauses (the world is frozen)"
else ko "pause: '$a' vs '$b'"; fi

rm -f "$T/save.srm"
out=$(run --frames 3000 --opt bot=1 --opt botstop=50 --sram "$T/save.srm")
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

# 4 scripted pads: join on the title, P1 starts, everyone steers to a rhythm
printf "20 P2 tap A\n25 P3 tap A\n30 P4 tap A\n60 tap A\n" > "$T/play4.input"
i=200; while [ $i -lt 1200 ]; do
    echo "$i UP" >> "$T/play4.input"; echo "$((i + 30)) LEFT" >> "$T/play4.input"
    echo "$((i + 7)) P2 DOWN" >> "$T/play4.input"; echo "$((i + 40)) P2 RIGHT" >> "$T/play4.input"
    echo "$((i + 13)) P3 UP" >> "$T/play4.input"; echo "$((i + 50)) P3 RIGHT" >> "$T/play4.input"
    echo "$((i + 21)) P4 DOWN" >> "$T/play4.input"; echo "$((i + 33)) P4 LEFT" >> "$T/play4.input"
    i=$((i + 61))
done
out=$(run --frames 600 --input "$T/play4.input" --opt strict=1)
check "4 scripted pads join and play" "state: st=1 players=4 .*scores=[1-9][0-9]*,[1-9][0-9]*,[1-9][0-9]*,[1-9][0-9]*" "$out"
strict "4 players" "$out"
out=$(run --frames 1500 --opt bot=4 --opt botskill=2 --opt strict=1)
check "4 players (4 bots): all four eat" "players=4 .*scores=[1-9][0-9]*,[1-9][0-9]*,[1-9][0-9]*,[1-9][0-9]*" "$out"
strict "4 bots" "$out"
[ -n "$Q" ] && exit $fail

# the other rooms (their art, their mazes)
for r in 2 3 4 5; do
    out=$(run --frames 400 --opt bot=1 --opt ready=1 --opt room=$r --opt strict=1)
    check "room $r plays" "state: st=1 .*room=$r phase=2 .*scores=[1-9]" "$out"
    strict "room $r" "$out"
done

# the bot hook (bot_decide): it plays well: the kitchen clean, still in play
out=$(run --frames 6000 --opt bot=1 --opt seed=3 --opt sound=0)
r=$(field "$out" room)
if [ "${r:-0}" -ge 2 ] && echo "$out" | grep -q "ks=0 "; then ok "the bot reaches room $r uncaught"
else ko "the bot: $(echo "$out" | grep state:)"; fi

# determinism: the same inputs, the same run (state hash and picture), twice; also with 4 players
: > "$T/rhythm.input"
echo "30 tap A" >> "$T/rhythm.input"
i=150; while [ $i -lt 1500 ]; do echo "$i UP" >> "$T/rhythm.input"; echo "$((i + 23)) RIGHT" >> "$T/rhythm.input"
    echo "$((i + 41)) DOWN" >> "$T/rhythm.input"; echo "$((i + 57)) LEFT" >> "$T/rhythm.input"; i=$((i + 71)); done
for k in 1 2; do
    run --frames 1500 --input "$T/rhythm.input" --png "$T/det$k.png" | grep "state:" > "$T/det$k.txt"
    run --frames 2000 --opt bot=1 --png "$T/bot$k.png" | grep "state:" > "$T/bot$k.txt"
    run --frames 1200 --input "$T/play4.input" --png "$T/det4p$k.png" | grep "state:" > "$T/det4p$k.txt"
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

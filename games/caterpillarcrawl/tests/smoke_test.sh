#!/bin/sh
# Caterpillar Crawl smoke tests: scripted runs through the headless runner (the title with players joining, the start,
# turning, the wall and the game over, back to the title, pause, save RAM, 1-4 players, the arena), the bot hook and
# determinism.
#   smoke_test.sh <headless binary> <tmp dir> [quick]      (quick: without the determinism runs)
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/caterpillarcrawl/LICENSE.
H=$1
T=${2:-/tmp}/caterpillarcrawl-tests
QUICK=$3
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

# screens: st=0 the title, 1 play, 2 the crash (before the panel), 3 the game-over panel (no "get ready")
# caterpillars (hs): 1 ready, 2 crawling, 3 hit, 5 down, 7 waiting; directions: 0 right, 1 down, 2 left, 3 up
out=$(run --frames 300 --opt strict=1)
check "title: nothing happens without input" "state: st=0 .*score=0 .*runs=0 " "$out"
if echo "$out" | grep -q "rs strict"; then ko "strict mode warnings: $(echo "$out" | grep 'rs strict')"; else ok "no strict-mode warning (VRAM, sprites, voices...)"; fi

printf "30 tap A\n" > "$T/start.input"
out=$(run --frames 61 --input "$T/start.input")
check "A on the title starts at once: it crawls right, a cell every 10 frames" "state: st=1 .*hs=2 len=4 dir=0 hx=9 hy=6 " "$out"
printf "30 tap UP\n" > "$T/up.input"
out=$(run --frames 61 --input "$T/up.input")
check "Up starts it too, and is its first turn" "state: st=1 .*hs=2 len=4 dir=3 hx=5 hy=2 " "$out"
printf "30 tap A\n55 tap DOWN\n" > "$T/turn.input"
out=$(run --frames 81 --input "$T/turn.input")
check "Down turns it at the next step" "state: st=1 .*dir=1 hx=8 hy=9 " "$out"
printf "30 tap A\n55 tap LEFT\n" > "$T/back.input"
out=$(run --frames 81 --input "$T/back.input")
check "never straight back into itself (Left is ignored)" "state: st=1 .*hs=2 len=4 dir=0 hx=11 hy=6 " "$out"
printf "30 tap A\n53 tap UP\n56 tap LEFT\n" > "$T/uturn.input"
out=$(run --frames 81 --input "$T/uturn.input")
check "a quick Up, Left: a U-turn in two steps" "state: st=1 .*hs=2 len=4 dir=2 hx=6 hy=5 " "$out"

# the game over: no input, it crawls into the right wall (one hit)
out=$(run --frames 400 --input "$T/start.input")
check "into the wall: game over" "state: st=3 .*runs=1 .*hs=5 " "$out"
f=$(over_frame "$out")
[ -n "$f" ] || { ko "the run did not end"; f=300; }
printf "30 tap A\n%d tap A\n" $((f + 50)) > "$T/retry.input"
out=$(run --frames $((f + 60)) --input "$T/retry.input")
check "game over: one press goes back to the title (the only menu)" "state: st=0 .*runs=1 " "$out"
printf "30 tap A\n%d tap SELECT\n" $((f + 50)) > "$T/sel.input"
out=$(run --frames $((f + 60)) --input "$T/sel.input")
check "Select on the panel: back to the title too" "state: st=0 .*runs=1 " "$out"
printf "30 tap A\n%d tap A\n" $((f + 10)) > "$T/early.input"
out=$(run --frames $((f + 20)) --input "$T/early.input")
check "...not before the panel's lock" "state: st=3 .*runs=1 " "$out"
printf "30 tap A\n%d tap A\n%d tap A\n" $((f + 50)) $((f + 70)) > "$T/again.input"
out=$(run --frames $((f + 80)) --input "$T/again.input")
check "...and A on the title starts the next run" "state: st=1 .*runs=1 .*hs=2 len=4 " "$out"

printf "30 tap A\n60 tap SELECT\n" > "$T/pause.input"
a=$(run --frames 120 --input "$T/pause.input" | sed -n 's/.*\(len=.* hy=[0-9]*\).* paused=\([0-9]\) hash=\([0-9a-f]*\).*/\1 \2 \3/p')
b=$(run --frames 300 --input "$T/pause.input" | sed -n 's/.*\(len=.* hy=[0-9]*\).* paused=\([0-9]\) hash=\([0-9a-f]*\).*/\1 \2 \3/p')
if [ -n "$a" ] && [ "$a" = "$b" ] && echo "$a" | grep -q " 1 "; then ok "Select pauses (the world is frozen: $a)"
else ko "pause: '$a' vs '$b'"; fi
printf "30 tap A\n60 tap START\n70 tap UP\n100 tap START\n" > "$T/pause2.input"
out=$(run --frames 140 --input "$T/pause2.input")
check "Start pauses too; a press while paused does nothing" "state: st=1 .*dir=0 hx=12 hy=6 .*paused=0 " "$out"

rm -f "$T/save.srm"
out=$(run --frames 3000 --opt bot=1 --opt botstop=8 --sram "$T/save.srm")
s=$(field "$out" score)
check "the bot's botstop: a game over" "state: st=3 .*runs=1 " "$out"
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

# the arena: 4 scripted pads join, P1 starts: ROUND 1, then everyone crawls and turns to a rhythm
printf "20 P2 tap A\n25 P3 tap A\n30 P4 tap A\n60 tap A\n" > "$T/play4.input"
out=$(run --frames 140 --input "$T/play4.input")
check "2-4 players: the arena, ROUND 1 (they wait)" "state: st=1 players=4 .*hs=7,7,7,7 .*phase=4 round=1 " "$out"
i=160; while [ $i -lt 1400 ]; do
    echo "$i tap DOWN" >> "$T/play4.input"; echo "$((i + 30)) tap RIGHT" >> "$T/play4.input"
    echo "$((i + 7)) P2 tap UP" >> "$T/play4.input"; echo "$((i + 40)) P2 tap LEFT" >> "$T/play4.input"
    echo "$((i + 13)) P3 tap LEFT" >> "$T/play4.input"; echo "$((i + 50)) P3 tap DOWN" >> "$T/play4.input"
    echo "$((i + 21)) P4 tap RIGHT" >> "$T/play4.input"; echo "$((i + 33)) P4 tap UP" >> "$T/play4.input"
    i=$((i + 61))
done
out=$(run --frames 1400 --input "$T/play4.input" --opt strict=1)
check "4 scripted pads crawl the arena, rounds are won" "state: st=[123] players=4 .*round=[2-9] " "$out"
if echo "$out" | grep -q "rs strict"; then ko "4 players: strict mode warnings: $(echo "$out" | grep 'rs strict')"; else ok "4 players: no strict-mode warning"; fi

out=$(run --frames 2500 --opt bot=4 --opt strict=1)
check "4 players (4 bots): a match goes on" "players=4 .*round=[2-9] " "$out"
if echo "$out" | grep -q "rs strict"; then ko "4 bots: strict mode warnings: $(echo "$out" | grep 'rs strict')"; else ok "4 bots: no strict-mode warning"; fi
out=$(run --frames 12000 --opt bot=2)
check "2 bots: the match ends, the results" "state: st=3 players=2 .*runs=1 .*wins=\(3,[0-2]\|[0-2],3\) " "$out"

# the bot hook (bot_decide): it plays well: 20 leaves and more in 4000 frames, alive
out=$(run --frames 4000 --opt bot=1 --opt seed=7 --opt strict=1)
l=$(field "$out" leaves)
if [ "${l:-0}" -ge 20 ] && echo "$out" | grep -q "hs=2 "; then ok "the bot eats $l leaves and crawls on (garden $(field "$out" garden))"
else ko "the bot: $(echo "$out" | grep state:)"; fi
if echo "$out" | grep -q "rs strict"; then ko "the butterfly: strict mode warnings"; else ok "the butterfly and the next garden: no strict-mode warning"; fi
out=$(run --frames 200 --opt bot=1 --opt ready=1 --opt garden=5)
check "garden=5: the fifth garden" "state: st=1 .*garden=5 " "$out"

[ "$QUICK" = quick ] && exit $fail

# determinism: the same inputs, the same run (state hash and picture), twice; also with 4 players
: > "$T/rhythm.input"
echo "30 tap A" >> "$T/rhythm.input"
i=70; while [ $i -lt 1500 ]; do echo "$i tap UP" >> "$T/rhythm.input"; echo "$((i + 23)) tap RIGHT" >> "$T/rhythm.input"
    echo "$((i + 45)) tap DOWN" >> "$T/rhythm.input"; echo "$((i + 66)) tap RIGHT" >> "$T/rhythm.input"; i=$((i + 90)); done
for k in 1 2; do
    run --frames 1500 --input "$T/rhythm.input" --png "$T/det$k.png" | grep "state:" > "$T/det$k.txt"
    run --frames 2500 --opt bot=1 --png "$T/bot$k.png" | grep "state:" > "$T/bot$k.txt"
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

#!/bin/sh
# Jelly Jam smoke tests: scripted runs through the headless runner (the title with players joining, the start, the
# duel's banner, moving, turning and dropping the pair on the pad, the game over, back to the title, pause, save RAM,
# 2-4 players), the bot hook and determinism.
#   smoke_test.sh <headless binary> <tmp dir> [quick]
# quick (the widescreen passes): the flow and 4 players only; the bot's runs and determinism run once, at 320.
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/jellyjam/LICENSE.
H=$1
T=${2:-/tmp}/jellyjam-tests
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

# screens: st=0 the title, 1 play (phase 0 the banner, 1 play, 2 won, 3 over), 2 the lost field, 3 the game-over panel
out=$(run --frames 300 --opt strict=1)
check "title: nothing happens without input" "state: st=0 .*score=0 .*runs=0 " "$out"
nostrict "the title" "$out"

printf "30 tap A\n" > "$T/start.input"
out=$(run --frames 40 --input "$T/start.input")
check "A on the title starts a duel against chef 1 (its banner first)" "state: st=1 players=1 score=0 level=1 round=1 phase=0 mode=0 " "$out"
printf "30 tap START\n" > "$T/start2.input"
out=$(run --frames 40 --input "$T/start2.input")
check "Start starts it too (an extra start input)" "state: st=1 " "$out"

# the pair comes in after the banner (frame 30 + 110): left moves it, A turns it clockwise, B counter-clockwise
printf "30 tap A\n150 tap LEFT\n" > "$T/left.input"
out=$(run --frames 160 --input "$T/left.input")
check "Left: the pair one column left" "state: st=1 .*phase=1 .*col=1 rot=0 fstate=2 " "$out"
printf "30 tap A\n150 LEFT\n180 -\n" > "$T/das.input"
out=$(run --frames 190 --input "$T/das.input")
check "Left held: it slides on to the wall" "state: st=1 .*col=0 rot=0 fstate=2 " "$out"
printf "30 tap A\n150 tap A\n" > "$T/cw.input"
out=$(run --frames 160 --input "$T/cw.input")
check "A: turned clockwise" "state: st=1 .*col=2 rot=1 fstate=2 " "$out"
printf "30 tap A\n150 tap B\n" > "$T/ccw.input"
out=$(run --frames 160 --input "$T/ccw.input")
check "B: turned counter-clockwise" "state: st=1 .*col=2 rot=3 fstate=2 " "$out"
printf "30 tap A\n150 DOWN\n200 -\n" > "$T/drop.input"
out=$(run --frames 200 --input "$T/drop.input")
s=$(field "$out" score); n=$(field "$out" count)
if [ "${s:-0}" -ge 10 ] && [ "${n:-0}" -ge 2 ]; then ok "Down: the pair drops and lands (score $s, $n jellies)"; else ko "soft drop: score $s, count $n"; fi

# the game over: the bot gives up after 600 frames of the duel
out=$(run --frames 6000 --opt bot=1 --opt botstop=600)
check "the player's field fills up: game over" "state: st=3 .*runs=1 " "$out"
f=$(over_frame "$out")
[ -n "$f" ] || { ko "the bot run did not end"; f=3000; }
printf "%d tap A\n" $((f + 50)) > "$T/retry.input"
out=$(run --frames $((f + 60)) --opt bot=1 --opt botstop=600 --input "$T/retry.input")
check "game over: one press goes back to the title (the only menu)" "state: st=0 .*runs=1 " "$out"
printf "%d tap A\n" $((f + 10)) > "$T/early.input"
out=$(run --frames $((f + 20)) --opt bot=1 --opt botstop=600 --input "$T/early.input")
check "...not before the panel's lock" "state: st=3 .*runs=1 " "$out"
printf "%d tap A\n%d tap A\n" $((f + 50)) $((f + 70)) > "$T/again.input"
out=$(run --frames $((f + 80)) --opt bot=1 --opt botstop=600 --input "$T/again.input")
check "...and A on the title starts the next run" "state: st=1 players=1 score=0 level=1 .*runs=1 " "$out"

printf "30 tap A\n150 tap SELECT\n" > "$T/pause.input"
a=$(run --frames 300 --input "$T/pause.input" | grep "state:" | sed 's/runs=[0-9]* //')
b=$(run --frames 500 --input "$T/pause.input" | grep "state:" | sed 's/runs=[0-9]* //')
if [ -n "$a" ] && [ "$a" = "$b" ] && echo "$a" | grep -q "paused=1"; then ok "Select pauses (the pair hangs in the air)"
else ko "pause: '$a' vs '$b'"; fi

# players 2-4 join on the title (A on their pad), leave with B; only P1 starts
printf "20 P2 tap A\n25 P3 tap A\n30 P4 tap A\n40 P2 tap B\n60 P2 tap A\n70 P2 tap START\n" > "$T/join.input"
out=$(run --frames 80 --input "$T/join.input")
check "players join and leave on the title; only P1 starts" "state: st=0 players=4 " "$out"
out=$(run --frames 2400 --opt bot=4 --opt strict=1)
check "4 players (4 bots): 4 fields side by side, all scoring" "players=4 .*mode=1 .*scores=[1-9][0-9]*,[1-9][0-9]*,[1-9][0-9]*,[1-9][0-9]* " "$out"
nostrict "4 fields" "$out"
out=$(run --frames 6000 --opt bot=2 --opt strict=1 --opt botstop=900)
check "2 players: a round is won" "players=2 .*mode=1 .*wins=\(1,0\|0,1\|2,0\|0,2\|1,1\|2,1\|1,2\)" "$out"
nostrict "2 fields" "$out"
out=$(run --frames 1200 --opt bot=3 --opt strict=1)
check "3 players" "players=3 .*mode=1 " "$out"
nostrict "3 fields" "$out"

[ -n "$Q" ] && exit $fail

rm -f "$T/save.srm"
out=$(run --frames 6000 --opt bot=1 --opt botstop=600 --sram "$T/save.srm")
s=$(field "$out" score)
out=$(run --frames 10 --sram "$T/save.srm")
check "save RAM: the best score ($s) persists" "best=${s:-x} runs=0" "$out"

# the bot hook (ai_play): it plays well: it beats chef 1 within 3 minutes
out=$(run --frames 10800 --opt bot=1 --opt seed=7 --opt ready=1)
check "the bot beats chef 1 and goes on" "state: st=1 .*level=[2-9]" "$out"
check "...with chains" "field 1 chain [3-9]" "$out"

# determinism: the same inputs, the same run (state hash and picture), twice; also 4 human-like bots
: > "$T/rhythm.input"
echo "30 tap A" >> "$T/rhythm.input"
i=150; while [ $i -lt 2400 ]; do echo "$i tap LEFT" >> "$T/rhythm.input"; echo "$((i + 9)) tap A" >> "$T/rhythm.input"; echo "$((i + 20)) DOWN" >> "$T/rhythm.input"; echo "$((i + 60)) -" >> "$T/rhythm.input"; i=$((i + 71)); done
for k in 1 2; do
    run --frames 2400 --input "$T/rhythm.input" --png "$T/det$k.png" | grep "state:" > "$T/det$k.txt"
    run --frames 3000 --opt bot=4 --opt botskill=3 --png "$T/bot4p$k.png" | grep "state:" > "$T/bot4p$k.txt"
done
if cmp -s "$T/det1.txt" "$T/det2.txt" && cmp -s "$T/det1.png" "$T/det2.png"; then ok "determinism: a scripted run twice, same state and picture"
else ko "determinism (script)"; fi
if cmp -s "$T/bot4p1.txt" "$T/bot4p2.txt" && cmp -s "$T/bot4p1.png" "$T/bot4p2.png"; then ok "determinism: 4 human-like bots twice, same state and picture"
else ko "determinism (4 bots)"; fi
exit $fail

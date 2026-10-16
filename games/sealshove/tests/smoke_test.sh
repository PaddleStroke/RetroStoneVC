#!/bin/sh
# Seal Shove smoke tests: scripted runs through the headless runner (the title with players joining, the start,
# waddling and sliding on the pad, the game over when a match is lost, back to the title, pause, save RAM, 1-4
# players), the bot hook and determinism.
#   smoke_test.sh <headless binary> <tmp dir> [quick]
# quick (the widescreen passes): the flow and 4 players only; the bot's runs and determinism run once, at 320.
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/sealshove/LICENSE.
H=$1
T=${2:-/tmp}/sealshove-tests
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

# screens: st=0 the title, 1 a match (phase 0 the round's intro, 1 the fight, 2 the round won, 3 the match won),
# 2 the lost match, 3 the game-over panel or the results
out=$(run --frames 300 --opt strict=1)
check "title: nothing happens without input" "state: st=0 .*score=0 .*runs=0 " "$out"
nostrict "the title" "$out"

printf "30 tap A\n" > "$T/start.input"
out=$(run --frames 40 --input "$T/start.input")
check "A on the title starts the match at once: round 1's intro" "state: st=1 .*match=1 round=1 phase=0 " "$out"
check "...the start press does not slide" "dashes=0 " "$out"
printf "30 tap DOWN\n" > "$T/down.input"
out=$(run --frames 40 --input "$T/down.input")
check "the d-pad starts it too (an extra start input)" "state: st=1 " "$out"
out=$(run --frames 125 --input "$T/start.input")
check "GO! after the intro" "state: st=1 .*phase=1 " "$out"

# the d-pad waddles (momentum: it gets going, slides on); A slides (then a cooldown)
a=$(run --frames 121 --input "$T/start.input")
printf "30 tap A\n121 RIGHT\n150 -\n" > "$T/move.input"
b=$(run --frames 151 --input "$T/move.input")
c=$(run --frames 190 --input "$T/move.input")
d1=$(( $(field "$b" x) - $(field "$a" x) )); d2=$(( $(field "$c" x) - $(field "$b" x) ))
if [ "$d1" -ge 15 ] && [ "$d1" -le 40 ] && [ "$d2" -ne 0 ]; then ok "Right held 30 frames: $d1 px (then it slides on, or is bumped: $d2 px)"
else ko "waddling: $d1 px, then $d2"; fi
printf "30 tap A\n125 tap A\n" > "$T/slide.input"
out=$(run --frames 150 --input "$T/slide.input")
check "A: a belly slide, then its cooldown" "state: st=1 .*dashcd=[1-9]" "$out"

# the game over: player 1 walks off the ice every round (botquit), the bots win the match
out=$(run --frames 20000 --opt bot=1 --opt botquit=1)
check "a match lost: game over" "state: st=3 .*runs=1 .*matches=0 " "$out"
f=$(over_frame "$out")
[ -n "$f" ] || { ko "the bot run did not end"; f=9000; }
printf "%d tap A\n" $((f + 50)) > "$T/retry.input"
out=$(run --frames $((f + 60)) --opt bot=1 --opt botquit=1 --input "$T/retry.input")
check "game over: one press goes back to the title (the only menu)" "state: st=0 .*runs=1 " "$out"
printf "%d tap A\n" $((f + 10)) > "$T/early.input"
out=$(run --frames $((f + 20)) --opt bot=1 --opt botquit=1 --input "$T/early.input")
check "...not before the panel's lock" "state: st=3 .*runs=1 " "$out"
printf "%d tap A\n%d tap A\n" $((f + 50)) $((f + 70)) > "$T/again.input"
out=$(run --frames $((f + 80)) --opt bot=1 --opt botquit=1 --input "$T/again.input")
check "...and A on the title starts the next match" "state: st=1 .*runs=1 match=1 round=1 " "$out"

printf "30 tap A\n125 RIGHT\n200 tap SELECT\n" > "$T/pause.input"
a=$(run --frames 260 --input "$T/pause.input" | sed -n 's/.* x=\([0-9]*\) y=\([0-9]*\) .* paused=\([0-9]\).* hash=\([0-9a-f]*\).*/\1 \2 \3 \4/p')
b=$(run --frames 400 --input "$T/pause.input" | sed -n 's/.* x=\([0-9]*\) y=\([0-9]*\) .* paused=\([0-9]\).* hash=\([0-9a-f]*\).*/\1 \2 \3 \4/p')
if [ -n "$a" ] && [ "$a" = "$b" ] && echo "$a" | grep -q " 1 [0-9a-f]*$"; then ok "Select pauses (the floe is frozen)"
else ko "pause: '$a' vs '$b'"; fi

# players 2-4 join on the title (A on their pad), leave with B; only P1 starts
printf "20 P2 tap A\n25 P3 tap A\n30 P4 tap A\n40 P2 tap B\n60 P2 tap A\n70 P2 tap DOWN\n" > "$T/join.input"
out=$(run --frames 80 --input "$T/join.input")
check "players join and leave on the title; only P1 starts" "state: st=0 players=4 " "$out"
printf "20 P2 tap A\n40 tap A\n" > "$T/two.input"
out=$(run --frames 300 --input "$T/two.input")
check "2 players: the bots fill the floe (4 seals)" "state: st=1 players=2 .*states=[01],[01],[01],[01] " "$out"
out=$(run --frames 3000 --opt bot=4 --opt strict=1)
check "4 players (4 bots): they shove each other off" "players=4 .*falls=[1-9]" "$out"
nostrict "4 seals and the floe breaking" "$out"
out=$(run --frames 3000 --opt bot=4 --opt seed=11)
check "...and rounds are won" "players=4 .*round=[2-9]" "$out"

[ -n "$Q" ] && exit $fail

# 2-4 players: one match, then the results
out=$(run --frames 30000 --opt bot=2 --opt players=2)
check "2 players: the match ends with the results" "state: st=3 players=2 .*runs=1 " "$out"

rm -f "$T/save.srm"
out=$(run --frames 30000 --opt bot=1 --opt botstop=60 --sram "$T/save.srm")
s=$(field "$out" score)
out2=$(run --frames 10 --sram "$T/save.srm")
check "save RAM: the best score ($s) persists" "best=${s:-x} runs=0" "$out2"

# the bot hook (bot_think): it plays well: it wins rounds against the first match's bots
out=$(run --frames 6000 --opt bot=1 --opt seed=7)
w=$(field "$out" wins); sc=$(field "$out" score)
if [ "${sc:-0}" -ge 25 ]; then ok "the bot scores $sc in 100 s (wins $w)"; else ko "the bot: score $sc, wins $w"; fi

# determinism: the same inputs, the same run (state hash and picture), twice; also 4 bots, and 4 human-like bots
: > "$T/rhythm.input"
echo "30 tap A" >> "$T/rhythm.input"
i=130; while [ $i -lt 1500 ]; do echo "$i RIGHT+UP" >> "$T/rhythm.input"; echo "$((i + 23)) LEFT+DOWN" >> "$T/rhythm.input"
    echo "$((i + 40)) tap A" >> "$T/rhythm.input"; i=$((i + 67)); done
for k in 1 2; do
    run --frames 1500 --input "$T/rhythm.input" --png "$T/det$k.png" | grep "state:" > "$T/det$k.txt"
    run --frames 2000 --opt bot=4 --png "$T/bot4p$k.png" | grep "state:" > "$T/bot4p$k.txt"
    run --frames 2000 --opt bot=4 --opt botskill=6 --png "$T/hum$k.png" | grep "state:" > "$T/hum$k.txt"
done
if cmp -s "$T/det1.txt" "$T/det2.txt" && cmp -s "$T/det1.png" "$T/det2.png"; then ok "determinism: a scripted run twice, same state and picture"
else ko "determinism (script)"; fi
if cmp -s "$T/bot4p1.txt" "$T/bot4p2.txt" && cmp -s "$T/bot4p1.png" "$T/bot4p2.png"; then ok "determinism: 4 bots twice, same state and picture"
else ko "determinism (4 bots)"; fi
if cmp -s "$T/hum1.txt" "$T/hum2.txt" && cmp -s "$T/hum1.png" "$T/hum2.png"; then ok "determinism: 4 human-like bots twice"
else ko "determinism (human-like bots)"; fi
exit $fail

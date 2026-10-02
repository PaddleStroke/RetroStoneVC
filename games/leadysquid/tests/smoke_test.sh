#!/bin/sh
# Leady Squid smoke tests: scripted runs through the headless runner, the bot and determinism.
#   smoke_test.sh <headless binary> <tmp dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/leadysquid/LICENSE.
H=$1
T=${2:-/tmp}/leadysquid-tests
mkdir -p "$T"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() {   # check <name> <expected regexp> <output>
    if echo "$3" | grep -q "$2"; then ok "$1"; else ko "$1: expected '$2' in: $(echo "$3" | grep 'state:\|strict')"; fi
}
run() { $H --opt music=0 --opt dump=1 "$@" 2>&1; }

out=$(run --frames 300 --opt strict=1)
check "title: nothing happens without input" "state: st=0 .*score=0 .*scroll=0 " "$out"
if echo "$out" | grep -q "rs strict"; then ko "strict mode warnings: $(echo "$out" | grep 'rs strict')"; else ok "no strict-mode warning (VRAM, sprites, voices...)"; fi

printf "30 tap A\n" > "$T/one.input"
out=$(run --frames 400 --input "$T/one.input")
check "one flap, then nothing: sinks to the seabed, game over with 0" "state: st=4 .*score=0 .*runs=1 " "$out"

: > "$T/rhythm.input"
i=30; while [ $i -lt 1500 ]; do echo "$i tap A" >> "$T/rhythm.input"; i=$((i + 17)); done
out=$(run --frames 1500 --input "$T/rhythm.input" --opt seed=3)
check "a blind rhythm hits an obstacle" "player 1 hit at frame" "$out"
check "... and the panel shows (then the rhythm's next taps retry at once)" "run 1 over at frame" "$out"

# no "get ready": the retry press starts the next race at once (it is P1's first flap)
printf "30 tap A\n500 tap B\n560 tap START\n" > "$T/retry.input"
out=$(run --frames 540 --input "$T/retry.input")
check "game over: one press returns to the title" "state: st=0 .*runs=1 .*scroll=0" "$out"
out=$(run --frames 600 --input "$T/retry.input")
check "... and the next flap swims (Start is a swim input)" "state: st=2 .*runs=1 " "$out"
printf "30 tap A\n500 tap SELECT\n" > "$T/back.input"
out=$(run --frames 540 --input "$T/back.input")
check "Select on the panel: back to the title" "state: st=0 .*runs=1 .*scroll=0 " "$out"
printf "30 tap UP\n" > "$T/up.input"
out=$(run --frames 60 --input "$T/up.input")
check "Up on the title starts the race too (a start input)" "state: st=2 " "$out"

printf "450 tap SELECT\n" > "$T/pause.input"
a=$(run --frames 500 --opt bot=1 --opt seed=4 --input "$T/pause.input" | sed -n 's/.*scroll=\([0-9]*\).*paused=\([0-9]\).*/\1 \2/p')
b=$(run --frames 700 --opt bot=1 --opt seed=4 --input "$T/pause.input" | sed -n 's/.*scroll=\([0-9]*\).*paused=\([0-9]\).*/\1 \2/p')
if [ "$a" = "$b" ] && [ "${a#* }" = "1" ]; then ok "Select pauses (the world is frozen: scroll ${a% *})"; else ko "pause: '$a' vs '$b'"; fi
printf "450 tap SELECT\n520 tap SELECT\n" > "$T/pause2.input"
out=$(run --frames 700 --opt bot=1 --opt seed=4 --input "$T/pause2.input")
check "Select again resumes" "paused=0 " "$out"

rm -f "$T/save.srm"
run --frames 3000 --opt bot=1 --opt seed=8 --opt botstop=12 --sram "$T/save.srm" > /dev/null
out=$(run --frames 10 --sram "$T/save.srm")
check "save RAM: the best score (12) persists" "best=12 runs=0" "$out"

# players 2-4 join on the title (A on their pad), leave with B; only P1 starts the race
printf "20 P2 tap A\n" > "$T/join.input"
out=$(run --frames 60 --input "$T/join.input")
check "player 2 joins on the title (race mode)" "state: st=0 players=2 " "$out"
printf "20 P2 tap A\n40 P2 tap A\n60 P2 tap UP\n" > "$T/p2start.input"
out=$(run --frames 80 --input "$T/p2start.input")
check "... a joined player does not start the race (P1 does)" "state: st=0 players=2 .*scroll=0 " "$out"
printf "20 P2 tap A\n25 P3 tap A\n30 P4 tap A\n" > "$T/join4.input"
out=$(run --frames 60 --input "$T/join4.input")
check "4 players join on the title" "state: st=0 players=4 " "$out"
printf "20 P2 tap A\n25 P3 tap A\n30 P4 tap A\n40 P3 tap B\n" > "$T/leave.input"
out=$(run --frames 60 --input "$T/leave.input")
check "B leaves (4 -> 3 players)" "state: st=0 players=3 " "$out"
# 4 scripted pads: join, P1 starts, each flaps to its own rhythm until it hits something; the others go on
printf "20 P2 tap A\n25 P3 tap A\n30 P4 tap A\n60 tap A\n" > "$T/race4.input"
for p in 1 2 3 4; do
    i=$((70 + p * 3)); while [ $i -lt 1600 ]; do
        if [ $p = 1 ]; then echo "$i tap A"; else echo "$i P$p tap A"; fi
        i=$((i + 14 + p * 2))
    done >> "$T/race4.input"
done
out=$(run --frames 1600 --input "$T/race4.input" --opt seed=3 --opt strict=1)
check "4 scripted pads join and race" "state: st=[234] players=4 " "$out"
check "... the squids are hit one by one (the race goes on)" "player [234] hit at frame" "$out"
if echo "$out" | grep -q "rs strict"; then ko "4 players: strict mode warnings: $(echo "$out" | grep 'rs strict')"; else ok "4 players: no strict-mode warning (sprites per line, VRAM...)"; fi
out=$(run --frames 2500 --opt bot=4 --opt seed=6)
check "a 4-squid race (4 bots): all four swim and score" "players=4 .*scores=[1-9][0-9]*,[1-9][0-9]*,[1-9][0-9]*,[1-9][0-9]*$" "$out"
out=$(run --frames 3000 --opt bot=4 --opt seed=6 --opt botstop=2)
check "... the squids sink one by one and the results show" "state: st=4 players=4 .*runs=1 " "$out"
out=$(run --frames 2500 --opt bot=2 --opt seed=6 --opt players=2)
check "race: both squids swim and score" "players=2 score=[1-9][0-9]* score2=[1-9][0-9]* " "$out"

# the bot plays from the screen only (OAM): it must reach 30 with the default settings
out=$(run --frames 5000 --opt bot=1)
s=$(echo "$out" | sed -n 's/.*state: .* score=\([0-9]*\) .*/\1/p')
if [ "${s:-0}" -ge 30 ]; then ok "the bot reaches $s (>= 30) with the default settings"; else ko "the bot only reached $s: $(echo "$out" | grep state)"; fi
all=""
for seed in 1 2 3 4 5 6 7 8 9 10; do
    s=$(run --frames 3000 --opt bot=1 --opt seed=$seed | sed -n 's/.*state: .* score=\([0-9]*\) .*/\1/p')
    all="$all $s"
    [ "${s:-0}" -ge 30 ] || { ko "the bot only reached $s on seed $seed"; }
done
ok "bot scores in 3000 frames (50 s), seeds 1-10:$all"

# determinism: the same inputs, the same run (state hash and picture), twice
for k in 1 2; do
    run --frames 1500 --input "$T/rhythm.input" --opt seed=3 --png "$T/det$k.png" | grep "state:" > "$T/det$k.txt"
    run --frames 2000 --opt bot=1 --png "$T/bot$k.png" | grep "state:" > "$T/bot$k.txt"
done
if cmp -s "$T/det1.txt" "$T/det2.txt" && cmp -s "$T/det1.png" "$T/det2.png"; then ok "determinism: a scripted run twice, same state and picture"
else ko "determinism (script)"; fi
if cmp -s "$T/bot1.txt" "$T/bot2.txt" && cmp -s "$T/bot1.png" "$T/bot2.png"; then ok "determinism: a bot run twice, same state and picture"
else ko "determinism (bot)"; fi
for k in 1 2; do
    run --frames 1600 --input "$T/race4.input" --opt seed=3 --png "$T/det4p$k.png" | grep "state:" > "$T/det4p$k.txt"
    run --frames 2000 --opt bot=4 --opt botstop=3 --opt botruns=2 --png "$T/bot4p$k.png" | grep "state:" > "$T/bot4p$k.txt"
done
if cmp -s "$T/det4p1.txt" "$T/det4p2.txt" && cmp -s "$T/det4p1.png" "$T/det4p2.png"; then ok "determinism: 4 scripted pads twice, same state and picture"
else ko "determinism (4 scripted pads)"; fi
if cmp -s "$T/bot4p1.txt" "$T/bot4p2.txt" && cmp -s "$T/bot4p1.png" "$T/bot4p2.png"; then ok "determinism: 4 bots twice (2 races), same state and picture"
else ko "determinism (4 bots)"; fi
exit $fail

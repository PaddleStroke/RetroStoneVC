#!/bin/sh
# Ladybug Laps smoke tests: scripted runs through the headless runner (the title with players joining, the start, the
# countdown, driving, pause, the results, the game over, back to the title, save RAM, 1-4 players), the bot hook and
# determinism.
#   smoke_test.sh <headless binary> <tmp dir> [quick]
# quick (the widescreen passes): the flow and 4 players only; the bot's races and determinism run once, at 320.
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/ladybuglaps/LICENSE.
H=$1
T=${2:-/tmp}/ladybuglaps-tests
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
frame_of() { echo "$1" | sed -n "s/.*$2 at frame \([0-9]*\).*/\1/p" | head -n 1; }
field() { echo "$1" | grep "state:" | sed -n "s/.* $2=\([^ ]*\).*/\1/p"; }

# screens: st=0 the title (the bots race behind it), 1 a race (phase 0 the lights, 1 racing, 2 the flag), 2 the
# results, 4 the game-over panel
out=$(run --frames 300 --opt strict=1)
check "title: nothing happens without input" "state: st=0 .*race=0 .*runs=0 " "$out"
nostrict "the title and its race" "$out"
printf "30 tap A\n" > "$T/start.input"
out=$(run --frames 40 --input "$T/start.input")
check "A on the title starts the championship: the lights" "state: st=1 players=1 race=0 track=0 rev=0 phase=0 " "$out"
printf "30 tap DOWN\n" > "$T/down.input"
out=$(run --frames 40 --input "$T/down.input")
check "the d-pad starts it too (an extra start input)" "state: st=1 " "$out"

# driving: nothing moves during the lights; then A drives east (Tea Time's start), faster than without
printf "30 tap A\n32 A\n" > "$T/early.input"
a=$(run --frames 150 --input "$T/early.input")
b=$(run --frames 150 --input "$T/start.input")
[ "$(field "$a" x)" = "$(field "$b" x)" ] && ok "the lights: A does nothing yet" || ko "moved before GO: $(field "$a" x) / $(field "$b" x)"
printf "30 tap A\n160 A\n220 -\n" > "$T/gas.input"
a=$(run --frames 221 --input "$T/gas.input")
d=$(( $(field "$a" x) - $(field "$b" x) ))
if [ "$d" -ge 50 ] && [ "$d" -le 110 ]; then ok "A held 1 s after GO: $d px east"; else ko "gas: $d px"; fi
# the pad rotates the ladybug: heading east, Right turns its nose to its right (south), Left to its left (north)
printf "30 tap A\n160 A+RIGHT\n220 -\n" > "$T/steer.input"
s=$(run --frames 221 --input "$T/steer.input")
[ "$(field "$s" y)" -gt "$(field "$a" y)" ] && ok "A + Right: it turns to its right, south ($(field "$s" y) > $(field "$a" y))" || ko "steer: $s"
printf "30 tap A\n160 A+LEFT\n220 -\n" > "$T/steerl.input"
s=$(run --frames 221 --input "$T/steerl.input")
[ "$(field "$s" y)" -lt "$(field "$a" y)" ] && ok "A + Left: to its left, north ($(field "$s" y) < $(field "$a" y))" || ko "steer left: $s"
printf "30 tap A\n160 A+DOWN\n220 -\n" > "$T/down2.input"
s=$(run --frames 221 --input "$T/down2.input")
[ "$(field "$s" hash)" = "$(field "$a" hash)" ] && ok "Down does not steer" || ko "down: $s"
printf "30 tap A\n160 UP\n220 -\n" > "$T/up.input"
s=$(run --frames 221 --input "$T/up.input")
[ "$(field "$s" hash)" = "$(field "$a" hash)" ] && ok "Up is the gas too (as A)" || ko "up: $s"
printf "30 tap A\n160 A\n200 B\n260 -\n" > "$T/brake.input"
s=$(run --frames 261 --input "$T/brake.input")
[ "$(field "$s" x)" -lt "$(( $(field "$a" x) + 20 ))" ] && ok "B brakes (and backs up)" || ko "brake: $s"

printf "30 tap A\n160 A\n200 tap SELECT\n" > "$T/pause.input"
a=$(run --frames 260 --input "$T/pause.input" | sed -n 's/.* x=\([0-9]*\) y=\([0-9]*\) ang=\([0-9]*\) t=\([0-9]*\).* paused=\([0-9]\).* hash=\([0-9a-f]*\).*/\1 \2 \3 \4 \5 \6/p')
b=$(run --frames 400 --input "$T/pause.input" | sed -n 's/.* x=\([0-9]*\) y=\([0-9]*\) ang=\([0-9]*\) t=\([0-9]*\).* paused=\([0-9]\).* hash=\([0-9a-f]*\).*/\1 \2 \3 \4 \5 \6/p')
if [ -n "$a" ] && [ "$a" = "$b" ] && echo "$a" | grep -q " 1 [0-9a-f]*$"; then ok "Select pauses (the race is frozen)"
else ko "pause: '$a' vs '$b'"; fi

# players 2-4 join on the title (A on their pad), leave with B; only P1 starts
printf "20 P2 tap A\n25 P3 tap A\n30 P4 tap A\n40 P2 tap B\n60 P2 tap A\n70 P2 tap DOWN\n" > "$T/join.input"
out=$(run --frames 80 --input "$T/join.input")
check "players join and leave on the title; only P1 starts" "state: st=0 players=4 " "$out"

# the game over: the player bot does not drive (botout): 4th, out after the first race's results
out=$(run --frames 14000 --opt bot=1 --opt botout=1 --opt strict=1)
check "4th in the first race: out, the game-over panel" "state: st=4 .*runs=1 out=1 champ=0 " "$out"
nostrict "a race on Tea Time" "$out"
f=$(frame_of "$out" "run 1 over")
[ -n "$f" ] || { ko "the run did not end"; f=6000; }
r=$(frame_of "$out" "race 1 done")
printf "%d tap A\n" $((r + 20)) > "$T/res.input"
out=$(run --frames $((r + 30)) --opt bot=1 --opt botout=1 --input "$T/res.input")
check "the results after the race (A is locked a moment)" "state: st=2 " "$out"
printf "%d tap A\n" $((f + 50)) > "$T/retry.input"
out=$(run --frames $((f + 60)) --opt bot=1 --opt botout=1 --input "$T/retry.input")
check "game over: one press goes back to the title (the only menu)" "state: st=0 .*runs=1 " "$out"
printf "%d tap A\n" $((f + 10)) > "$T/early2.input"
out=$(run --frames $((f + 20)) --opt bot=1 --opt botout=1 --input "$T/early2.input")
check "...not before the panel's lock" "state: st=4 .*runs=1 " "$out"
printf "%d tap A\n%d tap A\n" $((f + 50)) $((f + 70)) > "$T/again.input"
out=$(run --frames $((f + 80)) --opt bot=1 --opt botout=1 --input "$T/again.input")
check "...and A on the title starts the next championship" "state: st=1 .*race=0 .*runs=1 " "$out"

# 4 players (4 bots): a cup of 3 races on one screen, the camera on the leader; points for everyone
out=$(run --frames 2400 --opt bot=4 --opt strict=1)
check "4 players on one screen" "state: st=1 players=4 .*phase=1 " "$out"
nostrict "4 players" "$out"
out=$(run --frames 1500 --opt bot=1 --opt ready=1 --opt track=4 --opt strict=1)
check "track=4: the Rainy Patio" "state: st=1 .*track=4 " "$out"
nostrict "the Rainy Patio" "$out"
out=$(run --frames 200 --opt ready=1 --opt race=-1)   # bug found: race=-1 started on tracks[-1] (a crash)
check "race=-1: the championship from its first race" "state: st=1 .*race=0 track=0 " "$out"

[ -n "$Q" ] && exit $fail

rm -f "$T/save.srm"
out=$(run --frames 6000 --opt bot=1 --opt race=9 --sram "$T/save.srm")
s=$(field "$out" score)
out=$(run --frames 10 --sram "$T/save.srm")
check "save RAM: the best championship ($s points) persists" "best=${s:-x} runs=0" "$out"

# the bot hook: the player bot wins the first race and goes on
out=$(run --frames 7000 --opt bot=1)
check "the player bot finishes the first race in the top 3" "race 1 done at frame [0-9]*: track 0, places [123] " "$out"
check "...and starts the second (Crayon Desk)" "state: st=1 .*race=1 track=1 " "$out"

# determinism: the same inputs, the same run (state hash and picture), twice; also 4 human-like bots
: > "$T/rhythm.input"
echo "30 tap A" >> "$T/rhythm.input"
i=160; while [ $i -lt 1500 ]; do echo "$i A+DOWN" >> "$T/rhythm.input"; echo "$((i + 23)) A+RIGHT" >> "$T/rhythm.input"; i=$((i + 47)); done
for k in 1 2; do
    run --frames 1500 --input "$T/rhythm.input" --png "$T/det$k.png" | grep "state:" > "$T/det$k.txt"
    run --frames 2000 --opt bot=4 --opt botskill=3 --png "$T/bot4p$k.png" | grep "state:" > "$T/bot4p$k.txt"
done
if cmp -s "$T/det1.txt" "$T/det2.txt" && cmp -s "$T/det1.png" "$T/det2.png"; then ok "determinism: a scripted race twice, same state and picture"
else ko "determinism (script)"; fi
if cmp -s "$T/bot4p1.txt" "$T/bot4p2.txt" && cmp -s "$T/bot4p1.png" "$T/bot4p2.png"; then ok "determinism: 4 human-like bots twice, same state and picture"
else ko "determinism (4 bots)"; fi
exit $fail

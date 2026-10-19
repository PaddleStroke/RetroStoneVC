#!/bin/sh
# Goat Gorge smoke tests: scripted runs through the headless runner: the title and its chapters (locked, opened),
# play, a death and the room again, pause (and back to the title), the bot climbing every chapter to the results with
# no death and every berry, the records in save RAM, the next chapter opening, determinism.
#   smoke_test.sh <headless binary> <tmp dir> [quick]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/goatgorge/LICENSE.
H=$1
T=${2:-/tmp}/goatgorge-tests
Q=$3
mkdir -p "$T"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() {   # check <name> <expected regexp> <output>
    if echo "$3" | grep -q "$2"; then ok "$1"; else ko "$1: expected '$2' in: $(echo "$3" | grep 'state:\|strict\|climbed')"; fi
}
run() { $H --opt music=0 --opt dump=1 "$@" 2>&1; }

# screens: st=0 the title, 1 play, 2 the results
out=$(run --frames 300 --opt strict=1)
check "title: nothing happens without input" "state: st=0 chapter=1 .*sel=1 " "$out"
if echo "$out" | grep -q "rs strict"; then ko "strict mode warnings: $(echo "$out" | grep 'rs strict')"; else ok "no strict-mode warning on the title"; fi
printf "30 tap A\n" > "$T/start.input"
out=$(run --frames 60 --input "$T/start.input")
check "A on the title starts chapter 1, room 1" "state: st=1 chapter=1 room=1 " "$out"
printf "30 tap START\n" > "$T/start2.input"
out=$(run --frames 60 --input "$T/start2.input")
check "Start starts it too" "state: st=1 chapter=1 room=1 " "$out"
printf "20 tap RIGHT\n40 tap RIGHT\n" > "$T/right.input"
out=$(run --frames 60 --input "$T/right.input")
check "chapter 2 is locked before chapter 1 is climbed" "state: st=0 .*sel=1 " "$out"
out=$(run --frames 60 --input "$T/right.input" --opt unlock=1)
check "... open (unlock=1): Right twice shows chapter 3" "state: st=0 .*sel=3 " "$out"
printf "20 tap RIGHT\n40 tap RIGHT\n60 tap LEFT\n80 tap A\n" > "$T/pick.input"
out=$(run --frames 100 --input "$T/pick.input" --opt unlock=1)
check "Left goes back, A starts the chapter shown" "state: st=1 chapter=2 room=1 " "$out"

# a death: room 1-4 (spikes on the floor), walking right into them
printf "0 RIGHT\n" > "$T/die.input"
out=$(run --frames 200 --input "$T/die.input" --opt ready=1 --opt room=4)
check "running into the spikes: a death, counted" "state: st=1 chapter=1 room=4 .*deaths=[1-9]" "$out"
out=$(run --frames 200 --input "$T/die.input" --opt ready=1 --opt room=4 --opt god=1)
check "... not with god=1 (the test switch)" "state: st=1 chapter=1 .*deaths=0 " "$out"
printf "0 RIGHT\n50 -\n" > "$T/die2.input"
out=$(run --frames 200 --input "$T/die2.input" --opt ready=1 --opt room=4)
check "... and the room again from its start (the kid back at S)" "state: st=1 chapter=1 room=4 .*deaths=1 .*x=19 y=195 dead=0 " "$out"

# pause: the world holds still; paused, B goes back to the title
printf "60 tap START\n" > "$T/pause.input"
a=$(run --frames 100 --opt bot=1 --opt ready=1 --input "$T/pause.input" | sed -n 's/.*time=\([0-9]*\) .*paused=\([0-9]\) .*x=\([0-9-]*\) y=\([0-9-]*\).*/\1 \2 \3 \4/p')
b=$(run --frames 300 --opt bot=1 --opt ready=1 --input "$T/pause.input" | sed -n 's/.*time=\([0-9]*\) .*paused=\([0-9]\) .*x=\([0-9-]*\) y=\([0-9-]*\).*/\1 \2 \3 \4/p')
if [ -n "$a" ] && [ "$a" = "$b" ] && [ "$(echo "$a" | cut -d' ' -f2)" = 1 ]; then ok "Start pauses (the time and the kid frozen: $a)"; else ko "pause: '$a' vs '$b'"; fi
printf "60 tap SELECT\n90 tap B\n" > "$T/quit.input"
out=$(run --frames 120 --opt bot=1 --opt ready=1 --input "$T/quit.input")
check "Select pauses too; paused, B goes back to the title" "state: st=0 .*paused=0 " "$out"

# the bot climbs chapter 1 (its recorded solutions, the berries' routes): the results, no death, every berry
rm -f "$T/save.srm"
out=$(run --frames 4000 --opt bot=1 --sram "$T/save.srm")
check "the bot climbs chapter 1 to the results (no death, all its berries)" "chapter 1 climbed at frame .* deaths 0 berries \([0-9]*\)/\1" "$out"
check "... then back on the title, chapter 2 chosen" "state: st=0 .*sel=2 .*cleared=10000" "$out"
out=$(run --frames 10 --sram "$T/save.srm")
check "save RAM: chapter 1's record kept, chapter 2 open (shown first)" "state: st=0 .*sel=2 .*cleared=10000 score=[1-9]" "$out"

# every chapter: the bot climbs it from start to end (the plain solutions, then the berries' ones)
if [ -z "$Q" ]; then
    for c in 1 2 3 4 5; do
        grep -q "^$c 1 " games/goatgorge/levels/solutions.txt 2>/dev/null || continue
        for berry in 0 1; do
            out=$(run --frames 12000 --opt bot=1 --opt botberry=$berry --opt chapter=$c --opt unlock=1 --opt strict=1)
            check "chapter $c (bot, berries=$berry): climbed with no death" "chapter $c climbed at frame .* deaths 0 " "$out"
            if echo "$out" | grep -q "rs strict"; then ko "chapter $c: strict mode warnings: $(echo "$out" | grep 'rs strict')"; fi
        done
    done
fi

# determinism: the same inputs, the same run (state hash and picture), twice
: > "$T/rhythm.input"
i=30; while [ $i -lt 1200 ]; do echo "$i tap A" >> "$T/rhythm.input"; echo "$((i + 9)) RIGHT" >> "$T/rhythm.input"; i=$((i + 37)); done
for k in 1 2; do
    run --frames 1200 --input "$T/rhythm.input" --opt ready=1 --png "$T/det$k.png" | grep "state:" > "$T/det$k.txt"
    run --frames 1500 --opt bot=1 --png "$T/bot$k.png" | grep "state:" > "$T/bot$k.txt"
done
if cmp -s "$T/det1.txt" "$T/det2.txt" && cmp -s "$T/det1.png" "$T/det2.png"; then ok "determinism: a scripted run twice, same state and picture"
else ko "determinism (script)"; fi
if cmp -s "$T/bot1.txt" "$T/bot2.txt" && cmp -s "$T/bot1.png" "$T/bot2.png"; then ok "determinism: a bot run twice, same state and picture"
else ko "determinism (bot)"; fi
exit $fail

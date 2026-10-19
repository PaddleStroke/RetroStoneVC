#!/bin/sh
# Lemur Leap smoke tests: scripted runs through the headless runner: the title (the wardrobe: browsing, buying a look,
# wearing it), a climb, the touches (a half held steers, a second finger shoots), pause (the climb frozen; B: the climb
# kept, CONTINUE or a new climb on the title), the fall out and the record in save RAM, the bot climbing (several seeds,
# the strict mode on: no sprite, VRAM or voice guideline exceeded), a late climb in space within the guidelines too,
# determinism.
#   smoke_test.sh <headless binary> <tmp dir> [quick]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/lemurleap/LICENSE.
H=$1
T=${2:-/tmp}/lemurleap-tests
Q=$3
mkdir -p "$T"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() {   # check <name> <expected regexp> <output>
    if echo "$3" | grep -q "$2"; then ok "$1"; else ko "$1: expected '$2' in: $(echo "$3" | grep 'state:\|strict\|climb \|bought\|fell')"; fi
}
nostrict() {   # nostrict <name> <output>
    if echo "$2" | grep -q "^\[rs strict\]"; then ko "$1: strict mode warnings: $(echo "$2" | grep "^\[rs strict\]" | head -3)"
    else ok "$1: no strict-mode warning"; fi
}
run() { $H --opt music=0 --opt dump=1 "$@" 2>&1; }

# the title: st=0 the title, 1 the climb, 2 the results
out=$(run --frames 300 --opt strict=1)
check "title: nothing happens without input" "state: st=0 " "$out"
nostrict "the title" "$out"
printf "30 tap A\n" > "$T/start.input"
out=$(run --frames 90 --input "$T/start.input")
check "A on the title starts a climb" "state: st=1 .*t=[0-9]\{2\} " "$out"
printf "30 tap START\n" > "$T/start2.input"
out=$(run --frames 60 --input "$T/start2.input")
check "Start starts it too" "state: st=1 " "$out"
printf "20 tap RIGHT\n40 tap X\n" > "$T/poor.input"
out=$(run --frames 70 --input "$T/poor.input")
check "the leaf cap costs 30 coins: X buys nothing with none" "state: st=0 .*look=1 hat=0 .*purse=0 .*owned=101 " "$out"
out=$(run --frames 70 --input "$T/poor.input" --opt coins=100)
check "... with 100 coins X buys it (70 left) and the lemur wears it" "look 1 bought.*state: st=0 .*look=1 hat=1 .*purse=70 .*owned=103 " "$(echo $out)"
printf "20 tap LEFT\n40 tap X\n60 tap A\n" > "$T/fur.input"
out=$(run --frames 90 --input "$T/fur.input" --opt coins=600)
check "Left wraps to the midnight fur (500): bought, worn, then the climb" "state: st=1 .*fur=3 .*purse=100 " "$out"

# the touches: the finger down the longest steers by its half, another finger shoots, a tap starts from the title
printf "10 tap-at 50%% 20%%\n60 T1 touch 10%% 80%%\n" > "$T/tleft.input"
a=$(run --frames 80 --input "$T/tleft.input" | sed -n 's/.*state: st=1 .* x=\([0-9]*\) .*/\1/p')
printf "10 tap-at 50%% 20%%\n60 T1 touch 90%% 80%%\n" > "$T/tright.input"
b=$(run --frames 80 --input "$T/tright.input" | sed -n 's/.*state: st=1 .* x=\([0-9]*\) .*/\1/p')
if [ -n "$a" ] && [ -n "$b" ] && [ "$a" -lt 80 ] && [ "$b" -gt 112 ]; then ok "touch: a tap starts; holding the left half steers left (x $a), the right half right (x $b)"
else ko "touch steering: x '$a' (left held) and '$b' (right held)"; fi
printf "10 tap-at 50%% 20%%\n60 T1 touch 10%% 80%%\n80 T2 tap-at 80%% 50%%\n120 T1 up\n" > "$T/tshoot.input"
out=$(run --frames 130 --input "$T/tshoot.input")
check "touch: a second finger shoots a seed" "state: st=1 .*shots=1 " "$out"

# pause: the climb holds still; paused, B goes back to the title, the climb kept
printf "300 tap START\n" > "$T/pause.input"
a=$(run --frames 360 --opt bot=1 --opt ready=1 --opt seed=5 --input "$T/pause.input" | sed -n 's/.*m=\([0-9]*\) .* t=\([0-9]*\) .*paused=\([0-9]\).*/\1 \2 \3/p')
b=$(run --frames 600 --opt bot=1 --opt ready=1 --opt seed=5 --input "$T/pause.input" | sed -n 's/.*m=\([0-9]*\) .* t=\([0-9]*\) .*paused=\([0-9]\).*/\1 \2 \3/p')
if [ -n "$a" ] && [ "$a" = "$b" ] && [ "${a##* }" = 1 ]; then ok "Start pauses (the climb frozen: $a)"; else ko "pause: '$a' vs '$b'"; fi
rm -f "$T/run.srm"
printf "30 tap A\n400 tap START\n410 tap B\n" > "$T/quit.input"
out=$(run --frames 430 --input "$T/quit.input" --sram "$T/run.srm")
check "paused, B goes back to the title, the climb kept" "state: st=0 .*kept=1 " "$out"
printf "30 tap A\n" > "$T/cont.input"
out=$(run --frames 60 --input "$T/cont.input" --sram "$T/run.srm")
check "the title continues the climb kept (A)" "climb starts at frame [0-9]*: continued.*state: st=1 .*t=[0-9]\{3\} " "$(echo $out)"
printf "30 tap B\n" > "$T/new.input"
out=$(run --frames 60 --input "$T/new.input" --sram "$T/run.srm")
check "... or B starts a new one" "climb starts at frame [0-9]*: new.*state: st=1 .*t=[0-9]\{1,2\} " "$(echo $out)"

# the fall out: the results, the record and the coins kept in save RAM
rm -f "$T/over.srm"
out=$(run --frames 400 --opt ready=1 --opt fall=1 --opt height=150 --sram "$T/over.srm")
check "nothing under the lemur: it falls out, the results" "fell out at 15[0-9] m.*state: st=2 " "$(echo $out)"
out=$(run --frames 10 --sram "$T/over.srm")
check "save RAM: the record kept" "state: st=0 .*best=15[0-9] " "$out"
printf "300 tap A\n" > "$T/back.input"
out=$(run --frames 330 --opt ready=1 --opt fall=1 --input "$T/back.input")
check "the results: A goes back to the title" "state: st=0 " "$out"

# the bot: it climbs, several seeds, no guideline exceeded
for seed in 1 2 3; do
    [ -n "$Q" ] && [ $seed -gt 1 ] && break
    out=$(run --frames 5400 --opt bot=1 --opt ready=1 --opt seed=$seed --opt strict=1)
    check "the bot (seed $seed) climbs 300 m in 1:30" "state: st=[12] .*m=\([3-9][0-9][0-9]\|[0-9]\{4\}\) " "$out"
    nostrict "the bot (seed $seed)" "$out"
done
if [ -z "$Q" ]; then
    out=$(run --frames 2400 --opt bot=1 --opt ready=1 --opt seed=7 --opt height=1100 --opt god=1 --opt strict=1)
    check "a late climb in space (1100 m), the pests harmless" "state: st=1 .*m=1[1-9][0-9][0-9] " "$out"
    nostrict "space" "$out"
fi

# determinism: the same inputs, the same climb (state hash and picture), twice
: > "$T/zig.input"
echo "20 tap A" >> "$T/zig.input"
i=60; while [ $i -lt 1500 ]; do echo "$i RIGHT" >> "$T/zig.input"; echo "$((i + 30)) LEFT+A" >> "$T/zig.input"; echo "$((i + 70)) -" >> "$T/zig.input"; i=$((i + 110)); done
for k in 1 2; do
    run --frames 1500 --input "$T/zig.input" --opt seed=9 --png "$T/det$k.png" | grep "state:" > "$T/det$k.txt"
    run --frames 3000 --opt bot=1 --opt ready=1 --opt seed=9 --png "$T/bot$k.png" | grep "state:" > "$T/bot$k.txt"
done
if cmp -s "$T/det1.txt" "$T/det2.txt" && cmp -s "$T/det1.png" "$T/det2.png"; then ok "determinism: a scripted climb twice, same state and picture"
else ko "determinism (script)"; fi
if cmp -s "$T/bot1.txt" "$T/bot2.txt" && cmp -s "$T/bot1.png" "$T/bot2.png"; then ok "determinism: a bot climb twice, same state and picture"
else ko "determinism (bot)"; fi
exit $fail

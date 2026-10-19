#!/bin/sh
# Prickle Patch smoke tests: scripted runs through the headless runner: the title (the gardeners, buying one, the
# shed), a run, a level-up's choice, a chest, pause (the time frozen; B: the run kept, CONTINUE or a new run on the
# title), the game over and the best run in save RAM, the bot surviving its first minutes with random choices (several
# seeds, the strict mode on: no sprite, VRAM or voice guideline exceeded), a late garden (minute 10, a boss) within
# the guidelines too, determinism.
#   smoke_test.sh <headless binary> <tmp dir> [quick]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pricklepatch/LICENSE.
H=$1
T=${2:-/tmp}/pricklepatch-tests
Q=$3
mkdir -p "$T"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() {   # check <name> <expected regexp> <output>
    if echo "$3" | grep -q "$2"; then ok "$1"; else ko "$1: expected '$2' in: $(echo "$3" | grep 'state:\|strict\|run \|down\|bought')"; fi
}
nostrict() {   # nostrict <name> <output>
    if echo "$2" | grep -q "^\[rs strict\]"; then ko "$1: strict mode warnings: $(echo "$2" | grep "^\[rs strict\]" | head -3)"
    else ok "$1: no strict-mode warning"; fi
}
run() { $H --opt music=0 --opt dump=1 "$@" 2>&1; }

# the title: st=0 the title, 1 a run, 2 a level-up, 3 a chest, 4 the shed, 5 the results
out=$(run --frames 300 --opt strict=1)
check "title: nothing happens without input" "state: st=0 chr=0 " "$out"
nostrict "the title" "$out"
printf "30 tap A\n" > "$T/start.input"
out=$(run --frames 60 --input "$T/start.input")
check "A on the title starts a run with the hedgehog" "state: st=1 chr=0 .*weapons=1 " "$out"
printf "30 tap START\n" > "$T/start2.input"
out=$(run --frames 60 --input "$T/start2.input")
check "Start starts it too" "state: st=1 chr=0 " "$out"
printf "20 tap RIGHT\n40 tap A\n" > "$T/locked.input"
out=$(run --frames 70 --input "$T/locked.input")
check "the tortoise is locked: A does not start (no coins)" "state: st=0 chr=1 " "$out"
out=$(run --frames 70 --input "$T/locked.input" --opt coins=300)
check "... with 300 coins A buys it (50 left)" "gardener 1 bought.*state: st=0 chr=1 .*coins=50 " "$(echo $out)"
printf "20 tap RIGHT\n40 tap A\n60 tap A\n" > "$T/buyplay.input"
out=$(run --frames 90 --input "$T/buyplay.input" --opt coins=300)
check "... then A starts a run with it (its watering can)" "state: st=1 chr=1 .*weapons=1 " "$out"
printf "20 tap LEFT\n40 tap A\n" > "$T/hen.input"
out=$(run --frames 70 --input "$T/hen.input" --opt unlock=1)
check "Left wraps to the hen" "state: st=1 chr=2 " "$out"

# the shed: X opens it, Up / Down choose, A buys, B goes back
printf "20 tap X\n40 tap A\n60 tap DOWN\n80 tap A\n100 tap B\n" > "$T/shed.input"
out=$(run --frames 130 --input "$T/shed.input" --opt coins=100)
check "the shed: MIGHT bought (60), VIGOR too dear (100 > 40 left), back on the title" "state: st=0 .*coins=40 " "$out"
printf "20 tap X\n" > "$T/shed2.input"
out=$(run --frames 60 --input "$T/shed2.input")
check "X opens the shed" "state: st=4 " "$out"

# a level-up: three choices, A takes one; a chest gives its upgrades
out=$(run --frames 60 --opt ready=1 --opt levelup=1)
check "a level-up waits for a choice" "state: st=2 " "$out"
printf "40 tap DOWN\n45 tap A\n" > "$T/pick.input"
out=$(run --frames 60 --opt ready=1 --opt levelup=1 --opt seed=3 --input "$T/pick.input")
check "... A takes it: the run goes on with a new tool or passive" "state: st=1 .*\(weapons=2\|passives=1\)" "$out"
out=$(run --frames 30 --opt ready=1 --opt chest=1)
check "a chest shows its treasure" "state: st=3 " "$out"
printf "40 tap A\n" > "$T/chest.input"
out=$(run --frames 60 --opt ready=1 --opt chest=1 --input "$T/chest.input")
check "... A goes on" "state: st=1 " "$out"

# pause: the garden holds still; paused, B goes back to the title, the run kept
printf "300 tap START\n" > "$T/pause.input"
a=$(run --frames 360 --opt bot=1 --opt ready=1 --opt seed=5 --input "$T/pause.input" | sed -n 's/.*t=\([0-9]*\) .*kills=\([0-9]*\) .*paused=\([0-9]\).*/\1 \2 \3/p')
b=$(run --frames 600 --opt bot=1 --opt ready=1 --opt seed=5 --input "$T/pause.input" | sed -n 's/.*t=\([0-9]*\) .*kills=\([0-9]*\) .*paused=\([0-9]\).*/\1 \2 \3/p')
if [ -n "$a" ] && [ "$a" = "$b" ] && [ "${a##* }" = 1 ]; then ok "Start pauses (the time and the pests frozen: $a)"; else ko "pause: '$a' vs '$b'"; fi
rm -f "$T/run.srm"
printf "30 tap A\n400 tap START\n410 tap B\n" > "$T/quit.input"
out=$(run --frames 430 --input "$T/quit.input" --sram "$T/run.srm")
check "paused, B goes back to the title" "state: st=0 " "$out"
printf "30 tap A\n" > "$T/cont.input"
out=$(run --frames 60 --input "$T/cont.input" --sram "$T/run.srm")
check "the title continues the run kept (A)" "run starts at frame [0-9]*: continued.*state: st=1 .*t=[0-9]\{3\}" "$(echo $out)"
printf "30 tap B\n" > "$T/new.input"
out=$(run --frames 60 --input "$T/new.input" --sram "$T/run.srm")
check "... or B starts a new one" "run starts at frame [0-9]*: new.*state: st=1 .*t=[0-9]\{1,2\} " "$(echo $out)"

# the game over: a gardener with 1 health standing still; the best run kept in save RAM, the coins earned
rm -f "$T/over.srm"
out=$(run --frames 2400 --opt ready=1 --opt frail=1 --sram "$T/over.srm")
check "standing still with 1 health: the gardener is down, the results" "gardener is down.*state: st=5 " "$(echo $out)"
out=$(run --frames 10 --sram "$T/over.srm")
check "save RAM: the best run kept" "state: st=0 .*best=[1-9]" "$out"

# the bot: random choices, it survives the first minutes (several seeds), no guideline exceeded
for seed in 1 2 3; do
    [ -n "$Q" ] && [ $seed -gt 1 ] && break
    out=$(run --frames 11000 --opt bot=1 --opt ready=1 --opt seed=$seed --opt strict=1)
    check "the bot (seed $seed) survives 3:00 with random choices" "state: st=[12] .*t=10[0-9]\{3\} .*dead=0 " "$out"
    nostrict "the bot (seed $seed)" "$out"
done
if [ -z "$Q" ]; then
    out=$(run --frames 19000 --opt bot=1 --opt ready=1 --opt seed=4 --opt strict=1)
    check "the bot (seed 4) survives 5:00 (the Slug Queen out)" "boss 1 at 5:00.*state: st=[123] .*t=18[0-9]\{3\} .*dead=0 " "$(echo $out)"
    out=$(run --frames 2400 --opt bot=1 --opt ready=1 --opt seed=7 --opt time=600 --opt god=1 --opt strict=1)
    check "a late garden (10:00, the Stag Beetle, a full build)" "boss 2 at 10:00.*state: st=[123] .*pests=[0-9]\{3\} " "$(echo $out)"
    nostrict "the late garden" "$out"
    out=$(run --frames 1800 --opt bot=1 --opt ready=1 --opt seed=7 --opt time=900 --opt god=1 --opt strict=1)
    check "15:00: the Crow King" "boss 3 at 15:00" "$out"
    nostrict "15:00" "$out"
fi

# determinism: the same inputs, the same run (state hash and picture), twice
: > "$T/zig.input"
i=30; while [ $i -lt 1500 ]; do echo "$i RIGHT+DOWN" >> "$T/zig.input"; echo "$((i + 40)) LEFT" >> "$T/zig.input"; echo "$((i + 80)) UP" >> "$T/zig.input"; i=$((i + 120)); done
for k in 1 2; do
    run --frames 1500 --input "$T/zig.input" --opt ready=1 --opt seed=9 --png "$T/det$k.png" | grep "state:" > "$T/det$k.txt"
    run --frames 3000 --opt bot=1 --opt ready=1 --opt seed=9 --png "$T/bot$k.png" | grep "state:" > "$T/bot$k.txt"
done
if cmp -s "$T/det1.txt" "$T/det2.txt" && cmp -s "$T/det1.png" "$T/det2.png"; then ok "determinism: a scripted run twice, same state and picture"
else ko "determinism (script)"; fi
if cmp -s "$T/bot1.txt" "$T/bot2.txt" && cmp -s "$T/bot1.png" "$T/bot2.png"; then ok "determinism: a bot run twice, same state and picture"
else ko "determinism (bot)"; fi
exit $fail

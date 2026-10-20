#!/bin/sh
# Puffin Plunge smoke tests: scripted runs through the headless runner: the title (A, Start), a dive, a rest stop's shop
# (buying, diving on), a side room's door, shop and gift, pause (the dive frozen; B: the dive kept, CONTINUE or a new dive
# on the title), the game over and the best score and depth in save RAM, the deepest dive's marker passed, the bot diving
# past 150 m on several seeds with the strict mode on (no sprite, VRAM or voice guideline exceeded), the zones deeper
# down, a walrus, the second round of the zones, determinism.
#   smoke_test.sh <headless binary> <tmp dir> [quick]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/puffinplunge/LICENSE.
H=$1
T=${2:-/tmp}/puffinplunge-tests
Q=$3
mkdir -p "$T"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() {   # check <name> <expected regexp> <output>
    if echo "$3" | grep -q "$2"; then ok "$1"; else ko "$1: expected '$2' in: $(echo "$3" | grep 'state:\|strict\|zone \|down\|bought\|dive \|walrus\|reached')"; fi
}
nostrict() {   # nostrict <name> <output>
    if echo "$2" | grep -q "^\[rs strict\]"; then ko "$1: strict mode warnings: $(echo "$2" | grep "^\[rs strict\]" | head -3)"
    else ok "$1: no strict-mode warning"; fi
}
run() { $H --opt music=0 --opt dump=1 "$@" 2>&1; }

# the title: st=0 the title, 1 the dive, 3 a shop, 4 the results
out=$(run --frames 300 --opt strict=1)
check "title: nothing happens without input" "state: st=0 depth=0 zone=1 " "$out"
nostrict "the title" "$out"
printf "30 tap A\n" > "$T/start.input"
out=$(run --frames 60 --input "$T/start.input")
check "A on the title starts a dive" "dive starts at frame [0-9]*: new.*state: st=1 depth=0 zone=1 hp=4/4 ammo=8 " "$(echo $out)"
printf "30 tap START\n" > "$T/start2.input"
out=$(run --frames 60 --input "$T/start2.input")
check "Start starts it too" "state: st=1 " "$out"

# a rest stop's shop: A buys the first offer (the gems spent), the last line dives on
printf "20 tap A\n40 tap DOWN\n50 tap DOWN\n60 tap DOWN\n70 tap A\n" > "$T/shop.input"
out=$(run --frames 120 --opt ready=1 --opt seed=3 --opt shop=1 --opt gems=200 --input "$T/shop.input")
check "a rest stop's shop: A buys an upgrade, then DIVE ON dives on" "upgrade [0-9]* bought.*state: st=1 .*gems=1[0-9][0-9] " "$(echo $out)"
printf "20 tap A\n" > "$T/poor.input"
out=$(run --frames 60 --opt ready=1 --opt seed=3 --opt shop=1 --opt gems=5 --input "$T/poor.input")
check "... too few gems: nothing bought" "state: st=3 .*gems=5 " "$out"
# a side room: the puffin by its door (the wall the log names), pushing into it
side=$(run --frames 2 --opt ready=1 --opt seed=7 --opt depth=400 --opt door=1 | sed -n 's/.*side room 1: row [0-9]*, the \([a-z]*\) wall.*/\1/p')
key=$([ "$side" = left ] && echo LEFT || echo RIGHT)
printf "30 $key\n" > "$T/door.input"
out=$(run --frames 90 --opt ready=1 --opt seed=7 --opt depth=400 --opt door=1 --input "$T/door.input")
check "a side room: pushing into its door from the ledge opens it ($side wall)" "side room 1: row [0-9]*.*state: st=3 " "$(echo $out)"
printf "20 tap A\n40 tap A\n" > "$T/gift.input"
out=$(run --frames 80 --opt ready=1 --opt seed=3 --opt room=2 --input "$T/gift.input")
check "a side room's gift: A takes one free, then back in the shaft" "upgrade [0-9]* bought.*state: st=1 .*gems=0 " "$(echo $out)"

# pause: the dive holds still; paused, B goes back to the title, the dive kept
printf "200 tap START\n" > "$T/pause.input"
a=$(run --frames 260 --opt bot=1 --opt ready=1 --opt seed=5 --input "$T/pause.input" | sed -n 's/.*state: \(.*\) best=.*/\1/p')
b=$(run --frames 500 --opt bot=1 --opt ready=1 --opt seed=5 --input "$T/pause.input" | sed -n 's/.*state: \(.*\) best=.*/\1/p')
if [ -n "$a" ] && [ "$a" = "$b" ] && echo "$a" | grep -q "paused=1"; then ok "Start pauses (the dive frozen)"; else ko "pause: '$a' vs '$b'"; fi
rm -f "$T/run.srm"
printf "30 tap A\n40 RIGHT\n60 A\n200 tap START\n210 tap B\n" > "$T/quit.input"
out=$(run --frames 240 --input "$T/quit.input" --sram "$T/run.srm")
check "paused, B goes back to the title" "state: st=0 " "$out"
printf "30 tap A\n" > "$T/cont.input"
out=$(run --frames 60 --input "$T/cont.input" --sram "$T/run.srm")
check "the title continues the dive kept (A), from where it was" "dive starts at frame [0-9]*: continued" "$out"
printf "20 tap RIGHT\n30 tap A\n" > "$T/new.input"
out=$(run --frames 60 --input "$T/new.input" --sram "$T/run.srm")
check "... or Right then A starts a new one" "dive starts at frame [0-9]*: new" "$out"
printf "30 tap B\n" > "$T/newb.input"
out=$(run --frames 60 --input "$T/newb.input" --sram "$T/run.srm")
check "... B too" "dive starts at frame [0-9]*: new" "$out"

# the game over: the results, then one press back to the title; the best score and depth kept in save RAM
rm -f "$T/over.srm"
printf "30 tap A\n40 RIGHT\n" > "$T/go.input"
out=$(run --frames 400 --input "$T/go.input" --opt hurtat=150 --opt score=345 --sram "$T/over.srm")
check "the dive lost: the results" "puffin is down.*dive over.*state: st=4 " "$(echo $out)"
out=$(run --frames 10 --sram "$T/over.srm" --opt bot=0)
check "save RAM: a best score and the deepest dive kept" "state: st=0 .*best=[1-9][0-9]* bestdepth=[1-9]" "$out"
out=$(run --frames 600 --opt bot=1 --opt ready=1 --opt seed=7 --opt bestdepth=40)
check "the deepest dive's marker passed: NEW BEST!" "past the deepest dive (40 m)" "$out"

# the bot: past the gentle first 150 m on several seeds, no guideline exceeded
for seed in 1 2 3 4; do
    [ -n "$Q" ] && [ $seed -gt 1 ] && break
    out=$(run --frames 9000 --opt bot=1 --opt ready=1 --opt seed=$seed --opt botdepth=150 --opt strict=1)
    check "the bot (seed $seed) dives past 150 m" "the bot reached 1[5-9][0-9] m" "$out"
    nostrict "the bot (seed $seed)" "$out"
done
if [ -z "$Q" ]; then
    for d in 400 700 1000 1300 2700; do
        out=$(run --frames 2400 --opt bot=1 --opt ready=1 --opt seed=7 --opt depth=$d --opt god=1 --opt strict=1)
        check "at $d m (zone $(( d / 300 + 1 ))): the bot dives on" "dive starts.*at [0-9]* m (zone $(( d / 300 + 1 )))" "$(echo $out)"
        nostrict "at $d m" "$out"
    done
    out=$(run --frames 7000 --opt bot=1 --opt ready=1 --opt seed=7 --opt depth=1150 --opt god=1 --opt strict=1)
    check "the walrus at the frozen deep's end: beaten (the bot, god mode), its floor gives way" "walrus wakes.*walrus is beaten.*zone 5 at" "$(echo $out)"
    nostrict "the walrus" "$out"
fi

# determinism: the same inputs, the same dive (state hash and picture), twice
: > "$T/zig.input"
i=30; while [ $i -lt 1500 ]; do echo "$i RIGHT+A" >> "$T/zig.input"; echo "$((i + 40)) LEFT" >> "$T/zig.input"; echo "$((i + 80)) A" >> "$T/zig.input"; i=$((i + 120)); done
for k in 1 2; do
    run --frames 1500 --input "$T/zig.input" --opt ready=1 --opt seed=9 --png "$T/det$k.png" | grep "state:" > "$T/det$k.txt"
    run --frames 3000 --opt bot=1 --opt ready=1 --opt seed=9 --png "$T/bot$k.png" | grep "state:" > "$T/bot$k.txt"
done
if cmp -s "$T/det1.txt" "$T/det2.txt" && cmp -s "$T/det1.png" "$T/det2.png"; then ok "determinism: a scripted dive twice, same state and picture"
else ko "determinism (script)"; fi
if cmp -s "$T/bot1.txt" "$T/bot2.txt" && cmp -s "$T/bot1.png" "$T/bot2.png"; then ok "determinism: a bot dive twice, same state and picture"
else ko "determinism (bot)"; fi
exit $fail

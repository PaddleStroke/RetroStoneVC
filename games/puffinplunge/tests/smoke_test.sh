#!/bin/sh
# Puffin Plunge smoke tests: scripted runs through the headless runner: the title (A, Start), a dive, the level's shop
# (buying, leaving), a side room's shop and gift, pause (the dive frozen; B: the dive kept, CONTINUE or a new dive on
# the title), the game over and the best score in save RAM, the bot diving through levels 1-2 on several seeds with the
# strict mode on (no sprite, VRAM or voice guideline exceeded), the deep zones and the boss within the guidelines,
# determinism.
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
    if echo "$3" | grep -q "$2"; then ok "$1"; else ko "$1: expected '$2' in: $(echo "$3" | grep 'state:\|strict\|level \|down\|bought\|dive ')"; fi
}
nostrict() {   # nostrict <name> <output>
    if echo "$2" | grep -q "^\[rs strict\]"; then ko "$1: strict mode warnings: $(echo "$2" | grep "^\[rs strict\]" | head -3)"
    else ok "$1: no strict-mode warning"; fi
}
run() { $H --opt music=0 --opt dump=1 "$@" 2>&1; }

# the title: st=0 the title, 1 a level, 3 a shop, 4 the results
out=$(run --frames 300 --opt strict=1)
check "title: nothing happens without input" "state: st=0 level=1 " "$out"
nostrict "the title" "$out"
printf "30 tap A\n" > "$T/start.input"
out=$(run --frames 60 --input "$T/start.input")
check "A on the title starts a dive at level 1-1" "level 1-1 starts.*state: st=1 level=1 hp=4/4 ammo=8 " "$(echo $out)"
printf "30 tap START\n" > "$T/start2.input"
out=$(run --frames 60 --input "$T/start2.input")
check "Start starts it too" "state: st=1 level=1 " "$out"

# the level's shop: A buys the first offer (the gems spent), the last line goes on down to level 1-2
printf "20 tap A\n40 tap DOWN\n50 tap DOWN\n60 tap DOWN\n70 tap A\n" > "$T/shop.input"
out=$(run --frames 120 --opt ready=1 --opt seed=3 --opt shop=1 --opt gems=200 --input "$T/shop.input")
check "the shop: A buys an upgrade, then DIVE ON goes to level 1-2" "upgrade [0-9]* bought.*level 1-2 starts.*state: st=1 level=2 .*gems=1[0-9][0-9] " "$(echo $out)"
printf "20 tap A\n" > "$T/poor.input"
out=$(run --frames 60 --opt ready=1 --opt seed=3 --opt shop=1 --opt gems=5 --input "$T/poor.input")
check "... too few gems: nothing bought" "state: st=3 .*gems=5 " "$out"
printf "30 RIGHT\n" > "$T/door.input"
out=$(run --frames 90 --opt ready=1 --opt seed=7 --opt level=4 --opt door=1 --input "$T/door.input")
check "a side room: pushing into its door from the ledge opens it (a gift)" "side room 1: row [0-9]*, the right wall, a gift.*state: st=3 " "$(echo $out)"
printf "20 tap A\n40 tap A\n" > "$T/gift.input"
out=$(run --frames 80 --opt ready=1 --opt seed=3 --opt room=2 --input "$T/gift.input")
check "a side room's gift: A takes one free, then back in the shaft" "upgrade [0-9]* bought.*state: st=1 .*gems=0 " "$(echo $out)"

# pause: the dive holds still; paused, B goes back to the title, the dive kept
printf "200 tap START\n" > "$T/pause.input"
a=$(run --frames 260 --opt bot=1 --opt ready=1 --opt seed=5 --input "$T/pause.input" | sed -n 's/.*state: \(.*\) best=.*/\1/p')
b=$(run --frames 500 --opt bot=1 --opt ready=1 --opt seed=5 --input "$T/pause.input" | sed -n 's/.*state: \(.*\) best=.*/\1/p')
if [ -n "$a" ] && [ "$a" = "$b" ] && echo "$a" | grep -q "paused=1"; then ok "Start pauses (the dive frozen)"; else ko "pause: '$a' vs '$b'"; fi
rm -f "$T/run.srm"
printf "30 tap A\n200 tap START\n210 tap B\n" > "$T/quit.input"
out=$(run --frames 240 --input "$T/quit.input" --sram "$T/run.srm")
check "paused, B goes back to the title" "state: st=0 " "$out"
printf "30 tap A\n" > "$T/cont.input"
out=$(run --frames 60 --input "$T/cont.input" --sram "$T/run.srm")
check "the title continues the dive kept (A)" "dive starts at frame [0-9]*: continued" "$out"
printf "20 tap RIGHT\n30 tap A\n" > "$T/new.input"
out=$(run --frames 60 --input "$T/new.input" --sram "$T/run.srm")
check "... or Right then A starts a new one" "dive starts at frame [0-9]*: new" "$out"
printf "30 tap B\n" > "$T/newb.input"
out=$(run --frames 60 --input "$T/newb.input" --sram "$T/run.srm")
check "... B too" "dive starts at frame [0-9]*: new" "$out"

# the game over: the results, then one press back to the title; the best score kept in save RAM
rm -f "$T/over.srm"
printf "30 tap A\n" > "$T/go.input"
out=$(run --frames 400 --input "$T/go.input" --opt hurtat=120 --opt score=345 --sram "$T/over.srm")
check "the dive lost: the results" "puffin is down.*dive over.*state: st=4 " "$(echo $out)"
out=$(run --frames 10 --sram "$T/over.srm" --opt bot=0)
check "save RAM: a best score kept" "state: st=0 .*best=[1-9]" "$out"

# the bot: levels 1-2 on several seeds, no guideline exceeded
for seed in 1 2 3 4; do
    [ -n "$Q" ] && [ $seed -gt 1 ] && break
    out=$(run --frames 6000 --opt bot=1 --opt ready=1 --opt seed=$seed --opt botlevels=2 --opt strict=1)
    check "the bot (seed $seed) dives through levels 1-1 and 1-2" "level 2 cleared.*bot stops after 2 levels" "$(echo $out)"
    nostrict "the bot (seed $seed)" "$out"
done
if [ -z "$Q" ]; then
    for lv in 4 7 10; do
        out=$(run --frames 2400 --opt bot=1 --opt ready=1 --opt seed=7 --opt level=$lv --opt god=1 --opt strict=1)
        check "level $lv (zone $(( (lv + 2) / 3 ))): the bot dives with a build of that depth" "level [0-9]-[0-9] starts.*state: st=[13] " "$(echo $out)"
        nostrict "level $lv" "$out"
    done
    out=$(run --frames 7000 --opt bot=1 --opt ready=1 --opt seed=7 --opt level=12 --opt god=1 --opt strict=1)
    check "the bottom: the Grumpy Walrus beaten (the bot, god mode), the results" "walrus is beaten.*dive over.*won\|walrus is beaten" "$(echo $out)"
    nostrict "the boss" "$out"
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

#!/bin/sh
# Mole Marbles: the game-over panel's BEST line. NEW BEST only when the run beats the best saved before it (and scores
# more than 0), and then the value shown is the new best (the score); otherwise BEST and the saved best.
# (It used to show "NEW BEST 0": the best before the run.) The dump line gives the panel's value and the flag.
#   best_test.sh <headless binary> <tmp dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/molemarbles/LICENSE.
H=$1
T=${2:-/tmp}/molemarbles-best
mkdir -p "$T"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
run() { $H --opt music=0 --opt dump=1 "$@" 2>&1 | grep "state:"; }
field() { echo "$2" | sed -n "s/.* $1=\([0-9]*\) .*/\1/p"; }

# 1. a first run (no save yet) that scores: NEW BEST, the score
rm -f "$T/save.srm"
out=$(run --frames 6000 --opt bot=1 --opt seed=8 --opt botstop=30 --sram "$T/save.srm")
s=$(field score "$out"); p=$(field panel "$out"); r=$(field record "$out")
if [ "$(field st "$out")" = 3 ] && [ "${s:-0}" -gt 0 ] && [ "$p" = "$s" ] && [ "$r" = 1 ]; then
    ok "a first run scoring $s: NEW BEST $p"
else ko "first run: score $s, panel $p, new best $r: $out"; fi
first=$s

# 2. a lower score on the same save: BEST, the saved best (not the score)
out=$(run --frames 6000 --opt bot=1 --opt seed=8 --opt botstop=7 --sram "$T/save.srm")
s=$(field score "$out"); p=$(field panel "$out"); r=$(field record "$out")
if [ "$(field st "$out")" = 3 ] && [ "${s:-0}" -lt "$first" ] && [ "$p" = "$first" ] && [ "$r" = 0 ]; then
    ok "a lower score ($s): BEST $p, no NEW BEST"
else ko "lower run: score $s, panel $p (expected $first), new best $r: $out"; fi

# 3. a higher one: NEW BEST, the new score
out=$(run --frames 9000 --opt bot=1 --opt seed=8 --opt botstop=45 --sram "$T/save.srm")
s=$(field score "$out"); p=$(field panel "$out"); r=$(field record "$out")
if [ "$(field st "$out")" = 3 ] && [ "${s:-0}" -gt "$first" ] && [ "$p" = "$s" ] && [ "$r" = 1 ]; then
    ok "a higher score ($s > $first): NEW BEST $p"
else ko "higher run: score $s, panel $p, new best $r: $out"; fi

# 4. 0 points on a fresh save (A starts and fires once, then nothing): no NEW BEST (BEST 0)
rm -f "$T/zero.srm"
printf "30 tap A\n" > "$T/zero.input"
out=$(run --frames 6000 --opt seed=4 --input "$T/zero.input" --sram "$T/zero.srm")
s=$(field score "$out"); p=$(field panel "$out"); r=$(field record "$out")
if [ "$(field st "$out")" = 3 ] && [ "$s" = 0 ] && [ "$p" = 0 ] && [ "$r" = 0 ]; then ok "0 points: BEST 0, no NEW BEST"
else ko "0 points: score $s, panel $p, new best $r: $out"; fi
rm -rf "$T"
exit $fail

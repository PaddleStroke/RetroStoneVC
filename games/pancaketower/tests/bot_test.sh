#!/bin/sh
# Pancake Tower: the bot plays from the screen state only (bot.c: the sprites in OAM, a 12-frame reaction delay,
# a Gaussian timing jitter of 0.8 frame): over 10 seeds (the jitter's seed) it must reach 60 pancakes on average.
# Prints the distribution (the heights, their mean, min and max, and the jitters applied).
#   bot_test.sh <headless binary> <tmp dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pancaketower/LICENSE.
H=$1
T=${2:-/tmp}/pancaketower-bot
mkdir -p "$T"
fail=0
for s in 1 2 3 4 5 6 7 8 9 10; do
    $H --frames 40000 --opt music=0 --opt sound=0 --opt bot=1 --opt seed=$s --opt dump=1 > "$T/bot$s.txt" 2>&1 &
done
wait
all="" sum=0 n=0 min=100000 max=0 j0=0 j1=0 j2=0 j3=0 j4=0
for s in 1 2 3 4 5 6 7 8 9 10; do
    h=$(sed -n 's/.*run 1 over at frame [0-9]*: height \([0-9]*\).*/\1/p' "$T/bot$s.txt")
    [ -n "$h" ] || { echo "  FAIL seed $s: the run did not end ($(grep state: "$T/bot$s.txt"))"; fail=1; h=0; }
    all="$all $h"
    sum=$((sum + h)); n=$((n + 1))
    [ "$h" -lt "$min" ] && min=$h
    [ "$h" -gt "$max" ] && max=$h
    j=$(sed -n 's/.*jitter=\([-0-9,]*\) .*/\1/p' "$T/bot$s.txt")
    IFS=, read a b c d e <<JIT
$j
JIT
    j0=$((j0 + a)); j1=$((j1 + b)); j2=$((j2 + c)); j3=$((j3 + d)); j4=$((j4 + e))
done
mean10=$((sum * 10 / n))
echo "  bot heights, seeds 1-10:$all"
echo "  mean $((mean10 / 10)).$((mean10 % 10)), min $min, max $max; jitter (frames -2..+2): $j0 $j1 $j2 $j3 $j4"
if [ $((sum)) -ge $((60 * n)) ]; then echo "  ok   the bot reaches 60 pancakes on average (reaction delay and jitter)"
else echo "  FAIL the bot's mean is under 60"; fail=1; fi
exit $fail

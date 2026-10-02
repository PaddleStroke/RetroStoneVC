#!/bin/sh
# Pogo Mamie: the screen-reading bot (src/bot.c) over 10 seeds with the default settings, one run each until its
# fall. Fatal hazard contacts replaced roof recoveries: retain a long-run coverage check (at least one seed
# reaches 1000 m), and guard the retuned bot's aggregate progress (mean at least 350 m). Prints the distribution.
#   bot_test.sh <headless binary> <tmp dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pogomamie/LICENSE.
H=$1
T=${2:-/tmp}/pogomamie-bot
mkdir -p "$T"
for s in 1 2 3 4 5 6 7 8 9 10; do
    ( python3 "$(dirname "$0")/bot_run.py" "$H" "$T/bot$s.txt" "$s" ) &
done
wait
all="" sum=0 n=0 min=999999 max=0
for s in 1 2 3 4 5 6 7 8 9 10; do
    d=$(sed -n 's/.*run 1 over at frame [0-9]*: \([0-9]*\) m.*/\1/p' "$T/bot$s.txt")
    [ -n "$d" ] || d=$(sed -n 's/.*state: .* dist=\([0-9]*\) .*/\1/p' "$T/bot$s.txt")
    all="$all $d"
    sum=$((sum + d)); n=$((n + 1))
    [ "$d" -lt "$min" ] && min=$d
    [ "$d" -gt "$max" ] && max=$d
done
mean=$((sum / n))
if [ "$mean" -ge 350 ] && [ "$max" -ge 1000 ]; then
    echo "  ok   the bot (screen only, default settings), seeds 1-10: mean $mean m, min $min, max $max:$all"
    exit 0
fi
echo "  FAIL fatal-hazard bot coverage: mean $mean m (need 350), longest $max m (need 1000), seeds 1-10:$all"
exit 1

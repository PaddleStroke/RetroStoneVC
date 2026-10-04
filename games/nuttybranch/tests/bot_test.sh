#!/bin/sh
# Nutty Branch: the screen-only bot (bot.c reads OAM and the branch layer's per-line registers, never the world)
# plays N courses (seeds 1..N, up to 4 minutes each); its distances are reported and its median must reach the
# target (BOT_TARGET metres).
#   bot_test.sh <headless binary> <tmp dir> [runs] [verbose]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE.
H=$1
T=${2:-/tmp}/nuttybranch-bot
N=${3:-12}
V=$4
TARGET=${BOT_TARGET:-300}
mkdir -p "$T"
: > "$T/dists.txt"
i=1
while [ $i -le "$N" ]; do
    out=$($H --frames 14400 --opt music=0 --opt sound=0 --opt bot=1 --opt ready=1 --opt seed=$i --opt dump=1 2>&1)
    d=$(echo "$out" | sed -n 's/.*state: .* dist=\([0-9]*\) score=\([0-9]*\).*fall=\([0-9]*\).*/\1 \2 \3/p')
    echo "$i $d" >> "$T/dists.txt"
    [ -n "$V" ] && echo "  seed $i: $d"
    i=$((i + 1))
done
sort -n -k2 "$T/dists.txt" | awk -v target="$TARGET" '
    { d[NR] = $2; s += $2; sc += $3; f[$4]++ }
    END {
        n = NR; med = (n % 2) ? d[(n + 1) / 2] : (d[n / 2] + d[n / 2 + 1]) / 2;
        printf "  bot (screen only): %d runs, distance min %d / median %d / mean %d / max %d m, mean score %d", n, d[1], med, s / n, d[n], sc / n;
        printf "; ends: slip %d, gap %d, crash %d, stump %d, still running %d\n", f[1], f[2], f[3], f[4], f[0];
        if (med < target) { printf "  FAIL the median (%d m) is under the target (%d m)\n", med, target; exit 1 }
        printf "  ok   the median reaches the target (%d m)\n", target
    }'

#!/bin/sh
# Mouse Espresso: the screen-only bot (src/bot.c, a decent first-run player) over 48 seeds: its scores must sit in the
# target (DESIGN.md "The bot": a decent first run ends around 300-800; 3000 takes real skill): the median between
# 300 and 800, none at 3000. The old course's bot never crashed (14,000+ after ten minutes on every seed).
#   bot_test.sh <mouseespresso_headless> <out dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/mouseespresso/LICENSE.
H=$1
O=${2:-/tmp/mouseespresso-bot}
N=${BOT_SEEDS:-48}
mkdir -p "$O"
rm -f "$O"/seed-*.txt
seq 1 "$N" | xargs -P "$(nproc 2>/dev/null || echo 4)" -I{} sh -c \
    "$H --frames 20000 --opt music=0 --opt bot=1 --opt ready=1 --opt seed={} 2>&1 | sed -n 's/.*run 1 over at frame [0-9]*: score \([0-9]*\).*/\1/p' > $O/seed-{}.txt"
cat "$O"/seed-*.txt | sort -n > "$O/scores.txt"
n=$(grep -c "" "$O/scores.txt")
awk -v n="$n" '{a[NR]=$1; s+=$1} $1>=3000{c++} END {
    if (n == 0) { print "  FAIL the bot: no game over"; exit 1 }
    med=a[int((n+1)/2)];
    printf "  the bot over %d seeds: min %d, 25%% %d, median %d, 75%% %d, 90%% %d, max %d, mean %d; %d at 3000+\n",
        n, a[1], a[int(n/4)+1], med, a[int(n*3/4)+1], a[int(n*9/10)+1], a[n], s/n, c+0;
    if (med < 300 || med > 800 || c > 0) { print "  FAIL the bot is out of the target"; exit 1 }
    print "  ok   the bot: a decent first run (the median in 300-800, none at 3000)" }' "$O/scores.txt"

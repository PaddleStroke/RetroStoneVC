#!/bin/sh
# Nutty Branch on a tall screen (a phone held upright; DESIGN.md "Tall screen"): the zoomed screen is ZOOM_H lines, and
# the game is the same as on a 4:3 one: the bot (it reads the branch layer's registers of the stage's 240 lines and the
# sprites a 4:3 screen draws, not what only the tall screen adds) decides the same frame by frame, the same runs end at
# the same frames with the same distances and scores. And the picture is as tall as the display asks.
#   tall_test.sh <nuttybranch_headless> <out dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE.
H=$1
O=${2:-/tmp}/nuttybranch-tall
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
png_wh() { od -An -tu1 -j16 -N8 "$1" | awk '{print $1 * 16777216 + $2 * 65536 + $3 * 256 + $4 "x" $5 * 16777216 + $6 * 65536 + $7 * 256 + $8}'; }
# the game's log without the screen: the start-up line and the VRAM count (the sprite pool follows what is shown)
game_log() { grep -v "RetroStone VC\|vram:\|tall screen:"; }
DISPLAYS="1080x2520 1080x2340 720x1440"

for d in $DISPLAYS; do
    $H --display "$d" --frames 130 --opt music=0 --png "$O/t$d.png" > /dev/null 2>&1
    wh=$(png_wh "$O/t$d.png")
    case $wh in 320x480) ok "--display $d: a 320x480 picture (zoomed)";; *) ko "--display $d: the picture is '$wh'";; esac
done

same() {   # same <what> <headless options...>
    what=$1; shift
    $H --width 320 --opt music=0 --opt sound=0 --opt dump=1 "$@" 2>&1 | game_log > "$O/same-240.log"
    n=$(grep -c "" "$O/same-240.log")
    for d in $DISPLAYS; do
        $H --display "$d" --opt music=0 --opt sound=0 --opt dump=1 "$@" 2>&1 | game_log > "$O/same-$d.log"
        if cmp -s "$O/same-240.log" "$O/same-$d.log"; then ok "$what: the same $n log lines at $d as at 320x240"
        else ko "$what at $d:"; diff "$O/same-240.log" "$O/same-$d.log" | head -4; fi
    done
}
same "1 player, the bot's every decision (botlog)" --frames 3000 --opt bot=1 --opt seed=7 --opt ready=1 --opt botlog=1
same "from the title, three runs of the bot" --frames 9000 --opt bot=1 --opt botruns=3 --opt botstop=150 --opt seed=4
same "deep in the course (night, winter: every trap)" --frames 6000 --opt bot=1 --opt seed=11 --opt ready=1 --opt start=1500 --opt botlog=1
same "4 players, four bots" --frames 6000 --opt bot=4 --opt players=4 --opt seed=1 --opt ready=1 --opt botlog=1
for k in gap fork wind rain owl crow swing bees shower twig pecker web hole; do
    same "the $k (the bot's decisions)" --frames 1500 --opt bot=1 --opt seed=3 --opt ready=1 --opt only=$k --opt start=1000 --opt botlog=1
done
[ $fail = 0 ] && echo "tall_test: all passed" || { echo "tall_test: FAILED"; exit 1; }

#!/bin/sh
# Nutty Branch widescreen (DESIGN.md "Widescreen"): the picture is as wide as asked (320..560), and the game is the same
# at every width: the bot (it reads the screen: the branch layer's per-line registers and the sprites of the stage's 320
# columns) decides the same frame by frame, the same runs end at the same frames with the same distances and scores; a
# width change in the middle of a run (--resize: a save state, a restart at the new width, the state loaded) goes on
# exactly as the run that never changed; and a save state made at one width loads and plays at another
# (test_states --any-width).
#   wide_test.sh <nuttybranch_headless> <nuttybranch_test_states> <out dir> [widths]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE.
H=$1
T=$2
O=${3:-/tmp}/nuttybranch-wide
WS=${4:-427 520}
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
png_w() { od -An -tu1 -j16 -N4 "$1" | awk '{print $1 * 16777216 + $2 * 65536 + $3 * 256 + $4}'; }
# the game's log without the width: the start-up line, the resizes and the VRAM count (the sprite pool's tiles follow
# what the wider screen shows)
game_log() { grep -v "RetroStone VC\|frame [0-9]*: width [0-9]*$\|vram:"; }

for w in 320 $WS 560; do
    $H --width "$w" --frames 130 --opt music=0 --png "$O/w$w.png" > /dev/null 2>&1
    pw=$(png_w "$O/w$w.png")
    if [ "$pw" = "$w" ]; then ok "--width $w: a ${pw}x240 picture"; else ko "--width $w: the picture is '$pw' wide"; fi
done

# the same game at every width (the rules never look at the width; the bot watches the stage's 320 columns)
same() {   # same <what> <headless options...>
    what=$1; shift
    $H --width 320 --opt music=0 --opt sound=0 --opt dump=1 "$@" 2>&1 | game_log > "$O/same-320.log"
    n=$(grep -c "" "$O/same-320.log")
    for w in $WS 560; do
        $H --width "$w" --opt music=0 --opt sound=0 --opt dump=1 "$@" 2>&1 | game_log > "$O/same-$w.log"
        if cmp -s "$O/same-320.log" "$O/same-$w.log"; then ok "$what: the same $n log lines at $w as at 320"
        else ko "$what at $w:"; diff "$O/same-320.log" "$O/same-$w.log" | head -4; fi
    done
}
same "1 player, the bot's every decision (botlog)" --frames 3000 --opt bot=1 --opt seed=7 --opt ready=1 --opt botlog=1
same "1 player, the bot, 2.5 minutes" --frames 9000 --opt bot=1 --opt seed=3 --opt ready=1
same "from the title, three runs of the bot" --frames 9000 --opt bot=1 --opt botruns=3 --opt botstop=150 --opt seed=4
same "deep in the course (night, winter: every trap)" --frames 6000 --opt bot=1 --opt seed=11 --opt ready=1 --opt start=1500
same "2 players, two bots" --frames 5000 --opt bot=2 --opt players=2 --opt seed=6 --opt ready=1
same "4 players, four bots" --frames 6000 --opt bot=4 --opt players=4 --opt seed=1 --opt ready=1
same "4 players, to the results" --frames 4000 --opt bot=4 --opt seed=5 --opt ready=1 --opt botstop=40
for k in wind rain owl crow swing bees shower twig; do
    same "the $k (the bot's decisions)" --frames 1500 --opt bot=1 --opt seed=3 --opt ready=1 --opt only=$k --opt start=1000 --opt botlog=1
done

# the width changes during a run: the run goes on as if nothing happened (the final state, the events)
events() {    # the run's events, then its final state (a restart at a new width logs a state too)
    $H --width 320 --opt music=0 --opt sound=0 --opt dump=1 "$@" 2>&1 | game_log > "$O/rz-all.log"
    grep -v "state:" "$O/rz-all.log"
    grep "state:" "$O/rz-all.log" | tail -1
}
resized() {   # resized <what> <resizes> <headless options...>
    what=$1; rz=$2; shift 2
    events "$@" > "$O/rz-ref.log"
    events $rz "$@" > "$O/rz.log"
    if cmp -s "$O/rz-ref.log" "$O/rz.log"; then ok "$what: resized ($rz), the run goes on the same"
    else ko "$what ($rz):"; diff "$O/rz-ref.log" "$O/rz.log" | head -4; fi
}
resized "1 player, the bot" "--resize 600:520 --resize 1300:427 --resize 2000:560" --frames 2600 --opt bot=1 --opt seed=7 --opt ready=1
resized "4 players, four bots" "--resize 400:560 --resize 900:320 --resize 1500:427" --frames 2000 --opt bot=4 --opt seed=5 --opt ready=1
resized "the title, then a run" "--resize 60:427 --resize 700:320" --frames 1500 --opt bot=1 --opt seed=2

# a save state made at one width, loaded at another (in a fresh process, before its first frame): it loads and plays
scenario() {   # scenario <name> <save width> <frames> <after> <opts...>
    name=$1; sw=$2; n=$3; m=$4; shift 4
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    [ -f "$O/$name.input" ] || : > "$O/$name.input"
    out=$($T --width "$sw" $opts --input "$O/$name.input" --frames "$n" --after "$m" --out "$O/nb-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then ok "$name: saved at width $sw (frame $n), replayed the same"
    else ko "$name at width $sw:"; echo "$out" | grep "FAIL\|first"; fi
    for lw in 320 $WS 560; do
        [ "$lw" = "$sw" ] && continue
        out=$($T --width "$lw" $opts --input "$O/$name.input" --resume "$O/nb-$name" --any-width 2>&1)
        if echo "$out" | grep -q "all passed"; then ok "$name: the width-$sw state loads at width $lw and plays $m frames"
        else ko "$name: width $sw -> $lw:"; echo "$out" | grep "FAIL"; fi
    done
}
scenario run 427 700 400 bot=1 ready=1
scenario run560 560 900 300 bot=1 ready=1 seed=5
scenario title 320 60 200
printf "20 P2 tap A\n50 P3 tap A\n" > "$O/lobby.input"
scenario lobby 520 80 200
printf "400 tap SELECT\n520 tap SELECT\n" > "$O/paused.input"
scenario paused 520 450 200 bot=1 ready=1
scenario over 427 1500 300 bot=1 botstop=20 ready=1 botruns=2 seed=3
scenario four 560 500 300 bot=4 ready=1
scenario results4 520 1500 300 bot=4 botstop=10 ready=1 botruns=2 seed=3
exit $fail

#!/bin/sh
# Duck Parade widescreen (DESIGN.md "Widescreen"): the picture is as wide as asked (320..560), the world is revealed
# wider, a third of the extra width behind the duck (the stage origin) and two thirds ahead, and the game is the same
# at every width: the same runs, the same decisions of the bot (it plays from the screen: it looks at the 4:3 part of
# it, the stage), the same logs and states; a width change in the middle of a run
# (--resize: a save state, a restart at the new width, the state loaded) goes on exactly as the run that never
# changed; and a save state made at one width loads and plays at another (test_states --any-width).
#   wide_test.sh <duckparade_headless> <duckparade_test_states> <out dir> [widths]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/duckparade/LICENSE.
H=$1
T=$2
O=${3:-/tmp}/duckparade-wide
WS=${4:-427 520}
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
png_w() { od -An -tu1 -j16 -N4 "$1" | awk '{print $1 * 16777216 + $2 * 65536 + $3 * 256 + $4}'; }
# the game's log without the width: the start-up line, the VRAM count (the maps follow the width) and the generator's
# work counts (it makes the columns further ahead on a wider screen: the same columns, sooner)
game_log() { grep -v "RetroStone VC\|frame [0-9]*: width [0-9]*$" | sed 's/ vram=[0-9]*//; s/ judged=[0-9]* rerolls=[0-9]* fallbacks=[0-9]*//'; }

for w in 320 $WS 560; do
    $H --width "$w" --frames 400 --opt music=0 --opt bot=1 --opt seed=7 --opt ready=1 --png "$O/w$w.png" > /dev/null 2>&1
    pw=$(png_w "$O/w$w.png")
    if [ "$pw" = "$w" ]; then ok "--width $w: a ${pw}x240 picture"; else ko "--width $w: the picture is '$pw' wide"; fi
done

# a third of the extra width behind the duck: the screen's left edge is stage x -(w - 320) / 3, the right edge is
# 320 + 2 (w - 320) / 3 (a finger there targets that world column: the camera is still at 0 before the first hop)
for w in 320 $WS 560; do
    cols=""
    for x in 0 $((w - 1)); do
        printf "30 tap-at @160 120\n50 tap-at $x 120\n" > "$O/edge.input"
        cols="$cols$($H --width "$w" --opt music=0 --input "$O/edge.input" --frames 52 2>&1 | sed -n 's/.*touch target col \(-*[0-9]*\) row.*/\1/p') "
    done
    l=$(( (w - 320) / 3 )); want="$(( -((l + 15) / 16) )) $(( (w - 1 - l) / 16 )) "
    if [ "$cols" = "$want" ]; then ok "$w: $l px behind the stage, $((w - 320 - l)) ahead (the screen's edges: world columns $cols)"
    else ko "$w: the screen's edges are world columns '$cols', not '$want'"; fi
done

# the start of a run (the owner's rule for the side-scrollers): the first hazards are as far from the duck as at 320
# (the same lanes ahead of the same camera, the duck in the same column), so a wide screen shows them from the first
# frame: no longer run-up
printf "30 tap A\n" > "$O/start.input"
for start in "ready --opt ready=1 --frames 2" "title --input $O/start.input --frames 32"; do
    name=${start%% *}; opts=${start#* }
    $H --width 320 --opt music=0 --opt dump=1 $opts 2>&1 | grep "view:\|state:" | game_log > "$O/start-320.log"
    first=$(sed -n 's/.*view: col=0 \(.*\)/\1/p' "$O/start-320.log" | tr ',' '\n' | grep -n -v "^[GDN]" | head -1 | cut -d: -f1)
    for w in $WS 560; do
        $H --width "$w" --opt music=0 --opt dump=1 $opts 2>&1 | grep "view:\|state:" | game_log > "$O/start-$w.log"
        if [ -n "$first" ] && cmp -s "$O/start-320.log" "$O/start-$w.log"; then
            ok "start ($name) at $w: the duck and the lanes ahead as at 320 (the first hazard in stage column $((first - 1)))"
        else ko "start ($name) at $w:"; diff "$O/start-320.log" "$O/start-$w.log" | head -4; fi
    done
done

# the same game at every width (the world is the same; the bot looks at the 4:3 part of the screen)
same() {   # same <what> <headless options...>
    what=$1; shift
    $H --width 320 --opt music=0 --opt dump=1 "$@" 2>&1 | game_log > "$O/same-320.log"
    n=$(grep -c "" "$O/same-320.log")
    for w in $WS 560; do
        $H --width "$w" --opt music=0 --opt dump=1 "$@" 2>&1 | game_log > "$O/same-$w.log"
        if cmp -s "$O/same-320.log" "$O/same-$w.log"; then ok "$what: the same $n log lines at $w as at 320"
        else ko "$what at $w:"; diff "$O/same-320.log" "$O/same-$w.log" | head -4; fi
    done
}
same "1 player, the bot" --frames 6000 --opt bot=1 --opt seed=3 --opt ready=1
same "from the title, three runs of the bot" --frames 9000 --opt bot=1 --opt botruns=3 --opt botstop=40 --opt seed=4
same "co-op, two bots" --frames 4000 --opt bot=2 --opt players=2 --opt seed=6 --opt ready=1
same "four bots, to the results" --frames 5000 --opt bot=4 --opt players=4 --opt ready=1 --opt seed=8 --opt botstop=40

# the width changes during a run: the run goes on as if nothing happened (the final state, the events)
events() {    # the run's events, then its final state (a restart at a new width may log a state too)
    $H --width 320 --opt music=0 --opt dump=1 "$@" 2>&1 | game_log > "$O/rz-all.log"
    grep -v "state:\|view:" "$O/rz-all.log"
    grep "state:" "$O/rz-all.log" | tail -1
    grep "view:" "$O/rz-all.log" | tail -1
}
resized() {   # resized <what> <resizes> <headless options...>
    what=$1; rz=$2; shift 2
    events "$@" > "$O/rz-ref.log"
    events $rz "$@" > "$O/rz.log"
    if cmp -s "$O/rz-ref.log" "$O/rz.log"; then ok "$what: resized ($rz), the run goes on the same"
    else ko "$what ($rz):"; diff "$O/rz-ref.log" "$O/rz.log" | head -4; fi
}
resized "1 player, the bot" "--resize 600:520 --resize 1300:427 --resize 2000:320" --frames 2600 --opt bot=1 --opt seed=7 --opt ready=1
resized "co-op, two bots" "--resize 500:427 --resize 1100:560 --resize 1700:320" --frames 2400 --opt bot=2 --opt players=2 --opt seed=6 --opt ready=1
resized "four bots" "--resize 400:520 --resize 900:320 --resize 1500:427" --frames 2000 --opt bot=4 --opt players=4 --opt ready=1 --opt seed=8

# a save state made at one width, loaded at another (in a fresh process, before its first frame): it loads and plays
scenario() {   # scenario <name> <save width> <frames> <after> <opts...>
    name=$1; sw=$2; n=$3; m=$4; shift 4
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    [ -f "$O/$name.input" ] || : > "$O/$name.input"
    out=$($T --width "$sw" $opts --input "$O/$name.input" --frames "$n" --after "$m" --out "$O/dp-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then ok "$name: saved at width $sw (frame $n), replayed the same"
    else ko "$name at width $sw:"; echo "$out" | grep "FAIL\|first"; fi
    for lw in 320 $WS 560; do
        [ "$lw" = "$sw" ] && continue
        out=$($T --width "$lw" $opts --input "$O/$name.input" --resume "$O/dp-$name" --any-width 2>&1)
        if echo "$out" | grep -q "all passed"; then ok "$name: the width-$sw state loads at width $lw and plays $m frames"
        else ko "$name: width $sw -> $lw:"; echo "$out" | grep "FAIL"; fi
    done
}
scenario run 427 1500 400 bot=1 ready=1 seed=11
scenario run520 520 900 300 bot=1 ready=1 seed=12
scenario title 320 60 200
printf "400 tap START\n520 tap START\n" > "$O/paused.input"
scenario paused 520 450 200 bot=1 ready=1 seed=13
scenario coop4 427 900 300 bot=4 ready=1 seed=12
scenario over 560 1500 300 bot=1 botstop=20 ready=1 botruns=2 seed=14
exit $fail

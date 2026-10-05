#!/bin/sh
# Pancake Tower: save states across widths (docs/spec.md "Widescreen": the width is not in a state). A state saved at
# one width loads at every other one and the game runs on (draw.c lays the views and the maps out again for the new
# width): saved at the first wide width and at 320, resumed at the others (--any-width: the pictures differ, the state
# must load before the first frame and the frames run). And the world must be the same: the game's own state (its
# dump=1 line: the towers, the score, the camera, the hash of the run) after those frames is the one of a fresh
# process resumed at the save's width.
#   state_widths.sh <pancaketower_test_states> <out dir> "427 520"
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pancaketower/LICENSE.
T=$1
O=${2:-/tmp/states-widths}
WIDTHS="320 ${3:-427 520}"
mkdir -p "$O"
fail=0
first=$(echo ${3:-427 520} | cut -d' ' -f1)
world() { sed -n 's/.*state: \(st=.*hash=[0-9a-f]*\).*/\1/p' | tail -1; }   # the game's dump (dump=1), but the VRAM count
scenario() {
    name=$1; n=$2; m=$3; shift 3
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    [ -f "$O/$name.input" ] || : > "$O/$name.input"
    for s in $first 320; do
        out=$($T --width $s $opts --opt dump=1 --input "$O/$name.input" --frames "$n" --after "$m" --out "$O/$name-$s" 2>&1)
        if ! echo "$out" | grep -q "all passed"; then
            echo "  FAIL $name: saving at $s columns:"; echo "$out" | grep "FAIL" | head -5; fail=1; continue
        fi
        # the world at frame n + m: a fresh process resumed at the save's width (its pictures compared too)
        out=$($T --width $s $opts --opt dump=1 --input "$O/$name.input" --resume "$O/$name-$s" 2>&1)
        echo "$out" | grep -q "all passed" || { echo "  FAIL $name: resumed at $s, the save's width"; fail=1; }
        want=$(echo "$out" | world)
        for r in $WIDTHS; do
            [ "$r" = "$s" ] && continue
            out=$($T --width $r $opts --opt dump=1 --input "$O/$name.input" --resume "$O/$name-$s" --any-width 2>&1)
            if echo "$out" | grep -q "all passed" && [ -n "$want" ] && [ "$(echo "$out" | world)" = "$want" ]; then
                echo "  ok   $name: saved at $s columns (frame $n), resumed at $r: $m frames, the same world"
            else echo "  FAIL $name: saved at $s, resumed at $r:"; echo "$out" | grep "FAIL" | head -5
                 echo "       world: $(echo "$out" | world)"; echo "       want:  $want"; fail=1; fi
        done
    done
    # a live width change (the desktop runner's window resized: a state, a restart, the state loaded) at frame n: m
    # frames later the picture is the one of a run at the new width from the start (the scenery laid out again)
    set -- $WIDTHS
    for pair in "$2 $3" "$3 $1" "$1 $2"; do
        a=${pair% *}; b=${pair#* }
        $H --width $a $opts --input "$O/$name.input" --frames $((n + m)) --resize $n:$b --png "$O/$name-$a-$b.png" > /dev/null 2>&1
        $H --width $b $opts --input "$O/$name.input" --frames $((n + m)) --png "$O/$name-$b.png" > /dev/null 2>&1
        if [ -s "$O/$name-$b.png" ] && cmp -s "$O/$name-$a-$b.png" "$O/$name-$b.png"; then
            echo "  ok   $name: $a columns, $b from frame $n: the picture of a run at $b"
        else echo "  FAIL $name: $a columns, $b from frame $n: the picture differs from a run at $b"; fail=1; fi
    done
}
H=${T%_test_states}_headless
# 1 player: the kitchen, the attic with the ceiling broken (the carved hole), high up in space; the game over
scenario run 777 300 bot=1 ready=1 seed=3
scenario attic 1234 300 bot=1 ready=1 seed=3
scenario space 200 300 bot=1 ready=1 start=110 seed=6
scenario over 420 200 bot=1 botstop=3 ready=1 botruns=2 seed=4
# the title (1 player, then 3 joined), 2 and 4 players (wider panes)
scenario title 60 200
printf "20 P2 tap A\n25 P3 tap A\n" > "$O/join3.input"
scenario join3 60 200
scenario versus 600 300 bot=2 players=2 ready=1 seed=5
scenario four 600 300 bot=4 players=4 ready=1 seed=5 start=30
exit $fail

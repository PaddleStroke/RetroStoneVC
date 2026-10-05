#!/bin/sh
# Leady Squid widescreen (DESIGN.md "Widescreen"): the picture is as wide as asked, the world is revealed wider, and a
# save state made at one width loads at another (the course and the race go on; the maps are set up for the new width).
#   wide_test.sh <leadysquid_headless> <leadysquid_test_states> <out dir> [widths]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/leadysquid/LICENSE.
H=$1
T=$2
O=${3:-/tmp}/leadysquid-wide
WS=${4:-427 520}
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
png_w() { od -An -tu1 -j16 -N4 "$1" | awk '{print $1 * 16777216 + $2 * 65536 + $3 * 256 + $4}'; }

for w in 320 $WS 560; do
    $H --width "$w" --frames 400 --opt music=0 --opt bot=1 --opt seed=7 --opt ready=1 --png "$O/w$w.png" > "$O/w$w.log" 2>&1
    pw=$(png_w "$O/w$w.png")
    if [ "$pw" = "$w" ]; then ok "--width $w: a ${pw}x240 picture"; else ko "--width $w: the picture is $pw wide"; fi
done
# a state saved at one width, loaded at another (in a fresh process, before its first frame): it loads and plays on
scenario() {   # scenario <name> <save width> <frames> <after> <opts...>
    name=$1; sw=$2; n=$3; m=$4; shift 4
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    [ -f "$O/$name.input" ] || : > "$O/$name.input"
    out=$($T --width "$sw" $opts --input "$O/$name.input" --frames "$n" --after "$m" --out "$O/ls-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then ok "$name: saved at width $sw (frame $n), replayed the same"
    else ko "$name at width $sw:"; echo "$out" | grep "FAIL\|first"; fi
    for lw in 320 $WS 560; do
        [ "$lw" = "$sw" ] && continue
        out=$($T --width "$lw" $opts --input "$O/$name.input" --resume "$O/ls-$name" --any-width 2>&1)
        if echo "$out" | grep -q "all passed"; then ok "$name: the width-$sw state loads at width $lw and plays $m frames"
        else ko "$name: width $sw -> $lw:"; echo "$out" | grep "FAIL"; fi
    done
}
scenario run 427 900 300 bot=1 ready=1
scenario run520 520 900 300 bot=1 ready=1
scenario title 320 60 200
printf "400 tap SELECT\n" > "$O/paused.input"
scenario paused 520 450 120 bot=1 ready=1
scenario results4 427 1100 300 bot=4 botstop=1 ready=1 botruns=2
exit $fail

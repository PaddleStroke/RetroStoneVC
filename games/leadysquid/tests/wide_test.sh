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

# The view (DESIGN.md "Widescreen", the owner's rule): a THIRD of the extra width (past 320 columns) is shown behind
# the player, on the left, two thirds ahead; 320 unchanged. --opt view=1 logs, at exit, "view: width=W left=L
# p1_x=X next_x=N": L the columns added on the left, X the screen column of P1's sprite (as drawn), N the screen column
# of the next obstacle. At each width: L = (W - 320) / 3 (rounded), X = X(320) + L, and N - X = N(320) - X(320) (the start distance
# on the screen is the same).
view_of() { $H --width "$1" --opt music=0 --opt view=1 $2 2>&1 | sed -n 's/.*view: //p' | tail -1; }
val() { echo "$1" | tr ' ' '\n' | sed -n "s/^$2=//p"; }
view_check() {   # view_check <what> "<headless options>"
    r=$(view_of 320 "$2"); x0=$(val "$r" p1_x); n0=$(val "$r" next_x)
    if [ "$(val "$r" left)" != 0 ] || [ -z "$x0" ] || [ -z "$n0" ]; then ko "$1 at 320: '$r'"; return; fi
    for w in $WS 560; do
        r=$(view_of "$w" "$2"); l=$(val "$r" left); x=$(val "$r" p1_x); n=$(val "$r" next_x)
        e=$(( ((w - 320) * 2 + 3) / 6 ))
        if [ "$l" = "$e" ] && [ "$x" = $((x0 + e)) ] && [ -n "$n" ] && [ $((n - x)) = $((n0 - x0)) ]; then
            ok "$1 at $w: a third of the extra width on the left ($l of $((w - 320)) px), P1 at x $x (320: $x0), the next obstacle $((n - x)) px ahead as at 320"
        else ko "$1 at $w: '$r' (expected left=$e p1_x=$((x0 + e)), the next obstacle $((n0 - x0)) px ahead; 320: '$(view_of 320 "$2")')"; fi
    done
}
view_check "the title" "--frames 60"
view_check "the start of a run (the first gate 224 px ahead)" "--frames 2 --opt ready=1 --opt bot=1 --opt seed=7"
view_check "a run of the bot" "--frames 300 --opt ready=1 --opt bot=1 --opt seed=7"
view_check "a 4-player race" "--frames 400 --opt ready=1 --opt bot=4 --opt seed=9"

# a live change of width (the desktop runner's resized window: a state, a restart at the new width, the state loaded)
out=$($H --frames 900 --opt music=0 --opt dump=1 --opt bot=1 --opt seed=7 --opt ready=1 --resize 400:520 --resize 650:320 \
      --shot 500:"$O/resize-520.png" --shot 800:"$O/resize-320.png" 2>&1)
if [ "$(png_w "$O/resize-520.png")" = 520 ] && [ "$(png_w "$O/resize-320.png")" = 320 ] &&
   echo "$out" | grep -q "frame 400: width 520" && echo "$out" | grep "state:" | tail -1 | grep -q "state: st=2 .*runs=0 "; then
    ok "a live change of width (320 -> 520 -> 320) in a run: it goes on"
else ko "a live change of width: $(echo "$out" | grep 'width\|state:' | tr '\n' ' ')"; fi
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

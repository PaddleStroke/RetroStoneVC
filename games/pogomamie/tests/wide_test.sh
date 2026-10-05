#!/bin/sh
# Pogo Mamie widescreen (DESIGN.md "Widescreen"): the picture is as wide as asked, the gameplay is the same at every
# width (the bot's runs: the same events, distances, scores and final state at 320, 427, 520 and 560), a live change
# of width goes on, and a save state made at one width loads at another (the maps are set up for the new width).
#   wide_test.sh <pogomamie_headless> <pogomamie_test_states> <out dir> [widths]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pogomamie/LICENSE.
H=$1
T=$2
O=${3:-/tmp}/pogomamie-wide
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

# the same game at every width: the bot (it reads the screen's 320 columns from the left edge) plays the same runs;
# the course is generated the same (640 px past the camera, beyond the widest screen); the events (big bounces,
# pigeons, stunts, falls...), the distances, the scores and the final state (positions, camera, hash) are identical
game() {   # game <name> <frames> <opts...>
    name=$1; n=$2; shift 2
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    for w in 320 $WS 560; do
        $H --width "$w" --frames "$n" --opt music=0 --opt evlog=1 --opt dump=1 $opts 2>&1 |
            grep "ev f=\|run [0-9]* over\|state:" > "$O/game-$name-$w.log"
    done
    ev=$(grep -c "ev f=" "$O/game-$name-320.log")
    runs=$(grep -c "over at" "$O/game-$name-320.log")
    same=1
    for w in $WS 560; do cmp -s "$O/game-$name-320.log" "$O/game-$name-$w.log" || same=0; done
    if [ "$same" = 1 ] && [ "$ev" -ge 5 ] && grep -q "state:" "$O/game-$name-320.log"; then
        ok "$name: the same game at 320, $WS and 560 ($ev events, $runs runs over, the same final state)"
    else
        ko "$name: the game differs between widths: $(for w in $WS 560; do cmp "$O/game-$name-320.log" "$O/game-$name-$w.log" 2>&1 | head -1; done)"
    fi
}
game solo 6000 bot=1 ready=1 seed=3
game botruns 7000 bot=1 seed=4 botruns=3
game seine 6000 bot=1 ready=1 seed=12 skip=526 botruns=3
game night 4000 bot=1 ready=1 seed=15 skip=2080
game duo 3000 bot=2 players=2 ready=1 seed=6
game quad 3600 bot=4 players=4 ready=1 seed=5 botstop=3 botruns=2

# a live change of width (the desktop runner's resized window: a state, a restart at the new width, the state loaded)
out=$($H --frames 900 --opt music=0 --opt dump=1 --opt bot=1 --opt seed=7 --opt ready=1 --resize 400:520 --resize 650:320 \
      --shot 500:"$O/resize-520.png" --shot 800:"$O/resize-320.png" 2>&1)
ref=$($H --frames 900 --opt music=0 --opt dump=1 --opt bot=1 --opt seed=7 --opt ready=1 2>&1 | grep "state:")
if [ "$(png_w "$O/resize-520.png")" = 520 ] && [ "$(png_w "$O/resize-320.png")" = 320 ] &&
   echo "$out" | grep -q "frame 400: width 520" && [ "$(echo "$out" | grep "state:" | tail -1)" = "$ref" ]; then
    ok "a live change of width (320 -> 520 -> 320) in a run: it goes on, the same run as without it"
else ko "a live change of width: $(echo "$out" | grep 'width\|state:' | tr '\n' ' ')"; fi

# a state saved at one width, loaded at another (in a fresh process, before its first frame): it loads and plays on
scenario() {   # scenario <name> <save width> <frames> <after> <opts...>
    name=$1; sw=$2; n=$3; m=$4; shift 4
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    [ -f "$O/$name.input" ] || : > "$O/$name.input"
    out=$($T --width "$sw" $opts --input "$O/$name.input" --frames "$n" --after "$m" --out "$O/pm-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then ok "$name: saved at width $sw (frame $n), replayed the same"
    else ko "$name at width $sw:"; echo "$out" | grep "FAIL\|first"; fi
    for lw in 320 $WS 560; do
        [ "$lw" = "$sw" ] && continue
        out=$($T --width "$lw" $opts --input "$O/$name.input" --resume "$O/pm-$name" --any-width 2>&1)
        if echo "$out" | grep -q "all passed"; then ok "$name: the width-$sw state loads at width $lw and plays $m frames"
        else ko "$name: width $sw -> $lw:"; echo "$out" | grep "FAIL"; fi
    done
}
scenario run 427 900 300 bot=1 ready=1 skip=4
scenario run520 520 900 300 bot=1 ready=1 skip=4
scenario night 560 900 300 bot=1 ready=1 skip=2100 seed=24
scenario title 320 60 200
printf "400 tap START\n" > "$O/paused.input"
scenario paused 520 450 120 bot=1 ready=1 skip=4
scenario four 427 600 300 bot=4 players=4 ready=1 seed=5
scenario four-over 520 3000 300 bot=4 players=4 ready=1 seed=5 botstop=3
exit $fail

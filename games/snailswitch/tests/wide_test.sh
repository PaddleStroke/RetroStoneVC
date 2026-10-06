#!/bin/sh
# Snail Switch widescreen (DESIGN.md "Widescreen"): the picture is as wide as asked (320..560), and the game is the same
# at every width: the vine and its hazards are world content (one world coordinate, generated per hazard from the
# seed), only revealed wider, so the same inputs give the same runs and the same logs at 320, 427, 520 and 560; a width
# change in the middle of a run (--resize: a save state, a restart at the new width, the state loaded) goes on exactly
# as the run that never changed; and a save state made at one width loads and plays at another (--any-width).
#   wide_test.sh <snailswitch_headless> <snailswitch_test_states> <out dir> [widths]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/snailswitch/LICENSE.
H=$1
T=$2
O=${3:-/tmp}/snailswitch-wide
WS=${4:-427 520}
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
png_w() { od -An -tu1 -j16 -N4 "$1" | awk '{print $1 * 16777216 + $2 * 65536 + $3 * 256 + $4}'; }
# the game's log without the width: the start-up line and the width changes
game_log() { grep -v "RetroStone VC\|frame [0-9]*: width [0-9]*$"; }

for w in 320 $WS 560; do
    out=$($H --width "$w" --frames 400 --opt music=0 --opt strict=1 --opt bot=1 --opt seed=7 --opt ready=1 --png "$O/w$w.png" 2>&1)
    pw=$(png_w "$O/w$w.png")
    if [ "$pw" = "$w" ]; then ok "--width $w: a ${pw}x240 picture"; else ko "--width $w: the picture is '$pw' wide"; fi
    if echo "$out" | grep -q "rs strict"; then ko "--width $w: strict-mode warnings: $(echo "$out" | grep 'rs strict' | head -2)"
    else ok "--width $w: no strict-mode warning (VRAM, sprites per line...)"; fi
done

# the same game at every width (the gameplay and the bots never look at the width)
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
same "1 player, the bot" --frames 3600 --opt bot=1 --opt seed=7 --opt ready=1
same "from the title, three runs of the bot" --frames 9000 --opt bot=1 --opt botruns=3 --opt botstop=40 --opt seed=4
same "2 players, two bots" --frames 3600 --opt bot=2 --opt players=2 --opt seed=9
same "four bots, to the results" --frames 4000 --opt bot=4 --opt players=4 --opt ready=1 --opt seed=5 --opt botstop=5
: > "$O/rhythm.input"
i=30; while [ $i -lt 1500 ]; do echo "$i tap A" >> "$O/rhythm.input"; i=$((i + 23)); done
same "scripted inputs" --frames 1500 --input "$O/rhythm.input" --opt seed=3


# The view (DESIGN.md "Widescreen", the owner's rule): a THIRD of the extra width (past 320 columns) is shown behind
# the player, on the left, two thirds ahead; 320 unchanged. --opt view=1 logs, at exit, "view: width=W left=L
# p1_x=X next_x=N": L the columns added on the left, X the screen column of P1's sprite (as drawn), N the screen column
# of the next hazard. At each width: L = (W - 320) / 3 (rounded), X = X(320) + L, and N - X = N(320) - X(320) (the start distance
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
            ok "$1 at $w: a third of the extra width on the left ($l of $((w - 320)) px), P1 at x $x (320: $x0), the next hazard $((n - x)) px ahead as at 320"
        else ko "$1 at $w: '$r' (expected left=$e p1_x=$((x0 + e)), the next hazard $((n0 - x0)) px ahead; 320: '$(view_of 320 "$2")')"; fi
    done
}
view_check "the title" "--frames 60"
view_check "the start of a run" "--frames 2 --opt ready=1 --opt bot=1 --opt seed=7"
view_check "a run of the bot" "--frames 300 --opt ready=1 --opt bot=1 --opt seed=7"
view_check "4 snails" "--frames 300 --opt ready=1 --opt bot=4 --opt players=4 --opt seed=5"

# the width changes during a run: the run goes on as if nothing happened (the final state, the events)
events() {    # the run's events, then its final state (a restart at a new width may log a state too)
    $H --width 320 --opt music=0 --opt dump=1 "$@" 2>&1 | game_log > "$O/rz-all.log"
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
resized "1 player, the bot" "--resize 600:520 --resize 1300:427 --resize 2000:320" --frames 2600 --opt bot=1 --opt seed=7 --opt ready=1
resized "2 players, two bots" "--resize 500:427 --resize 1100:560 --resize 1700:320" --frames 2400 --opt bot=2 --opt players=2 --opt seed=9
resized "four bots, to the results" "--resize 150:520 --resize 280:320 --resize 330:427" --frames 600 --opt bot=4 --opt players=4 --opt ready=1 --opt seed=5 --opt botstop=5
$H --opt music=0 --frames 900 --opt bot=1 --opt seed=7 --opt ready=1 --resize 400:520 --resize 650:320 \
   --shot 500:"$O/resize-520.png" --shot 800:"$O/resize-320.png" > /dev/null 2>&1
if [ "$(png_w "$O/resize-520.png")" = 520 ] && [ "$(png_w "$O/resize-320.png")" = 320 ]; then
    ok "a live change of width (320 -> 520 -> 320): the pictures follow"
else ko "a live change of width: the pictures are $(png_w "$O/resize-520.png") and $(png_w "$O/resize-320.png") wide"; fi

# a save state made at one width, loaded at another (in a fresh process, before its first frame): it loads and plays
scenario() {   # scenario <name> <save width> <frames> <after> <opts...>
    name=$1; sw=$2; n=$3; m=$4; shift 4
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    [ -f "$O/$name.input" ] || : > "$O/$name.input"
    out=$($T --width "$sw" $opts --input "$O/$name.input" --frames "$n" --after "$m" --out "$O/ss-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then ok "$name: saved at width $sw (frame $n), replayed the same"
    else ko "$name at width $sw:"; echo "$out" | grep "FAIL\|first"; fi
    for lw in 320 $WS 560; do
        [ "$lw" = "$sw" ] && continue
        out=$($T --width "$lw" $opts --input "$O/$name.input" --resume "$O/ss-$name" --any-width 2>&1)
        if echo "$out" | grep -q "all passed"; then ok "$name: the width-$sw state loads at width $lw and plays $m frames"
        else ko "$name: width $sw -> $lw:"; echo "$out" | grep "FAIL"; fi
    done
}
scenario run 427 700 400 bot=1 ready=1
scenario run520 520 900 300 bot=1 ready=1
printf "20 P2 tap A\n50 P3 tap A\n90 P4 tap A\n150 tap A\n" > "$O/lobby.input"
scenario lobby 320 60 200
printf "400 tap SELECT\n520 tap SELECT\n" > "$O/paused.input"
scenario paused 520 450 200 bot=1 ready=1
scenario over 427 700 400 bot=1 botstop=2 ready=1 botruns=2
scenario four 560 500 300 bot=4 ready=1
scenario results4 427 920 300 bot=4 botstop=2 ready=1 botruns=2
exit $fail

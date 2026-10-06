#!/bin/sh
# Mouse Espresso widescreen (DESIGN.md "Widescreen"): the picture is as wide as asked (320..560) and the game is the
# same at every width (the same runs, the same bot decisions, the same logs and final states); a width change in the
# middle of a run (--resize: a save state, a restart at the new width, the state loaded) goes on exactly as the run
# that never changed; a save state made at one width loads and plays at another (test_states --any-width).
#   wide_test.sh <mouseespresso_headless> <mouseespresso_test_states> <out dir> [widths]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/mouseespresso/LICENSE.
H=$1
T=$2
O=${3:-/tmp}/mouseespresso-wide
WS=${4:-427 520}
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
png_w() { od -An -tu1 -j16 -N4 "$1" | awk '{print $1 * 16777216 + $2 * 65536 + $3 * 256 + $4}'; }
# the game's log without the width: the start-up line and the VRAM count (the maps follow the width)
game_log() { grep -v "RetroStone VC\|frame [0-9]*: width [0-9]*$" | sed 's/ vram=[0-9]*//'; }

for w in 320 $WS 560; do
    $H --width "$w" --frames 400 --opt music=0 --opt bot=1 --opt seed=7 --opt ready=1 --png "$O/w$w.png" > /dev/null 2>&1
    pw=$(png_w "$O/w$w.png")
    if [ "$pw" = "$w" ]; then ok "--width $w: a ${pw}x240 picture"; else ko "--width $w: the picture is '$pw' wide"; fi
done

# the same game at every width (the cafe is only revealed wider: the rules never look at the width)
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
# the start of a run: the course does not depend on the width, so the first pads, hills and hazards sit at the same
# distance from the player as at 320 (a wider screen shows them from the first frame); no longer run-up
same "the start of a run: the first challenges at the 320 distance" --frames 40 --opt bot=1 --opt seed=7 --opt ready=1
same "1 player, the bot" --frames 3000 --opt bot=1 --opt seed=7 --opt ready=1
same "from the title, three runs of the bot" --frames 9000 --opt bot=1 --opt botruns=3 --opt botstop=30 --opt seed=4
same "four bots, to the results" --frames 4000 --opt bot=4 --opt seed=5 --opt ready=1 --opt botstop=5


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
view_check "4 mice" "--frames 400 --opt ready=1 --opt bot=4 --opt seed=5"

# the width changes during a run: the run goes on as if nothing happened (the events, the final state)
events() {
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
resized "four bots" "--resize 400:520 --resize 900:320 --resize 1500:427" --frames 2000 --opt bot=4 --opt seed=5 --opt ready=1

# a save state made at one width, loaded at another (in a fresh process, before its first frame): it loads and plays
scenario() {   # scenario <name> <save width> <frames> <after> <opts...>
    name=$1; sw=$2; n=$3; m=$4; shift 4
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    [ -f "$O/$name.input" ] || : > "$O/$name.input"
    out=$($T --width "$sw" $opts --input "$O/$name.input" --frames "$n" --after "$m" --out "$O/me-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then ok "$name: saved at width $sw (frame $n), replayed the same"
    else ko "$name at width $sw:"; echo "$out" | grep "FAIL\|first"; fi
    for lw in 320 $WS 560; do
        [ "$lw" = "$sw" ] && continue
        out=$($T --width "$lw" $opts --input "$O/$name.input" --resume "$O/me-$name" --any-width 2>&1)
        if echo "$out" | grep -q "all passed"; then ok "$name: the width-$sw state loads at width $lw and plays $m frames"
        else ko "$name: width $sw -> $lw:"; echo "$out" | grep "FAIL"; fi
    done
}
scenario run 427 700 400 bot=1 ready=1
scenario run520 520 900 300 bot=1 ready=1
scenario title 320 60 200
printf "400 tap SELECT\n520 tap SELECT\n" > "$O/paused.input"
scenario paused 520 450 200 bot=1 ready=1
scenario four 520 600 300 bot=4 ready=1 seed=5
scenario results4 427 2400 300 bot=4 botstop=2 ready=1 botruns=1
exit $fail

#!/bin/sh
# Mole Marbles widescreen (DESIGN.md "Widescreen"): the picture is as wide as asked (320..560) and the arena is the
# whole screen (320: the classic game); a run is the same twice at every width, the bot plays well; a width change in the
# middle of a run (--resize: a save state, a restart at the new width, the state loaded) goes on in the new arena,
# the same every time; a save state made at one width loads and plays at another (test_states --any-width).
#   wide_test.sh <molemarbles_headless> <molemarbles_test_states> <out dir> [widths]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/molemarbles/LICENSE.
H=$1
T=$2
O=${3:-/tmp}/molemarbles-wide
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

# the arena is the whole screen (inside the timbers at its edges, 6 px each; 320: the screen's edges): the game is the
# same at 320 as it always was (the classic tests), wider it is wider; at every width a run is deterministic
arena() { if [ "$1" = 320 ]; then echo "0,320"; else m=$((($1 - 320) / 2)); echo "$((6 - m)),$(($1 - m - 6))"; fi; }
field() { echo "$2" | sed -n "s/.* $1=\([0-9,-]*\) .*/\1/p"; }
same() {   # same <what> <headless options...>
    what=$1; shift
    for w in 320 $WS 560; do
        $H --width "$w" --opt music=0 --opt dump=1 "$@" 2>&1 | game_log > "$O/same-a.log"
        $H --width "$w" --opt music=0 --opt dump=1 "$@" 2>&1 | game_log > "$O/same-b.log"
        a=$(field arena "$(grep "state:" "$O/same-a.log")")
        if cmp -s "$O/same-a.log" "$O/same-b.log" && [ "$a" = "$(arena "$w")" ]; then
            ok "$what at $w: the arena is $a, the same run twice ($(grep -c "" "$O/same-a.log") log lines)"
        else ko "$what at $w: arena '$a' (expected $(arena "$w")), or two runs differ"; diff "$O/same-a.log" "$O/same-b.log" | head -4; fi
    done
}
same "1 player, the bot" --frames 3000 --opt bot=1 --opt seed=7 --opt ready=1
same "from the title, three runs of the bot" --frames 9000 --opt bot=1 --opt botruns=3 --opt botstop=60 --opt seed=4
same "four bots, to the results" --frames 4000 --opt bot=4 --opt seed=5 --opt ready=1 --opt botstop=5
# the bot still plays well on a wide arena
for w in $WS 560; do
    s=$($H --width "$w" --opt music=0 --opt dump=1 --frames 4000 --opt bot=1 2>&1 | sed -n 's/.*state: .* score=\([0-9]*\) .*/\1/p')
    if [ "${s:-0}" -ge 20 ]; then ok "at $w the bot reaches $s (>= 20)"; else ko "at $w the bot only reached $s"; fi
done

# the width changes during a run (a save state, a restart at the new width, the state loaded): the arena follows the
# screen (the marbles and moles kept inside its walls) and the run goes on, the same every time
resized() {   # resized <what> <resizes> <last width> <headless options...>
    what=$1; rz=$2; lw=$3; shift 3
    $H --width 320 --opt music=0 --opt dump=1 $rz "$@" 2>&1 | game_log > "$O/rz-a.log"
    $H --width 320 --opt music=0 --opt dump=1 $rz "$@" 2>&1 | game_log > "$O/rz-b.log"
    a=$(field arena "$(grep "state:" "$O/rz-a.log" | tail -1)")   # (each restart dumps a state line: the last)
    if cmp -s "$O/rz-a.log" "$O/rz-b.log" && [ "$a" = "$(arena "$lw")" ]; then ok "$what: resized ($rz), the arena $a, the same twice"
    else ko "$what ($rz): arena '$a' (expected $(arena "$lw")), or two runs differ"; diff "$O/rz-a.log" "$O/rz-b.log" | head -4; fi
}
resized "1 player, the bot" "--resize 600:520 --resize 1300:427 --resize 2000:320" 320 --frames 2600 --opt bot=1 --opt seed=7 --opt ready=1
resized "four bots" "--resize 400:520 --resize 900:320 --resize 1500:427" 427 --frames 2000 --opt bot=4 --opt seed=5 --opt ready=1

# a save state made at one width, loaded at another (in a fresh process, before its first frame): it loads and plays
scenario() {   # scenario <name> <save width> <frames> <after> <opts...>
    name=$1; sw=$2; n=$3; m=$4; shift 4
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    [ -f "$O/$name.input" ] || : > "$O/$name.input"
    out=$($T --width "$sw" $opts --input "$O/$name.input" --frames "$n" --after "$m" --out "$O/mm-$name" 2>&1)
    if echo "$out" | grep -q "all passed"; then ok "$name: saved at width $sw (frame $n), replayed the same"
    else ko "$name at width $sw:"; echo "$out" | grep "FAIL\|first"; fi
    for lw in 320 $WS 560; do
        [ "$lw" = "$sw" ] && continue
        out=$($T --width "$lw" $opts --input "$O/$name.input" --resume "$O/mm-$name" --any-width 2>&1)
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

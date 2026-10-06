#!/bin/sh
# Radish Rascals touch controls (rs_game.touch_native; DESIGN.md "Controls"): scripted touches in the headless runner,
# each run compared with the same run played on the pad (the final state line with its hash, the game-over lines and
# the last picture must be identical), at each width:
#   - a tap on the title starts, as A does, and does not act in the first frame of play (a second finger already
#     down on a bed then does nothing either);
#   - a finger going down on or near a bed defends it at once, as the pad direction of that bed: a seeded run of
#     visitors, each rabbit's bed touched (all four beds, the touch points off the beds' centres, wider than 320 the
#     side beds touched in the garden's margins), the friends left alone: the same score as the pad's, still running;
#   - a touch on the wrong bed ends the run, as the wrong direction; a finger held on a bed does not repeat;
#   - while paused touches never act, not even after the pause (a finger held through it);
#   - 2 players: P1's beds in its own (smaller) view; a touch in P2's view acts on no bed;
#   - after the game over a tap goes back to the title (not before the panel's lock, as the pad).
#   touch_test.sh <radishrascals_headless> <out dir> [widths]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/radishrascals/LICENSE.
H=$1
O=${2:-/tmp}/radishrascals-touch
WS=${3:-320 427 520}
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }

# run <name> <width> <frames> <script> [opts...]: the state line and the game-over lines in $O/<name>.log
run() {
    name=$1; w=$2; frames=$3; printf "$4" > "$O/$name.input"; shift 4
    opts=""
    for o in "$@"; do opts="$opts --opt $o"; done
    $H --width "$w" --frames "$frames" --input "$O/$name.input" --opt music=0 --opt dump=1 --opt strict=1 $opts \
        --png "$O/$name.png" 2>&1 | grep "state:\|over at frame\|rs strict" > "$O/$name.log"
}
# same <what> <touch run> <pad run>: the same log and picture
same() {
    if [ -s "$O/$2.log" ] && ! grep -q "rs strict" "$O/$2.log" && cmp -s "$O/$2.log" "$O/$3.log" && cmp -s "$O/$2.png" "$O/$3.png"; then ok "$1"
    else ko "$1"; diff "$O/$2.log" "$O/$3.log" | head -4; fi
}
field() { sed -n "s/.*state:.* $2=\([0-9,]*\).*/\1/p" "$O/$1.log"; }
st() { sed -n 's/.*state: st=\([0-9]*\).*/\1/p' "$O/$1.log"; }

# The visitors of seed 4 (play.c spawn(): the first at step 25, then every 54 steps: wave w goes active at step
# 42 + 54w; its bed: wave 0 bed 0, then mix(seed + w * 2654435761) & 3, the next free one if busy: a friend's bed
# is still busy at the next spawn; a friend every 5th from the 4th). The run starts at frame 30 (step 1): the bed of
# wave w is pressed at frame 81 + 54w. Prints "frame bed friend" for 11 waves (the pace changes at the 12th).
waves() {
    python3 - <<'PY'
M = 0xffffffff
def mix(v):
    v ^= v >> 16; v = (v * 0x7feb352d) & M; v ^= v >> 15; v = (v * 0x846ca68b) & M; return v ^ (v >> 16)
busy = -1
for w in range(11):
    slot = 0 if w == 0 else mix((4 + w * 2654435761) & M) & 3
    bed = next((slot + k) & 3 for k in range(4) if (slot + k) & 3 != busy)
    friend = w % 5 == 3
    busy = bed if friend else -1
    print(81 + 54 * w, bed, int(friend))
PY
}
waves > "$O/waves.txt"
beds=$(awk '$3 == 0 {print $2}' "$O/waves.txt" | sort -u | tr -d '\n')
if [ "$beds" = "0123" ]; then ok "seed 4: rabbits in all four beds in the first 11 visitors"
else ko "seed 4: the rabbits' beds are only '$beds'"; fi

for w in $WS; do
    m=$(( (w - 320) / 2 ))
    echo "== width $w"
    # the title: a tap starts (released at frame 30, as A pressed at 30); a second finger down at the start is ignored
    run t-touch "$w" 100 "27 tap-at 50%% 60%%\n30 T2 touch @160 92\n95 T2 up\n" seed=4
    run t-pad "$w" 100 "30 tap A\n" seed=4
    [ "$(st t-touch)" = 1 ] && ok "$w: a tap on the title starts" || ko "$w: a tap on the title does not start"
    same "$w: the start tap and a finger already down act like A alone (no first move)" t-touch t-pad

    # every rabbit's bed touched (points off the centre; wide: the side beds in the margins), friends left alone
    tscript="27 tap-at @160 120\n"; pscript="30 tap A\n"; last=0
    while read f bed friend; do
        last=$f
        [ "$friend" = 1 ] && continue
        k=$(( f % 3 ))
        case $bed in
            0) x=$(( 140 + 20 * k )); y=$(( 70 + 15 * k )); b=UP ;;
            1) x=$(( m > 30 ? 320 + m - 15 : 270 + 10 * k )); y=$(( 125 + 20 * k )); b=RIGHT ;;
            2) x=$(( 135 + 25 * k )); y=$(( 215 + 10 * k )); b=DOWN ;;
            3) x=$(( m > 30 ? 10 - m : 50 - 15 * k )); y=$(( 165 - 20 * k )); b=LEFT ;;
        esac
        tscript="$tscript$f tap-at @$x $y\n"; pscript="$pscript$f tap $b\n"
    done < "$O/waves.txt"
    run beds-touch "$w" $((last + 30)) "$tscript" seed=4
    run beds-pad "$w" $((last + 30)) "$pscript" seed=4
    s=$(field beds-touch score)
    if [ "$(st beds-touch)" = 1 ] && [ "${s:-0}" -gt 50 ]; then ok "$w: touching each rabbit's bed defends it (score $s, running)"
    else ko "$w: the touched beds were not defended (st $(st beds-touch), score $s)"; fi
    same "$w: the bed touches play as the pad's directions" beds-touch beds-pad

    # a wrong bed ends the run; a finger held on a bed does not repeat (wave 1 is missed, as with UP held)
    run wrong-touch "$w" 160 "27 tap-at @160 120\n81 tap-at @250 150\n" seed=4
    run wrong-pad "$w" 160 "30 tap A\n81 tap RIGHT\n" seed=4
    [ "$(st wrong-touch)" != 1 ] && [ "$(field wrong-touch score)" = 0 ] && ok "$w: the wrong bed ends the run" ||
        ko "$w: the wrong bed did not end the run"
    same "$w: the wrong bed as the wrong direction" wrong-touch wrong-pad
    run hold-touch "$w" 260 "27 tap-at @160 120\n81 touch @160 88\n240 up\n" seed=4
    run hold-pad "$w" 260 "30 tap A\n81 UP\n240 -\n" seed=4
    same "$w: a held finger as a held direction (no repeat)" hold-touch hold-pad

    # paused: a tap and a finger held through the pause act on nothing, even after it
    run pause-touch "$w" 200 "27 tap-at @160 120\n50 tap START\n55 tap-at @160 92\n60 T2 touch @250 150\n70 tap START\n120 T2 up\n" seed=4
    run pause-pad "$w" 200 "30 tap A\n50 tap START\n70 tap START\n" seed=4
    same "$w: touches during the pause never act" pause-touch pause-pad

    # 2 players: P1's view (the left one), its beds smaller; the right view is P2's (a pad): a touch there does nothing
    eval "$(python3 - "$w" <<'PY'
import sys
W = int(sys.argv[1]); X = -((W - 320) // 2); hw = (W - 2) // 2
def bed(vx, b):
    xs, ys = (160, 252, 160, 68), (90, 145, 202, 145)
    return vx + (hw - 159) // 2 + 8 + xs[b] * 143 // 320, ys[b]
print("p1x=%d p1y=%d p2x=%d p2y=%d" % (bed(X, 0) + bed(W - hw + X, 0)))
PY
)"
    run two-touch "$w" 100 "27 tap-at @160 120\n81 tap-at @$p1x $p1y\n" seed=4 players=2
    run two-pad "$w" 100 "30 tap A\n81 tap UP\n" seed=4 players=2
    same "$w: 2 players: P1's up bed in its view" two-touch two-pad
    run two-other "$w" 100 "27 tap-at @160 120\n81 tap-at @$p2x $p2y\n" seed=4 players=2
    run two-none "$w" 100 "30 tap A\n" seed=4 players=2
    same "$w: 2 players: a touch in P2's view acts on no bed" two-other two-none

    # the game over: a tap goes back to the title (the panel's lock first, as the pad)
    run over "$w" 400 "27 tap-at @160 120\n" seed=4
    g=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$O/over.log")
    if [ -z "$g" ]; then ko "$w: no game over"; continue; fi
    run back-early "$w" $((g + 30)) "27 tap-at @160 120\n$((g + 10)) tap-at 30%% 40%%\n" seed=4
    [ "$(st back-early)" = 3 ] && ok "$w: a tap before the panel's lock stays on the game over" || ko "$w: an early tap left the game over"
    run back-touch "$w" $((g + 60)) "27 tap-at @160 120\n$((g + 47)) tap-at 30%% 40%%\n" seed=4
    run back-pad "$w" $((g + 60)) "30 tap A\n$((g + 50)) tap A\n" seed=4
    [ "$(st back-touch)" = 0 ] && ok "$w: a tap after the game over goes back to the title" || ko "$w: the tap did not go back to the title"
    same "$w: back to the title as with A" back-touch back-pad
done
exit $fail

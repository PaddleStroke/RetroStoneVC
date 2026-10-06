#!/bin/sh
# Raccoon Recycler touch controls (rs_game.touch_native; DESIGN.md "Controls"): scripted touches in the headless
# runner, each run compared with the same run played on the pad (the final state line with its hash, the game-over
# lines and the last picture must be identical), at each width:
#   - a tap on the title starts, as A does, and does not act in the first frame of play (a second finger going down on
#     a branch on that frame does not set the junction);
#   - a finger going down sets the junction at once to the branch of the bin under it (the screen in three columns
#     around the three bins, the whole height; wider than 320 the margins are the side branches'): the first toy of
#     a seed for each bin (a duck: left, a sea star: centre, a snail: right) routed by touch scores, as the pad's
#     Left, Up/Down and Right; the wrong branch ends the run;
#   - while paused touches never act, not even after the pause (a finger held through it);
#   - after the game over a tap goes back to the title (not before the panel's lock, as the pad).
#   touch_test.sh <raccoonrecycler_headless> <out dir> [widths]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/raccoonrecycler/LICENSE.
H=$1
O=${2:-/tmp}/raccoonrecycler-touch
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
    if [ -s "$O/$2.log" ] && ! grep -q "rs strict" "$O/$2.log" && cmp -s "$O/$2.log" "$O/$3.log" &&
        cmp -s "$O/$2.png" "$O/$3.png"; then ok "$1"
    else ko "$1"; diff "$O/$2.log" "$O/$3.log" | head -4; fi
}
field() { sed -n "s/.*state:.* $2=\([0-9]*\).*/\1/p" "$O/$1.log"; }

# the first toy of a seed (play.c: world_init's spawn, xorshift32 from the seed): its species (the opening order is
# duck / sea star / snail = left / centre / right): the first seed from 1 with a snail first
snail=$(python3 -c '
M = 0xffffffff
def first(seed):
    x = seed; x ^= (x << 13) & M; x ^= x >> 17; x ^= (x << 5) & M; return (x & M) % 9 // 3
print(next(s for s in range(1, 1000) if first(s) == 2))')

for w in $WS; do
    m=$(( (w - 320) / 2 ))
    echo "== width $w"
    # the title: a tap starts (released at frame 30, as A pressed at 30); a finger going down then on the right branch
    # does not move the junction (seed 7: a duck, the left bin, where the junction starts)
    run t-touch "$w" 160 "27 tap-at 50%% 60%%\n30 T2 touch @290 120\n40 T2 up\n" seed=7
    run t-pad "$w" 160 "30 tap A\n" seed=7
    [ "$(field t-touch st)" = 1 ] && [ "$(field t-touch score)" = 1 ] && ok "$w: a tap on the title starts" ||
        ko "$w: a tap on the title does not start (or the start acted)"
    same "$w: the start tap and a finger going down with it act like A alone" t-touch t-pad

    # each bin's branch by touch, as the pad: the right then the left (the junction starts on the left) for the duck
    # of seed 7, the centre for the sea star of seed 17, the right for the first snail; the points: anywhere in the
    # bin's column (the top, the conveyor, the bins; wide: the margins for the side branches)
    lx=$(( m > 30 ? -m + 10 : 40 )); rx=$(( m > 30 ? 320 + m - 10 : 280 ))
    run left-touch "$w" 160 "27 tap-at @160 120\n60 tap-at @$rx 30\n120 tap-at @$lx 200\n" seed=7
    run left-pad "$w" 160 "30 tap A\n60 tap RIGHT\n120 tap LEFT\n" seed=7
    [ "$(field left-touch score)" = 1 ] && ok "$w: the left bin's column routes left (the duck scores)" || ko "$w: left"
    same "$w: the left column as Left" left-touch left-pad
    run centre-touch "$w" 160 "27 tap-at @160 120\n60 tap-at @150 100\n" seed=17
    run centre-pad "$w" 160 "30 tap A\n60 tap UP\n" seed=17
    [ "$(field centre-touch score)" = 1 ] && ok "$w: the conveyor's column routes to the centre (the sea star scores)" ||
        ko "$w: centre"
    same "$w: the centre column as Up/Down" centre-touch centre-pad
    run right-touch "$w" 160 "27 tap-at @160 120\n60 tap-at @225 215\n" seed=$snail
    run right-pad "$w" 160 "30 tap A\n60 tap RIGHT\n" seed=$snail
    [ "$(field right-touch score)" = 1 ] && ok "$w: the right bin's column routes right (the snail of seed $snail scores)" ||
        ko "$w: right"
    same "$w: the right column as Right" right-touch right-pad
    run wrong-touch "$w" 210 "27 tap-at @160 120\n60 tap-at @$rx 140\n" seed=7
    run wrong-pad "$w" 210 "30 tap A\n60 tap RIGHT\n" seed=7
    [ "$(field wrong-touch st)" = 3 ] && [ "$(field wrong-touch score)" = 0 ] && ok "$w: the wrong branch ends the run" ||
        ko "$w: the wrong branch did not end the run"
    same "$w: the wrong branch as the pad's" wrong-touch wrong-pad

    # paused: a tap and a finger held through the pause act on nothing, even after it (the duck still goes left)
    run pause-touch "$w" 200 "27 tap-at @160 120\n50 tap START\n55 tap-at @$rx 120\n60 T2 touch @$rx 60\n70 tap START\n120 T2 up\n" seed=7
    run pause-pad "$w" 200 "30 tap A\n50 tap START\n70 tap START\n" seed=7
    [ "$(field pause-touch score)" = 1 ] && ok "$w: touches during the pause never act" || ko "$w: a touch acted in the pause"
    same "$w: the pause with touches as without" pause-touch pause-pad

    # the game over: a tap goes back to the title (the panel's lock first, as the pad)
    run over "$w" 400 "27 tap-at @160 120\n60 tap-at @$rx 120\n" seed=7
    g=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$O/over.log")
    if [ -z "$g" ]; then ko "$w: no game over"; continue; fi
    run back-early "$w" $((g + 30)) "27 tap-at @160 120\n60 tap-at @$rx 120\n$((g + 10)) tap-at 30%% 40%%\n" seed=7
    [ "$(field back-early st)" = 3 ] && ok "$w: a tap before the panel's lock stays on the game over" ||
        ko "$w: an early tap left the game over"
    run back-touch "$w" $((g + 60)) "27 tap-at @160 120\n60 tap-at @$rx 120\n$((g + 47)) tap-at 30%% 40%%\n" seed=7
    run back-pad "$w" $((g + 60)) "30 tap A\n60 tap RIGHT\n$((g + 50)) tap A\n" seed=7
    [ "$(field back-touch st)" = 0 ] && ok "$w: a tap after the game over goes back to the title" ||
        ko "$w: the tap did not go back to the title"
    same "$w: back to the title as with A" back-touch back-pad
done
exit $fail

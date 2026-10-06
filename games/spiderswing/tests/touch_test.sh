#!/bin/sh
# Spider Swing touch controls (DESIGN.md "Controls", docs/spec.md "Touch"; rs_game.touch_native): headless runs with
# scripted fingers, checked on the final state's log line (st, mode, x, the state hash...) and against the same runs
# played on the pad. A tap starts on the title; a finger going down lets go of the web on its press (the very run
# of A); in flight a finger held on the left / right half steers (the very run of Left / Right held), a quick tap
# never nudges the flight; a finger on the web neither lets go nor advances; nothing while paused (and nothing kept
# for later); a tap on the game-over panel goes back to the title (not a finger left from the run, not a tap before
# the lock). The same at every width.
#   touch_test.sh <spiderswing_headless> <out dir> [widths]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/spiderswing/LICENSE.
H=$1
O=${2:-/tmp}/spiderswing-touch
WS=${3:-427 520}
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
# run <name> <frames> <script> [width]: the final state's log line
run() {
    printf "$3" > "$O/$1.input"
    $H --width "${4:-320}" --frames "$2" --input "$O/$1.input" --opt seed=4 --opt strict=1 --opt dump=1 \
        --opt music=0 2> "$O/$1.log" > /dev/null
    if grep -q "rs strict" "$O/$1.log"; then ko "$1: strict mode"; grep "rs strict" "$O/$1.log" | head -3; fi
    grep "state:" "$O/$1.log" | tail -1
}
field() { echo "$1" | sed -n "s/.* $2=\([^ ]*\).*/\1/p"; }
same() { [ "$(field "$1" hash)" = "$(field "$2" hash)" ] && [ "$(field "$1" x)" = "$(field "$2" x)" ] &&
         [ "$(field "$1" vx)" = "$(field "$2" vx)" ] && [ "$(field "$1" mode)" = "$(field "$2" mode)" ]; }

START="20 tap-at 50%% 30%%\n"         # a tap: the title starts as the finger is lifted (frame 23)
PAD="23 tap A\n"                      # the same start on the pad
LET="63 tap-at 50%% 50%%\n"           # a tap on the web: lets go on its press (frame 63)

# the title
s=$(run title 60 "")
[ "$(field "$s" st)" = 0 ] && ok "the title waits (no touch)" || ko "the title did not wait: $s"
a=$(run start 50 "$START")
b=$(run start-pad 50 "$PAD")
if [ "$(field "$a" st)" = 1 ] && [ "$(field "$a" mode)" = 0 ] && same "$a" "$b"; then
    ok "a tap on the title starts the run (the very run of A), the spider on its web"
else ko "a tap on the title: $a / pad $b"; fi
s=$(run hold-title 60 "20 touch 50%% 30%%\n")
[ "$(field "$s" st)" = 0 ] && ok "a finger still down on the title does not start yet" || ko "finger down: $s"

# letting go of the web
a=$(run let-go 64 "${START}${LET}")
b=$(run let-go-pad 64 "${PAD}63 tap A\n")
if [ "$(field "$a" mode)" = 1 ] && same "$a" "$b"; then ok "a tap lets go of the web on its press (the very run of A)"
else ko "let go: $a / pad $b"; fi
a=$(run tap-flight 100 "${START}${LET}")
b=$(run tap-flight-pad 100 "${PAD}63 tap A\n")
same "$a" "$b" && ok "the tap does not nudge the flight (the same run as A, 37 frames on)" || ko "tap flight: $a / $b"
a=$(run long-press 90 "${START}63 touch 90%% 50%%\n86 up\n")
b=$(run long-press-pad 90 "${PAD}63 tap A\n82 RIGHT\n86 -\n")
same "$a" "$b" && ok "a finger held from the let-go steers once it is no tap (> 18 frames)" || ko "long press: $a / $b"
s=$(run web-hold 200 "${START}25 touch 90%% 50%%\n")
if [ "$(field "$s" mode)" = 0 ] && [ "$(field "$s" anchor)" = 0 ] && [ "$(field "$s" score)" = 0 ]; then
    ok "a finger held on the web (down too early to let go) neither lets go nor advances"
else ko "web hold: $s"; fi

# steering in flight
n=$(run fly-neutral 78 "${START}${LET}")
r=$(run fly-right 78 "${START}${LET}66 T2 touch 90%% 50%%\n")
l=$(run fly-left 78 "${START}${LET}66 T2 touch 10%% 50%%\n")
rp=$(run fly-right-pad 78 "${PAD}63 tap A\n66 RIGHT\n")
lp=$(run fly-left-pad 78 "${PAD}63 tap A\n66 LEFT\n")
if [ "$(field "$n" st)" = 1 ] && [ "$(field "$r" st)" = 1 ] && [ "$(field "$l" st)" = 1 ] &&
   [ "$(field "$r" x)" -gt "$(field "$n" x)" ] && [ "$(field "$l" x)" -lt "$(field "$n" x)" ]; then
    ok "in flight, holding the right half steers right, the left half left (x $(field "$l" x) < $(field "$n" x) < $(field "$r" x))"
else ko "steering: left $l / neutral $n / right $r"; fi
same "$r" "$rp" && same "$l" "$lp" && ok "  the very runs of Right / Left held" || ko "steer vs pad: $r / $rp; $l / $lp"
b=$(run fly-both 78 "${START}${LET}66 T2 touch 90%% 50%%\n66 T3 touch 10%% 50%%\n")
same "$b" "$n" && ok "both halves held cancel out (as both directions)" || ko "both halves: $b / $n"
a=$(run fly-catch 140 "${START}${LET}66 T2 touch 90%% 50%%\n")
b=$(run fly-catch-pad 140 "${PAD}63 tap A\n66 RIGHT\n")
same "$a" "$b" && ok "held through the catch: the same run as Right held (no let-go, no advance on the web)" ||
    ko "held through the catch: $a / $b"

# pause: the touches do nothing (and are not kept for later)
s=$(run paused 62 "${START}40 tap SELECT\n45 tap-at 50%% 50%%\n50 T2 touch 90%% 50%%\n")
if [ "$(field "$s" paused)" = 1 ] && [ "$(field "$s" mode)" = 0 ]; then ok "paused: a tap does not let go"
else ko "paused: $s"; fi
a=$(run unpaused 90 "${START}40 tap SELECT\n45 tap-at 50%% 50%%\n60 tap SELECT\n")
b=$(run unpaused-ref 90 "${START}40 tap SELECT\n60 tap SELECT\n")
if [ "$(field "$a" paused)" = 0 ] && [ "$(field "$a" mode)" = 0 ] && same "$a" "$b"; then
    ok "a tap during the pause is not kept for after it"
else ko "unpaused: $a / $b"; fi

# the game over: a tap goes back to the title
s=$(run over 900 "$START")
over=$(sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p' "$O/over.log")
if [ "$(field "$s" st)" = 3 ] && [ -n "$over" ]; then ok "an idle run ends (the panel at frame $over)"
else ko "no game over: $s"; over=600; fi
s=$(run back $((over + 60)) "${START}$((over + 40)) tap-at 50%% 50%%\n")
[ "$(field "$s" st)" = 0 ] && ok "a tap on the game-over panel goes back to the title" || ko "back: $s"
s=$(run early $((over + 60)) "${START}$((over + 10)) tap-at 50%% 50%%\n")
[ "$(field "$s" st)" = 3 ] && ok "not a tap before the panel's lock" || ko "early tap: $s"
s=$(run held $((over + 60)) "${START}$((over - 30)) touch 50%% 50%%\n$((over + 45)) up\n")
[ "$(field "$s" st)" = 3 ] && ok "not a finger left down from the run" || ko "held finger: $s"
s=$(run again $((over + 80)) "${START}$((over + 40)) tap-at 50%% 50%%\n$((over + 60)) tap-at 50%% 50%%\n")
[ "$(field "$s" st)" = 1 ] && ok "and another tap starts another run" || ko "again: $s"

# every width: the same game (the halves are the screen's)
ref=$(run w320 110 "${START}${LET}66 T2 touch 90%% 50%%\n74 T2 move 10%% 50%%\n" 320)
for w in $WS 560; do
    s=$(run "w$w" 110 "${START}${LET}66 T2 touch 90%% 50%%\n74 T2 move 10%% 50%%\n" "$w")
    same "$s" "$ref" && ok "width $w: the same run as at 320" || ko "width $w: $s / $ref"
done
exit $fail

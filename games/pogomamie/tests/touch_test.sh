#!/bin/sh
# Pogo Mamie touch tests (DESIGN.md "Controls": rs_game.touch_native): scripted fingers through the headless runner.
# A tap starts the title and "get ready" and leaves the game-over panel; a half held steers (the same run as the pad's
# Left / Right held, from the finger's 3rd frame), both halves cancel out; a swipe up arms the big bounce for the next
# landing (the same run as A held until then, a glint on the pogo meanwhile) and never steers; while paused the
# fingers do nothing, not even later. The halves are the screen's, at any width (RS_WIDTH).
#   touch_test.sh <headless binary> <tmp dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pogomamie/LICENSE.
H=$1
T=${2:-/tmp}/pogomamie-touch
mkdir -p "$T"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() {   # check <name> <expected regexp> <output>
    if echo "$3" | grep -q "$2"; then ok "$1"; else ko "$1: expected '$2' in: $(echo "$3" | grep 'state:')"; fi
}
run() { $H --opt music=0 --opt dump=1 "$@" 2>&1; }
num() { echo "$2" | sed -n "s/.*state: .* $1=\([0-9a-f]*\).*/\1/p"; }
script() { f="$T/$1.input"; shift; printf "$@" > "$f"; echo "$f"; }
bigs() { echo "$1" | sed -n 's/.*ev f=\([0-9]*\) p=0 big.*/\1/p' | tr '\n' ' ' | sed 's/ $//'; }

# the title and "get ready": a tap starts (the finger's release, as A's press); a finger only held does not
out=$(run --frames 60 --input "$(script title '30 tap-at 50%% 50%%\n')")
check "a tap on the title starts the run" "state: st=2 " "$out"
out=$(run --frames 60 --input "$(script title_l '30 tap-at 10%% 80%%\n')")
check "... anywhere (a tap on the left half too)" "state: st=2 " "$out"
out=$(run --frames 90 --input "$(script title_hold '30 touch 50%% 50%%\n')")
check "a finger held on the title does not start" "state: st=0 .*dist=0 " "$out"
out=$(run --frames 60 --opt ready=1 --input "$(script ready '30 tap-at 50%% 50%%\n')")
check "a tap on \"get ready\" starts" "state: st=2 " "$out"

# steering: a half held is the pad's Left / Right held from the finger's 3rd frame (a flick up has risen by then and
# never steers); both halves cancel out (the same run, the same state hash)
same() {    # same <name> <touch lines> <pad lines> [frames]: the run with the fingers = the run with the pad
    n=${4:-1500}
    a=$(run --frames "$n" --opt seed=5 --input "$(script "$1-t" "30 tap-at 50%% 50%%\n$2")")
    b=$(run --frames "$n" --opt seed=5 --input "$(script "$1-p" "30 tap-at 50%% 50%%\n$3")")
    ha=$(num hash "$a"); hb=$(num hash "$b")
    if [ -n "$ha" ] && [ "$ha" = "$hb" ]; then ok "$1 (x $(num x "$a"))"
    else ko "$1: touch '$(echo "$a" | grep state:)' vs pad '$(echo "$b" | grep state:)'"; fi
}
same "the right half held steers right, as Right held" '100 touch 75%% 50%%\n' '102 RIGHT\n'
same "the left half held steers left, as Left held" '100 touch 25%% 50%%\n400 up\n' '102 LEFT\n400 -\n' 600
same "both halves held: they cancel out, as no input" '100 T1 touch 20%% 50%%\n100 T2 touch 80%% 60%%\n' ''
same "one half, then both, then one: Right, nothing, Right" \
     '100 T1 touch 90%% 40%%\n300 T2 touch 5%% 40%%\n600 T2 up\n' '102 RIGHT\n302 -\n600 RIGHT\n'
xs() { num x "$(run --frames 200 --opt seed=5 --input "$(script "$1" "30 tap-at 50%% 50%%\n$2")")"; }
xn=$(xs x-none ''); xr=$(xs x-right '100 touch 75%% 50%%\n'); xl=$(xs x-left '100 touch 25%% 50%%\n')
if [ "${xr:-0}" -gt "${xn:-0}" ] && [ "${xn:-0}" -gt "${xl:-0}" ]; then
    ok "... where Mamie goes 70 frames later: x $xl (left half), $xn (no finger), $xr (right half)"
else ko "steering: x $xl (left half), $xn (no finger), $xr (right half)"; fi

# the big bounce: a swipe up arms it, the next landing makes it (once), the same run as A held until that landing
ev() {      # ev <name> <touch/pad lines> [runner options]: 400 frames from a tap-started run, the events logged
    n=$1; s=$2; shift 2
    run --frames 400 --opt seed=5 --opt evlog=1 --input "$(script "$n" "30 tap-at 50%% 50%%\n$s")" "$@"
}
none=$(ev none '')
sw=$(ev swipe '200 swipe 50%% 70%% 0 -40\n' --shot 215:"$T/armed.png" --shot 260:"$T/landed.png")
l=$(bigs "$sw")
if [ -z "$(bigs "$none")" ] && [ -n "$l" ] && [ "$l" = "${l% *}" ] && [ "$l" -gt 200 ] && [ "$l" -lt 300 ]; then
    ok "a swipe up (frame 200) arms the big bounce: the next landing (frame $l) makes it, once (none without a swipe)"
else ko "swipe up: big bounces at '$l' (without a swipe: '$(bigs "$none")')"; fi
if [ -n "$l" ]; then
    pad=$(ev pad-a "202 A\n$((l + 1)) -\n" --shot 215:"$T/pad-armed.png" --shot 260:"$T/pad-landed.png")
    if [ "$(num hash "$sw")" = "$(num hash "$pad")" ]; then ok "... the same run as A held from the swipe to that landing"
    else ko "swipe vs A held: $(num hash "$sw") vs $(num hash "$pad")"; fi
    if ! cmp -s "$T/armed.png" "$T/pad-armed.png" && cmp -s "$T/landed.png" "$T/pad-landed.png"; then
        ok "... a glint on her pogo while it is armed, gone after the landing"
    else ko "the armed glint: not shown while armed, or still there after the landing"; fi
fi
a=$(ev swipe-l '200 swipe 25%% 70%% 0 -40\n'); b=$(ev swipe-r '200 swipe 75%% 70%% 0 -40\n')
if [ "$(num hash "$a")" = "$(num hash "$sw")" ] && [ "$(num hash "$b")" = "$(num hash "$sw")" ]; then
    ok "a swipe does not steer: the same run from the left half, the middle or the right half"
else ko "a swipe steers: $(num hash "$a") (left) $(num hash "$sw") (middle) $(num hash "$b") (right)"; fi
fl=$(ev flick '100 touch 75%% 60%%\n300 move 75%% 30%%\n')
l2=$(bigs "$fl")
if [ -n "$l2" ] && [ "$l2" = "${l2% *}" ]; then
    pad=$(ev flick-pad "102 RIGHT\n300 A\n$((l2 + 1)) -\n")
    if [ "$(num hash "$fl")" = "$(num hash "$pad")" ]; then
        ok "a finger held on the right half, then flicked up: it steered, then armed the bounce (frame $l2), then no longer steers"
    else ko "the flick: $(num hash "$fl") vs pad $(num hash "$pad")"; fi
else ko "the flick from a held finger: big bounces at '$l2'"; fi

# the game-over panel: a tap goes back to the title (after the panel's lock, like A)
out=$(run --frames 5000 --opt seed=3 --input "$(script drift '30 tap-at 50%% 50%%\n')")
f=$(echo "$out" | sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p')
if [ -n "$f" ]; then
    out=$(run --frames $((f + 60)) --opt seed=3 --input "$(script over "30 tap-at 50%% 50%%\n$((f + 40)) tap-at 30%% 70%%\n")")
    check "a tap on the game-over panel goes back to the title" "state: st=0 .*runs=1 " "$out"
    out=$(run --frames $((f + 20)) --opt seed=3 --input "$(script over_lock "30 tap-at 50%% 50%%\n$((f + 5)) tap-at 30%% 70%%\n")")
    check "... not during the panel's lock" "state: st=4 .*runs=1 " "$out"
    out=$(run --frames $((f + 120)) --opt seed=3 --input "$(script over_go "30 tap-at 50%% 50%%\n$((f + 40)) tap-at 30%% 70%%\n$((f + 80)) tap-at 60%% 30%%\n")")
    check "... and the next tap starts again" "state: st=2 .*runs=1 " "$out"
else
    ko "no game over in 5000 frames without steering"
fi

# pause (the app's pause button presses Start): the fingers do nothing while paused, nor after (nothing buffered: a
# swipe up while paused arms nothing)
p0=$(run --frames 700 --opt seed=4 --input "$(script pause0 '30 tap-at 50%% 50%%\n400 tap START\n500 tap START\n')")
p1=$(run --frames 700 --opt seed=4 --input "$(script pause1 '30 tap-at 50%% 50%%\n400 tap START\n420 T1 touch 10%% 50%%\n420 T2 touch 90%% 50%%\n440 T1 up\n440 T2 up\n450 tap-at 50%% 50%%\n460 swipe 30%% 70%% 0 -40\n500 tap START\n')")
if [ -n "$(num hash "$p0")" ] && [ "$(num hash "$p0")" = "$(num hash "$p1")" ] && echo "$p1" | grep -q "paused=0 "; then
    ok "paused: the fingers act on nothing, then or later"
else ko "pause: '$(echo "$p0" | grep state:)' vs '$(echo "$p1" | grep state:)'"; fi
exit $fail

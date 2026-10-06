#!/bin/sh
# Pogo Mamie touch tests (DESIGN.md "Controls": rs_game.touch_native): scripted fingers through the headless runner.
# A tap starts the title and "get ready" and leaves the game-over panel; a half held steers (the same run as the pad's
# Left / Right held), both halves held is the big bounce (the same run as A held); while paused the fingers do
# nothing, not even later. The halves are the screen's, at any width (RS_WIDTH).
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

# the title and "get ready": a tap starts (the finger's release, as A's press); a finger only held does not
out=$(run --frames 60 --input "$(script title '30 tap-at 50%% 50%%\n')")
check "a tap on the title starts the run" "state: st=2 " "$out"
out=$(run --frames 60 --input "$(script title_l '30 tap-at 10%% 80%%\n')")
check "... anywhere (a tap on the left half too)" "state: st=2 " "$out"
out=$(run --frames 90 --input "$(script title_hold '30 touch 50%% 50%%\n')")
check "a finger held on the title does not start" "state: st=0 .*dist=0 " "$out"
out=$(run --frames 60 --opt ready=1 --input "$(script ready '30 tap-at 50%% 50%%\n')")
check "a tap on \"get ready\" starts" "state: st=2 " "$out"

# steering: a half held is the pad's Left / Right held, both halves its A held (the same run, the same state hash)
same() {    # same <name> <touch lines> <pad lines> [frames]: the run with the fingers = the run with the pad
    n=${4:-1500}
    a=$(run --frames "$n" --opt seed=5 --input "$(script "$1-t" "30 tap-at 50%% 50%%\n$2")")
    b=$(run --frames "$n" --opt seed=5 --input "$(script "$1-p" "30 tap-at 50%% 50%%\n$3")")
    ha=$(num hash "$a"); hb=$(num hash "$b")
    if [ -n "$ha" ] && [ "$ha" = "$hb" ]; then ok "$1 (x $(num x "$a"), $(num big "$a") big bounces)"
    else ko "$1: touch '$(echo "$a" | grep state:)' vs pad '$(echo "$b" | grep state:)'"; fi
}
same "the right half held steers right, as Right held" '100 touch 75%% 50%%\n' '100 RIGHT\n'
same "the left half held steers left, as Left held" '100 touch 25%% 50%%\n400 up\n' '100 LEFT\n400 -\n' 600
same "both halves held: the big bounce, as A held" '100 T1 touch 20%% 50%%\n100 T2 touch 80%% 60%%\n' '100 A\n'
same "steering, then both halves, then one: Right, A, Right" \
     '100 T1 touch 90%% 40%%\n300 T2 touch 5%% 40%%\n600 T2 up\n' '100 RIGHT\n300 A\n600 RIGHT\n'
xs() { num x "$(run --frames 200 --opt seed=5 --input "$(script "$1" "30 tap-at 50%% 50%%\n$2")")"; }
xn=$(xs x-none ''); xr=$(xs x-right '100 touch 75%% 50%%\n'); xl=$(xs x-left '100 touch 25%% 50%%\n')
if [ "${xr:-0}" -gt "${xn:-0}" ] && [ "${xn:-0}" -gt "${xl:-0}" ]; then
    ok "... where Mamie goes 70 frames later: x $xl (left half), $xn (no finger), $xr (right half)"
else ko "steering: x $xl (left half), $xn (no finger), $xr (right half)"; fi
none=$(run --frames 1500 --opt seed=5 --input "$(script none '30 tap-at 50%% 50%%\n')")
both=$(run --frames 1500 --opt seed=5 --input "$(script both '30 tap-at 50%% 50%%\n100 T1 touch 20%% 50%%\n100 T2 touch 80%% 60%%\n')")
b=$(num big "$both")
if [ "${b:-0}" -ge 1 ] && [ "$(num big "$none")" = 0 ]; then ok "both halves held: $b big bounces (none without a finger)"
else ko "both halves: big=$b, without a finger big=$(num big "$none")"; fi

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

# pause (the app's pause button presses Start): the fingers do nothing while paused, nor after (nothing buffered)
p0=$(run --frames 700 --opt seed=4 --input "$(script pause0 '30 tap-at 50%% 50%%\n400 tap START\n500 tap START\n')")
p1=$(run --frames 700 --opt seed=4 --input "$(script pause1 '30 tap-at 50%% 50%%\n400 tap START\n420 T1 touch 10%% 50%%\n420 T2 touch 90%% 50%%\n440 T1 up\n440 T2 up\n450 tap-at 50%% 50%%\n460 swipe 30%% 50%% 40 0\n500 tap START\n')")
if [ -n "$(num hash "$p0")" ] && [ "$(num hash "$p0")" = "$(num hash "$p1")" ] && echo "$p1" | grep -q "paused=0 "; then
    ok "paused: the fingers act on nothing, then or later"
else ko "pause: '$(echo "$p0" | grep state:)' vs '$(echo "$p1" | grep state:)'"; fi
exit $fail

#!/bin/sh
# Beaver Rush: the UI screenshot test. It shoots the screens (title, early play, a milestone, a golden log, night,
# pause, game over, versus and its result) at exact frames and checks each picture against the run's state
# (tests/ui_check.py: the timer bar's width, the score digits, the beaver's side, the logo, the panel, the
# medal, the split screen...), then that the screens differ and that a screen renders the same twice.
# Also makes games/beaverrush/docs/screenshots (tools/screenshots.sh: scale 2).
#   ui_test.sh <headless binary> <out dir> [scale]
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/beaverrush/LICENSE.
H=$1
O=${2:-/tmp/beaverrush-ui}
S=${3:-1}
D=$(dirname "$0")
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
# shot NAME FRAME OPTS...: the picture after frame FRAME and the state at that frame (NAME.log)
shot() {
    n=$1; f=$2; shift 2
    $H --scale "$S" --opt music=0 --opt dump=1 --frames $((f + 1)) --shot "$f:$O/$n.png" "$@" > "$O/$n.log" 2>&1
}
first() {   # first FRAME-PATTERN OPTS...: the frame of the first log line matching the pattern
    pat=$1; shift
    $H --opt music=0 --frames 20000 "$@" 2>&1 | sed -n "s/.*$pat at frame \([0-9]*\).*/\1/p" | head -1
}

shot title 120
shot play 400 --opt bot=1 --opt seed=7 --opt ready=1
m=$(first "milestone 1" --opt bot=1 --opt seed=7 --opt ready=1)
[ -n "$m" ] && shot milestone $((m + 14)) --opt bot=1 --opt seed=7 --opt ready=1 || ko "no milestone in the bot run"
g=$(first "golden log" --opt bot=1 --opt seed=7 --opt ready=1)
[ -n "$g" ] && shot golden $((g + 4)) --opt bot=1 --opt seed=7 --opt ready=1 || ko "no golden log in the bot run"
shot night 240 --opt bot=1 --opt seed=7 --opt ready=1 --opt skip=150
shot winter 240 --opt bot=1 --opt seed=3 --opt ready=1 --opt skip=410
printf "380 tap START\n" > "$O/pause.input"
shot pause 400 --opt bot=1 --opt seed=7 --opt ready=1 --input "$O/pause.input"
e=$(first "run 1 over" --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=60)
[ -n "$e" ] && shot gameover $((e + 50)) --opt bot=1 --opt seed=5 --opt ready=1 --opt botstop=60 || ko "the game-over run did not end"
shot versus 600 --opt bot=2 --opt players=2 --opt seed=9
v=$(first "run 1 over" --opt bot=2 --opt players=2 --opt seed=9)
[ -n "$v" ] && shot versus-over $((v + 50)) --opt bot=2 --opt players=2 --opt seed=9 || ko "the versus run did not end"

python3 "$D/ui_check.py" --dir "$O" --scale "$S" || fail=1
n=$(md5sum "$O"/*.png 2>/dev/null | awk '{print $1}' | sort -u | wc -l)
if [ "$n" -ge 10 ]; then ok "the $n screens all differ"; else ko "only $n different screens"; fi
cp "$O/title.png" "$O/title.1"
shot title 120
if cmp -s "$O/title.png" "$O/title.1"; then ok "the title renders the same twice"; else ko "the title differs between two runs"; fi
rm -f "$O/title.1" "$O"/*.log "$O/pause.input"
exit $fail

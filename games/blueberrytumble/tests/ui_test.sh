#!/bin/sh
# Blueberry Tumble: the screens as screenshots, checked: each one renders, they all differ, and the same run gives
# the same picture twice. Also makes games/blueberrytumble/docs/screenshots (make blueberrytumble-screenshots).
#   ui_test.sh <headless binary> <out dir> [scale]
# The moments (a pad, a dew drop, the glide, the snowberry, a splat, a gate) are found in the event log of a bot run
# (--opt evlog=1); the biomes with --opt god=1 (the berry bounces off everything, so the run gets there).
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/blueberrytumble/LICENSE.
H=$1
O=${2:-/tmp/blueberrytumble-ui}
S=${3:-1}
mkdir -p "$O"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
run() { $H --scale "$S" --opt music=0 --opt sound=0 "$@" > "$O/log.txt" 2>&1; }
first() {   # first <event> <seed> [<min metres>]: the frame of the first event of a bot run
    $H --frames 30000 --opt music=0 --opt sound=0 --opt bot=1 --opt ready=1 --opt seed=$2 --opt evlog=1 2>&1 |
        awk -v e=" $1" -v mm="${3:-0}" 'index($0, "ev p1") && index($0, e) { m = $7 + 0; if (m >= mm) { print $5; exit } }'
}
shot() {    # shot <name> <frame> <seed> [opts...]: a bot run up to frame, screenshot
    name=$1; f=$2; seed=$3; shift 3
    if [ -z "$f" ]; then ko "no moment for $name"; return; fi
    run --frames $((f + 1)) --opt bot=1 --opt ready=1 --opt seed=$seed "$@" --shot $f:"$O/$name.png"
}

run --frames 130 --shot 120:"$O/title.png"
run --frames 130 --opt ready=1 --opt players=2 --shot 120:"$O/get-ready-2-players.png"
# the biomes (god mode: the run goes on whatever happens), one shot inside each, and the night loop
shot biome-1-summit 900 3 --opt god=1
shot biome-2-forest 3700 3 --opt god=1
shot biome-3-meadows 6400 3 --opt god=1
shot biome-4-village 8800 3 --opt god=1
shot biome-5-night 11200 3 --opt god=1
# the moments
f=$(first pad 5); shot mushroom-pad $((f + 8)) 5
f=$(first orb 5); shot dew-drop $((f + 3)) 5
f=$(first glide 5); shot leaf-glider $((f + 40)) 5
f=$(first grow 5); shot snowberry $((f + 30)) 5
f=$(first die 11); shot splat $((f + 14)) 11
f=$(first die 11); shot gameover-medal $((f + 140)) 11
shot biome-gate 2720 3 --opt god=1
printf "600 tap SELECT\n" > "$O/pause.input"
run --frames 640 --opt bot=1 --opt seed=7 --opt ready=1 --input "$O/pause.input" --shot 630:"$O/pause.png"
run --frames 1400 --opt bot=2 --opt seed=9 --opt players=2 --opt ready=1 --shot 1390:"$O/race-2-players.png"
n=0
for s in title get-ready-2-players biome-1-summit biome-2-forest biome-3-meadows biome-4-village biome-5-night \
         mushroom-pad dew-drop leaf-glider snowberry splat gameover-medal biome-gate pause race-2-players; do
    if [ -s "$O/$s.png" ]; then ok "screen: $s"; n=$((n + 1)); else ko "screen $s missing"; fi
done
d=$(md5sum "$O"/*.png 2>/dev/null | awk '{print $1}' | sort -u | wc -l)
if [ "$d" -ge "$n" ]; then ok "the $n screens all differ"; else ko "only $d different screens of $n"; fi
cp "$O/title.png" "$O/title.1"
run --frames 130 --shot 120:"$O/title.png"
if cmp -s "$O/title.png" "$O/title.1"; then ok "the title renders the same twice"; else ko "the title differs between two runs"; fi
rm -f "$O/title.1" "$O/log.txt" "$O/pause.input"
exit $fail

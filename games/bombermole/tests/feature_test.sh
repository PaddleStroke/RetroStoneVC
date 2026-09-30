#!/bin/sh
# Bomber Mole feature tests (headless): windmill lanes in the 4 directions, the pause menu (resume, restart,
# quit with its question), levers (walking into one, a blast), the remote pickup, dev mode.
#   feature_test.sh <headless binary>
# All rights reserved, 8BCraft.
H=$1
D=$(dirname "$0")
T=$(mktemp -d)
fail=0
ok() { echo "  ok   $1"; }
bad() { echo "  FAIL $1"; fail=1; }
field() { echo "$2" | sed -n "s/.*[ :]$1=\([0-9,-]*\).*/\1/p" | head -1; }

# ---- wind: a windmill blows AWAY from itself; the mole is carried that way, the streaks go that way ----
for c in "right 4 2" "left 15 5" "down 3 9" "up 14 10"; do
    set -- $c
    dir=$1; x0=$2; y0=$3
    out=$($H --frames 150 --data $D/data --opt level=spring-10 --opt nointro=1 --opt dump=1 --opt god=1 \
          --opt spawn=0,$x0,$y0 2>&1)
    x=$(field x "$(echo "$out" | grep state:)"); y=$(field y "$(echo "$out" | grep state:)")
    p=$(field particles "$out"); l=$(field lanes "$out")
    i=0; for d in up right down left; do i=$((i + 1)); [ $d = $dir ] && k=$i; done
    pd=$(echo "$p" | cut -d, -f$k); ld=$(echo "$l" | cut -d, -f$k)
    case $dir in
        right) moved=$([ "$x" -gt "$x0" ] && [ "$y" -eq "$y0" ] && echo 1) ;;
        left) moved=$([ "$x" -lt "$x0" ] && [ "$y" -eq "$y0" ] && echo 1) ;;
        down) moved=$([ "$y" -gt "$y0" ] && [ "$x" -eq "$x0" ] && echo 1) ;;
        up) moved=$([ "$y" -lt "$y0" ] && [ "$x" -eq "$x0" ] && echo 1) ;;
    esac
    if [ "$moved" = 1 ] && [ "${pd:-0}" -gt 0 ] && [ "${ld:-0}" -gt 0 ]; then
        ok "windmill blowing $dir: the mole goes from $x0,$y0 to $x,$y (lane $ld cells, $pd streaks $dir)"
    else
        bad "windmill blowing $dir: mole $x0,$y0 -> $x,$y, lanes=$l particles=$p"
    fi
done

# ---- the pause menu ------------------------------------------------------------------------------------
printf "30 tap START\n40 tap B\n" > $T/resume.input
out=$($H --frames 90 --opt level=spring-1 --opt nointro=1 --opt dump=1 --input $T/resume.input 2>&1)
echo "$out" | grep -q "state: st=6 " && ok "pause: B (Esc) resumes" || bad "pause: B resumes: $(echo "$out" | grep state:)"
printf "30 tap START\n40 tap START\n" > $T/start.input
out=$($H --frames 90 --opt level=spring-1 --opt nointro=1 --opt dump=1 --input $T/start.input 2>&1)
echo "$out" | grep -q "state: st=6 " && ok "pause: Start on RESUME resumes" || bad "pause: Start resumes: $(echo "$out" | grep state:)"
printf "30 tap START\n40 tap RIGHT\n50 tap A\n" > $T/restart.input
out=$($H --frames 120 --opt level=spring-1 --opt nointro=1 --opt dump=1 --input $T/restart.input 2>&1)
echo "$out" | grep -q "restarts=1" && echo "$out" | grep -q "state: st=[56] " && ok "pause: RESTART restarts the level" \
    || bad "pause: RESTART: $(echo "$out" | grep "state:\|menu:")"
printf "30 tap START\n40 tap LEFT\n50 tap A\n60 tap LEFT\n70 tap A\n" > $T/quit.input
out=$($H --frames 120 --opt level=spring-1 --opt nointro=1 --opt dump=1 --input $T/quit.input 2>&1)
echo "$out" | grep -q "state: st=0 " && ok "pause: QUIT, YES goes back to the title" || bad "pause: QUIT: $(echo "$out" | grep state:)"
printf "30 tap START\n40 tap LEFT\n50 tap A\n60 tap A\n70 tap B\n" > $T/quitno.input
out=$($H --frames 120 --opt level=spring-1 --opt nointro=1 --opt dump=1 --input $T/quitno.input 2>&1)
echo "$out" | grep -q "state: st=6 " && ok "pause: QUIT, NO stays in the level" || bad "pause: QUIT NO: $(echo "$out" | grep state:)"

# ---- levers: walking into one, a blast ----------------------------------------------------------------
printf "10 tap RIGHT\n" > $T/bump.input
out=$($H --frames 40 --data $D/data --opt level=spring-11 --opt nointro=1 --opt dump=1 --opt spawn=0,5,6 \
      --input $T/bump.input 2>&1)
echo "$out" | grep -q "lever1=1" && ok "lever: walking into it switches it" || bad "lever bump: $(echo "$out" | grep menu:)"
printf "10 tap B\n14 LEFT\n40 -\n" > $T/blast.input
out=$($H --frames 260 --data $D/data --opt level=spring-11 --opt nointro=1 --opt dump=1 --opt god=1 \
      --opt spawn=0,4,6 --input $T/blast.input 2>&1)
echo "$out" | grep -q "lever1=1" && ok "lever: a blast switches it" || bad "lever blast: $(echo "$out" | grep menu:)"

# ---- the remote detonator: picked up (the banner names its button) ----------------------------------
printf "10 RIGHT\n40 -\n" > $T/remote.input
out=$($H --frames 60 --data $D/data --opt level=spring-11 --opt nointro=1 --opt dump=1 --input $T/remote.input 2>&1)
echo "$out" | grep -q "remote=1" && ok "remote detonator picked up" || bad "remote: $(echo "$out" | grep menu:)"

# ---- dev mode: --opt dev=1, F1 (god) from a script key --------------------------------------------------
printf "20 key F1\n" > $T/dev.input
out=$($H --frames 40 --opt level=spring-1 --opt nointro=1 --opt dump=1 --opt dev=1 --input $T/dev.input 2>&1)
echo "$out" | grep -q "dev=1 god=1" && ok "dev mode: F1 turns invincibility on" || bad "dev F1: $(echo "$out" | grep menu:)"
printf "20 key F3\n" > $T/skip.input
out=$($H --frames 120 --opt level=spring-1 --opt nointro=1 --opt dump=1 --opt dev=1 --input $T/skip.input 2>&1)
echo "$out" | grep -q "state: st=1[12] " && ok "dev mode: F3 skips the level" || bad "dev F3: $(echo "$out" | grep state:)"

# ---- summer ----------------------------------------------------------------------------------------------
S() { $H --data $D/data --opt nointro=1 --opt dump=1 "$@" 2>&1; }
out=$(S --frames 3 --opt level=summer-9 --opt spawn=0,8,5)
echo "$out" | grep -q "catsee=1" && ok "corn: the cat sees the mole on its row" || bad "corn visible: $(echo "$out" | grep summer:)"
out=$(S --frames 3 --opt level=summer-9 --opt spawn=0,9,5)
echo "$out" | grep -q "catsee=0" && ok "corn: in the corn the cat does not see it" || bad "corn hidden: $(echo "$out" | grep summer:)"
printf "5 tap B\n8 LEFT\n60 -\n" > $T/burn.input
out=$(S --frames 420 --opt level=summer-9 --opt spawn=0,8,2 --opt god=1 --input $T/burn.input)
echo "$out" | grep -q "burnt=9 " && ok "corn: a bomb sets the patch alight and the fire spreads to all 9 cells" \
    || bad "corn burn: $(echo "$out" | grep summer:)"
printf "5 tap B\n8 LEFT\n50 -\n" > $T/bees.input
out=$(S --frames 420 --opt level=summer-10 --opt spawn=0,8,3 --opt god=1 --input $T/bees.input)
k=$(field beekills "$out")
[ "${k:-0}" -eq 1 ] && [ "$(field stings "$out")" -eq 1 ] && \
    ok "bees: the swarm stings the nearest creature (a ferret), then flies off: 1 sting, the other ferret left alone" \
    || bad "bees nearest: $(echo "$out" | grep "summer:\|state:")"
printf "5 tap B\n8 LEFT\n60 -\n" > $T/shake.input
out=$(S --frames 420 --opt level=summer-14 --opt spawn=0,9,6 --input $T/shake.input)
echo "$out" | grep -q "shaken=1" && echo "$out" | grep -q "hearts=1" && ok "bees: a puddle shakes them off the mole" \
    || bad "bees shaken: $(echo "$out" | grep "summer:\|state:")"
base=$(field dirt "$(S --frames 5 --opt level=summer-11 | grep state:)")
out=$(S --frames 300 --opt level=summer-11)
echo "$out" | grep -q "warns=1 crushed=0" && ok "harvester: it warns first (lane flashing, rumble)" || bad "harvester warn: $(echo "$out" | grep summer:)"
out=$(S --frames 520 --opt level=summer-11)
dirt=$(field dirt "$(echo "$out" | grep state:)")
echo "$out" | grep -q "crushed=1" && [ "$dirt" -eq $((base - 2)) ] && ok "harvester: then it crushes the ferret and digs up the soil in its lane" \
    || bad "harvester sweep: $(echo "$out" | grep "summer:\|state:") (dirt before $base)"
out=$(S --frames 600 --opt level=summer-12 --opt spawn=1,5,5 --opt god=1)
echo "$out" | grep -q "boss_depth=1" && [ "$(field holes "$out")" -ge 1 ] && ok "badger: it digs its own hole to follow the mole below" \
    || bad "badger dig: $(echo "$out" | grep summer:)"
out=$(S --frames 480 --opt level=summer-12 --opt spawn=0,2,4 --opt god=1)
[ "$(field bstuns "$out")" -ge 1 ] && ok "badger: it charges along the row and knocks itself out on the rock" \
    || bad "badger charge: $(echo "$out" | grep summer:)"
printf "5 tap RIGHT\n" > $T/pipe.input
out=$(S --frames 40 --opt level=summer-13 --opt spawn=0,1,2 --input $T/pipe.input)
echo "$out" | grep -q "depth=0 x=15 y=10 " && ok "drain pipe: it takes the mole to its twin" || bad "pipe: $(echo "$out" | grep state:)"
printf "5 tap B\n8 LEFT\n60 -\n" > $T/gas.input
out=$(S --frames 300 --opt level=summer-13 --opt spawn=0,6,6 --opt god=1 --input $T/gas.input)
[ "$(field gas "$out")" -ge 1 ] && ok "gas pocket: the blast releases a cloud that stuns the ferret" || bad "gas: $(echo "$out" | grep summer:)"
# the crocodile: next to it, its eyes rise for 0.6 s (no bite yet), then it snaps; a blast stuns it
out=$(S --frames 25 --opt level=summer-16 --opt spawn=0,9,4)
echo "$out" | grep -q "hearts=1 " && echo "$out" | grep -q "bites=0" && ok "croc: next to it, it tells first (eyes up, no bite yet)" \
    || bad "croc tell: $(echo "$out" | grep "state:\|croc:")"
out=$(S --frames 90 --opt level=summer-16 --opt spawn=0,9,4 --opt god=1)
echo "$out" | grep -q "bites=1" && ok "croc: then it snaps (a hit)" || bad "croc snap: $(echo "$out" | grep croc:)"
printf "5 tap B\n8 UP\n40 -\n" > $T/croc.input
out=$(S --frames 260 --opt level=summer-16 --opt spawn=0,10,3 --opt god=1 --input $T/croc.input)
[ "$(field stuns "$out")" -ge 1 ] && ok "croc: a blast on the water stuns it" || bad "croc stun: $(echo "$out" | grep croc:)"
# the start box: the game waits (paused) until A
out=$($H --frames 200 --opt level=spring-3 --opt dump=1 2>&1)
echo "$out" | grep -q "state: st=5 " && ok "start box: the level waits behind it" || bad "start box wait: $(echo "$out" | grep state:)"
printf "60 tap A\n" > $T/box.input
out=$($H --frames 120 --opt level=spring-3 --opt dump=1 --input $T/box.input 2>&1)
echo "$out" | grep -q "state: st=6 " && ok "start box: A starts the level" || bad "start box A: $(echo "$out" | grep state:)"
rm -rf $T
exit $fail

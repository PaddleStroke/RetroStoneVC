#!/bin/sh
# Pogo Mamie smoke tests: scripted runs through the headless runner (the flow, pause, retry, save RAM, player 2),
# a bot run and determinism.
#   smoke_test.sh <headless binary> <tmp dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pogomamie/LICENSE.
H=$1
T=${2:-/tmp}/pogomamie-tests
mkdir -p "$T"
fail=0
ok() { echo "  ok   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() {   # check <name> <expected regexp> <output>
    if echo "$3" | grep -q "$2"; then ok "$1"; else ko "$1: expected '$2' in: $(echo "$3" | grep 'state:\|strict')"; fi
}
run() { $H --opt music=0 --opt dump=1 "$@" 2>&1; }
num() { echo "$2" | sed -n "s/.*state: .* $1=\([0-9]*\).*/\1/p"; }

out=$(run --frames 300 --opt strict=1)
check "title: nothing happens without input (Mamie bounces in place)" "state: st=0 .*dist=0 .*camx=0 " "$out"
if echo "$out" | grep -q "rs strict"; then ko "strict mode warnings: $(echo "$out" | grep 'rs strict')"; else ok "no strict-mode warning (VRAM, sprites, voices, samples...)"; fi
out=$(run --frames 4000 --opt strict=1 --opt bot=4 --opt players=4 --opt ready=1 --opt seed=5 --opt music=1)
if echo "$out" | grep -q "rs strict"; then ko "four-player music warnings: $(echo "$out" | grep 'rs strict' | head -3)"
else ok "four-player music and effects stay within hardware limits"; fi

printf "30 tap A\n" > "$T/title.input"
out=$(run --frames 60 --input "$T/title.input")
check "A on the title: starts immediately" "state: st=2 " "$out"
printf "30 tap A\n" > "$T/go.input"
out=$(run --frames 120 --input "$T/go.input")
check "... then A (or Start) starts the run" "state: st=2 " "$out"

# no input: the pogo drifts forward at cruise speed and sooner or later misses a roof
printf "30 tap A\n80 tap A\n" > "$T/drift.input"
out=$(run --frames 5000 --input "$T/drift.input" --opt seed=3)
check "no steering: a fall into the street, the game-over panel" "state: st=4 .*runs=1 " "$out"
d=$(num dist "$out")
if [ "${d:-0}" -ge 1 ]; then ok "... after $d m"; else ko "no distance: $d"; fi

# hold Right all the time and A: big bounces at full speed
printf "30 tap A\n80 tap A\n100 RIGHT+A\n" > "$T/rush.input"
out=$(run --frames 2000 --input "$T/rush.input" --opt seed=5)
b=$(num big "$out")
if [ "${b:-0}" -ge 1 ]; then ok "holding A on the landings: $b big bounces"; else ko "no big bounce: $(echo "$out" | grep state)"; fi

printf "30 tap A\n80 tap A\n100 RIGHT\n" > "$T/retry.input"
out=$(run --frames 3000 --input "$T/retry.input" --opt seed=4)
f=$(echo "$out" | sed -n 's/.*run 1 over at frame \([0-9]*\).*/\1/p')
if [ -n "$f" ]; then
    printf "30 tap A\n80 tap A\n100 RIGHT\n$((f + 10)) -\n$((f + 50)) tap A\n" > "$T/retry2.input"
    out=$(run --frames $((f + 60)) --input "$T/retry2.input" --opt seed=4)
    check "game over: one button back to the title" "state: st=0 .*runs=1 " "$out"
else
    ko "holding Right: no game over in 3000 frames"
fi

printf "30 tap A\n80 tap A\n400 tap START\n" > "$T/pause.input"
a=$(run --frames 450 --opt seed=4 --input "$T/pause.input" | sed -n 's/.* x=\([0-9]*\) .*paused=\([0-9]\).*/\1 \2/p')
b=$(run --frames 650 --opt seed=4 --input "$T/pause.input" | sed -n 's/.* x=\([0-9]*\) .*paused=\([0-9]\).*/\1 \2/p')
if [ "$a" = "$b" ] && [ "${a#* }" = "1" ]; then ok "Start pauses (the world is frozen: x ${a% *})"; else ko "pause: '$a' vs '$b'"; fi
printf "30 tap A\n80 tap A\n400 tap START\n460 tap START\n" > "$T/pause2.input"
out=$(run --frames 600 --opt seed=4 --input "$T/pause2.input")
check "Start again resumes" "paused=0 " "$out"

rm -f "$T/save.srm"
run --frames 30000 --opt bot=1 --opt seed=8 --opt botstop=120 --sram "$T/save.srm" > "$T/save.txt"
d=$(sed -n 's/.*run 1 over at frame [0-9]*: \([0-9]*\) m.*/\1/p' "$T/save.txt")
out=$(run --frames 10 --sram "$T/save.srm")
if [ -n "$d" ] && echo "$out" | grep -q "best=$d "; then ok "save RAM: the best distance ($d m) persists"; else ko "save RAM: '$d' vs $(echo "$out" | grep state)"; fi

printf "20 P2 tap A\n" > "$T/join.input"
out=$(run --frames 60 --input "$T/join.input")
check "Papi joins on the title" "players=2 " "$out"
out=$(run --frames 2500 --opt bot=2 --opt seed=6 --opt players=2 --opt ready=1)
check "race: both bounce along" "players=2 dist=[1-9][0-9]* dist2=[1-9][0-9]* " "$out"

# the bot plays from the screen only: aggregate progress plus one long run under fatal hazards (tests/bot_test.sh)
sh "$(dirname "$0")/bot_test.sh" "$H" "$T" || fail=1

# determinism: the same inputs, the same run (state hash and picture), twice
for k in 1 2; do
    run --frames 1500 --input "$T/rush.input" --opt seed=5 --png "$T/det$k.png" | grep "state:" > "$T/det$k.txt"
    run --frames 3000 --opt bot=1 --opt ready=1 --png "$T/bot$k.png" | grep "state:" > "$T/bot$k.txt"
done
if cmp -s "$T/det1.txt" "$T/det2.txt" && cmp -s "$T/det1.png" "$T/det2.png"; then ok "determinism: a scripted run twice, same state and picture"
else ko "determinism (script)"; fi
if cmp -s "$T/bot1.txt" "$T/bot2.txt" && cmp -s "$T/bot1.png" "$T/bot2.png"; then ok "determinism: a bot run twice, same state and picture"
else ko "determinism (bot)"; fi
exit $fail

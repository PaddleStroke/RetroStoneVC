#!/bin/sh
H=$1
T=${2:-/tmp}/biscuitbandit-tests
mkdir -p "$T"
fail=0
ok(){ echo "  ok   $1"; }
ko(){ echo "  FAIL $1"; fail=1; }
check(){ if echo "$3" | grep -q "$2"; then ok "$1"; else ko "$1: $3"; fi; }
run(){ $H --opt music=0 --opt dump=1 "$@" 2>&1; }
check "frozen title" 'state: st=0 .*score=0 .*runs=0 ' "$(run --frames 300 --opt strict=1)"
printf '30 tap A\n' > "$T/start.input"
check "A starts safely in the title world" 'state: st=1 .*scroll=[1-9]' "$(run --frames 40 --input "$T/start.input")"
check "one missed biscuit ends the run" 'state: st=3 .*runs=1 ' "$(run --frames 1800 --input "$T/start.input")"
printf '30 tap A\n2000 tap A\n' > "$T/retry.input"
check "game over goes to title" 'state: st=0 .*runs=1 ' "$(run --frames 2010 --input "$T/retry.input")"
printf '30 tap A\n90 tap SELECT\n' > "$T/pause.input"
a=$(run --frames 120 --input "$T/pause.input" | sed -n 's/.*scroll=\([0-9]*\) paused=\([0-9]\).*/\1 \2/p')
b=$(run --frames 300 --input "$T/pause.input" | sed -n 's/.*scroll=\([0-9]*\) paused=\([0-9]\).*/\1 \2/p')
if [ "$a" = "$b" ] && [ "${a#* }" = 1 ]; then ok 'pause freezes all motion'; else ko 'pause'; fi
printf '20 P2 tap A\n25 P3 tap A\n30 P4 tap A\n' > "$T/join.input"
check 'four pads join without starting' 'state: st=0 players=4 ' "$(run --frames 60 --input "$T/join.input")"
printf '20 P4 tap A\n40 P4 tap B\n' > "$T/leave.input"
check 'nonsequential pad joins and leaves' 'state: st=0 players=1 ' "$(run --frames 60 --input "$T/leave.input")"
rm -f "$T/save.srm"
out=$(run --frames 4000 --opt bot=1 --opt botstop=7 --opt seed=8 --sram "$T/save.srm")
saved=$(echo "$out" | sed -n 's/.*state: .*best=\([0-9]*\) .*/\1/p')
if [ "${saved:-0}" -gt 0 ]; then check 'exact saved catch score persists' "best=$saved runs=0" "$(run --frames 10 --sram "$T/save.srm")"; else ko 'no score saved'; fi
for n in 1 2 3 4; do
 out=$(run --frames 4000 --opt bot=$n --opt seed=6 --opt strict=1)
 check "$n players catch and score" 'score=[1-9][0-9]* ' "$out"
 if echo "$out" | grep -q 'rs strict'; then ko "$n players strict warning"; else ok "$n players no strict warnings"; fi
 for k in 1 2; do run --frames 2400 --opt bot=$n --opt seed=6 --png "$T/det$n-$k.png" | grep 'state:' > "$T/det$n-$k.txt"; done
 if cmp -s "$T/det$n-1.txt" "$T/det$n-2.txt" && cmp -s "$T/det$n-1.png" "$T/det$n-2.png"; then ok "$n players deterministic controls and picture"; else ko "$n players determinism"; fi
done
exit $fail

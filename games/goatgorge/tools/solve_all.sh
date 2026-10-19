#!/bin/sh
# Goat Gorge: records every room's solutions (tests/solver.c) into levels/solutions.txt, the chapters in parallel.
# Run it after a room changed (make goatgorge-solve), look at the times it prints, commit the file.
#   solve_all.sh <goatgorge_solver> <levels dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/goatgorge/LICENSE.
S=$1
L=${2:-games/goatgorge/levels}
T=$(mktemp -d)
fail=0
for c in 1 2 3 4 5; do
    [ -f "$L/ch$c.txt" ] || continue
    ( "$S" "$L/ch$c.txt" $c > "$T/ch$c.sol" 2> "$T/ch$c.log"; echo $? > "$T/ch$c.rc" ) &
done
wait
{
    echo "# Goat Gorge: each room's recorded solutions (tests/solver.c, make goatgorge-solve): <chapter> <room> exit|berry"
    echo "# <frames> <inputs, run-length coded>. Replayed by tests/test_rooms.c, played by the bot (--opt bot=1)."
    for c in 1 2 3 4 5; do [ -f "$T/ch$c.sol" ] && cat "$T/ch$c.sol"; done
} > "$L/solutions.txt"
for c in 1 2 3 4 5; do
    [ -f "$T/ch$c.log" ] || continue
    cat "$T/ch$c.log"
    [ "$(cat "$T/ch$c.rc")" = 0 ] || fail=1
done
rm -rf "$T"
exit $fail

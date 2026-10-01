#!/bin/sh
# Duck Parade: the UI screens on the real picture (tests/test_ui.c) and their screenshots.
#   ui_test.sh <duckparade_test_ui> <out dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/duckparade/LICENSE.
U=$1
O=${2:-/tmp/duckparade-ui}
mkdir -p "$O"
fail=0
step() {
    out=$($U --out "$O" "$@" 2>&1)
    echo "$out" | grep "^  "
    if echo "$out" | grep -q "^all passed"; then echo "  ok   $(echo "$out" | grep '^all passed')"
    else echo "  FAIL $(echo "$out" | tail -1)"; fail=1; fi
}
step --mode title --opt seed=4
step --mode run --opt bot=1 --opt ready=1 --opt seed=3 --opt botstop=90
step --mode coop --opt bot=2 --opt players=2 --opt ready=1 --opt seed=6 --opt botstop=40
step --mode coop --opt bot=4 --opt players=4 --opt ready=1 --opt seed=8 --opt botstop=40
n=$(md5sum "$O"/*.png 2>/dev/null | awk '{print $1}' | sort -u | wc -l)
if [ "$n" -ge 6 ]; then ok_n=1; echo "  ok   $n different screens saved in $O"; else echo "  FAIL only $n different screens"; fail=1; fi
exit $fail

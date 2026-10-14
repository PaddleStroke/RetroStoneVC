#!/bin/sh
# Runs an iOS app in the simulator and takes screenshots (macOS with Xcode), after apps/ios/build.sh <app> simulator:
#
#   apps/ios/sim-shots.sh board        build/ios/shots/board-iphone.png, board-ipad.png (+ each run's log)
#
# For each of an iPhone (one with the Dynamic Island when the simulator has one: the safe area) and an iPad: boots
# the newest iOS simulator of that kind, installs the app, launches it, waits WAIT seconds (default 20), takes a
# screenshot, checks the app still runs (else its crash report is copied), shuts the simulator down.
# Exits 1 when the app is not running after the wait on any device.
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../.." && pwd)
APP=${1:-}
case "$APP" in board|arcade) ;; *) echo "usage: $0 board|arcade" >&2; exit 2 ;; esac
cd "$REPO"
OUT=build/ios/$APP
SHOTS=build/ios/shots
WAIT=${WAIT:-20}
env_of() { sed -n "s/^$1=//p" "$OUT/app.env"; }
TARGET=$(env_of TARGET)
BUNDLE_ID=$(env_of BUNDLE_ID)
[ -d "$OUT/$TARGET.app" ] || { echo "$OUT/$TARGET.app is missing: apps/ios/build.sh $APP simulator" >&2; exit 1; }
mkdir -p "$SHOTS"

# the udid of the newest available simulator whose name starts with $1, preferring names with $2 in them
pick() {
    xcrun simctl list devices available -j | python3 -c '
import json, re, sys
kind, prefer = sys.argv[1], sys.argv[2]
best = None
for runtime, devices in json.load(sys.stdin)["devices"].items():
    m = re.search(r"iOS-(\d+)-(\d+)", runtime)
    if not m:
        continue
    version = (int(m.group(1)), int(m.group(2)))
    for d in devices:
        if not d["name"].startswith(kind):
            continue
        key = (version, prefer in d["name"], d["name"])
        if best is None or key > best[0]:
            best = (key, d["udid"])
print(best[1] if best else "")
' "$1" "$2"
}

FAILED=0
for KIND in iPhone iPad; do
    if [ "$KIND" = iPhone ]; then PREFER=Pro; else PREFER=Air; fi
    UDID=$(pick "$KIND" "$PREFER")
    [ -n "$UDID" ] || { echo "no $KIND simulator available" >&2; FAILED=1; continue; }
    NAME=$(echo "$APP-$KIND" | tr '[:upper:]' '[:lower:]')
    echo "== $KIND: $(xcrun simctl list devices | grep "$UDID" | sed 's/^ *//')"
    xcrun simctl boot "$UDID" 2>/dev/null || true
    xcrun simctl bootstatus "$UDID" -b > /dev/null
    xcrun simctl install "$UDID" "$OUT/$TARGET.app"
    xcrun simctl launch --stdout="$REPO/$SHOTS/$NAME-stdout.log" --stderr="$REPO/$SHOTS/$NAME-stderr.log" \
        "$UDID" "$BUNDLE_ID"
    sleep "$WAIT"
    xcrun simctl io "$UDID" screenshot "$SHOTS/$NAME.png"
    if xcrun simctl spawn "$UDID" launchctl list | grep -q "$BUNDLE_ID"; then
        echo "$KIND: running; $SHOTS/$NAME.png"
    else
        echo "$KIND: the app is NOT running after ${WAIT}s" >&2
        FAILED=1
        find "$HOME/Library/Logs/DiagnosticReports" -name "$TARGET*" -newer "$OUT/$TARGET.app" \
            -exec cp {} "$SHOTS/" \; 2>/dev/null || true
    fi
    xcrun simctl spawn "$UDID" log show --last 3m --style compact \
        --predicate "process == \"$TARGET\"" > "$SHOTS/$NAME-system.log" 2>&1 || true
    xcrun simctl shutdown "$UDID" || true
done
exit "$FAILED"

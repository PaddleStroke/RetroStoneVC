#!/bin/bash
# RetroStone Arcade Free: a smoke test of the APK in the Android emulator (WSL with /dev/kvm, no window).
#
#   apps/android/smoke-emulator.sh [APK]     (default: dist/android/<EXE_NAME>-debug.apk)
#
# Needs the emulator once: apps/android/setup-toolchain.sh --emulator (an x86_64 Android 11 image, ~2 GB in
# ~/android-app-sdk). Boots the AVD rsvc_smoke headless, installs the APK, starts it, then: the menu is up, a tap on
# the first card starts the first free game, Back returns to the menu, Home saves the game; screenshots in
# build/android/emulator-*.png; the app's log in build/android/emulator-logcat.txt. Stops the emulator at the end.
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../.." && pwd)
. "${ANDROID_APP_SDK:-$HOME/android-app-sdk}/env.sh"
cd "$REPO"
conf() { sed -n "s/^$1=//p" "$HERE/app.conf"; }
APK=${1:-dist/android/$(conf EXE_NAME)-debug.apk}
PKG=$(conf APP_ID)
FIRST=$(conf FREE_GAMES | awk '{print $1}')
OUT=build/android
ADB="adb -s emulator-5580"
fail() { echo "FAIL: $*"; $ADB emu kill > /dev/null 2>&1; exit 1; }

[ -x "$ANDROID_HOME/emulator/emulator" ] || { echo "no emulator: apps/android/setup-toolchain.sh --emulator"; exit 1; }
echo "== booting the emulator (headless)"
"$ANDROID_HOME/emulator/emulator" -avd rsvc_smoke -port 5580 -no-window -no-audio -no-boot-anim -no-snapshot \
    -gpu swiftshader_indirect -memory 2048 > "$OUT/emulator.log" 2>&1 &
timeout 120 $ADB wait-for-device || fail "the emulator did not start (see $OUT/emulator.log)"
for i in $(seq 1 120); do
    [ "$($ADB shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" = 1 ] && break
    sleep 2
done
[ "$($ADB shell getprop sys.boot_completed | tr -d '\r')" = 1 ] || fail "the emulator did not boot"
$ADB shell settings put global window_animation_scale 0
$ADB shell input keyevent 82 > /dev/null 2>&1        # unlock
sleep 15                                             # the home screen settles (it would come over the app)

echo "== install and start $PKG"
$ADB install -r "$APK" | tail -1
$ADB logcat -c
$ADB shell am start -W -n "$PKG/com.eightbcraft.retrostonearcade.ArcadeActivity" | grep -E "Status|TotalTime"
for i in 1 2 3 4 5; do                               # in front? (else start it again)
    sleep 4
    $ADB shell dumpsys activity activities | grep -q "mResumedActivity.*$PKG" && break
    $ADB shell am start -n "$PKG/com.eightbcraft.retrostonearcade.ArcadeActivity" > /dev/null
done
sleep 3
$ADB exec-out screencap -p > "$OUT/emulator-menu.png"
log() { $ADB logcat -d -s SDL/APP:I SDL:I AndroidRuntime:E DEBUG:F libc:F > "$OUT/emulator-logcat.txt"; }
log
grep -q "start: launcher" "$OUT/emulator-logcat.txt" || fail "the menu did not start (see $OUT/emulator-logcat.txt)"
grep -qE "FATAL|SIGSEGV|SIGABRT" "$OUT/emulator-logcat.txt" && fail "a crash (see $OUT/emulator-logcat.txt)"

# the first card's centre, in display pixels (rsb_sdl.c's scaling; launcher.c's grid)
read W H < <($ADB shell wm size | tail -1 | sed 's/.*: //; s/x/ /' | tr -d '\r')
[ "$W" -lt "$H" ] && { t=$W; W=$H; H=$t; }
read TX TY < <(python3 - "$W" "$H" <<'PY'
import sys
W, H = int(sys.argv[1]), int(sys.argv[2])
fw = min(560, max(320, 240 * W // H)) & ~1
k = H // 240
dh = k * 240 if k * 2400 >= H * 9 else H
dw = fw * dh // 240
cols = min(5, fw // 104)
gx = (fw - (cols * 104 - 8)) // 2
x, y = gx + 48, 32 + 40
print(int((W - dw) / 2 + x * dh / 240), int((H - dh) / 2 + y * dh / 240))
PY
)
echo "== tap the first card at $TX,$TY (display ${W}x$H)"
$ADB shell input tap "$TX" "$TY"
sleep 4
$ADB shell input tap $((W / 2)) $((H / 2))           # a tap in the game (A)
sleep 2
$ADB exec-out screencap -p > "$OUT/emulator-game.png"
log
grep -q "start: $FIRST" "$OUT/emulator-logcat.txt" || fail "the tap did not start $FIRST"
echo "== Back: to the menu"
$ADB shell input keyevent KEYCODE_BACK
sleep 3
log
[ "$(grep -c 'start: launcher' "$OUT/emulator-logcat.txt")" -ge 2 ] || fail "Back did not return to the menu"
echo "== the game again, then Home (saved), then back to the app (held)"
$ADB shell input tap "$TX" "$TY"
sleep 3
$ADB shell input keyevent KEYCODE_HOME
sleep 3
$ADB shell am start -n "$PKG/com.eightbcraft.retrostonearcade.ArcadeActivity" > /dev/null
sleep 4
$ADB exec-out screencap -p > "$OUT/emulator-resumed.png"
log
grep -q "resumed $FIRST" "$OUT/emulator-logcat.txt" || fail "$FIRST did not resume from its state"
$ADB shell run-as "$PKG" ls files 2>/dev/null | tr '\n' ' '; echo
grep -qE "FATAL|SIGSEGV|SIGABRT" "$OUT/emulator-logcat.txt" && fail "a crash (see $OUT/emulator-logcat.txt)"
$ADB emu kill > /dev/null 2>&1
echo "emulator smoke test passed: $OUT/emulator-{menu,game,resumed}.png"

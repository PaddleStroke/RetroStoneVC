#!/bin/bash
# RetroStone Arcade Free: builds the Android app in one command (WSL / Linux).
#
#   apps/android/build.sh             dist/android/<EXE_NAME>-debug.apk (signed with the debug key: for your phone)
#   apps/android/build.sh --release   also dist/android/<EXE_NAME>-release.aab and .apk, signed with YOUR release key
#                                     (~/android-app-sdk/release-signing.properties: apps/android/README.md, "Publishing")
#
# Needs the toolchain once: apps/android/setup-toolchain.sh. The app's name, package id, version, games and the
# paid app's link: apps/android/app.conf. Steps:
#   1. SDL2's source release (tools/fetch_sdl2_src.sh) and libSDL2.so for each ABI (ndk-build, build/android/sdl2)
#   2. libmain.so for each ABI: the launcher, the free games as modules, the SDK (apps/launcher/bundle.mk)
#   3. the icons (apps/android/tools/make_icon.py), then Gradle (the APK), then a check of the APK (ABIs, size)
# Everything goes to build/android/ and dist/android/. The root Makefile's builds are only used for the games'
# generated assets, under the lock /tmp/rsvc-build.lock.
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
set -euo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../.." && pwd)
SDK_ROOT=${ANDROID_APP_SDK:-$HOME/android-app-sdk}
ABIS="arm64-v8a armeabi-v7a x86_64"
SDL_V=2.32.10
RELEASE=0
[ "${1:-}" = "--release" ] && RELEASE=1

if [ ! -f "$SDK_ROOT/env.sh" ]; then
    echo "The Android toolchain is missing: run apps/android/setup-toolchain.sh first." >&2
    exit 1
fi
. "$SDK_ROOT/env.sh"
cd "$REPO"
mkdir -p build/android
conf() { sed -n "s/^$1=//p" "$HERE/app.conf"; }
NAME=$(conf EXE_NAME)
JOBS=$(nproc 2>/dev/null || echo 4)
say() { printf '\n== %s\n' "$*"; }

# ---- 1. SDL2 ----------------------------------------------------------------------------------------------------------
sh tools/fetch_sdl2_src.sh build/deps
SDL=build/deps/src/SDL2-$SDL_V
OUT_SDL=build/android/sdl2
STAMP="$OUT_SDL/built-$SDL_V-$(echo $ABIS | tr ' ' '_')"
if [ ! -f "$STAMP" ]; then
    say "SDL2 $SDL_V for $ABIS (ndk-build)"
    "$ANDROID_NDK_HOME/ndk-build" -j"$JOBS" NDK_PROJECT_PATH=null APP_BUILD_SCRIPT="$SDL/Android.mk" \
        APP_MODULES=SDL2 APP_ABI="$ABIS" APP_PLATFORM=android-21 APP_STL=c++_static \
        APP_SUPPORT_FLEXIBLE_PAGE_SIZES=true NDK_OUT="$OUT_SDL/obj" NDK_LIBS_OUT="$OUT_SDL/libs" > "$OUT_SDL.log" 2>&1 \
        || { tail -30 "$OUT_SDL.log"; exit 1; }
    touch "$STAMP"
fi

# ---- 2. libmain.so ------------------------------------------------------------------------------------------------------
for abi in $ABIS; do
    say "libmain.so ($abi)"
    make --no-print-directory -f apps/launcher/bundle.mk -j"$JOBS" PLATFORM=android-$abi NDK="$ANDROID_NDK_HOME" \
        > "build/android/libmain-$abi.log" 2>&1 || { grep -E "error|Error" "build/android/libmain-$abi.log" | head -30; exit 1; }
    mkdir -p build/android/jniLibs/$abi
    cp "$OUT_SDL/libs/$abi/libSDL2.so" build/bundle/android-$abi/libmain.so build/android/jniLibs/$abi/
done

# ---- 3. icons, Gradle, checks ----------------------------------------------------------------------------------------------
say "icons"
mkdir -p dist/android
python3 apps/android/tools/make_icon.py --conf "$HERE/app.conf" --res build/android/res --store dist/android/icon-512.png

TASKS=assembleDebug
[ "$RELEASE" = 1 ] && TASKS="assembleDebug assembleRelease bundleRelease"
say "Gradle: $TASKS"
( cd "$HERE" && ./gradlew --no-daemon --quiet --project-cache-dir "$REPO/build/android/gradle-cache" $TASKS ) \
    > build/android/gradle.log 2>&1 || { tail -40 build/android/gradle.log; exit 1; }
G=build/android/gradle/app/outputs
cp "$G/apk/debug/app-debug.apk" "dist/android/$NAME-debug.apk"
if [ "$RELEASE" = 1 ]; then
    cp "$G/bundle/release/app-release.aab" "dist/android/$NAME-release.aab"
    cp "$G/apk/release/"app-release*.apk "dist/android/$NAME-release.apk"
fi

say "the APK"
APK="dist/android/$NAME-debug.apk"
BT="$ANDROID_HOME/build-tools/$(ls "$ANDROID_HOME/build-tools" | sort -V | tail -1)"
"$BT/aapt2" dump badging "$APK" | grep -E "^(package|minSdkVersion|targetSdkVersion|application-label:|native-code)" || true
unzip -l "$APK" | awk '/lib\// {printf "  %-40s %9d bytes\n", $4, $1}'
"$BT/zipalign" -c -P 16 4 "$APK" > /dev/null && echo "  zip alignment: ok (16 KB pages)" || echo "  zip alignment: NOT 16 KB"
ls -l "$APK" | awk '{printf "  %s: %.1f MB\n", $9, $5 / 1048576}'
[ "$RELEASE" = 1 ] && ls -l dist/android/"$NAME"-release.*
echo
echo "Install on a phone: adb install -r $APK   (or copy the file to the phone: apps/android/README.md)"

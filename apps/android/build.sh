#!/bin/bash
# RetroStone Arcade: builds the Android apps in one command (WSL / Linux).
#
#   apps/android/build.sh [free|full] [--release]
#     free (the default)   RetroStone Arcade Free (apps/android/app.conf): dist/android/RetroStoneArcadeFree-debug.apk
#     full                 RetroStone Arcade, the paid app (apps/android/app-full.conf): dist/android/RetroStoneArcade-debug.apk
#     --release            also <EXE_NAME>-release.aab (Google Play) and <EXE_NAME>-release.apk (sideloading), signed with
#                          YOUR upload key: ~/.retrostone-keys/arcade.properties (apps/android/make-keystore.sh makes it;
#                          RSA_SIGNING=<file> to use another). Without that file --release stops: no unsigned release.
#
# Needs the toolchain once: apps/android/setup-toolchain.sh. The app's name, package id, version and games: its .conf.
# Steps:
#   1. SDL2's source release (tools/fetch_sdl2_src.sh) and libSDL2.so for each ABI (ndk-build, build/android/sdl2)
#   2. libmain.so for each ABI: the launcher, the games as modules, the SDK (apps/launcher/bundle.mk CONF=<the .conf>)
#   3. the icons (apps/android/tools/make_icon.py), then Gradle (the APK, the AAB), then checks (ABIs, alignment,
#      the signature of the release files)
# Everything goes to build/android/<free|full>/ (SDL2: build/android/sdl2) and dist/android/. The root Makefile's
# builds are only used for the games' generated assets, under the lock /tmp/rsvc-build.lock.
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
set -euo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../.." && pwd)
SDK_ROOT=${ANDROID_APP_SDK:-$HOME/android-app-sdk}
ABIS="arm64-v8a armeabi-v7a x86_64"
SDL_V=2.32.10
RELEASE=0
PROFILE=free
for a in "$@"; do
    case "$a" in
    free|full) PROFILE=$a ;;
    --release) RELEASE=1 ;;
    *) echo "usage: $0 [free|full] [--release]" >&2; exit 2 ;;
    esac
done
CONF="$HERE/app.conf"
[ "$PROFILE" = full ] && CONF="$HERE/app-full.conf"
SIGNING=${RSA_SIGNING:-$HOME/.retrostone-keys/arcade.properties}

if [ ! -f "$SDK_ROOT/env.sh" ]; then
    echo "The Android toolchain is missing: run apps/android/setup-toolchain.sh first." >&2
    exit 1
fi
if [ "$RELEASE" = 1 ] && [ ! -f "$SIGNING" ]; then
    echo "--release needs your upload key's properties file: $SIGNING" >&2
    echo "Create the key once with apps/android/make-keystore.sh (apps/android/README.md, \"Publishing\")." >&2
    exit 1
fi
. "$SDK_ROOT/env.sh"
cd "$REPO"
conf() { sed -n "s/^$1=//p" "$CONF"; }
NAME=$(conf EXE_NAME)
TAG=$([ "$PROFILE" = free ] && echo "" || echo "-$PROFILE")
OUT=build/android/$PROFILE
JOBS=$(nproc 2>/dev/null || echo 4)
say() { printf '\n== %s\n' "$*"; }
mkdir -p "$OUT" dist/android

# ---- 1. SDL2 ----------------------------------------------------------------------------------------------------------
sh tools/fetch_sdl2_src.sh build/deps
SDL=build/deps/src/SDL2-$SDL_V
OUT_SDL=build/android/sdl2
STAMP="$OUT_SDL/built-$SDL_V-$(echo $ABIS | tr ' ' '_')"
if [ ! -f "$STAMP" ]; then
    say "SDL2 $SDL_V for $ABIS (ndk-build)"
    mkdir -p "$OUT_SDL"
    "$ANDROID_NDK_HOME/ndk-build" -j"$JOBS" NDK_PROJECT_PATH=null APP_BUILD_SCRIPT="$SDL/Android.mk" \
        APP_MODULES=SDL2 APP_ABI="$ABIS" APP_PLATFORM=android-21 APP_STL=c++_static \
        APP_SUPPORT_FLEXIBLE_PAGE_SIZES=true NDK_OUT="$OUT_SDL/obj" NDK_LIBS_OUT="$OUT_SDL/libs" > "$OUT_SDL.log" 2>&1 \
        || { tail -30 "$OUT_SDL.log"; exit 1; }
    touch "$STAMP"
fi

# ---- 2. libmain.so ------------------------------------------------------------------------------------------------------
SKIP=                                       # the games' assets: copied once (the first ABI), shared by the others
for abi in $ABIS; do
    say "libmain.so ($PROFILE, $abi)"
    make --no-print-directory -f apps/launcher/bundle.mk -j"$JOBS" CONF="$CONF" PLATFORM=android-$abi $SKIP \
        NDK="$ANDROID_NDK_HOME" > "$OUT/libmain-$abi.log" 2>&1 \
        || { grep -E "error|Error" "$OUT/libmain-$abi.log" | head -30; exit 1; }
    SKIP=SKIP_ASSETS=1
    mkdir -p "$OUT/jniLibs/$abi"
    cp "$OUT_SDL/libs/$abi/libSDL2.so" "build/bundle/android-$abi$TAG/libmain.so" "$OUT/jniLibs/$abi/"
done

# ---- 3. icons, Gradle, checks ----------------------------------------------------------------------------------------------
say "icons"
python3 apps/android/tools/make_icon.py --conf "$CONF" --res "$OUT/res" --store "dist/android/$NAME-icon-512.png"

TASKS=assembleDebug
[ "$RELEASE" = 1 ] && TASKS="assembleDebug assembleRelease bundleRelease"
say "Gradle ($PROFILE): $TASKS"
( cd "$HERE" && RSA_CONF="$CONF" RSA_OUT="$REPO/$OUT" RSA_SIGNING="$SIGNING" \
    ./gradlew --no-daemon --quiet --project-cache-dir "$REPO/build/android/gradle-cache" $TASKS ) \
    > "$OUT/gradle.log" 2>&1 || { tail -40 "$OUT/gradle.log"; exit 1; }
G=$OUT/gradle/app/outputs
cp "$G/apk/debug/app-debug.apk" "dist/android/$NAME-debug.apk"
if [ "$RELEASE" = 1 ]; then
    cp "$G/bundle/release/app-release.aab" "dist/android/$NAME-release.aab"
    cp "$G/apk/release/app-release.apk" "dist/android/$NAME-release.apk"
fi

say "the APK"
APK="dist/android/$NAME-debug.apk"
BT="$ANDROID_HOME/build-tools/$(ls "$ANDROID_HOME/build-tools" | sort -V | tail -1)"
"$BT/aapt2" dump badging "$APK" | grep -E "^(package|minSdkVersion|targetSdkVersion|application-label:|native-code)" || true
unzip -l "$APK" | awk '/lib\// {printf "  %-40s %9d bytes\n", $4, $1}'
"$BT/zipalign" -c -P 16 4 "$APK" > /dev/null && echo "  zip alignment: ok (16 KB pages)" || echo "  zip alignment: NOT 16 KB"
ls -l "$APK" | awk '{printf "  %s: %.1f MB\n", $9, $5 / 1048576}'
if [ "$RELEASE" = 1 ]; then
    say "the release files (signed with the key of $SIGNING)"
    R="dist/android/$NAME-release"
    "$BT/apksigner" verify --print-certs "$R.apk" | grep -E "Signer #1 certificate (DN|SHA-256)" \
        || { echo "the release APK is not signed" >&2; exit 1; }
    "$BT/zipalign" -c -P 16 4 "$R.apk" > /dev/null && echo "  zip alignment: ok (16 KB pages)"
    jarsigner -verify "$R.aab" 2>&1 | grep -q "^jar verified" && echo "  $R.aab: signed (jarsigner -verify: jar verified)" \
        || { echo "the AAB is not signed" >&2; exit 1; }
    ls -l "$R.aab" "$R.apk" | awk '{printf "  %s: %.1f MB\n", $9, $5 / 1048576}'
fi
echo
echo "Install on a phone: adb install -r $APK   (or copy the file to the phone: apps/android/README.md)"

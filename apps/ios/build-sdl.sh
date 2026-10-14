#!/bin/sh
# SDL2 for the iOS apps (macOS with Xcode): the source release the Android build uses (tools/fetch_sdl2_src.sh),
# built by its own Xcode project (Xcode/SDL/SDL.xcodeproj, target "Static Library-iOS") for the devices (arm64) and
# for the simulator (arm64: Apple silicon Macs, as GitHub's macOS runners), into build/ios/sdl/:
#   include/                     its headers (SDL_config.h picks SDL_config_iphoneos.h)
#   iphoneos/libSDL2.a           the devices
#   iphonesimulator/libSDL2.a    the simulator
# Done once per SDL version (a stamp file); the CI caches build/ios/sdl.
# HIDAPI is left out (SDL_HIDAPI_DISABLED): it talks to Bluetooth game pads through CoreBluetooth, which the apps do
# not need, and the App Store would ask why the app uses Bluetooth.
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../.." && pwd)
cd "$REPO"
V=$(sed -n 's/^V=//p' tools/fetch_sdl2_src.sh)
SDL=build/deps/src/SDL2-$V
OUT=build/ios/sdl
STAMP=$OUT/built-$V
if [ -f "$STAMP" ] && [ -f "$OUT/iphoneos/libSDL2.a" ] && [ -f "$OUT/iphonesimulator/libSDL2.a" ]; then
    echo "SDL2 $V for iOS: $OUT (already built)"
    exit 0
fi
sh tools/fetch_sdl2_src.sh build/deps
rm -rf "$OUT"
mkdir -p "$OUT"
for SDK in iphoneos iphonesimulator; do
    ARCHS=arm64                                 # the devices; the simulator of Apple silicon Macs (the CI's)
    echo "== SDL2 $V: $SDK ($ARCHS)"
    xcodebuild -project "$SDL/Xcode/SDL/SDL.xcodeproj" -target "Static Library-iOS" -configuration Release \
        -sdk "$SDK" ARCHS="$ARCHS" ONLY_ACTIVE_ARCH=NO IPHONEOS_DEPLOYMENT_TARGET=15.0 \
        GCC_PREPROCESSOR_DEFINITIONS="GLES_SILENCE_DEPRECATION SDL_HIDAPI_DISABLED=1" \
        SYMROOT="$REPO/$OUT/sym" OBJROOT="$REPO/$OUT/obj" CODE_SIGNING_ALLOWED=NO \
        build > "$OUT/build-$SDK.log" 2>&1 || { tail -60 "$OUT/build-$SDK.log"; exit 1; }
    LIB=$(find "$OUT/sym" -path "*Release-$SDK*" -name libSDL2.a | head -1)
    [ -n "$LIB" ] || { echo "no libSDL2.a for $SDK under $OUT/sym" >&2; exit 1; }
    mkdir -p "$OUT/$SDK"
    cp "$LIB" "$OUT/$SDK/libSDL2.a"
    lipo -info "$OUT/$SDK/libSDL2.a"
done
cp -R "$SDL/include" "$OUT/include"
rm -rf "$OUT/obj" "$OUT/sym"
touch "$STAMP"
echo "SDL2 $V for iOS: $OUT"

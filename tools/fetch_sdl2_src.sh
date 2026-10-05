#!/bin/sh
# Download the official SDL2 source release (zlib licence) into <dir>/src/SDL2-<version> (default build/deps).
# Used by the Android app (apps/android/build.sh): SDL's android-project Java classes and its ndk-build files.
# The same SDL2 version as tools/fetch_sdl2_mingw.sh (the Windows builds).
set -e
V=2.32.10
DIR=${1:-build/deps}
mkdir -p "$DIR/src"
cd "$DIR/src"
[ -f SDL2-$V/Android.mk ] && [ -d SDL2-$V/android-project ] && exit 0
F=SDL2-$V.tar.gz
[ -f "$F" ] || curl -fsSLo "$F" "https://github.com/libsdl-org/SDL/releases/download/release-$V/$F"
tar xzf "$F"
echo "SDL2 $V (source) in $DIR/src/SDL2-$V"

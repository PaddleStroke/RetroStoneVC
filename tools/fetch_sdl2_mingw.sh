#!/bin/sh
# Download the official SDL2 development package for mingw-w64 (zlib licence)
# into <dir> (default build/deps). Used by "make windows".
set -e
V=2.32.10
DIR=${1:-build/deps}
mkdir -p "$DIR"
cd "$DIR"
[ -d SDL2-$V/x86_64-w64-mingw32 ] && exit 0
F=SDL2-devel-$V-mingw.tar.gz
[ -f "$F" ] || curl -sSLo "$F" "https://github.com/libsdl-org/SDL/releases/download/release-$V/$F"
tar xzf "$F"
echo "SDL2 $V (mingw) in $DIR/SDL2-$V"

#!/bin/sh
# A game's generated assets (build/gen/<id>/, made by the root Makefile) copied into the bundle's build folder.
#   game_assets.sh <id> <dest dir>
# The root Makefile's tree is shared with other work: its make runs under the lock /tmp/rsvc-build.lock, and the
# files are copied while the lock is held (only the files that changed, so the bundle rebuilds only what it must).
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
set -e
ID=$1
DEST=$2
mkdir -p "$DEST"
flock /tmp/rsvc-build.lock sh -c '
    make --no-print-directory GAME="$1" assets >&2
    cd build/gen/"$1"
    find . -type f ! -name "*.stamp" | while read -r f; do
        mkdir -p "$2/$(dirname "$f")"
        cmp -s "$f" "$2/$f" || cp "$f" "$2/$f"
    done
' sh "$ID" "$(cd "$DEST" && pwd)"

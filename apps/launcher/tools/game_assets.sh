#!/bin/sh
# The games' generated assets (build/gen/<id>/, made by the root Makefile) copied into the bundle's build folder.
#   game_assets.sh <dest dir> <id>...        each game's files go to <dest dir>/<id>/
# The root Makefile's tree is shared with other work: its makes run under the lock /tmp/rsvc-build.lock, taken ONCE
# for all the games (the lock is busy: one wait, not one per game), and the files are copied while it is held (only
# the files that changed, so the bundle rebuilds only what it must).
# STANDALONE=<dir> (the bundle's tests): each game's standalone headless build too (build/host/<id>_headless), copied
# to <dir>/<id>_standalone in the same wait, so that it and the bundle come from the same moment of the games' sources
# (other work changes them all the time).
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
set -e
DEST=$1
shift
mkdir -p "$DEST"
[ -n "${STANDALONE:-}" ] && mkdir -p "$STANDALONE" && STANDALONE=$(cd "$STANDALONE" && pwd)
export STANDALONE
flock /tmp/rsvc-build.lock sh -c '
    dest=$1
    shift
    for id in "$@"; do
        if [ -n "$STANDALONE" ]; then
            rm -f "$STANDALONE/${id}_standalone"
            make --no-print-directory GAME="$id" build/host/"$id"_headless > /dev/null &&
                cp build/host/"$id"_headless "$STANDALONE/${id}_standalone" ||
                echo "game_assets: $id: its standalone build failed" >&2
        fi
        if ! make --no-print-directory GAME="$id" assets >&2; then
            # a game being edited by someone else may not build its assets right now: its last generated files
            # (or the copy the bundle already has) are used, and the game still builds against them
            echo "game_assets: $id: make assets failed: using its last generated files" >&2
            [ -f build/gen/"$id"/game_text.c ] || [ -d "$dest/$id" ] || { echo "game_assets: $id: none" >&2; exit 1; }
        fi
        mkdir -p "$dest/$id"
        [ -d build/gen/"$id" ] || continue
        ( cd build/gen/"$id"
          find . -type f ! -name "*.stamp" | while read -r f; do
              mkdir -p "$dest/$id/$(dirname "$f")"
              cmp -s "$f" "$dest/$id/$f" || cp "$f" "$dest/$id/$f"
          done )
    done
' sh "$(cd "$DEST" && pwd)" "$@"

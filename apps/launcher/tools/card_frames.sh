#!/bin/sh
# The screens the launcher's cards are cut from: apps/launcher/cards/<id>.png, each game played headless at 1x to the
# frame of its line in apps/launcher/cards/cards.cfg (tools/gen_catalog.py cuts the card out of it). Run from the
# repository root after a change of a game's look, then commit the pictures:
#
#   sh apps/launcher/tools/card_frames.sh [BIN_DIR [OUT_DIR [ID ...]]]
#
# BIN_DIR: where the games' headless runners are (build/host: make GAME=<id> build/host/<id>_headless);
# OUT_DIR: apps/launcher/cards. IDs: only these games (default: every line of the table).
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
set -e
BIN=${1:-build/host}
OUT=${2:-apps/launcher/cards}
[ $# -gt 2 ] && shift 2 && ONLY=" $* " || ONLY=""
CFG=apps/launcher/cards/cards.cfg
mkdir -p "$OUT"
sed 's/#.*//' "$CFG" | while read -r id frame crop rest; do
    [ -n "$id" ] || continue
    [ -z "$ONLY" ] || case "$ONLY" in *" $id "*) ;; *) continue ;; esac
    opts=""
    for kv in $rest; do
        case "$kv" in sprite=*|panel=*) ;; *) opts="$opts --opt $kv" ;; esac
    done
    # the line's options first: the first --opt of a key wins
    "$BIN/${id}_headless" $opts --opt bot=1 --opt ready=1 --opt seed=7 --opt music=0 --opt sound=0 \
        --frames $((frame + 1)) --shot "$frame:$OUT/$id.png" > /dev/null 2>&1
    echo "$OUT/$id.png: frame $frame"
done

#!/usr/bin/env python3
"""The phone screenshots of the RetroStone store listing, made by the app itself in its picture mode
(apps/boardgames/README.md, "Picture mode"), in 3 languages like the Board's (store_shots_board.py):
apps/android/store/retrostone/screenshots/<en|fr|de>/NN-<what>.png.

    python3 apps/android/store/tools/store_shots_retrostone.py [--no-build]

It builds the app first in its own folder (make -f apps/boardgames/Makefile host B=build/retrostone-store
EDITION=retrostone: the Arcade tab too). Every picture is the app's screen at 1080 x 1920 (9:16, a phone held
upright; an Arcade game with a tall screen, as Beaver Rush, plays upright too; WIDE, 1920 x 1080, for one that turns
the phone to landscape), the app's own layout at that size, no caption. The menus and the Arcade tab show the app as
it is: free, nothing locked.
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import argparse
import os
import subprocess
import sys
import tempfile

from PIL import Image

import store_shots_board as SB

STORE, ROOT = SB.STORE, SB.ROOT
B = "build/retrostone-store"
UPRIGHT, WIDE = "1080x1920", "1920x1080"
LANGS = ("en", "fr", "de")

# what to show: (file name, size, the picture mode's arguments)
# (the owner's list, 26 October 2026: three menus, then the games)
SHOTS = [
    ("01-puzzles", UPRIGHT, ["--tab", "0"]),                                     # the menu: the Puzzles tab
    ("02-strategy", UPRIGHT, ["--tab", "1"]),                                    # the Strategy tab
    ("03-arcade", UPRIGHT, ["--tab", "6"]),                                      # the Arcade tab
    # a trick on the table, the 1898 trumps in the hand: the device plays every seat 30 moves, the hand shown
    ("04-tarot", UPRIGHT, ["--game", "tarot", "--seed", "3", "--bots", "all:m", "--steps", "30", "--reveal"]),
    ("05-chess", UPRIGHT, SB.CHESS),
    # upright (the game's tall screen, 320 x 440: the whole trunk); mid-run: one chop made, the timer bar running
    # (blind taps meet a branch soon: the GAME OVER card)
    ("06-beaverrush", UPRIGHT, ["--game", "arcade_beaverrush", "--at", "5000", "--pad", "START@60 RIGHT@120"]),
    ("07-mahjong", UPRIGHT, ["--game", "mahjong", "--seed", "2", "--moves", "=12"]),
    ("08-rollblock", UPRIGHT, ["--game", "rollblock", "--pick", "--solved", "1-14 15-16:2 17:1"]),   # its levels
]


def build():
    log = os.path.join(tempfile.gettempdir(), "store-shots-retrostone.log")
    with open(log, "w") as fh:
        r = subprocess.run(["make", "--no-print-directory", "-f", "apps/boardgames/Makefile", "host", f"B={B}",
                            "EDITION=retrostone"], cwd=ROOT, stdout=fh, stderr=subprocess.STDOUT)
    if r.returncode:
        sys.exit(f"the build failed: {log}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--no-build", action="store_true", help="use the app already built")
    ap.add_argument("--only", help="only the shots whose name holds this")
    a = ap.parse_args()
    if not a.no_build:
        build()
    exe = os.path.join(ROOT, B, "host", "retrostoneboard")
    tmp = tempfile.mkdtemp(prefix="rs-shots-")
    for lang in LANGS:
        d = os.path.join(STORE, "retrostone", "screenshots", lang)
        os.makedirs(d, exist_ok=True)
        for name, size, args in SHOTS:
            if a.only and a.only not in name:
                continue
            bmp = os.path.join(tmp, "shot.bmp")
            r = subprocess.run([exe, "--shot", bmp, "--size", size, "--lang", lang] + args, cwd=ROOT,
                               capture_output=True, text=True)
            if r.returncode:
                sys.exit(f"{lang} {name}: {r.stderr.strip()}")
            Image.open(bmp).convert("RGB").save(os.path.join(d, name + ".png"), optimize=True)
        print(f"{d}: {len(SHOTS)} screenshots")
    return 0


if __name__ == "__main__":
    sys.exit(main())

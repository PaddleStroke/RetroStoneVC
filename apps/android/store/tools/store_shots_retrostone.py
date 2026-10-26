#!/usr/bin/env python3
"""The phone screenshots of the RetroStone store listing, made by the app itself in its picture mode
(apps/boardgames/README.md, "Picture mode"), in 3 languages like the Board's (store_shots_board.py):
apps/android/store/retrostone/screenshots/<en|fr|de>/NN-<what>.png.

    python3 apps/android/store/tools/store_shots_retrostone.py [--no-build]

It builds the app first in its own folder (make -f apps/boardgames/Makefile host B=build/retrostone-store
EDITION=retrostone: the Arcade tab too). The board games' pictures are the app's screen at 1080 x 1920 (9:16, a phone
held upright), the Arcade game's at 1920 x 1080 (16:9: the app turns to landscape for it), the app's own layout at
that size, no caption. The menus, the Stats page and the Arcade tab show the app as it is: free, nothing locked.
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
SHOTS = [
    ("01-menu", UPRIGHT, ["--tab", "4"]),                                        # the Cards and tiles tab
    ("02-chess", UPRIGHT, SB.CHESS),
    ("03-arcade", UPRIGHT, ["--tab", "6"]),                                      # the Arcade tab
    # mid-play: the coconut in the air, the shells still up (later, the round ends on its GAME OVER card)
    ("04-arcade-game", WIDE, ["--game", "arcade_crabcoconut", "--at", "2500", "--pad", "START@60"]),
    ("05-belote", UPRIGHT, ["--game", "belote", "--seed", "3", "--moves", "p p p t Qd Td Kd 7d Ah 8h Js",
                            "--bots", "1:m,2:m,3:m"]),
    ("06-mahjong", UPRIGHT, ["--game", "mahjong", "--seed", "2", "--moves", "=12"]),
    ("07-nonograms", UPRIGHT, ["--game", "nonogram", "--pick", "--solved", "1-14 15-16:2 17:1"]),
    ("08-stats", UPRIGHT, ["--page", "stats", "--stats-demo"]),
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

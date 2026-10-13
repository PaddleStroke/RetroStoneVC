#!/usr/bin/env python3
"""The phone screenshots of the RetroStone Board and RetroStone Board Free store listings, made by the app itself in
its picture mode (apps/boardgames/README.md, "Picture mode"), in 3 languages like the Arcade's (store_shots.py):
apps/android/store/<board|boardfree>/screenshots/<en|fr|de>/NN-<what>.png.

    python3 apps/android/store/tools/store_shots_board.py [--no-build]

It builds both editions first, each in its own folder (make -f apps/boardgames/Makefile host B=build/boardgames-store
and B=build/boardgames-store-free EDITION=boardfree). Each picture is the app's screen at 1080 x 1920 (9:16, a phone
held upright, the app's own layout at that size: sharp, nothing scaled), no caption (the Arcade's have none).
The free app's pictures show only what it plays (its FREE_GAMES, the puzzles' first 40 levels): the picture mode
refuses a locked game or level, so a shot that strays fails here. Its menu shows the Puzzles tab, where nothing is
locked. Card games: the deal comes from --seed and the moves are the bots' own (a game of bots, then the person's
seat given back), so the picture is the person's view with the device in the other seats.
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import argparse
import os
import subprocess
import sys
import tempfile

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
STORE = os.path.dirname(HERE)
ROOT = os.path.abspath(os.path.join(STORE, "..", "..", ".."))
SIZE = "1080x1920"
LANGS = ("en", "fr", "de")
BUILDS = {"board": ("build/boardgames-store", "board"), "boardfree": ("build/boardgames-store-free", "boardfree")}

CHESS = ["--game", "chess", "--moves", "e2e4 e7e5 g1f3 b8c6 f1c4 g8f6 d2d3 f8c5 c2c3 d7d6", "--bots", "1:h",
         "--taps", "0.69,0.69"]                     # the device plays black (hard); the white knight picked up
BACKGAMMON = ["--game", "backgammon", "--moves", "31: 8/5 6/5 64: 24/18 13/9 52: 13/8 13/11 42: 13/9 13/11 "
              "61: 13/7* 8/7 53: bar/22 13/8 64:", "--bots", "1:m"]     # the device plays ebony; ivory to move 6-4
# what to show: (file name, the picture mode's arguments)
SHOTS = {
    "board": [
        ("01-menu", ["--tab", "4"]),                                    # the Cards and tiles tab
        ("02-chess", CHESS),
        ("03-belote", ["--game", "belote", "--seed", "3", "--moves", "p p p t Qd Td Kd 7d Ah 8h Js",
                       "--bots", "1:m,2:m,3:m"]),
        ("04-backgammon", BACKGAMMON),
        ("05-mahjong", ["--game", "mahjong", "--seed", "2", "--moves", "=12"]),
        ("06-nonograms", ["--game", "nonogram", "--pick", "--solved", "1-14 15-16:2 17:1"]),   # the level screen
        ("07-logicgates", ["--game", "logicgates", "--level", "110", "--moves", "s1 s3"]),
        ("08-mines", ["--game", "mines", "--level", "20", "--moves", "#"]),                    # solved: the stars
    ],
    "boardfree": [
        ("01-menu", ["--tab", "0"]),                                    # the Puzzles tab: nothing locked
        ("02-chess", CHESS),
        ("03-hearts", ["--game", "hearts", "--players", "4", "--seed", "3", "--moves",
                       "pJd8c6d pAsKhAc pKsAhQh pJhThKc 2c Tc Qc 3c 9c Ah 5c 8c Qd Td", "--bots", "1:m,2:m,3:m"]),
        ("04-backgammon", BACKGAMMON),
        ("05-sudoku", ["--game", "sudoku", "--pick", "--solved", "1-14 15-16:2 17:1"]),     # levels 1-25 shown
        ("06-logicgates", ["--game", "logicgates", "--level", "38", "--moves", "s1"]),
        ("07-solitaire", ["--game", "klondike", "--level", "12", "--moves", "? ? ? ? ? ? ? ? ? ? ? ?"]),
        ("08-nonogram", ["--game", "nonogram", "--level", "40", "--moves", "=999"]),          # solved: the stars
    ],
}


def build(app):
    b, edition = BUILDS[app]
    log = os.path.join(tempfile.gettempdir(), f"store-shots-{app}.log")
    with open(log, "w") as fh:
        r = subprocess.run(["make", "--no-print-directory", "-f", "apps/boardgames/Makefile", "host", f"B={b}",
                            f"EDITION={edition}"], cwd=ROOT, stdout=fh, stderr=subprocess.STDOUT)
    if r.returncode:
        sys.exit(f"the {app} build failed: {log}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--no-build", action="store_true", help="use the editions already built")
    a = ap.parse_args()
    tmp = tempfile.mkdtemp(prefix="rsb-shots-")
    for app, shots in SHOTS.items():
        if not a.no_build:
            build(app)
        exe = os.path.join(ROOT, BUILDS[app][0], "host", "retrostoneboard")
        for lang in LANGS:
            d = os.path.join(STORE, app, "screenshots", lang)
            os.makedirs(d, exist_ok=True)
            for name, args in shots:
                bmp = os.path.join(tmp, "shot.bmp")
                r = subprocess.run([exe, "--shot", bmp, "--size", SIZE, "--lang", lang] + args, cwd=ROOT,
                                   capture_output=True, text=True)
                if r.returncode:
                    sys.exit(f"{app} {lang} {name}: {r.stderr.strip()}")
                Image.open(bmp).convert("RGB").save(os.path.join(d, name + ".png"), optimize=True)
            print(f"{d}: {len(shots)} screenshots")
    return 0


if __name__ == "__main__":
    sys.exit(main())

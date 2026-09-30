#!/usr/bin/env python3
"""Pancake Tower: checks the UI screenshots of ui_test.sh (what each screen must show).

    ui_check.py DIR [SCALE]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pancaketower/LICENSE.
"""
import sys

from PIL import Image

D, S = sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 1
fails = 0


def load(name):
    im = Image.open("%s/%s.png" % (D, name)).convert("RGB")
    return im.resize((im.width // S, im.height // S), Image.NEAREST) if S > 1 else im


def near(c, ref, tol=12):
    return all(abs(a - b) <= tol for a, b in zip(c, ref))


def count(im, box, ref, tol=12):
    x0, y0, x1, y1 = box
    px = im.load()
    return sum(1 for y in range(y0, y1) for x in range(x0, x1) if near(px[x, y], ref, tol))


def check(cond, what):
    global fails
    print("  %s %s" % ("ok  " if cond else "FAIL", what))
    if not cond:
        fails += 1


title = load("title")
# the logo: "PANCAKE" in the pancake ramp (240,184,96), "TOWER" in the syrup ramp (196,104,44), the outline
check(count(title, (0, 16, 160, 80), (240, 184, 96)) > 150, "title: PANCAKE in golden letters (rows 2-9)")
check(count(title, (160, 16, 320, 80), (196, 104, 44)) > 100, "title: TOWER in syrup letters")
check(count(title, (0, 224, 320, 232), (255, 246, 220), 16) > 60, "title: the copyright line in cream (row 28)")

over = load("gameover-medal")
check(count(over, (84, 36, 236, 60), (250, 210, 90)) > 80, "game over: GAME OVER in gold on its banner")
check(count(over, (84, 76, 236, 164), (42, 68, 118)) > 4000, "game over: the navy score panel")
# the fork medal (24x24 at 172, 124): one of the four tiers' light colours
tiers = [(210, 142, 82), (222, 230, 238), (252, 216, 72), (242, 202, 214)]
check(max(count(over, (170, 122, 198, 150), t, 10) for t in tiers) > 60, "game over: a fork medal")

play, pause = load("play"), load("pause")
lum = lambda im: sum(sum(p) for p in im.getdata()) / (im.width * im.height)   # noqa: E731
check(lum(pause) < lum(play) * 0.8, "pause: the picture is dimmed (%.0f vs %.0f)" % (lum(pause), lum(play)))
check(count(pause, (100, 104, 220, 120), (153, 147, 132), 14) > 40, "pause: PAUSED in big letters (row 13, dimmed with the picture)")

vs = load("versus-2-players")
px = vs.load()
div = [px[159, y] for y in range(0, 240, 8)] + [px[160, y] for y in range(0, 240, 8)]
check(all(near(c, (22, 18, 40), 10) for c in div), "versus: the split screen's divider (x 159-160)")
ready2 = load("get-ready-2-players")
check(count(ready2, (40, 48, 120, 64), (255, 246, 220), 16) > 30 and count(ready2, (200, 48, 280, 64), (255, 246, 220), 16) > 30,
      "ready, 2 players: READY on each half")
sys.exit(1 if fails else 0)

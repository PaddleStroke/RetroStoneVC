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
title2 = load("title-2-players")
check(count(title2, (0, 16, 320, 80), (240, 184, 96)) > 150,
      "two players: the same title logo, no second menu")
check(count(title2, (0, 208, 320, 216), (255, 246, 220), 16) > 40,
      "two players: join/leave instructions on the title")
colours = [(226, 72, 66), (74, 146, 232), (86, 184, 102), (242, 168, 62)]
for name, players in [("title", 1), ("title-2-players", 2), ("title-3-players", 3),
                      ("title-4-players", 4), ("join4", 4), ("leave3", 3), ("leave2", 2), ("leave1", 1)]:
    im = load(name)
    check(count(im, (0, 80, 320, 144), (255, 239, 206)) > 2500,
          "%s: kitchen remains behind the menu" % name)
    check(count(im, (0, 150, 320, 196), (240, 184, 96)) > 30,
          "%s: pancakes retain their golden palette" % name)
    for p in range(players):
        cx = 60 + 72 * p
        check(count(im, (cx - 16, 96, cx + 16, 144), (250, 250, 252)) > 80,
              "%s: P%d chef is visible" % (name, p + 1))
        check(count(im, (cx - 16, 96, cx + 16, 144), colours[p]) > 5,
              "%s: P%d has their own colour" % (name, p + 1))
for players in [2, 3, 4]:
    im = load("results-%d" % players)
    check(count(im, (80, 56, 240, 64), (42, 68, 118)) < 5,
          "%d-player results: clear gap between winner banner and ranking panel" % players)
    check(count(im, (0, 0, 320, 20), (165, 105, 57)) > 500,
          "%d-player results: ceiling remains behind the panel" % players)
    check(count(im, (66, 65, 254, (8 + 3 * players + 4) * 8 - 2), (42, 68, 118)) > 4500,
          "%d-player results: ranking panel is visible" % players)
    row = 10 + 3 * players
    check(count(im, (100, row * 8, 220, row * 8 + 8), (255, 246, 220), 16) > 35,
          "%d-player results: menu prompt is in the footer below every score" % players)
    for place in range(players):
        y = (10 + 3 * place) * 8
        check(count(im, (180, y - 4, 198, y + 12), (255, 255, 255)) > 15,
              "%d-player results: score %d remains readable" % (players, place + 1))
    slide = load("results-%d-slide" % players)
    check(count(slide, (0, 0, 320, 20), (165, 105, 57)) > 500,
          "%d-player results: background remains during the slide" % players)
sys.exit(1 if fails else 0)

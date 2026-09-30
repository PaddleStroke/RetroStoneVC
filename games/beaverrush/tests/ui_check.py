#!/usr/bin/env python3
"""Beaver Rush: checks the UI screenshots of tests/ui_test.sh against the state of the run at the same frame
(the NAME.log next to each NAME.png holds the game's "state:" line).

    ui_check.py --dir DIR [--scale S]

Colours are computed as the console shows them: RGB888 -> RGB555 -> the PPU's RGB565 -> the PNG's RGB888, and the
sprites of the scene with the time-of-day tint of src/scene.c (the UI kit's colours are never tinted).
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/beaverrush/LICENSE.
"""
import argparse
import os
import re
import sys

from PIL import Image

fails = 0


def ok(msg):
    print("  ok   " + msg)


def ko(msg):
    global fails
    fails += 1
    print("  FAIL " + msg)


def c555(rgb):
    return (rgb[0] >> 3, rgb[1] >> 3, rgb[2] >> 3)


def shown(v):
    """a 555 colour as the PNG shows it"""
    r, g, b = v
    g6 = (g << 1) | (g >> 4)
    return ((r << 3) | (r >> 2), (g6 << 2) | (g6 >> 4), (b << 3) | (b >> 2))


# src/scene.c TOD: (brightness, tint, amount); sprites take half
TOD = [(236, (0xff, 0xa0, 0x8c), 44), (256, (0, 0, 0), 0), (214, (0xff, 0x78, 0x32), 70), (118, (0x1e, 0x32, 0x82), 92)]


def tinted(rgb, stage, sprite=True):
    mul, tint, k = TOD[stage % 4]
    if sprite:
        mul, k = 256 - (256 - mul) // 2, k // 2
    r, g, b = c555(rgb)
    d = (r * mul // 256, g * mul // 256, b * mul // 256)
    if k:
        t = c555(tint)
        d = tuple((d[i] * (256 - k) + t[i] * k) // 256 for i in range(3))
    return shown(d)


def ui(rgb):
    return shown(c555(rgb))


class Shot:
    def __init__(self, d, name, scale):
        self.name = name
        path = os.path.join(d, name + ".png")
        self.ok = os.path.exists(path)
        if not self.ok:
            ko("%s: no picture" % name)
            return
        im = Image.open(path).convert("RGB")
        self.w, self.h = im.width // scale, im.height // scale
        src = im.load()
        self.px = [[src[x * scale, y * scale] for x in range(self.w)] for y in range(self.h)]
        self.state = {}
        log = os.path.join(d, name + ".log")
        if os.path.exists(log):
            for line in open(log):
                if "state:" in line:
                    self.state = dict(re.findall(r"(\w+)=([-\w,]*)", line.split("state:", 1)[1]))

    def st(self, k):
        return int(self.state.get(k, "0").split(",")[0], 16 if k == "hash" else 10)

    def count(self, rgb, x0, y0, x1, y1, tol=0):
        n = 0
        for y in range(max(0, y0), min(self.h, y1)):
            row = self.px[y]
            for x in range(max(0, x0), min(self.w, x1)):
                p = row[x]
                if abs(p[0] - rgb[0]) <= tol and abs(p[1] - rgb[1]) <= tol and abs(p[2] - rgb[2]) <= tol:
                    n += 1
        return n

    def luma(self, x0, y0, x1, y1):
        s = n = 0
        for y in range(y0, y1):
            for x in range(x0, x1):
                p = self.px[y][x]
                s += (p[0] * 3 + p[1] * 6 + p[2]) // 10
                n += 1
        return s // max(1, n)


CREAM, GOLD_UI, FILL = ui((255, 246, 220)), ui((250, 210, 90)), ui((42, 68, 118))
BAR_LIGHT = ui((255, 226, 110))
DIGIT = ui((250, 250, 250))
TEETH = (255, 190, 80)
BRONZE = ui((210, 142, 82))


def check_bar(s, x0, inner, what):
    """the timer bar's filled width against the state (bar in permille)"""
    want = s.st("bar") * inner * 8 // 1000
    got = s.count(BAR_LIGHT, x0, 33, x0 + inner * 8, 34)
    if abs(got - want) <= 1:
        ok("%s: the timer bar shows %d of %d px (bar %d/1000)" % (what, got, inner * 8, s.st("bar")))
    else:
        ko("%s: the timer bar shows %d px, the timer says %d" % (what, got, want))


def check_beaver_side(s, cx, what):
    stage = s.st("stage")
    teeth = tinted(TEETH, stage)
    left = s.count(teeth, cx - 60, 164, cx - 20, 202, tol=6)
    right = s.count(teeth, cx + 20, 164, cx + 60, 202, tol=6)
    side = s.st("side")
    if (side == 1 and left > 2 and right == 0) or (side == 2 and right > 2 and left == 0):
        ok("%s: the beaver's big teeth are on its side (%s)" % (what, "left" if side == 1 else "right"))
    else:
        ko("%s: the beaver's teeth: %d px left, %d right, the state says side %d" % (what, left, right, side))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", required=True)
    ap.add_argument("--scale", type=int, default=1)
    a = ap.parse_args()
    S = {n: Shot(a.dir, n, a.scale) for n in ("title", "play", "milestone", "golden", "night", "winter", "pause",
                                               "gameover", "versus", "versus-over")}
    t = S["title"]
    if t.ok:
        logo = t.count(ui((252, 204, 64)), 0, 16, 320, 80) + t.count(ui((190, 124, 68)), 0, 16, 320, 80)
        (ok if logo > 400 else ko)("title: the BEAVER RUSH logo (%d px of its ramps)" % logo)
        n = t.count(CREAM, 0, 224, 320, 232)
        (ok if n > 60 else ko)("title: the copyright line (%d px)" % n)
        n = t.count(ui((222, 230, 238)), 60, 160, 120, 184)
        (ok if n > 20 else ko)("title: the A glyph of the prompt (%d px)" % n)
    p = S["play"]
    if p.ok:
        check_bar(p, 112, 12, "play")
        n = p.count(DIGIT, 120, 10, 200, 26)
        (ok if n > 30 else ko)("play: the score digits (%d px, score %d)" % (n, p.st("score")))
        check_beaver_side(p, 160, "play")
    m = S["milestone"]
    if m.ok:
        n = m.count(CREAM, 0, 56, 320, 72)
        (ok if n > 80 and m.st("stage") == 1 else ko)("milestone: the new scene's name on row 7 (%d px, stage %d)" %
                                                       (n, m.st("stage")))
        check_bar(m, 112, 12, "milestone")
    g = S["golden"]
    if g.ok:
        gold = tinted((252, 216, 72), g.st("stage"))
        n = g.count(gold, 0, 60, 320, 210, tol=8)
        (ok if n > 20 else ko)("golden log: the golden log flies off (%d gold px)" % n)
    n_ = S["night"]
    if n_.ok:
        top = n_.luma(0, 0, 320, 24)
        (ok if n_.st("stage") == 3 and top < 40 else ko)("night: a dark sky (luma %d, stage %d)" % (top, n_.st("stage")))
        check_beaver_side(n_, 160, "night")
    w = S["winter"]
    if w.ok:
        (ok if 8 <= w.st("stage") <= 11 else ko)("winter: stage %d" % w.st("stage"))
    ps = S["pause"]
    if ps.ok and p.ok:
        n = ps.count(shown(tuple(v * 9 // 15 for v in c555((255, 246, 220)))), 0, 104, 320, 120, tol=10)
        (ok if ps.st("paused") == 1 and ps.luma(0, 0, 320, 240) < p.luma(0, 0, 320, 240) and n > 40 else ko)(
            "pause: the picture dims and PAUSED shows (%d px)" % n)
    o = S["gameover"]
    if o.ok:
        f = o.count(FILL, 80, 72, 240, 168)
        b = o.count(GOLD_UI, 80, 32, 240, 64)
        md = o.count(BRONZE, 160, 120, 200, 152)
        (ok if f > 6000 and b > 100 else ko)("game over: the panel (%d px) and GAME OVER in gold (%d px)" % (f, b))
        (ok if md > 10 and o.st("score") >= 50 else ko)("game over: the bronze medal for %d logs (%d px)" %
                                                        (o.st("score"), md))
    v = S["versus"]
    if v.ok:
        div = sum(1 for y in range(240) for x in (159, 160) if v.px[y][x] == ui((12, 20, 44)))
        (ok if div >= 470 else ko)("versus: the split screen's divider (%d of 480 px)" % div)
        l, r = v.count(DIGIT, 40, 10, 120, 26), v.count(DIGIT, 200, 10, 280, 26)
        (ok if l > 20 and r > 20 else ko)("versus: a score in each half (%d, %d px)" % (l, r))
        bl, br = v.count(BAR_LIGHT, 48, 33, 112, 34), v.count(BAR_LIGHT, 208, 33, 272, 34)
        (ok if bl > 0 and br > 0 else ko)("versus: a timer bar in each half (%d, %d px)" % (bl, br))
    vo = S["versus-over"]
    if vo.ok:
        f = vo.count(FILL, 80, 72, 240, 168)
        div = sum(1 for y in range(0, 30) for x in (159, 160) if vo.px[y][x] == ui((12, 20, 44)))
        win = vo.st("winner")
        (ok if f > 6000 and div > 50 and win in (-1, 0, 1) else ko)(
            "versus over: the panel over both halves (%d px), winner %d" % (f, win))
    print("ui_check: %s" % ("all passed" if not fails else "%d FAILED" % fails))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())

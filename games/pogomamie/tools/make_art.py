#!/usr/bin/env python3
"""Pogo Mamie: the code-drawn art (final: no image generation).

Clean SNES style, the look of Leady Squid: flat shades (2-4 per material), a 1-px dark outline on the
characters and props, light from the top-left, readable at 1x. Every colour comes from a palette of
pm_sheets.py, by name, so the asset build maps pixels to palette indices exactly.

Writes, in the art folder (default games/pogomamie/art):
  sprites.png                   every sprite (pm_sheets.SPRITES)
  tiles/bg2.png                 the play layer's 8x8 tiles (pm_sheets.BG_TILES), 16 per row
  tiles/mid.png                 the mid-ground roofs (BG3)
  tiles/far_<district>.png      the four district backdrops (BG4)

    python3 tools/make_placeholders.py --game pogomamie [--out DIR] [--preview]
    python3 games/pogomamie/tools/make_art.py [--out DIR] [--preview]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pogomamie/LICENSE.
"""
import argparse
import math
import os
import random
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.dirname(HERE)
ROOT = os.path.abspath(os.path.join(GAME, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "sdk", "tools"))
sys.path.insert(0, os.path.join(ROOT, "games", "common", "tools"))
import house_style as hs  # noqa: E402
import pm_sheets as S  # noqa: E402
from gen_font import G as FONT  # noqa: E402

MAG = S.MAGENTA


class Canvas(hs.Canvas):
    """The house Canvas (games/common/tools/house_style.py) drawn with colour NAMES of one of our palettes: the
    pixels hold the RGB of the palette entry, so the asset build maps them back to indices exactly."""

    def __init__(self, w, h, pal):
        super().__init__(w, h)
        self.pal = dict(pal)

    @staticmethod
    def wrap(hc, pal):
        cv = Canvas(hc.w, hc.h, pal)
        cv.p = hc.p
        return cv

    def rgb(self, c):
        return None if c is None else c if isinstance(c, tuple) else self.pal[c]

    def set(self, x, y, c):
        super().set(int(x), int(y), self.rgb(c))

    def is_(self, x, y, name):
        return self.get(x, y) == self.pal[name]

    def rect(self, x0, y0, x1, y1, c):
        for y in range(int(y0), int(y1) + 1):
            for x in range(int(x0), int(x1) + 1):
                self.set(x, y, c)

    def hline(self, x0, x1, y, c):
        self.rect(x0, y, x1, y, c)

    def vline(self, x, y0, y1, c):
        self.rect(x, y0, x, y1, c)

    def ellipse(self, cx, cy, rx, ry, c, shade=None):
        """Filled ellipse; shade(dx, dy) -> colour name overrides c (light from the top-left)."""
        for y in range(int(cy - ry - 1), int(cy + ry + 2)):
            for x in range(int(cx - rx - 1), int(cx + rx + 2)):
                dx, dy = (x + 0.5 - cx) / max(rx, 0.1), (y + 0.5 - cy) / max(ry, 0.1)
                if dx * dx + dy * dy <= 1.0:
                    self.set(x, y, shade(dx, dy) if shade else c)

    def line(self, x0, y0, x1, y1, c, width=1):
        n = int(max(abs(x1 - x0), abs(y1 - y0)) * 2) + 1
        for i in range(n + 1):
            t = i / float(n)
            x, y = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
            for k in range(width):
                self.set(round(x - 0.5) + (k if abs(x1 - x0) < abs(y1 - y0) else 0),
                         round(y - 0.5) + (0 if abs(x1 - x0) < abs(y1 - y0) else k), c)

    def outline(self, c, diag=False):
        super().outline(self.rgb(c), diag)

    def squashed(self, sx):
        """house squash and stretch (area kept, the feet on the ground)"""
        return Canvas.wrap(hs.squash(self, sx), self.pal)


def text5(cv, s, x, y, c, scale=1):
    for i, ch in enumerate(s):
        g = FONT.get(ch, FONT[" "])
        for gy, row in enumerate(g[:7]):
            for gx, v in enumerate(row):
                if v == "#":
                    cv.rect(x + (i * 6 + gx) * scale, y + gy * scale, x + (i * 6 + gx) * scale + scale - 1,
                            y + gy * scale + scale - 1, c)


# ==== Mamie ==============================================================================================================
MP = S.OBJ_PALS["mamie"]
MP_RGB = dict(MP)


def mamie(pose):
    """One 24x32 frame, facing right; the pogo's tip at the bottom centre (x 11-12, row 31)."""
    cv = Canvas(24, 32, MP)
    P = dict(body=0, spring=4, legs="bent", arms="bar", scarf=0, mouth="smile", eyes="open", tilt=0, bag=0, sq=1.0)
    P.update({
        "idle": {}, "squash1": dict(body=2, spring=2, legs="squat", sq=1.12),
        "squash2": dict(body=3, spring=1, legs="squat", mouth="puff", sq=1.25),
        "stretch": dict(body=-1, spring=6, legs="straight", sq=0.82),
        "rise": dict(body=-1, spring=5, legs="tuck", scarf=1, sq=0.92),
        "fall": dict(body=0, spring=4, legs="straight", scarf=-1, bag=-2), "big": dict(body=-1, spring=5,
                                                                                         legs="kneeup", mouth="grin",
                                                                                         bag=-3),
        "stumble": dict(body=0, spring=4, legs="straight", arms="flail", eyes="wide", mouth="o", tilt=-1),
    }.get(pose, {}))
    if pose in ("flail1", "flail2", "sit", "float", "hang"):
        return mamie_special(pose)
    b = P["body"]
    # the pogo: rubber tip, spring, foot pegs, rod, handlebar
    tip_y = 31
    cv.rect(11, tip_y - 1, 12, tip_y, "out")
    spr = P["spring"]
    top_spring = tip_y - 2 - spr
    for y in range(top_spring, tip_y - 1):
        cv.rect(10, y, 13, y, "spring" if (y - top_spring) % 2 == 0 else "bag_d")
    peg_y = top_spring - 1
    cv.rect(8, peg_y, 15, peg_y, "metal_d")
    rod_top = 14 + b
    cv.rect(11, rod_top, 12, peg_y - 1, "metal_l")
    cv.vline(12, rod_top, peg_y - 1, "metal_d")
    bar_y = rod_top
    cv.rect(8, bar_y, 15, bar_y, "metal_l")
    cv.set(8, bar_y, "out"), cv.set(15, bar_y, "out")
    # legs: dark stockings and little black shoes on the pegs
    legs = P["legs"]
    if legs == "squat":
        for sx in (-1, 1):
            cv.line(12 + sx * 2, peg_y - 1, 12 + sx * 5, peg_y - 4, "skirt", 2)
            cv.line(12 + sx * 5, peg_y - 4, 12 + sx * 2, 18 + b, "skirt", 2)
    elif legs == "tuck":
        for sx in (-1, 1):
            cv.line(12 + sx * 2, peg_y - 1, 12 + sx * 4, peg_y - 3, "skirt", 2)
            cv.line(12 + sx * 4, peg_y - 3, 12 + sx * 2, 18 + b, "skirt", 2)
    elif legs == "kneeup":
        for sx in (-1, 1):
            cv.line(12 + sx * 2, peg_y - 1, 12 + sx * 6, 20 + b, "skirt", 2)
            cv.line(12 + sx * 6, 20 + b, 12 + sx * 2, 18 + b, "skirt", 2)
    else:
        for sx in (-1, 1):
            cv.line(12 + sx * 2, peg_y - 1, 12 + sx * 2, 18 + b, "skirt", 2)
    for sx in (-1, 1):
        cv.rect(12 + sx * 3 - 1, peg_y - 1, 12 + sx * 3, peg_y - 1, "out")
    # the skirt (flared), the cardigan (buttons), arms on the handlebar
    for y in range(15 + b, 20 + b):
        half = 4 + (y - (15 + b)) // 2 + (1 if P["scarf"] < 0 and y > 17 + b else 0)
        for x in range(12 - half, 12 + half):
            cv.set(x, y, "skirt" if x < 15 else "out" if y == 19 + b else "skirt")
    cv.hline(12 - 6, 12 + 5, 19 + b, "skirt")
    for y in range(8 + b, 16 + b):
        half = 4 if y < 10 + b else 5
        for x in range(12 - half, 12 + half):
            cv.set(x, y, "cardi_l" if x < 12 + half - 3 else "cardi_d")
    for y in (10 + b, 12 + b, 14 + b):
        cv.set(13, y, "white")
    if P["arms"] == "bar":
        cv.line(8, 10 + b, 8, bar_y - 1, "cardi_d", 2)
        cv.line(15, 10 + b, 15, bar_y - 1, "cardi_l", 2)
        cv.set(8, bar_y - 1, "skin_l"), cv.set(15, bar_y - 1, "skin_l")
    else:                                   # flailing: one arm up, the other out
        cv.line(7, 10 + b, 3, 4 + b, "cardi_d", 2)
        cv.set(3, 3 + b, "skin_l"), cv.set(4, 3 + b, "skin_l")
        cv.line(16, 10 + b, 20, 8 + b, "cardi_l", 2)
        cv.set(21, 8 + b, "skin_l")
    # the handbag on her left arm (behind), swinging
    by = 12 + b + P["bag"]
    cv.rect(2, by, 6, by + 4, "bag_l")
    cv.rect(2, by + 3, 6, by + 4, "bag_d")
    cv.line(4, by - 2, 7, 10 + b, "bag_d")
    cv.set(4, by + 1, "spring")
    # the head: face, round glasses, the headscarf tied under the chin, a grey curl
    hx, hy = 13 + P["tilt"], 4 + b
    cv.ellipse(hx, hy + 0.5, 4.2, 4.2, "skin_l", lambda dx, dy: "skin_d" if dx < -0.55 and dy > -0.2 else "skin_l")
    # scarf over the top and back of the head
    for y in range(hy - 5, hy + 1):
        for x in range(hx - 5, hx + 4):
            dx, dy = (x + 0.5 - hx) / 4.8, (y + 0.5 - hy + 0.5) / 4.8
            if dx * dx + dy * dy <= 1.0 and (y < hy - 1 or x < hx - 1):
                cv.set(x, y, "scarf_l" if (x + y) % 5 else "white")
    for x in range(hx - 5, hx - 1):
        cv.set(x, hy + 1, "scarf_d")
    # the knot at the back and its flapping ends
    fl = P["scarf"]
    cv.rect(hx - 6, hy + 1, hx - 5, hy + 2, "scarf_d")
    cv.line(hx - 6, hy + 2, hx - 9, hy + 3 - fl * 2, "scarf_l", 2)
    cv.set(hx + 1, hy - 1, "hair"), cv.set(hx + 2, hy - 1, "hair")
    # the house eye (a 2x3 white, a 1x2 pupil looking where she goes) behind round glasses
    ey = hy - 1
    cv.rect(hx + 1, ey - 1, hx + 4, ey + 3, "metal_d")
    hs.eye(cv, hx + 2, ey, look=(1, 0), white=MP_RGB["white"], pupil=MP_RGB["out"])
    if P["eyes"] == "wide":
        cv.set(hx + 3, ey + 1, "white"), cv.set(hx + 3, ey + 2, "out")
    cv.set(hx + 1, ey - 1, "metal_l")                   # the lens's glint (top-left)
    cv.set(hx + 5, ey + 2, "skin_d"), cv.set(hx + 5, ey + 3, "skin_d")    # the nose
    ey = hy
    m = P["mouth"]
    if m == "smile":
        cv.set(hx + 2, ey + 3, "out"), cv.set(hx + 3, ey + 3, "out")
    elif m == "grin":
        cv.rect(hx + 1, ey + 3, hx + 3, ey + 3, "out")
        cv.set(hx + 2, ey + 2, "white")
    elif m == "o":
        cv.set(hx + 3, ey + 3, "out")
    else:
        cv.set(hx + 4, ey + 3, "out")
        cv.set(hx + 2, ey + 2, "skin_d")
    cv.set(hx + 1, ey + 2, "scarf_l")                   # the rosy cheek (the scarf's pink)
    if P["sq"] != 1.0:                                  # house squash and stretch, the pogo's tip on the ground
        cv = cv.squashed(P["sq"])
    cv.outline("out")
    return cv


def mamie_special(pose):
    cv = Canvas(24, 32, MP)

    def head(hx, hy, mouth="o", eyes="x"):
        cv.ellipse(hx, hy, 4.2, 4.2, "skin_l")
        for y in range(int(hy) - 5, int(hy) + 1):
            for x in range(int(hx) - 5, int(hx) + 4):
                dx, dy = (x + 0.5 - hx) / 4.8, (y + 1 - hy) / 4.8
                if dx * dx + dy * dy <= 1.0 and (y < hy - 1 or x < hx - 1):
                    cv.set(x, y, "scarf_l" if (x + y) % 5 else "white")
        cv.set(hx + 2, hy, "metal_d"), cv.set(hx + 4, hy, "metal_d")
        if eyes == "x":
            cv.set(hx + 3, hy, "out")
            cv.set(hx + 2, hy + 1, "out"), cv.set(hx + 4, hy + 1, "out")
        else:
            cv.set(hx + 3, hy + 1, "out")
        if mouth == "o":
            cv.rect(hx + 2, hy + 3, hx + 3, hy + 3, "out")
        else:
            cv.set(hx + 2, hy + 3, "out")

    if pose in ("flail1", "flail2"):
        k = 1 if pose == "flail2" else -1
        # upside-ish: arms up, legs kicking, the pogo slipping away below
        cv.line(12, 20, 12 + 3 * k, 31, "metal_l", 2)
        cv.rect(11 + 3 * k, 26, 14 + 3 * k, 28, "spring")
        cv.rect(8, 12, 15, 19, "cardi_l")
        cv.rect(13, 12, 15, 19, "cardi_d")
        cv.rect(7, 18, 16, 22, "skirt")
        cv.line(9, 22, 7 - k, 27, "skirt", 2)
        cv.line(14, 22, 16 + k, 26, "skirt", 2)
        cv.line(8, 13, 3, 6 + 2 * k, "cardi_d", 2)
        cv.line(15, 13, 20, 6 - 2 * k, "cardi_l", 2)
        cv.set(3, 5 + 2 * k, "skin_l"), cv.set(20, 5 - 2 * k, "skin_l")
        cv.rect(1, 8 + k, 5, 11 + k, "bag_l")
        head(12, 7, "o", "wide")
    elif pose == "sit":
        # sitting on the sidewalk, legs out, the pogo bent beside her
        cv.line(17, 31, 22, 22, "metal_l", 2)
        cv.line(22, 22, 20, 17, "metal_d", 2)
        cv.rect(17, 27, 19, 29, "spring")
        cv.rect(5, 20, 12, 27, "cardi_l")
        cv.rect(10, 20, 12, 27, "cardi_d")
        cv.rect(4, 26, 16, 30, "skirt")
        cv.rect(14, 29, 19, 30, "skirt")
        cv.rect(19, 29, 20, 30, "out")
        cv.rect(1, 25, 4, 29, "bag_l")
        head(9, 16, "o", "x")
    elif pose == "float":
        # in the river with a life ring (the scarf's red and white)
        for y in range(18, 26):
            for x in range(1, 23):
                dx, dy = (x + 0.5 - 12) / 11.0, (y + 0.5 - 22) / 4.0
                if dx * dx + dy * dy <= 1.0 and not (abs(x + 0.5 - 12) < 6 and y < 22):
                    cv.set(x, y, "white" if (x // 4) % 2 else "scarf_l")
        cv.rect(7, 14, 16, 20, "cardi_l")
        cv.line(7, 16, 3, 19, "cardi_d", 2)
        cv.line(16, 16, 20, 19, "cardi_l", 2)
        head(12, 9, "o", "open")
    else:                               # hang: pulled up by the yarn
        cv.line(12, 0, 12, 5, "scarf_l")
        cv.line(10, 6, 12, 2, "cardi_d", 2)
        cv.line(15, 6, 12, 2, "cardi_l", 2)
        cv.set(12, 1, "skin_l")
        cv.rect(8, 10, 15, 17, "cardi_l")
        cv.rect(13, 10, 15, 17, "cardi_d")
        cv.rect(7, 17, 16, 21, "skirt")
        cv.line(10, 21, 10, 26, "skirt", 2)
        cv.line(14, 21, 14, 26, "skirt", 2)
        cv.line(12, 22, 12, 31, "metal_l", 2)
        cv.rect(11, 28, 13, 30, "spring")
        head(12, 8, "s", "open")
    cv.outline("out")
    return cv


def papi_head():
    """Papi's beret and moustache, drawn at the head of each frame (palette: Mamie's, recoloured)."""
    cv = Canvas(16, 16, MP)
    # the beret: a flat navy disc (Papi's 'skirt' colour), a little stalk
    cv.ellipse(8, 6, 6.2, 2.4, "skirt")
    cv.hline(3, 12, 7, "skirt")
    cv.set(8, 3, "skirt")
    cv.hline(4, 9, 5, "cardi_d")
    # the moustache (grey)
    cv.rect(9, 12, 13, 12, "hair")
    cv.rect(10, 13, 12, 13, "hair")
    cv.outline("out")
    return cv


# ==== animals ================================================================================================================
AP = S.OBJ_PALS["animals"]


def cat(frame):
    cv = Canvas(16, 16, AP)
    shade = lambda dx, dy: "cat_l" if dx + dy < -0.3 else "cat_m" if dx + dy < 0.7 else "cat_d"  # noqa: E731
    if frame in (0, 1):                 # sitting, facing left (looking back), tail curling
        cv.ellipse(9, 11.5, 4, 4, None, shade)
        cv.ellipse(6, 6.5, 3.4, 3, None, shade)
        cv.set(4, 3, "cat_m"), cv.set(4, 2, "cat_d"), cv.set(8, 3, "cat_m"), cv.set(8, 2, "cat_d")   # ears
        cv.set(5, 6, "eye"), cv.set(4, 6, "out")         # the eye looking back (left)
        cv.set(3, 7, "pink")
        cv.rect(6, 14, 8, 15, "white")                   # white paws
        cv.set(10, 9, "cat_d"), cv.set(11, 11, "cat_d")  # stripes
        if frame == 0:
            cv.line(13, 14, 15, 10, "cat_m", 1), cv.line(15, 10, 14, 7, "cat_m", 1)
        else:
            cv.line(13, 14, 15, 13, "cat_m", 1), cv.line(15, 13, 15, 9, "cat_m", 1)
        cv.rect(5, 8, 6, 8, "white")                     # the muzzle
    elif frame == 2:                    # leaping right, stretched
        cv.ellipse(8, 9, 6, 2.6, None, shade)
        cv.ellipse(13, 7, 2.6, 2.4, None, shade)
        cv.set(12, 4, "cat_m"), cv.set(14, 4, "cat_m")
        cv.set(14, 7, "out")
        cv.line(2, 8, 0, 5, "cat_m")
        cv.line(4, 11, 2, 13, "cat_d"), cv.line(12, 11, 14, 13, "white")
    else:                               # landing crouch
        cv.ellipse(8, 12, 6, 3, None, shade)
        cv.ellipse(12, 9.5, 3, 2.6, None, shade)
        cv.set(11, 7, "cat_m"), cv.set(13, 7, "cat_m")
        cv.set(13, 9, "out")
        cv.line(2, 11, 0, 8, "cat_m")
        cv.rect(10, 14, 12, 15, "white")
    cv.outline("out")
    return cv


def pigeon(frame):
    cv = Canvas(16, 16, AP)
    bob = [0, 1, 3, 0, 0][frame]
    shade = lambda dx, dy: "pg_l" if dy < -0.3 else "pg_m" if dy < 0.5 else "pg_d"  # noqa: E731
    if frame < 3:                       # walking right / pecking
        cv.ellipse(7, 11, 5, 3.4, None, shade)
        cv.line(2, 11, 0, 9, "pg_d", 2)              # the tail
        hx, hy = 11 + (1 if frame == 1 else 0) + (1 if frame == 2 else 0), 6 + bob
        cv.ellipse(hx, hy + 2, 2, 2.4, "pg_green")    # the iridescent neck
        cv.set(hx - 1, hy + 3, "pg_purple")
        cv.ellipse(hx, hy, 2.2, 2, "pg_l")
        cv.set(hx + 1, hy - 1, "eye")
        cv.set(hx + 3, hy, "beak"), cv.set(hx + 3, hy + 1, "beak")
        cv.line(6, 12, 9, 10, "pg_d")                 # the wing
        cv.set(6, 15, "beak"), cv.set(9, 15, "beak")
        cv.set(6, 14, "beak"), cv.set(9 - (frame == 1), 14, "beak")
    else:                               # flying
        up = frame == 3
        cv.ellipse(8, 10, 5, 2.6, None, shade)
        cv.ellipse(13, 8, 2, 2, "pg_l")
        cv.set(14, 7, "eye"), cv.set(15, 8, "beak")
        for i in range(6):
            cv.line(5 + i, 9, 3 + i, 3 if up else 14, "pg_m" if i % 2 else "pg_l")
        cv.line(3, 10, 0, 12, "pg_d", 2)
    cv.outline("out")
    return cv


# ==== props ==================================================================================================================
PP = S.OBJ_PALS["props"]


def awning(frame):
    cv = Canvas(24, 16, PP)
    press = frame == 1
    depth = 9 if not press else 7
    for y in range(depth):
        for x in range(24):
            cv.set(x, y + (1 if press and 4 <= x < 20 and y == 0 else 0), "red" if (x // 4) % 2 == 0 else "white")
        if y == depth - 1:
            for x in range(24):
                cv.set(x, y, "red_d" if (x // 4) % 2 == 0 else "white_d")
    cv.hline(0, 23, 0, "white") if not press else None
    for x in range(24):                     # the scalloped valance
        if (x % 4) in (1, 2):
            cv.set(x, depth, "red_d" if (x // 4) % 2 == 0 else "white_d")
    cv.rect(0, 0, 23, 0, "white")
    cv.outline("out")
    return cv


def pot():
    cv = Canvas(16, 16, PP)
    for x in range(1, 15):                  # geraniums and leaves above the box
        h = 3 + (x * 7) % 4
        for y in range(8 - h, 8):
            cv.set(x, y, "leaf_l" if (x + y) % 3 else "leaf_d")
    for fx, fy in ((3, 3), (7, 2), (11, 3), (13, 5), (5, 5)):
        cv.rect(fx - 1, fy, fx, fy + 1, "flower")
    cv.rect(0, 8, 15, 13, "pot_l")
    cv.rect(0, 12, 15, 13, "pot_d")
    cv.hline(0, 15, 8, "white")
    cv.rect(2, 14, 3, 15, "metal_d"), cv.rect(12, 14, 13, 15, "metal_d")
    return cv


def cradle():
    cv = Canvas(32, 16, PP)
    cv.hline(0, 31, 0, "metal_l")
    cv.hline(0, 31, 1, "metal_d")
    for x in (0, 10, 21, 31):
        cv.vline(x, 1, 6, "metal_d")
    cv.rect(0, 6, 31, 12, "wood_l")
    cv.rect(0, 11, 31, 12, "wood_d")
    for x in range(3, 31, 7):
        cv.vline(x, 7, 12, "wood_d")
    cv.rect(4, 3, 7, 5, "blue")             # a bucket
    cv.rect(24, 2, 26, 5, "white")          # a squeegee
    return cv


def ledge(frame):
    cv = Canvas(8, 8, PP)
    for x in range(8):
        cv.set(x, 0, "pot_l")
        cv.set(x, 1, "pot_l" if x % 4 else "pot_d")
        cv.set(x, 2, "pot_d")
    if frame == 0:
        cv.line(0, 3, 5, 7, "wood_d", 2)
    elif frame == 2:
        cv.set(7, 1, None), cv.set(7, 2, None), cv.set(6, 2, None)
    cv.set(3 + frame, 3, "wood_d")
    return cv


def line_seg(k):
    cv = Canvas(8, 8, PP)
    s = [2, 1, 0, -1, -2][k]
    for x in range(8):
        y = 3 + (s * x) // 8 if s >= 0 else 3 + (s * x - 7) // 8 + 1
        cv.set(x, y, "white_d")
    return cv


def clothes(k):
    cv = Canvas(8, 8, PP)
    if k == 0:                              # a shirt
        cv.rect(1, 0, 6, 6, "blue")
        cv.rect(0, 0, 7, 2, "blue")
        cv.set(3, 1, "white"), cv.set(4, 1, "white")
    elif k == 1:                            # a sock
        cv.rect(3, 0, 5, 5, "red")
        cv.rect(3, 5, 6, 6, "red")
        cv.hline(3, 5, 0, "white")
    elif k == 2:                            # bloomers
        cv.rect(1, 0, 6, 3, "white")
        cv.rect(1, 4, 2, 6, "white"), cv.rect(5, 4, 6, 6, "white")
        cv.hline(1, 6, 0, "flower")
    else:                                   # a towel
        cv.rect(1, 0, 6, 7, "leaf_l")
        cv.hline(1, 6, 5, "white")
    cv.set(1, 0, "wood_d"), cv.set(6, 0, "wood_d")
    cv.outline("out")
    return cv


def antenna(frame):
    cv = Canvas(16, 32, PP)
    lean = [0, -1, 1][frame]
    for y in range(4, 32):
        x = 7 + (lean * (31 - y)) // 14
        cv.set(x, y, "metal_l")
        cv.set(x + 1, y, "metal_d")
    for i, (y, half) in enumerate(((5, 6), (10, 5), (15, 4))):
        xc = 8 + (lean * (31 - y)) // 14
        cv.hline(xc - half, xc + half - 1, y, "metal_l")
        cv.set(xc - half, y + 1, "metal_d"), cv.set(xc + half - 1, y + 1, "metal_d")
    cv.rect(6, 29, 9, 31, "metal_d")
    cv.outline("out")
    return cv


# ==== items ===================================================================================================================
IP = S.OBJ_PALS["items"]


def item(k, small=False):
    n = 8 if small else 16
    cv = Canvas(n, n, IP)
    s = n / 16.0
    if k == 0:                              # the umbrella, closed, slanted
        cv.line(3 * s, 13 * s, 13 * s, 3 * s, "umb_l", max(1, int(3 * s)))
        cv.line(4 * s, 13 * s, 13 * s, 4 * s, "umb_d", 1)
        cv.set(13 * s, 2 * s, "needle")
        cv.line(3 * s, 13 * s, 2 * s, 15 * s, "handle", 1)
        cv.set(1 * s + 1, 15 * s, "handle")
    elif k == 1:                            # a baguette
        cv.line(2 * s, 12 * s, 13 * s, 3 * s, "bread_m", max(1, int(4 * s)))
        cv.line(3 * s, 11 * s, 12 * s, 3 * s, "bread_l", 1)
        if not small:
            for i in range(3):
                cv.line(5 + i * 3, 10 - i * 2, 7 + i * 3, 9 - i * 2, "bread_d")
    elif k == 2:                            # knitting yarn with two needles
        cv.ellipse(8 * s, 9 * s, 5 * s, 5 * s, None,
                   lambda dx, dy: "yarn_l" if dx + dy < 0 else "yarn_d")
        if not small:
            for i in range(-3, 4, 2):
                cv.line(8 + i - 2, 5, 8 + i + 2, 13, "yarn_d")
            cv.line(2, 2, 11, 11, "needle"), cv.line(14, 2, 5, 11, "needle")
        cv.set(1, 13 * s, "yarn_d")
    else:                                   # a croissant
        for i in range(5):
            ang = math.pi * (0.1 + 0.8 * i / 4.0)
            cx, cy = 8 * s - math.cos(ang) * 5 * s, 10 * s - math.sin(ang) * 4 * s
            r = (2.6 if i in (1, 2, 3) else 1.8) * s
            cv.ellipse(cx, cy, r + 0.3, r, None, lambda dx, dy: "bread_l" if dy < -0.2 else "bread_m")
        if not small:
            cv.line(5, 6, 6, 9, "bread_d"), cv.line(10, 6, 9, 9, "bread_d")
    cv.outline("out")
    return cv


def umbrella_open(frame):
    cv = Canvas(24, 16, IP)
    sway = frame
    for y in range(2, 10):
        for x in range(24):
            dx, dy = (x + 0.5 - 12 - sway) / 11.0, (y + 0.5 - 9) / 7.0
            if dx * dx + dy * dy <= 1.0 and y <= 9:
                cv.set(x, y, "umb_l" if (int((x - sway) / 4)) % 2 == 0 else "umb_d")
    for x in range(24):                     # the scalloped edge
        if cv.get(x, 9) and (x + sway) % 4 == 0:
            cv.set(x, 9, None)
    cv.set(12 + sway, 1, "needle")
    cv.vline(12 + sway, 9, 15, "handle")
    cv.outline("out")
    return cv


def glow(frame):
    cv = Canvas(16, 16, IP)
    r = 7.0 if frame == 0 else 6.2
    for y in range(16):
        for x in range(16):
            d = math.hypot(x + 0.5 - 8, y + 0.5 - 8)
            if abs(d - r) < 0.6 and (x + y + frame) % 2 == 0:
                cv.set(x, y, "glow" if frame == 0 else "glow_d")
    return cv


def baguette_plank(k):
    cv = Canvas(8, 8, IP)
    for x in range(8):
        cv.set(x, 0, "bread_l")
        for y in (1, 2):
            cv.set(x, y, "bread_m")
        cv.set(x, 3, "bread_d")
        if (x + 2) % 6 == 0:
            cv.set(x, 1, "bread_d")
    if k == 0:
        cv.set(0, 0, None), cv.set(0, 3, None)
    elif k == 2:
        cv.set(7, 0, None), cv.set(7, 3, None)
    return cv


# ==== effects =================================================================================================================
FP = S.OBJ_PALS["fx"]


def dust(frame):
    cv = Canvas(8, 8, FP)
    r = [1.8, 2.8, 3.4][frame]
    for y in range(8):
        for x in range(8):
            d = math.hypot(x + 0.5 - 4, y + 0.5 - 5)
            if d < r and not (frame == 2 and (x + y) % 2):
                cv.set(x, y, "dust_l" if y < 5 else "dust_d")
    return cv


def shard(frame, a="glass_l", b="glass_d"):
    cv = Canvas(8, 8, FP)
    if frame == 0:
        cv.line(1, 6, 5, 1, a), cv.line(2, 6, 5, 3, b)
    else:
        cv.line(2, 2, 6, 5, a), cv.set(1, 6, b), cv.set(6, 1, a)
    return cv


def wind(frame):
    cv = Canvas(16, 8, FP)
    for i, (y, x0, x1) in enumerate(((2, 1, 11), (5, 5, 15)) if frame == 0 else ((3, 3, 14), (6, 0, 8))):
        cv.hline(x0, x1, y, "wind_l")
        cv.set(x1, y, "wind_d")
    return cv


def splash(frame):
    cv = Canvas(16, 16, FP)
    h = [6, 11, 8][frame]
    for i, x in enumerate((3, 6, 9, 12)):
        hh = h - (2 if i in (0, 3) else 0)
        for y in range(15 - hh, 16):
            cv.set(x + (1 if y < 15 - hh // 2 and i < 2 else 0), y, "water_l" if y < 12 else "water_d")
    if frame > 0:
        for x, y in ((1, 16 - h), (14, 15 - h), (7, 13 - h)):
            cv.set(x, max(0, y), "white")
    return cv


def curse(frame):
    cv = Canvas(32, 16, FP)
    for y in range(12):
        for x in range(32):
            dx, dy = (x + 0.5 - 16) / 15.5, (y + 0.5 - 6) / 6.2
            if dx * dx + dy * dy <= 1.0:
                cv.set(x, y, "white")
    cv.line(8, 11, 5, 15, "white", 2)
    syms = ["#@$%!", "%!#@$"][frame]
    cols = ["curse_r", "out", "curse_y", "curse_r", "out"]
    for i, ch in enumerate(syms):
        g = FONT.get(ch, FONT[" "])
        for gy, row in enumerate(g[:7]):
            for gx, v in enumerate(row):
                if v == "#" and gx < 5:
                    cv.set(3 + i * 5 + gx * 4 // 5, 2 + gy, cols[(i + frame) % 5])
    cv.outline("out")
    return cv


def star(frame):
    cv = Canvas(8, 8, FP)
    r = 3 if frame == 0 else 2
    for i in range(-r, r + 1):
        cv.set(4 + i, 4, "star"), cv.set(4, 4 + i, "star")
    cv.set(4, 4, "white")
    if frame:
        for d in (-1, 1):
            cv.set(4 + d, 4 + d, "star"), cv.set(4 + d, 4 - d, "star")
    return cv


def small_glyph(k):
    cv = Canvas(8, 8, FP)
    ch = "0123456789+x"[k]
    g = FONT[ch] if ch != "x" else ["     ", "     ", "#   #", " # # ", "  #  ", " # # ", "#   #"]
    for y, row in enumerate(g[:7]):
        for x, v in enumerate(row):
            if v == "#":
                cv.set(1 + x, y, "star" if y < 3 else "white")
    cv.outline("out")
    return cv


# ==== UI ======================================================================================================================
UP = S.OBJ_PALS["ui"]


def medal(kind):
    """Cat-catch medals in the house tiers (bronze, silver, gold, pearl; the kit's colours): a round medal with a
    cat's face embossed, its whiskers; the last one ("the cat caught") in pearl with a gold rim."""
    cv = Canvas(24, 24, UP)
    dark, light = [("bd", "bl"), ("sd", "sl"), ("gd", "gl"), ("pd", "pl")][kind]
    cv.line(6, 0, 10, 7, dark, 2), cv.line(17, 0, 13, 7, dark, 2)          # the ribbon
    cv.ellipse(12, 14.5, 8.6, 8.6, None, lambda dx, dy: light if dx + dy < 0.35 else dark)
    if kind == 3:
        cv.ellipse(12, 14.5, 6.8, 6.8, "pearl")
    # the cat's face: ears, face, eyes, nose, whiskers
    face = dark if kind < 3 else "gl"
    cv.ellipse(12, 15.5, 4.6, 3.8, face)
    for sx in (-1, 1):
        cv.line(12 + sx * 4, 13, 12 + sx * 4, 10, face), cv.line(12 + sx * 3, 12, 12 + sx * 3, 11, face)
    cv.set(10, 15, "out"), cv.set(14, 15, "out"), cv.set(12, 17, "pd" if kind < 3 else "gd")
    for sx in (-1, 1):
        cv.hline(min(12 + sx * 5, 12 + sx * 8), max(12 + sx * 5, 12 + sx * 8), 17, "white")
        cv.set(12 + sx * 6, 18, "white")
    cv.set(7, 9, "white"), cv.set(8, 8, "white")                           # the shine, top-left
    cv.outline("out")
    return cv


def unit_m():
    """a lowercase m in the kit digits' style (2x, white, the bottom two rows blue-grey, a diagonal outline)"""
    cv = Canvas(16, 16, UP)
    g = ["     ", "     ", "## # ", "# # #", "# # #", "# # #", "# # #"]
    for y, row in enumerate(g):
        for x, c in enumerate(row):
            if c == "#":
                cv.rect(2 + x * 2, 1 + y * 2, 3 + x * 2, 2 + y * 2, "shade" if y >= 5 else "white")
    cv.outline("out", diag=True)
    return cv

def arrow():
    cv = Canvas(8, 8, UP)
    for y in range(5):
        cv.hline(4 - y, 3 + y, y + 1, "gl")
    cv.outline("out")
    return cv


def cafe(frame):
    CP = S.OBJ_PALS["cafe"]
    cv = Canvas(32, 16, CP)
    press = frame == 2
    top = 2 if press else 0
    for y in range(top, 10):
        for x in range(32):
            cv.set(x, y, "green" if (x // 4) % 2 == 0 else "cream")
    for x in range(32):
        cv.set(x, 10, "green_d" if (x // 4) % 2 == 0 else "cream_d")
        if x % 4 in (1, 2):
            cv.set(x, 11, "green_d" if (x // 4) % 2 == 0 else "cream_d")
    if frame == 0:
        text5(cv, "CAFE", 5, 3, "gold")
    cv.vline(0 if frame != 1 else 31, 10, 15, "out")
    cv.outline("out")
    return cv


def sprite_frame(name, i):
    if name == "mamie":
        return mamie(S.MAMIE_FRAMES[i])
    if name == "papi_head":
        return papi_head()
    if name == "umbrella":
        return umbrella_open(i)
    if name == "cat":
        return cat(i)
    if name == "pigeon":
        return pigeon(i)
    if name == "awning":
        return awning(i)
    if name == "pot":
        return pot()
    if name == "cradle":
        return cradle()
    if name == "ledge":
        return ledge(i)
    if name == "line":
        return line_seg(i)
    if name == "clothes":
        return clothes(i)
    if name == "antenna":
        return antenna(i)
    if name == "baguette":
        return baguette_plank(i)
    if name == "item":
        return item(i)
    if name == "icon":
        return item(i, small=True)
    if name == "glow":
        return glow(i)
    if name == "dust":
        return dust(i)
    if name == "shard":
        return shard(i)
    if name == "debris":
        return shard(i, "deb_l", "deb_d")
    if name == "wind":
        return wind(i)
    if name == "splash":
        return splash(i)
    if name == "curse":
        return curse(i)
    if name == "star":
        return star(i)
    if name == "small":
        return small_glyph(i)
    if name == "unit_m":
        return unit_m()
    if name == "medal":
        return medal(i)
    if name == "arrow":
        return arrow()
    if name == "cafe":
        return cafe(i)
    raise KeyError(name)


def sprites_sheet():
    lay, h = S.sprite_layout()
    im = Image.new("RGB", (S.SPRITE_SHEET_W, h), MAG)
    for s in S.SPRITES:
        for i, (x, y, w, hh) in enumerate(S.frame_rects(s.name)):
            cv = sprite_frame(s.name, i)
            assert (cv.w, cv.h) == (w, hh), s.name
            im.paste(cv.image(), (x, y))
    return im


# ==== BG2 tiles ===============================================================================================================
def tile(name):
    pal = dict(S.BG_PALS)[dict(S.BG_TILES)[name]]
    cv = Canvas(8, 8, pal)
    R = cv.rect
    n = name
    if n == "blank":
        return cv
    # ---- zinc roofs: a standing seam every 8 px, the top row light (the surface), the rest mid and dark
    if n in ("zinc_top", "zinc_top_end"):
        R(0, 0, 7, 0, "zinc_h")
        R(0, 1, 7, 5, "zinc_l")
        R(0, 6, 7, 7, "zinc_d")
        cv.vline(7, 1, 5, "zinc_m")
        if n == "zinc_top_end":            # the gutter at the end (drawn for the left end, h-flipped for the right)
            cv.vline(0, 1, 7, "zinc_d")
            cv.set(0, 0, "zinc_l")
    elif n == "mans_diag":                 # 45-degree slope rising to the right: the surface at row 7 - c
        for c in range(8):
            for y in range(7 - c, 8):
                cv.set(c, y, "zinc_h" if y == 7 - c else "zinc_l" if y - (7 - c) < 3 else "zinc_m")
    elif n in ("mans_face", "mans_face_b"):
        R(0, 0, 7, 7, "zinc_m")
        cv.vline(3, 0, 7, "zinc_l")
        cv.vline(7, 0, 7, "zinc_d")
        if n == "mans_face_b":
            R(0, 6, 7, 7, "zinc_d")
    elif n == "mans_dormer_t":
        R(0, 0, 7, 7, "zinc_m")
        R(1, 2, 6, 7, "stucco_l")
        cv.hline(0, 7, 2, "zinc_h")
        cv.hline(1, 6, 1, "zinc_l")
        R(2, 4, 5, 7, "glass_d")
        cv.set(2, 4, "glass_l")
    elif n == "mans_dormer_b":
        R(0, 0, 7, 7, "zinc_m")
        R(1, 0, 6, 5, "stucco_l")
        R(2, 0, 5, 4, "glass_d")
        cv.vline(3, 0, 4, "stucco_l")
        cv.hline(1, 6, 5, "stucco_d")
        R(0, 6, 7, 7, "zinc_d")
    # ---- terracotta pitched roofs (1:2 slopes)
    elif n in ("pitch_a", "pitch_b", "pitch_fill"):
        for c in range(8):
            s = {"pitch_a": 7 - c // 2, "pitch_b": 3 - c // 2, "pitch_fill": 0}[n]
            for y in range(s, 8):
                d = y - s
                col = "terra_l" if d == 0 else "terra_m" if (y + c // 2) % 3 else "terra_d"
                if n == "pitch_fill":
                    col = "terra_m" if (y + (c + y // 3 * 2) // 4) % 3 else "terra_d"
                    if y % 3 == 0:
                        col = "terra_d"
                cv.set(c, y, col)
    elif n == "pitch_eave":                # the underside of the eaves, over the wall
        R(0, 0, 7, 1, "terra_d")
        R(0, 2, 7, 2, "out")
    # ---- chimneys: two terracotta pots on top, a stucco stack
    elif n == "chim_top":
        R(0, 0, 7, 1, "terra_l")
        cv.hline(0, 7, 1, "terra_m")
        R(1, 2, 6, 5, "terra_m")
        cv.vline(1, 2, 5, "terra_l")
        cv.vline(6, 2, 5, "terra_d")
        R(0, 6, 7, 7, "stucco_d")
        cv.set(0, 0, "terra_m"), cv.set(7, 0, "terra_d")
    elif n == "chim_body":
        R(0, 0, 7, 7, "stucco_l")
        cv.vline(7, 0, 7, "stucco_d")
        cv.hline(0, 7, 3, "stucco_d") if False else None
        cv.set(2, 2, "stucco_d"), cv.set(5, 5, "stucco_d")
    elif n == "chim_base":
        R(0, 0, 7, 7, "stucco_l")
        cv.vline(7, 0, 7, "stucco_d")
        R(0, 6, 7, 7, "zinc_m")
    # ---- skylights and attics
    elif n == "sky_glass":
        R(0, 0, 7, 0, "zinc_d")
        R(0, 1, 7, 6, "glass_d")
        for i in range(4):
            cv.set(1 + i, 4 - i, "glass_l"), cv.set(2 + i, 4 - i, "glass_l")
        cv.vline(7, 1, 6, "zinc_d")
        R(0, 7, 7, 7, "zinc_d")
    elif n == "sky_broken":
        R(0, 0, 7, 0, "zinc_d")
        R(0, 1, 7, 7, "attic")
        cv.set(0, 1, "glass_l"), cv.set(1, 1, "glass_d"), cv.set(6, 1, "glass_l"), cv.set(7, 2, "glass_d")
        cv.vline(7, 1, 7, "zinc_d")
    elif n == "attic":
        R(0, 0, 7, 7, "attic")
        cv.line(0, 5, 7, 2, "attic_l")
    elif n == "attic_floor":
        R(0, 0, 7, 7, "attic_l")
        cv.hline(0, 7, 0, "stucco_l")
        cv.hline(0, 7, 4, "attic")
        cv.set(3, 1, "attic"), cv.set(3, 2, "attic"), cv.set(3, 3, "attic")
    elif n == "rope":
        cv.vline(3, 0, 7, "rope")
    elif n == "davit_arm":
        R(0, 5, 7, 6, "zinc_d")
        cv.hline(0, 7, 5, "zinc_l")
    elif n == "davit_end":
        R(0, 5, 7, 6, "zinc_d")
        cv.hline(0, 7, 5, "zinc_l")
        R(2, 6, 4, 7, "out")
        cv.set(3, 7, "rope")
    # ---- Haussmann stone
    elif n == "cornice":
        R(0, 0, 7, 7, "st_l")
        cv.hline(0, 7, 0, "st_h")
        R(0, 1, 7, 2, "st_m")
        cv.hline(0, 7, 3, "st_d")
        for x in range(0, 8, 2):
            cv.set(x, 2, "st_d")
    elif n in ("st_wall", "st_edge"):
        R(0, 0, 7, 7, "st_l")
        cv.hline(0, 7, 7, "st_m")
        cv.set(3, 3, "st_m")
        if n == "st_edge":                 # quoins: alternating blocks at the corner
            R(0, 0, 2, 7, "st_h")
            cv.hline(0, 2, 3, "st_m")
            cv.vline(3, 0, 7, "st_m")
    elif n == "st_band":
        R(0, 0, 7, 7, "st_l")
        R(0, 5, 7, 6, "st_h")
        cv.hline(0, 7, 7, "st_d")
    elif n == "st_win_t":
        R(0, 0, 7, 7, "st_l")
        R(1, 1, 6, 7, "frame")
        R(2, 3, 5, 7, "glass_d")
        cv.hline(1, 6, 0, "st_d")
        cv.set(2, 3, "glass_l"), cv.set(3, 4, "glass_l")
    elif n == "st_win_b":
        R(0, 0, 7, 7, "st_l")
        R(1, 0, 6, 6, "frame")
        R(2, 0, 5, 5, "glass_d")
        cv.vline(3, 0, 5, "frame")
        cv.hline(1, 6, 7, "st_d")
        cv.set(2, 1, "glass_l")
    elif n in ("st_balc", "st_balc_wall"):
        R(0, 0, 7, 7, "st_l")
        if n == "st_balc":
            R(1, 0, 6, 3, "frame")
            R(2, 0, 5, 3, "glass_d")
        cv.hline(0, 7, 3, "iron")
        cv.hline(0, 7, 7, "iron")
        for x in range(0, 8, 2):
            cv.vline(x, 4, 6, "iron")
        cv.set(1, 5, "iron"), cv.set(5, 5, "iron")
    elif n in ("shop_t", "shop_b"):
        R(0, 0, 7, 7, "shop")
        if n == "shop_t":
            cv.hline(0, 7, 0, "shop_d")
            R(0, 2, 7, 5, "shop_d")
            cv.hline(1, 6, 3, "shop_gold")
            cv.set(2, 4, "shop_gold"), cv.set(5, 2, "shop_gold")
        else:
            R(1, 0, 6, 7, "glass_d")
            cv.set(2, 1, "glass_l"), cv.set(3, 2, "glass_l")
            cv.vline(7, 0, 7, "shop_d")
    elif n in ("st_door_t", "st_door_b"):
        R(0, 0, 7, 7, "st_l")
        R(1, 0 if n == "st_door_b" else 2, 6, 7, "door")
        cv.vline(4, 0 if n == "st_door_b" else 2, 7, "out")
        if n == "st_door_t":
            cv.hline(1, 6, 1, "st_d")
    # ---- Montmartre houses
    elif n == "h_eave":
        R(0, 0, 7, 7, "wash_l")
        R(0, 0, 7, 1, "out")
        cv.hline(0, 7, 2, "wash_d")
    elif n in ("h_wall", "h_edge"):
        R(0, 0, 7, 7, "wash_l")
        cv.set(2, 5, "wash_d"), cv.set(6, 2, "wash_h")
        if n == "h_edge":
            cv.vline(0, 0, 7, "wash_h")
            cv.vline(1, 0, 7, "wash_d")
    elif n == "h_win_t":
        R(0, 0, 7, 7, "wash_l")
        R(0, 2, 1, 7, "shut_l"), R(6, 2, 7, 7, "shut_d")
        R(2, 2, 5, 7, "glass_d")
        cv.hline(0, 7, 1, "wash_d")
        cv.set(2, 3, "glass_l")
        cv.set(0, 4, "shut_d"), cv.set(7, 4, "out")
    elif n == "h_win_b":
        R(0, 0, 7, 7, "wash_l")
        R(0, 0, 1, 4, "shut_l"), R(6, 0, 7, 4, "shut_d")
        R(2, 0, 5, 4, "glass_d")
        cv.hline(1, 6, 5, "wash_d")
        cv.set(3, 6, "shut_l"), cv.set(4, 6, "shut_d")      # a flower on the sill
    elif n in ("h_door_t", "h_door_b"):
        R(0, 0, 7, 7, "wash_l")
        R(1, 2 if n == "h_door_t" else 0, 6, 7, "shut_d")
        cv.vline(3, 2 if n == "h_door_t" else 0, 7, "shut_l")
        if n == "h_door_t":
            cv.hline(1, 6, 1, "wash_d")
    elif n == "h_base":
        R(0, 0, 7, 7, "wash_d")
        cv.hline(0, 7, 0, "wash_l")
    # ---- quays, bridges, the bouquinistes
    elif n in ("q_top", "br_top"):
        R(0, 0, 7, 7, "quay_m")
        cv.hline(0, 7, 0, "quay_l")
        R(0, 1, 7, 2, "quay_l")
        cv.hline(0, 7, 3, "quay_d")
        if n == "br_top":                  # a balustrade under the rail
            R(0, 4, 7, 7, "quay_l")
            for x in (1, 5):
                R(x, 4, x + 1, 7, "quay_d")
    elif n in ("q_wall", "q_ring", "br_wall"):
        R(0, 0, 7, 7, "quay_m")
        cv.hline(0, 7, 7, "quay_d")
        cv.vline(0 if n != "br_wall" else 4, 0, 3, "quay_d")
        cv.vline(4 if n != "br_wall" else 0, 4, 7, "quay_d")
        cv.set(1, 1, "quay_l")
        if n == "q_ring":
            cv.ellipse(4, 4, 2, 2, "out")
            cv.ellipse(4, 4, 1, 1, "quay_m")
    elif n == "br_band":
        R(0, 0, 7, 7, "quay_l")
        cv.hline(0, 7, 6, "quay_d")
        cv.hline(0, 7, 7, "quay_m")
    elif n == "br_arch_c":                 # the top of an arch (24 wide): the curve over the middle tile
        R(0, 0, 7, 7, "quay_m")
        for x in range(8):
            y0 = 5 + (1 if x in (0, 7) else 0)
            for y in range(y0, 8):
                cv.set(x, y, None)
            cv.set(x, y0 - 1, "quay_d")
    elif n == "br_arch_s":                 # the side of an arch: stone on the left, open on the right (h-flip)
        R(0, 0, 7, 7, "quay_m")
        for y in range(8):
            edge = 3 + (7 - y) // 3
            for x in range(edge, 8):
                cv.set(x, y, None)
            cv.set(edge - 1, y, "quay_d")
    elif n == "br_lamp":
        cv.vline(3, 2, 7, "out")
        R(2, 0, 4, 1, "lamp")
    elif n in ("box_top", "box_top_end"):  # the bouquinistes' green boxes (lid)
        R(0, 0, 7, 7, "box_l")
        cv.hline(0, 7, 0, "box_l")
        R(0, 3, 7, 3, "box_d")
        cv.hline(0, 7, 7, "box_d")
        if n == "box_top_end":
            cv.vline(0, 0, 7, "box_d")
    elif n in ("box_body", "box_body_end"):
        R(0, 0, 7, 7, "box_d")
        R(1, 1, 6, 6, "box_l")
        cv.set(4, 3, "lamp")
        if n == "box_body_end":
            cv.vline(0, 0, 7, "out")
    # ---- ground
    elif n == "walk":
        R(0, 0, 7, 7, "walk_l")
        cv.hline(0, 7, 0, "curb")
        cv.vline(7, 1, 7, "walk_d")
        cv.hline(0, 7, 7, "walk_d")
    elif n == "curb":
        R(0, 0, 7, 7, "road_l")
        R(0, 0, 7, 2, "curb")
        cv.hline(0, 7, 2, "walk_d")
    elif n in ("road", "road2"):
        R(0, 0, 7, 7, "road_l")
        for x in range(8):
            for y in range(8):
                if (x + y * 3 + (4 if n == "road2" else 0)) % 7 == 0:
                    cv.set(x, y, "road_d")
    elif n == "water_top":
        R(0, 0, 7, 7, "water_m")
        cv.hline(0, 7, 0, "water_l")
        cv.set(2, 0, "foam"), cv.set(3, 0, "foam"), cv.set(6, 2, "water_l")
    elif n in ("water", "water2"):
        R(0, 0, 7, 7, "water_d" if n == "water2" else "water_m")
        cv.set(1 + (4 if n == "water2" else 0), 3, "water_l")
        cv.hline(4, 6, 6, "water_d" if n == "water" else "water_m")
    elif n in ("deck", "deck_end"):
        R(0, 0, 7, 7, "hull_l")
        R(0, 0, 7, 2, "deck")
        cv.hline(0, 7, 2, "deck_d")
        cv.set(3, 1, "deck_d")
        R(0, 6, 7, 7, "hull_d")
        if n == "deck_end":
            cv.vline(0, 0, 7, "out")
    elif n in ("hull", "hull_water"):
        R(0, 0, 7, 7, "hull_d")
        cv.set(2, 2, "hull_l")
        if n == "hull_water":
            R(0, 5, 7, 7, "water_m")
            cv.hline(0, 7, 5, "foam")
    elif n in ("cabin_top", "cabin_top_end"):
        R(0, 0, 7, 7, "cabin")
        cv.hline(0, 7, 0, "deck")
        cv.hline(0, 7, 1, "deck_d")
        if n == "cabin_top_end":
            cv.vline(0, 0, 7, "deck_d")
    elif n in ("cabin_win", "cabin_end"):
        R(0, 0, 7, 7, "cabin")
        if n == "cabin_win":
            R(2, 1, 5, 4, "water_d")
            cv.set(2, 1, "water_l")
        else:
            cv.vline(0, 0, 7, "deck_d")
    else:
        raise KeyError(n)
    return cv


def bg2_sheet():
    n = len(S.BG_TILES)
    im = Image.new("RGB", (128, ((n + 15) // 16) * 8), MAG)
    for i, (name, _) in enumerate(S.BG_TILES):
        im.paste(tile(name).image(), ((i % 16) * 8, (i // 16) * 8))
    return im


# ==== panoramas ================================================================================================================
def midground():
    """BG3: distant Paris roofs (mansards, chimney pots, a dome, a spire), two rows, repeating every 512 px."""
    cv = Canvas(S.MID_W, S.MID_H, S.BG_PALS["mid"])
    rng = random.Random(11)
    for row, (base, tone, win) in enumerate(((76, "m1", 0.06), (104, "m3", 0.10))):
        x = 0
        while x < S.MID_W:
            w = rng.choice((32, 40, 48, 56, 64))
            h = rng.randint(14, 34) + row * 6
            top = base - h
            mans = rng.random() < 0.7
            for xx in range(x, min(S.MID_W, x + w)):
                k = xx - x
                t = top + (max(0, 6 - k) if mans else 0) + (max(0, k - (w - 7)) if mans else 0)
                for y in range(t, S.MID_H):
                    cv.set(xx, y, tone)
                if mans and 6 <= k < w - 6:
                    cv.set(xx, top, "m1" if row else "m2")
            for cx in range(x + 8, x + w - 8, rng.choice((12, 16, 20))):       # chimney pots
                if rng.random() < 0.6:
                    ch = rng.randint(3, 6)
                    cv.rect(cx, top - ch, cx + 2, top - 1, tone)
            for wy in range(top + 8, S.MID_H - 4, 7):                          # windows
                for wx in range(x + 4, x + w - 4, 6):
                    if rng.random() < win:
                        cv.rect(wx, wy, wx + 1, wy + 2, "mwin")
            x += w
        if row == 0:                            # a dome and a spire on the far row
            for cx, kind in ((140, "dome"), (380, "spire")):
                if kind == "dome":
                    cv.ellipse(cx, base - 36, 12, 12, "m2")
                    cv.rect(cx - 12, base - 36, cx + 12, base, "m2")
                    cv.rect(cx - 1, base - 54, cx + 1, base - 46, "m2")
                else:
                    for y in range(base - 70, base):
                        half = max(0, (y - (base - 70)) // 10)
                        cv.hline(cx - half, cx + half, y, "m2")
    # the front row's lower part fades into the darkest tone
    for y in range(S.MID_H - 12, S.MID_H):
        for x in range(S.MID_W):
            if cv.get(x, y) is not None:
                cv.set(x, y, "mwin" if cv.is_(x, y, "mwin") else "m4")
    return cv


def base_skyline(cv, seed_tone="f3"):
    """The low far roofline shared by every district (periodic over 512 px, so the panoramas join)."""
    for x in range(S.FAR_W):
        h = 18 + int(6 * math.sin(x * 2 * math.pi / 128) + 4 * math.sin(x * 2 * math.pi / 64 + 1.3) +
                     3 * math.sin(x * 2 * math.pi / 32 + 0.4))
        if (x // 8) % 5 == 2:
            h += 4
        for y in range(S.FAR_H - h, S.FAR_H):
            cv.set(x, y, seed_tone)
        if x % 16 == 5:
            cv.rect(x, S.FAR_H - h - 3, x + 1, S.FAR_H - h - 1, seed_tone)


def far(district):
    cv = Canvas(S.FAR_W, S.FAR_H, S.BG_PALS["far"])
    H = S.FAR_H
    if district == "montmartre":
        # the butte, trees, Sacre-Coeur's white domes and its campanile
        for x in range(60, 460):
            d = (x - 260) / 200.0
            top = int(H - 78 * (1 - d * d) ** 0.8) if abs(d) < 1 else H
            for y in range(top, H):
                cv.set(x, y, "f2")
            if abs(d) < 1 and x % 7 == 0:
                cv.ellipse(x, top + 1, 4, 3, "f3")
        cx, base = 260, H - 76
        for dx, r, hh in ((-26, 7, 18), (26, 7, 18), (-14, 9, 26), (14, 9, 26), (0, 14, 40)):
            x0 = cx + dx
            cv.rect(x0 - r, base - hh + r, x0 + r, base, "f5")
            cv.ellipse(x0, base - hh + r, r, r * 1.3, None, lambda a, b: "f5" if a < 0.3 else "f1")
            cv.rect(x0 - 1, base - hh - r - 3, x0, base - hh + 1 - r, "f5")
        cv.rect(cx + 30, base - 58, cx + 38, base, "f1")          # the campanile
        cv.ellipse(cx + 34, base - 58, 5, 6, "f1")
        cv.rect(cx + 34, base - 70, cx + 34, base - 62, "f1")
        cv.rect(cx - 30, base - 8, cx + 30, base, "f5")
        for x in range(cx - 28, cx + 29, 6):
            cv.rect(x, base - 6, x + 1, base - 3, "f4")
    elif district == "seine":
        # Notre-Dame: two towers, the rose window, the spire; bridges on the river
        cx, base = 250, H - 26
        for dx in (-26, 6):
            cv.rect(cx + dx, base - 86, cx + dx + 20, base, "f3")
            for y in range(base - 80, base - 30, 12):
                cv.rect(cx + dx + 5, y, cx + dx + 7, y + 7, "f4"), cv.rect(cx + dx + 13, y, cx + dx + 15, y + 7, "f4")
            cv.hline(cx + dx, cx + dx + 20, base - 86, "f2")
        cv.rect(cx - 6, base - 60, cx + 6, base, "f3")
        cv.ellipse(cx, base - 48, 5, 5, "f5")
        cv.ellipse(cx, base - 48, 2, 2, "f2")
        cv.rect(cx + 26, base - 50, cx + 110, base, "f3")          # the nave
        for y in range(base - 132, base - 50):                      # the spire
            half = (y - (base - 132)) // 16
            cv.hline(cx + 68 - half, cx + 68 + half, y, "f3")
        for x in range(cx + 30, cx + 108, 10):                      # flying buttresses
            cv.line(x, base - 30, x + 6, base - 50, "f2")
        for x0, x1 in ((0, 150), (380, 512)):                        # bridges with arches
            for x in range(x0, x1):
                for y in range(H - 30, H - 22):
                    cv.set(x, y, "f4")
                k = (x - x0) % 40
                if 6 < k < 34:
                    a = (k - 20) / 14.0
                    top = int(H - 22 + (1 - a * a) ** 0.5 * -0 + 8 * a * a)
                    for y in range(H - 22, top + 2):
                        cv.set(x, y, "f4")
                else:
                    for y in range(H - 22, H):
                        cv.set(x, y, "f4")
        for x in range(0, S.FAR_W, 9):                              # trees along the quays
            if not (150 < x < 380):
                cv.ellipse(x, H - 36, 5, 4, "f3")
    elif district == "haussmann":
        # the Opera Garnier, the Arc de Triomphe, and a small Eiffel Tower far away (it grows)
        cx, base = 170, H - 24
        cv.rect(cx - 50, base - 40, cx + 50, base, "f3")
        for x in range(cx - 46, cx + 47, 8):
            cv.rect(x, base - 34, x + 2, base - 8, "f2")
        cv.ellipse(cx, base - 44, 22, 14, None, lambda a, b: "f1" if a < 0 else "f2")
        cv.rect(cx - 22, base - 44, cx + 22, base - 40, "f2")
        cv.rect(cx - 1, base - 64, cx + 1, base - 58, "f5")
        for sx in (-48, 48):
            cv.rect(cx + sx - 2, base - 50, cx + sx + 2, base - 40, "f5")
        ax, ab = 350, H - 24                                         # the Arc de Triomphe
        cv.rect(ax - 26, ab - 62, ax + 26, ab, "f2")
        cv.hline(ax - 28, ax + 28, ab - 62, "f1")
        cv.rect(ax - 28, ab - 60, ax + 28, ab - 54, "f2")
        for x in range(ax - 10, ax + 11):
            a = (x - ax) / 10.0
            for y in range(int(ab - 30 - 10 * (1 - a * a) ** 0.5), ab):
                cv.set(x, y, None)
        eiffel(cv, 470, H - 24, 62)
    else:
        eiffel(cv, 256, H, 150)
        for x in range(150, 360):                                     # the Champ-de-Mars trees
            if x % 6 == 0 and abs(x - 256) > 40:
                cv.ellipse(x, H - 8, 4, 4, "f3")
    base_skyline(cv, "f3" if district != "eiffel" else "f4")
    return cv


def eiffel(cv, cx, base, h):
    """The Eiffel Tower, h px tall: four legs meeting at the first platform, the lattice (every other pixel),
    the platforms, the top; the lights ('flight') blink at night (the C code cycles that colour)."""
    top = base - h
    for y in range(top, base):
        t = (y - top) / float(h)
        half = 1 + int((t ** 2.2) * h * 0.34)
        for x in range(cx - half, cx + half + 1):
            edge = abs(x - cx) >= half - max(1, half // 6)
            inner = t > 0.62 and abs(x - cx) < half * (t - 0.62) * 2.3 and y > base - h * 0.22
            if inner:
                continue
            if edge or (x + y) % 2 == 0:
                cv.set(x, y, "f3" if edge else "f4")
    for tt in (0.47, 0.72):
        y = int(top + h * tt)
        half = 1 + int((tt ** 2.2) * h * 0.34) + 2
        cv.hline(cx - half, cx + half, y, "f3")
        cv.hline(cx - half, cx + half, y + 1, "f4")
        for x in range(cx - half, cx + half + 1, 4):
            cv.set(x, y - 1, "flight")
    cv.rect(cx - 1, top - 6, cx, top, "f3")
    cv.set(cx, top - 7, "flight")
    for y in range(top + 6, base - 4, 9):
        cv.set(cx, y, "flight")


def write_all(out, preview=False):
    os.makedirs(os.path.join(out, "tiles"), exist_ok=True)
    written = []

    def save(im, f):
        im.save(os.path.join(out, f))
        written.append(f)

    save(sprites_sheet(), "sprites.png")
    save(bg2_sheet(), "tiles/bg2.png")
    save(midground().image(), "tiles/mid.png")
    for d in S.DISTRICTS:
        save(far(d).image(), "tiles/far_%s.png" % d)
    if preview:
        pdir = os.path.join(out, "preview")
        os.makedirs(pdir, exist_ok=True)
        for f in written:
            im = Image.open(os.path.join(out, f))
            im.resize((im.width * 4, im.height * 4), Image.NEAREST).save(os.path.join(pdir, os.path.basename(f)))
    return written


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", default=os.path.join(GAME, "art"))
    ap.add_argument("--preview", action="store_true", help="also write 4x previews in <out>/preview")
    a = ap.parse_args(argv)
    for f in write_all(a.out, a.preview):
        print("wrote %s/%s" % (a.out, f))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Caterpillar Crawl: the code-drawn art, in the 8BCraft house style (docs/art-direction.md).

Code-drawn art only, no image generation: every sprite and panorama is drawn here with
games/common/tools/house_style.py (1-px outlines, 3-4-shade ramps lit from the top-left, squash and stretch).
The sheet layout (SPRITES) is shared with build_assets.py.

The caterpillar: a round green head with a big happy face, two antennae with golden tips, rosy cheeks; its body a row of
round segments (every other one with two golden spots), little feet, a pointed tail; a swallowed leaf makes a fat
segment. Its head is a sprite (it slides from cell to cell); the body's segments are BG tiles drawn from the same
sprite tiles (build_assets.py: BG2 shares the sprites' VRAM, one BG palette per player).

Writes, in the art folder (default games/caterpillarcrawl/art):
  sprites.png (256x96), tiles/garden.png (576x240: the garden, stage x -128..447), tiles/obstacles.png (64x16)

    python3 games/caterpillarcrawl/tools/make_art.py [--out DIR] [--preview]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/caterpillarcrawl/LICENSE.
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
sys.path.insert(0, os.path.join(ROOT, "games", "common", "tools"))
import house_style as hs  # noqa: E402

Canvas = hs.Canvas
SHEET_W, SHEET_H = 256, 96
# name, x, y, w, h, frames, palette group (hero: OBJ 0, players 2-4 = OBJ 1, 4, 5 recoloured, and the BG palettes 1-4
# of the bodies; props: OBJ 2; small: OBJ 6)
SPRITES = [
    ("blank", 240, 0, 8, 8, 1, "hero"),         # OBJ tile 0 is blank: BG2's empty map entries (its base is the OBJ's)
    ("head", 0, 0, 24, 24, 8, "hero"),          # look right, down, up; chomp right, down, up; bonk; happy
    ("butterfly", 192, 0, 24, 24, 2, "hero"),   # wings open, closed
    ("seg", 0, 24, 16, 16, 5, "hero"),          # ball, ball with spots, fat (a leaf inside), tail right, tail down
    ("icon", 80, 24, 16, 16, 1, "hero"),        # the title's player slots, the results, the arena's HUD
    ("cocoon", 96, 24, 16, 16, 2, "hero"),
    ("body0", 0, 64, 16, 16, 6, "hero"),        # BG2's joined pieces (JOINS): a segment, a spotted one, a fat one
    ("body1", 96, 64, 16, 16, 6, "hero"),
    ("body2", 0, 80, 16, 16, 6, "hero"),
    ("bodytail", 96, 80, 16, 16, 2, "hero"),    # the tail joined on its left (pointing right), on top (pointing down)
    ("leaf", 128, 24, 16, 16, 2, "props"),
    ("flower", 160, 24, 16, 16, 2, "props"),
    ("puff", 192, 24, 16, 16, 3, "props"),
    ("berry", 0, 40, 16, 16, 2, "small"),
    ("leafpip", 0, 56, 8, 8, 2, "props"),       # the garden's leaves: eaten, to go
    ("starpip", 16, 56, 8, 8, 2, "small"),      # the arena's round wins: won, to win
    ("digit", 32, 56, 8, 8, 11, "small"),       # 0-9 and + (the points of a flower, a butterfly)
]
PANORAMAS = {"garden": ("tiles/garden.png", 576, 240)}
OBSTACLES = ("tiles/obstacles.png", 64, 16)     # rock, bush, flower pot, mushroom (16x16 each, BG3)
PANO_X0 = -128                                  # the garden's stage x of its left edge
GRID_X, GRID_Y, GRID_W, GRID_H, CELL = 16, 40, 18, 12, 16

# the caterpillar: a fresh green (the players' hue swaps turn it), golden antenna tips and spots, rosy cheeks
BODY = [(214, 244, 140), (150, 214, 84), (90, 168, 58), (48, 112, 52)]
OUT = (30, 34, 56)              # the outline: navy, never shifted by the swaps (low saturation)
GOLD = [(255, 230, 120), (232, 172, 40)]
CHEEK = (246, 140, 150)
MOUTH = (150, 40, 64)
WHITE = hs.HOUSE["white"][0]
INK = hs.HOUSE["ink"][0]
LEAF = hs.ramp("leaf")
PINK = [(255, 214, 230), (246, 150, 190), (206, 84, 140)]
BERRY = [(176, 190, 255), (104, 110, 222), (60, 52, 150)]
NAVY = (22, 18, 40)
CREAM = (255, 246, 220)


def _px(cv, pts, c):
    for x, y in pts:
        cv.set(x, y, c)


# ---- the caterpillar ---------------------------------------------------------------------------------------------
def head(frame):
    """24x24, the face looking where it crawls. 0 right, 1 down, 2 up, 3-5 chomping (mouth open), 6 bonk, 7 happy."""
    cv = Canvas(24, 24)
    look = [(1, 0), (0, 1), (0, -1)][frame % 3] if frame < 6 else (1, 0)
    lx, ly = look
    cx, cy = 12.0 + lx * 0.5, 13.5
    # antennae (behind the head), the tips golden
    for sx in (-1, 1):
        ax, ay = cx + sx * 3.5 - lx * 1.5, 6.0
        tx, ty = cx + sx * 6.0 - lx * 2.0, 1.5
        for i in range(8):
            t = i / 7.0
            cv.set(int(ax + (tx - ax) * t), int(ay + (ty - ay) * t), BODY[3])
        hs.ellipse(cv, tx + 0.5, ty + 0.5, 1.6, 1.6, GOLD + GOLD[1:] + GOLD[1:])
    hs.ellipse(cv, cx, cy, 9.0, 8.5, BODY)
    cv.set(int(cx) - 5, int(cy) - 5, WHITE)                 # the shine
    ex = int(cx) - 5 + lx * 2
    ey = int(cy) - 4 + ly * 2
    if frame == 6:                                          # bonk: X eyes, a wobbly mouth
        for e in (ex, ex + 6):
            for d in range(3):
                cv.set(e + d, ey + d, INK), cv.set(e + 2 - d, ey + d, INK)
        _px(cv, [(ex + 1, ey + 6), (ex + 2, ey + 5), (ex + 3, ey + 6), (ex + 4, ey + 5), (ex + 5, ey + 6)], MOUTH)
    elif frame == 7:                                        # happy: closed eyes ^ ^, a big smile
        for e in (ex, ex + 6):
            _px(cv, [(e, ey + 2), (e + 1, ey + 1), (e + 2, ey + 2)], INK)
        _px(cv, [(ex + 1, ey + 5), (ex + 2, ey + 6), (ex + 3, ey + 6), (ex + 4, ey + 6), (ex + 5, ey + 6),
                 (ex + 6, ey + 5)], MOUTH)
        cv.set(ex + 3, ey + 7, CHEEK), cv.set(ex + 4, ey + 7, CHEEK)
    else:
        hs.eye(cv, ex, ey, look=(lx if lx else 1, ly), big=True)
        hs.eye(cv, ex + 5, ey, look=(lx if lx else -1, ly), big=True)
        my = ey + 6
        if frame >= 3:                                      # chomp: the mouth wide open, a pink tongue
            cv.rect(ex + 2, my, ex + 5, my + 2, MOUTH)
            cv.set(ex + 3, my + 2, CHEEK), cv.set(ex + 4, my + 2, CHEEK)
        else:
            _px(cv, [(ex + 2, my), (ex + 3, my + 1), (ex + 4, my + 1), (ex + 5, my)], MOUTH)
    # cheeks
    for chx in (ex - 2, ex + 8):
        if frame != 6:
            cv.set(chx, ey + 5, CHEEK), cv.set(chx + 1, ey + 5, CHEEK)
    cv.outline(OUT)
    return cv


def ball(cv, cx, cy, rx, ry, spots=False, feet=True):
    if feet:
        for fx in (cx - 3.5, cx + 2.5):
            cv.rect(int(fx), int(cy + ry) - 1, int(fx) + 1, int(cy + ry), BODY[3])
    hs.ellipse(cv, cx, cy, rx, ry, BODY)
    cv.set(int(cx - rx * 0.5), int(cy - ry * 0.55), WHITE)
    if spots:
        _px(cv, [(int(cx) - 2, int(cy) - 3), (int(cx) + 2, int(cy) - 2)], GOLD[0])
        _px(cv, [(int(cx) - 2, int(cy) - 2), (int(cx) + 2, int(cy) - 1)], GOLD[1])


def segment(frame):
    """16x16 body pieces: 0 a segment, 1 one with golden spots, 2 fat (a leaf going down), 3 the tail pointing right,
    4 the tail pointing down."""
    cv = Canvas(16, 16)
    if frame == 0:
        ball(cv, 8.0, 7.5, 6.5, 6.0)
    elif frame == 1:
        ball(cv, 8.0, 7.5, 6.5, 6.0, spots=True)
    elif frame == 2:
        ball(cv, 8.0, 7.5, 7.4, 6.9, spots=True)
        _px(cv, [(6, 8), (7, 8), (8, 9), (9, 9)], BODY[0])
    elif frame == 3:
        hs.capsule(cv, 5.0, 8.0, 12.0, 8.5, 2.0, BODY)
        ball(cv, 5.5, 7.5, 5.0, 5.0)
    else:
        hs.capsule(cv, 8.0, 5.0, 8.5, 12.5, 2.0, BODY)
        ball(cv, 8.0, 5.5, 5.0, 4.6, feet=False)
    cv.outline(OUT)
    return cv


# the body on BG2: each piece joined to its neighbours by a neck reaching the cell's edge (the neighbour draws the
# other half), so straight runs and turns read as one caterpillar. Masks: R 1, D 2, L 4, U 8.
JOINS = [5, 10, 3, 6, 12, 9]                    # the 6 frames of each body piece: L-R, U-D, R-D, D-L, L-U, U-R


def neck(cv, mask):
    """The necks of a body piece (behind the ball): 5 rows (horizontal) or 6 columns (vertical) wide, lit from the
    top-left, from the centre to each joined edge."""
    for bit, (x0, y0, x1, y1) in ((1, (8, 5, 15, 9)), (4, (0, 5, 7, 9)), (2, (5, 8, 10, 15)), (8, (5, 0, 10, 7))):
        if not mask & bit:
            continue
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                k = y - 5 if bit in (1, 4) else (x - 5) * 5 // 6
                cv.set(x, y, BODY[1] if k < 2 else BODY[2] if k < 4 else BODY[3])


def body_piece(kind, mask):
    """16x16 joined body pieces for BG2: kind 0 a segment, 1 one with golden spots, 2 fat (a leaf going down),
    3 the tail pointing right (joined on its left), 4 the tail pointing down (joined on top)."""
    cv = Canvas(16, 16)
    b = Canvas(16, 16)
    feet = not mask & 2
    if kind == 0:
        ball(b, 8.0, 7.5, 6.5, 6.0, feet=feet)
    elif kind == 1:
        ball(b, 8.0, 7.5, 6.5, 6.0, spots=True, feet=feet)
    elif kind == 2:
        ball(b, 8.0, 7.5, 7.4, 6.9, spots=True, feet=False)
        _px(b, [(6, 8), (7, 8), (8, 9), (9, 9)], BODY[0])
    elif kind == 3:
        hs.capsule(b, 5.0, 8.0, 12.0, 8.5, 2.0, BODY)
        ball(b, 5.5, 7.5, 5.0, 5.0)
    else:
        hs.capsule(b, 8.0, 5.0, 8.5, 12.5, 2.0, BODY)
        ball(b, 8.0, 5.5, 5.0, 4.6, feet=False)
    neck(cv, mask)
    cv.paste(b, 0, 0)
    # a soft crease where two pieces meet (drawn by the piece on the left or above)
    for x0, y0, x1, y1, bit in ((15, 0, 15, 15, 1), (0, 15, 15, 15, 2)):
        if mask & bit:
            for y in range(y0, y1 + 1):
                for x in range(x0, x1 + 1):
                    if cv.get(x, y) is not None:
                        cv.set(x, y, BODY[3])
    cv.outline(OUT)
    return cv


def body(kind):
    return lambda i: body_piece(kind, JOINS[i] if kind < 3 else (4 if kind == 3 else 8))


def icon():
    cv = Canvas(16, 16)
    for sx in (-1, 1):
        for i in range(4):
            cv.set(8 + sx * (2 + i // 2) - (1 if sx < 0 else 0), 4 - i, BODY[3])
        cv.set(8 + sx * 4 - (1 if sx < 0 else 0), 0, GOLD[0])
    hs.ellipse(cv, 8.0, 9.0, 6.5, 6.0, BODY)
    cv.set(4, 6, WHITE)
    hs.eye(cv, 5, 7, look=(1, 0))
    hs.eye(cv, 9, 7, look=(1, 0))
    _px(cv, [(6, 11), (7, 12), (8, 12), (9, 11)], MOUTH)
    cv.set(4, 10, CHEEK), cv.set(11, 10, CHEEK)
    cv.outline(OUT)
    return cv


def cocoon(frame):
    cv = Canvas(16, 16)
    hs.ellipse(cv, 8.0, 8.5, 5.0, 7.0, [WHITE, WHITE, BODY[0], BODY[1]])
    for y in range(3, 15, 3):                               # the silk's threads
        for x in range(4, 12):
            if cv.get(x, y) is not None and (x + 2 * y) % 5 == 0:
                cv.set(x, y, BODY[0])
    _px(cv, [(8, 0), (8, 1)], BODY[2])                      # the thread it hangs from
    cv.set(6, 4, WHITE)
    if frame:
        cv = hs.squash(cv, 1.12, 0.92)
    cv.outline(OUT)
    return cv


def butterfly(frame):
    """24x24: 0 the wings open, 1 half closed. The right half is drawn, then mirrored (x <-> 23 - x)."""
    cv = Canvas(24, 24)
    k = 1.0 if frame == 0 else 0.55

    def wing(cx, cy, dx_, dy_, a, b, ramp):                 # an ellipse along (dx_, dy_), squeezed by k across
        for y in range(24):
            for x in range(12, 24):
                u, v = (x + 0.5 - cx) / k, y + 0.5 - cy
                p, q = u * dx_ + v * dy_, -u * dy_ + v * dx_
                if (p / a) ** 2 + (q / b) ** 2 <= 1.0:
                    cv.set(x, y, hs.shade_nd(-(u * 0.45 + v * 0.75) / max(a, b) + 0.2, ramp))

    dark, pale = [BODY[2], BODY[2], BODY[3], BODY[3]], [BODY[0], BODY[0], BODY[1], BODY[1]]
    # the big wing up and out, the small one down and out: a dark rim outside, the pale wing within
    wing(12 + 5.8 * k, 8.0, 0.78, -0.62, 6.8, 5.0, dark)
    wing(12 + 4.2 * k, 15.8, 0.62, 0.78, 5.0, 3.4, dark)
    wing(12 + 5.0 * k, 8.8, 0.78, -0.62, 5.4, 3.7, pale)
    wing(12 + 3.6 * k, 15.2, 0.62, 0.78, 3.8, 2.3, pale)
    ex = int(12 + 6.6 * k)                                  # the golden eye spots
    if frame == 0:
        cv.rect(ex, 7, ex + 1, 8, GOLD[1])
        cv.set(ex, 7, GOLD[0])
    else:
        cv.set(ex, 8, GOLD[0])
    cv.set(int(12 + 4.4 * k), 16, GOLD[0])
    cv.set(int(12 + 4.0 * k), 6, WHITE)
    for y in range(24):
        for x in range(12, 24):
            if cv.get(x, y) is not None:
                cv.set(23 - x, y, cv.get(x, y))
    hs.capsule(cv, 12.0, 6.0, 12.0, 19.0, 1.4, [OUT, OUT, OUT, OUT])
    _px(cv, [(11, 4), (10, 3), (9, 2), (13, 4), (14, 3), (15, 2)], OUT)
    cv.set(9, 1, GOLD[0]), cv.set(15, 1, GOLD[0])
    cv.outline(OUT)
    return cv


# ---- props ---------------------------------------------------------------------------------------------------------
def leaf(frame):
    """A fresh leaf, its tip swaying (two frames)."""
    cv = Canvas(16, 16)
    tip = 1.0 if frame else -1.0
    for y in range(16):
        for x in range(16):
            u, v = x + 0.5 - 8, y + 0.5 - 8.5
            ang = 0.55 + tip * 0.08
            a = u * math.cos(ang) + v * math.sin(ang)
            b = -u * math.sin(ang) + v * math.cos(ang)
            if (a / 6.6) ** 2 + (b / 3.9) ** 2 <= 1.0:
                nd = -(a * 0.2 + b * 0.9) / 4.0
                cv.set(x, y, hs.shade_nd(nd, LEAF))
    for i in range(-5, 6):                                  # the midrib
        ang = 0.55 + tip * 0.08
        cv.set(int(8 + i * math.cos(ang)), int(8.5 + i * math.sin(ang)), LEAF[0] if i < 0 else LEAF[1])
    cv.set(2, 4, LEAF[3]), cv.set(3, 5, LEAF[3])            # the stem
    cv.outline(LEAF[3])
    return cv


def flower(frame):
    cv = Canvas(16, 16)
    for k in range(5):
        a = k * 2 * math.pi / 5 - math.pi / 2 + (0.2 if frame else 0.0)
        hs.ellipse(cv, 8 + 4.2 * math.cos(a), 8 + 4.2 * math.sin(a), 3.0, 3.0, PINK + PINK[2:])
    hs.ellipse(cv, 8.0, 8.0, 2.4, 2.4, GOLD + GOLD[1:] + GOLD[1:])
    cv.set(7, 7, WHITE)
    cv.outline((120, 40, 80))
    return cv


def puff(frame):
    """A soft poof: a ring that grows and fades with the house checker dither."""
    cv = Canvas(16, 16)
    r = [3.0, 5.0, 6.8][frame]
    for y in range(16):
        for x in range(16):
            d = math.hypot(x + 0.5 - 8, y + 0.5 - 8)
            if r - 2.2 < d < r and (frame == 0 or hs.checker(x, y, frame)):
                cv.set(x, y, WHITE if d > r - 1.2 else (226, 236, 210))
    return cv


def leafpip(frame):
    cv = Canvas(8, 8)
    for y in range(8):
        for x in range(8):
            u, v = x + 0.5 - 4, y + 0.5 - 4
            a, b = (u + v) * 0.707, (v - u) * 0.707
            if (a / 3.6) ** 2 + (b / 2.0) ** 2 <= 1.0:
                cv.set(x, y, (LEAF[1] if a + b < 0 else LEAF[2]) if frame == 0 else None)
    if frame == 1:                                          # to go: the leaf's outline only
        for y in range(8):
            for x in range(8):
                u, v = x + 0.5 - 4, y + 0.5 - 4
                a, b = (u + v) * 0.707, (v - u) * 0.707
                q = (a / 3.6) ** 2 + (b / 2.0) ** 2
                if 0.45 < q <= 1.0:
                    cv.set(x, y, LEAF[3])
        return cv
    cv.outline(LEAF[3])
    return cv


def berry(frame):
    cv = Canvas(16, 16)
    hs.ellipse(cv, 8.0, 9.5, 5.5, 5.0, BERRY + BERRY[2:])
    _px(cv, [(6, 4), (7, 4), (8, 3), (9, 4), (10, 4)], LEAF[2])      # the crown
    _px(cv, [(8, 2), (9, 1), (10, 1)], LEAF[1])
    cv.set(5, 7, WHITE)
    if frame:
        cv.set(6, 7, WHITE), cv.set(5, 8, BERRY[0])
    cv.outline(NAVY)
    return cv


def starpip(frame):
    cv = Canvas(8, 8)
    pts = []
    for y in range(8):
        for x in range(8):
            u, v = x + 0.5 - 4, y + 0.5 - 4.2
            a = math.atan2(v, u)
            r = math.hypot(u, v)
            lim = 2.0 + 1.6 * (0.5 + 0.5 * math.cos(5 * (a + math.pi / 2)))
            if r <= lim:
                pts.append((x, y))
    for x, y in pts:
        cv.set(x, y, (GOLD[0] if x + y < 8 else GOLD[1]) if frame == 0 else (42, 68, 118))
    cv.outline(NAVY)
    return cv


DIGITS = ["111101101101111", "010110010010111", "111001111100111", "111001111001111", "101101111001001",
          "111100111001111", "111100111101111", "111001010010010", "111101111101111", "111101111001111",
          "000010111010000"]


def digit(i):
    cv = Canvas(8, 8)
    g = DIGITS[i]
    for y in range(5):
        for x in range(3):
            if g[y * 3 + x] == "1":
                cv.rect(2 + x, 1 + y, 2 + x, 1 + y, CREAM)
    cv.outline(NAVY, diag=True)
    return cv


def sprite_frame(name, i):
    return {"blank": lambda _i: Canvas(8, 8), "head": head, "butterfly": butterfly, "seg": segment, "body0": body(0), "body1": body(1), "body2": body(2),
            "bodytail": lambda i: body(3 + i)(0), "icon": lambda _i: icon(), "cocoon": cocoon,
            "leaf": leaf, "flower": flower, "puff": puff, "berry": berry, "leafpip": leafpip,
            "starpip": starpip, "digit": digit}[name](i)


# ---- the obstacles (BG3, 16x16 on a transparent ground) ------------------------------------------------------------
# 15 colours in all (BG palette 7): the stone, the bush, the pot, its bloom, the mushroom, white, the outline
ROCK = [(184, 178, 170), (140, 136, 132), (96, 94, 106)]
TERRA = [(240, 160, 110), (206, 112, 70), (150, 70, 46)]
BUSH = [(150, 206, 96), (98, 166, 70), (56, 116, 58)]
BLOOM = [(246, 150, 190), (206, 84, 140)]
SHROOM = [(246, 96, 90), (196, 48, 64)]
OB_OUT = (40, 30, 44)


def obstacle(kind):
    cv = Canvas(16, 16)
    if kind == 0:                                           # a round grey stone
        hs.ellipse(cv, 8.0, 8.5, 6.8, 5.8, [WHITE] + ROCK)
        hs.ellipse(cv, 11.0, 5.5, 2.5, 2.0, [WHITE] + ROCK)
        cv.set(4, 6, WHITE)
    elif kind == 1:                                         # a little bush
        for bx, by, r in ((5.5, 9.0, 4.2), (10.5, 9.0, 4.2), (8.0, 5.5, 4.5), (8.0, 10.5, 4.2)):
            hs.ellipse(cv, bx, by, r, r, BUSH + [OB_OUT])
        _px(cv, [(5, 5), (10, 8), (6, 10)], BUSH[0])
    elif kind == 2:                                         # a flower pot with a pink bloom
        for y in range(7, 15):
            half = 4 + (14 - y) // 3
            for x in range(8 - half, 8 + half):
                f = (x - (8 - half)) / float(2 * half - 1)
                cv.set(x, y, TERRA[0] if f < 0.25 else TERRA[1] if f < 0.7 else TERRA[2])
        cv.rect(2, 6, 13, 7, TERRA[0])
        cv.rect(2, 7, 13, 7, TERRA[1])
        hs.ellipse(cv, 8.0, 3.5, 3.2, 2.6, [WHITE, BLOOM[0], BLOOM[1], BLOOM[1]])
        cv.set(8, 4, WHITE)
    else:                                                   # a red mushroom with white dots
        cv.rect(6, 9, 9, 14, WHITE)
        cv.rect(9, 9, 9, 14, ROCK[0])
        for y in range(2, 10):
            for x in range(1, 15):
                u, v = (x + 0.5 - 8) / 6.8, (y + 0.5 - 9.5) / 7.0
                if u * u + v * v <= 1.0 and y < 10:
                    cv.set(x, y, SHROOM[0] if u + v < -0.2 else SHROOM[1])
        for x, y in ((5, 5), (9, 4), (11, 7), (4, 8)):
            cv.set(x, y, WHITE)
            cv.set(x + 1, y, WHITE)
    cv.outline(OB_OUT)
    return cv


# ---- the garden (BG4): a raised bed of soil in a wooden frame, on a lawn ---------------------------------------------
LAWN = [(150, 196, 104), (128, 180, 90), (104, 158, 80)]
SOIL = [(178, 128, 86), (164, 116, 78), (130, 88, 60)]
WOOD = [(222, 178, 118), (186, 136, 82), (120, 78, 48)]
FLOWERS = [(250, 170, 200), (255, 226, 120), (250, 250, 240), (176, 150, 230)]
STONE = [(206, 204, 194), (164, 160, 152)]


def garden():
    """576x240 from stage x PANO_X0: the lawn (a soft 16-px pattern, flowers and stepping stones on the 8-px grid so
    the tiles repeat), the bed's wooden frame 8 px around the plot, the plot's soil in a checker of cells with a
    darker groove on each cell's bottom and right edge (the grid reads at a glance)."""
    W, H = PANORAMAS["garden"][1:]
    cv = Canvas(W, H)
    rng = random.Random(11)
    for y in range(H):
        for x in range(W):
            u, v = x % 16, y % 16
            c = LAWN[1]
            if (u, v) in ((3, 4), (4, 3), (11, 12), (12, 11), (10, 3), (2, 11)):
                c = LAWN[0]
            elif (u, v) in ((4, 4), (12, 12), (10, 4), (3, 12), (7, 8)):
                c = LAWN[2]
            cv.set(x, y, c)
    px0 = GRID_X - PANO_X0
    bed = (px0 - 8, GRID_Y - 8, px0 + GRID_W * CELL + 8, GRID_Y + GRID_H * CELL + 8)

    def free(tx, ty, w=2, h=2):
        x0, y0 = tx * 8, ty * 8
        if y0 < bed[1] and bed[0] - 16 < x0 + w * 8 and x0 < bed[2] + 16:
            return False                                    # the HUD band over the bed stays plain lawn
        return x0 + w * 8 <= bed[0] - 8 or x0 >= bed[2] + 8 or y0 + h * 8 <= bed[1] - 4 or y0 >= bed[3] + 8

    def stamp(fn, tx, ty):
        c2 = Canvas(16, 16)
        fn(c2)
        cv.paste(c2, tx * 8, ty * 8)

    def daisy(c2, col):
        for dx, dy in ((0, -2), (2, 0), (0, 2), (-2, 0)):
            c2.rect(7 + dx, 7 + dy, 8 + dx, 8 + dy, col)
        c2.rect(7, 7, 8, 8, FLOWERS[1] if col != FLOWERS[1] else WOOD[1])
        c2.set(4, 12, LAWN[2]), c2.set(5, 11, LAWN[2])

    def clump(c2, cols):
        for (dx, dy), col in zip(((3, 4), (10, 3), (6, 10), (12, 11)), cols):
            c2.rect(dx, dy, dx + 1, dy + 1, col)
            c2.set(dx, dy + 2, LAWN[2])

    def stone(c2):
        hs.ellipse(c2, 8.0, 8.5, 6.0, 4.5, [STONE[0], STONE[0], STONE[1], STONE[1]])
        for x in range(2, 14):
            if c2.get(x, 12) is not None or c2.get(x, 11) is not None:
                c2.set(x, 13, LAWN[2])

    def tuft(c2, x=8, y=12):                                # three blades of longer grass
        for bx, top, lean in ((-2, 3, -1), (0, 5, 0), (2, 3, 1)):
            for i in range(top):
                c2.set(x + bx + (lean if i >= top - 1 else 0), y - i, LAWN[0] if bx == 0 and i == top - 1 else LAWN[2])

    def bee(c2):
        c2.rect(6, 7, 9, 9, FLOWERS[1])
        c2.rect(7, 7, 7, 9, WOOD[2]), c2.set(9, 8, WOOD[2])
        c2.rect(6, 5, 7, 6, FLOWERS[2]), c2.set(8, 5, FLOWERS[2])
        c2.rect(6, 12, 9, 12, LAWN[2])                      # its shadow on the grass

    def moth(c2):                                           # a lilac butterfly resting on the lawn
        for dx in (-3, 1):
            c2.rect(8 + dx, 6, 9 + dx, 7, FLOWERS[3])
            c2.set(8 + dx + (1 if dx < 0 else 0), 8, FLOWERS[3])
        c2.set(9 - 4, 6, FLOWERS[2]), c2.set(10, 6, FLOWERS[2])
        c2.rect(7, 6, 8, 8, WOOD[2])
        c2.set(6, 5, WOOD[2]), c2.set(9, 5, WOOD[2])
        c2.rect(6, 11, 9, 11, LAWN[2])

    def snail(c2):
        c2.rect(4, 10, 11, 10, STONE[0]), c2.rect(4, 11, 11, 11, STONE[1])    # its foot
        c2.set(11, 9, STONE[0]), c2.set(11, 8, STONE[1]), c2.set(12, 8, STONE[1])
        hs.ellipse(c2, 7.0, 7.5, 3.2, 3.0, [WOOD[0], WOOD[0], WOOD[1], WOOD[2]])
        c2.set(7, 7, WOOD[2]), c2.set(8, 7, WOOD[2]), c2.set(7, 8, WOOD[0])
        c2.rect(3, 12, 11, 12, LAWN[2])

    def ants(c2):                                           # a little trail of ants
        for ax, ay in ((2, 11), (7, 8), (12, 5)):
            c2.rect(ax, ay, ax + 2, ay, WOOD[2])
            c2.set(ax + 1, ay + 1, WOOD[2])

    # a stepping-stone path from each side of the bed out to the lawn's edge (seen on wide screens)
    placed = set()
    for side in (-1, 1):
        for i in range(8):
            sx = (bed[0] - 22 - i * 16) if side < 0 else (bed[2] + 6 + i * 16)
            sy = 150 + int(round(6 * math.sin(i * 0.9 + (0 if side < 0 else 1.7))))
            c2 = Canvas(14, 9)
            hs.ellipse(c2, 7.0, 4.0, 6.0, 3.4, [STONE[0], STONE[0], STONE[1], STONE[1]])
            for x in range(14):
                if c2.get(x, 6) is not None and c2.get(x, 7) is None:
                    c2.set(x, 7, LAWN[2])
            cv.paste(c2, sx, sy)
            for a in range(-1, 3):
                for b in range(-1, 3):
                    placed.add((sx // 8 + a, sy // 8 + b))
    # decorations on the 8-px grid (away from the bed and the path)
    for _ in range(170):
        tx, ty = rng.randrange(0, W // 8 - 1), rng.randrange(0, H // 8 - 1)
        if not free(tx, ty) or any((tx + a, ty + b) in placed for a in (-2, -1, 0, 1, 2) for b in (-2, -1, 0, 1, 2)):
            continue
        k = rng.randrange(12)
        if k < 3:
            stamp(lambda c2, col=FLOWERS[rng.choice((0, 2, 3))]: daisy(c2, col), tx, ty)
        elif k < 5:
            stamp(lambda c2: clump(c2, [FLOWERS[0], FLOWERS[1], FLOWERS[3], FLOWERS[2]]), tx, ty)
        elif k < 8:
            stamp(tuft, tx, ty)
        elif k < 9:
            stamp(stone, tx, ty)
        else:
            stamp([bee, moth, snail, ants][k - 9 if k < 11 else rng.randrange(4)], tx, ty)
        placed.add((tx, ty))
    # longer grass along the outside of the frame (the 8-px strips seen beside it at 320 too)
    erng = random.Random(3)
    for y in range(bed[1] + 4, bed[3] - 4, 5):
        for x in (bed[0] - 5, bed[2] + 4):
            if erng.random() < 0.7:
                c2 = Canvas(8, 8)
                tuft(c2, 3, 7)
                if erng.random() < 0.18:
                    c2.rect(2, 1, 3, 2, FLOWERS[erng.choice((0, 2, 3))])
                cv.paste(c2, x - 3 + erng.randrange(-1, 2), y - 4)
    for x in range(bed[0] + 4, bed[2] - 4, 6):                  # and along its top edge, under the HUD
        if erng.random() < 0.5:
            c2 = Canvas(8, 8)
            tuft(c2, 3, 7)
            cv.paste(c2, x - 3, bed[1] - 7)
    # the bed's frame: planks with a light top edge and nail dots, darker corners
    x0, y0, x1, y1 = bed
    for y in range(y0, y1):
        for x in range(x0, x1):
            if x0 + 8 <= x < x1 - 8 and y0 + 8 <= y < y1 - 8:
                continue
            horiz = y < y0 + 8 or y >= y1 - 8
            k = (y - y0) % 8 if horiz else (x - x0) % 8
            c = WOOD[0] if k == 0 else WOOD[2] if k == 7 else WOOD[1]
            if horiz and (x - x0) % 32 == 31 and not (x < x0 + 8 or x >= x1 - 8):
                c = WOOD[2]
            if not horiz and (y - y0) % 32 == 31:
                c = WOOD[2]
            if horiz and k == 3 and (x - x0) % 32 in (4, 27) or not horiz and k == 3 and (y - y0) % 32 in (4, 27):
                c = WOOD[2]
            cv.set(x, y, c)
    for cx, cy in ((x0, y0), (x1 - 8, y0), (x0, y1 - 8), (x1 - 8, y1 - 8)):   # the corner posts
        for y in range(8):
            for x in range(8):
                cv.set(cx + x, cy + y, WOOD[0] if x == 0 or y == 0 else WOOD[2] if x == 7 or y == 7 else WOOD[1])
        cv.set(cx + 3, cy + 3, WOOD[2])
    # the soil: a checker of cells, a groove on each cell's bottom and right edge, crumbs scattered differently from
    # cell to cell (one of a few patterns), now and then a pebble; the frame's shade on the top and left edges
    srng = random.Random(7)
    patterns = []
    for i in range(11):
        crumbs = [(srng.randrange(2, 14), srng.randrange(2, 14)) for _ in range(srng.randrange(2, 5))]
        grains = [(srng.randrange(2, 14), srng.randrange(2, 14)) for _ in range(2)]
        pebble = (srng.randrange(3, 11), srng.randrange(3, 11)) if i == 3 else None
        patterns.append((crumbs, grains, pebble))
    for gy in range(GRID_H):
        for gx in range(GRID_W):
            base = SOIL[0] if (gx + gy) % 2 == 0 else SOIL[1]
            crumbs, grains, pebble = patterns[(gx * 7 + gy * 13 + (gx * gy) % 5) % len(patterns)]
            for v in range(16):
                for u in range(16):
                    c = base
                    if u == 15 or v == 15 or (gy == 0 and v == 0) or (gx == 0 and u == 0):
                        c = SOIL[2]
                    elif (u, v) in crumbs:
                        c = SOIL[2]
                    elif (u, v) in grains:
                        c = SOIL[0] if base == SOIL[1] else SOIL[1]
                    cv.set(px0 + gx * CELL + u, GRID_Y + gy * CELL + v, c)
            if pebble:
                x, y = px0 + gx * CELL + pebble[0], GRID_Y + gy * CELL + pebble[1]
                cv.rect(x + 1, y, x + 2, y, STONE[1]), cv.rect(x, y + 1, x + 3, y + 1, SOIL[2])
                cv.set(x + 1, y + 1, STONE[1])
    # the top band's shadow under the HUD: nothing (the kit's digits have their own outline)
    return cv


def obstacles():
    w, h = OBSTACLES[1:]
    cv = Canvas(w, h)
    for k in range(4):
        cv.paste(obstacle(k), k * 16, 0)
    return cv


def build_sheet():
    im = Image.new("RGB", (SHEET_W, SHEET_H), hs.MAGENTA)
    for name, x, y, w, h, frames, _g in SPRITES:
        for i in range(frames):
            im.paste(sprite_frame(name, i).image(), (x + i * w, y))
    return im


def write_all(out, preview=False):
    os.makedirs(os.path.join(out, "tiles"), exist_ok=True)
    files = {"sprites.png": build_sheet(), PANORAMAS["garden"][0]: garden().image(),
             OBSTACLES[0]: obstacles().image()}
    for f, im in files.items():
        im.save(os.path.join(out, f))
        if preview:
            os.makedirs(os.path.join(out, "preview"), exist_ok=True)
            k = 2 if im.width > 300 else 4
            im.resize((im.width * k, im.height * k), Image.NEAREST).save(
                os.path.join(out, "preview", os.path.basename(f)))
    return sorted(files)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", default=os.path.join(GAME, "art"))
    ap.add_argument("--preview", action="store_true", help="also write enlarged previews in <out>/preview")
    a = ap.parse_args(argv)
    for f in write_all(a.out, a.preview):
        print("wrote %s/%s" % (a.out, f))


if __name__ == "__main__":
    main()

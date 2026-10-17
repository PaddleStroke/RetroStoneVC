#!/usr/bin/env python3
"""Seal Shove: the code-drawn art, in the 8BCraft house style (docs/art-direction.md).

Code-drawn art only, no image generation: every sprite and the sea panorama are drawn here with
games/common/tools/house_style.py (1-px outlines, 3-4-shade ramps lit from the top-left, the house eye). The floe
itself is not here: src/draw.c paints it pixel by pixel from the rules' grid, in the ice colours below (ICE_PAL).

Writes, in the art folder (default games/sealshove/art):
  seals.png       the seals in the 3/4 view (front, back, side x waddle, waddle, slide; left = the side mirrored), the title
                  seal, the head icon, the head in the water, the player tags: the seal palette (players 2-4 turn
                  the hat and scarf: tools/build_assets.py)
  props.png       splashes, droplets, snow puffs, ice chips, dizzy stars, the two fish, the power icons, the round
                  pips, the touch stick and DASH button (OBJ 2)
  penguins.png    the penguins watching from the icebergs (OBJ 7)
  tiles/sea.png   the sea panorama (1024 x 256: stage x = panorama x - 352): ripples, icebergs, ice bits, a
                  fishing boat; a wider screen shows more of it

    python3 games/sealshove/tools/make_art.py [--out DIR] [--preview]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/sealshove/LICENSE.
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
LD = hs.LIGHT_DIR
FONT = hs.FONT

# ---- palettes ------------------------------------------------------------------------------------------------------
# the seals (OBJ 0; players 2-4 = the hat and scarf turned: build_assets.py P2_HUE). Index order is the sheet's.
SEAL_PAL = [
    (34, 26, 62),      # 1 outline (navy violet)
    (226, 230, 242),   # 2 body highlight, the muzzle and belly
    (184, 192, 216),   # 3 body light
    (144, 152, 186),   # 4 body mid
    (104, 110, 148),   # 5 body dark
    (78, 82, 122),     # 6 flippers
    (24, 14, 34),      # 7 eye ink
    (255, 255, 255),   # 8 glint
    (238, 144, 170),   # 9 nose, cheeks
    (255, 150, 106),   # 10 hat and scarf: light
    (238, 86, 62),     # 11 mid
    (178, 50, 40),     # 12 dark
    (255, 244, 226),   # 13 pom-pom, the tag's digit
    (96, 168, 214),    # 14 water (the head in the sea)
    (190, 230, 250),   # 15 water light
]
OUT = SEAL_PAL[0]
BODY = [SEAL_PAL[1], SEAL_PAL[2], SEAL_PAL[3], SEAL_PAL[4]]
FLIP = [SEAL_PAL[3], SEAL_PAL[4], SEAL_PAL[5], SEAL_PAL[5]]
WOOL = [SEAL_PAL[9], SEAL_PAL[9], SEAL_PAL[10], SEAL_PAL[11]]
INK, GLINT, PINK, POM, WATER, WATER_L = SEAL_PAL[6], SEAL_PAL[7], SEAL_PAL[8], SEAL_PAL[12], SEAL_PAL[13], SEAL_PAL[14]

PROPS_PAL = [
    (22, 18, 40),      # 1 navy outline
    (250, 250, 250),   # 2 white
    (255, 246, 220),   # 3 cream
    (196, 238, 255),   # 4 water light
    (112, 184, 226),   # 5 water mid
    (120, 176, 244),   # 6 blue fish light
    (60, 100, 196),    # 7 blue fish dark
    (252, 216, 72),    # 8 gold light
    (206, 140, 30),    # 9 gold dark
    (96, 120, 184),    # 10 touch blue
    (150, 172, 226),   # 11 touch blue light
    (156, 162, 178),   # 12 iron
    (92, 98, 116),     # 13 iron dark
    (240, 120, 150),   # 14 pink
    (255, 238, 150),   # 15 star yellow
]


def pc(i):
    return PROPS_PAL[i - 1]


PENG_PAL = [(22, 18, 40), (44, 44, 60), (78, 80, 102), (250, 250, 250), (210, 216, 230), (252, 168, 60),
            (204, 104, 30), (240, 150, 172)]

# the floe's colours (BG palette 1; src/draw.c paints with these indices, OBJ 6 for the drifting chunks)
ICE_PAL = [
    (128, 204, 228),   # 1 the submerged skirt round the floe
    (78, 140, 194),    # 2 the floe's side, dark
    (126, 186, 226),   # 3 the floe's side, light
    (252, 254, 255),   # 4 the rim, lit (top-left)
    (232, 244, 252),   # 5 ice, light
    (216, 234, 250),   # 6 ice, drifts of snow
    (176, 206, 234),   # 7 ice, shade (bottom-right rim)
    (56, 92, 150),     # 8 a crack
    (232, 244, 252),   # 9 cracking ice (animated: blinks)
    (176, 206, 234),   # 10 cracking ice shade (animated)
    (255, 255, 255),   # 11 a twinkle (animated)
    (184, 212, 238),   # 12 hairline
    (255, 255, 255),   # 13 a snow speck
    (150, 200, 232),   # 14 wet ice (a chunk sinking)
    (98, 160, 206),    # 15 wet ice, dark
]


def ceil8(v):
    return (int(math.ceil(v)) + 7) // 8 * 8


# ---- the seals -------------------------------------------------------------------------------------------------------
VIEWS = ("front", "back", "side")       # 3/4 view: walking down, up, sideways (left = the side mirrored)


def _ell(cv, cx, cy, rx, ry, ramp, flat=None):
    hs.ellipse(cv, cx, cy, rx, ry, ramp, flat=flat)


def _cap(cv, cx, top, bottom, rx, brim_y):
    """the knitted cap over a head: rows top..bottom, shaded, a ribbed brim on row brim_y"""
    for y in range(int(top), int(bottom) + 1):
        for x in range(32):
            dx, dy = (x + 0.5 - cx) / rx, (y + 0.5 - bottom) / (bottom - top + 1)
            if dx * dx + dy * dy <= 1.0:
                cv.set(x, y, WOOL[3] if (y == brim_y and x % 2) else WOOL[2] if y >= brim_y else
                       hs.shade_nd(dx * LD[0] + dy * LD[1], WOOL))


def seal(view, frame):
    """A seal in its hat and scarf, 32x32, in the 3/4 view of the house (the body on the ground, the head above it):
    view front, back or side (facing right); frame 0/1 the waddle (flippers and the scarf's end swap), 2 the belly
    slide (flat, stretched, flippers back). The body's middle is at (16, 20): the seal's place on the floe."""
    cv = Canvas(32, 32)
    w = frame == 1
    slide = frame == 2
    if view == "side":
        if slide:
            _ell(cv, 2.8, 22.5, 2.2, 1.6, FLIP), _ell(cv, 3.2, 25.5, 2.2, 1.6, FLIP)
            _ell(cv, 14.0, 23.0, 12.4, 6.0, BODY)
            _ell(cv, 24.6, 18.6, 5.8, 5.0, BODY)
            _ell(cv, 12.0, 27.0, 4.0, 1.6, FLIP)                            # the flipper tucked back
            hx, hy = 24.6, 18.6
        else:
            _ell(cv, 3.0, 21.5 - (1 if w else 0), 2.4, 1.8, FLIP), _ell(cv, 3.4, 25.0 + (1 if w else 0), 2.4, 1.8, FLIP)
            _ell(cv, 14.0, 21.5, 11.0, 7.4, BODY)
            _ell(cv, 22.6, 14.6, 6.2, 5.6, BODY)
            _ell(cv, 17.0 + (1.6 if w else -1.0), 26.6, 3.6, 2.0, FLIP)       # the front flipper, waddling
            hx, hy = 22.6, 14.6
        for y in range(32):                                                 # the scarf round the neck
            for x in range(32):
                if abs(x + 0.5 - (hx - 4.2)) <= 1.2 and cv.get(x, y) is not None and hy - 1 <= y <= hy + 8:
                    cv.set(x, y, WOOL[1] if x % 2 else WOOL[2])
        end = [(hx - 5, hy + 3), (hx - 11, hy + (4 if slide else 6 - (1 if w else 0)))]
        for i in range(8):
            t = i / 7.0
            x, y = end[0][0] + (end[1][0] - end[0][0]) * t, end[0][1] + (end[1][1] - end[0][1]) * t
            cv.set(int(x), int(y), WOOL[2]), cv.set(int(x), int(y) + 1, WOOL[3])
        _cap(cv, hx - 0.8, hy - 6.2, hy - 2.0, 6.0, int(hy - 3))
        _ell(cv, hx - 2.6, hy - 6.8, 1.8, 1.6, None, flat=POM)
        cv.set(int(hx - 3.4), int(hy - 7.6), GLINT)
        _ell(cv, hx + 4.0, hy + 2.2, 2.4, 1.8, None, flat=BODY[0])           # the pale muzzle
        cv.rect(int(hx + 1.6), int(hy), int(hx + 2.6), int(hy + 1), INK)     # the eye
        cv.set(int(hx + 1.6), int(hy), GLINT)
        cv.set(int(hx + 5.8), int(hy + 1.2), INK), cv.set(int(hx + 6.2), int(hy + 1.2), INK)   # the nose
        cv.set(int(hx + 0.6), int(hy + 2.6), PINK)
    elif view == "front":
        by, hy = (23.0, 15.0) if slide else (21.0, 12.0)
        _ell(cv, 16.0, by, 11.4 if slide else 10.0, 6.6 if slide else 8.6, BODY)
        _ell(cv, 16.0, by + 2.0, 6.0, 4.6 if slide else 5.4, None, flat=BODY[0])    # the pale belly
        if slide:
            _ell(cv, 4.2, by + 2, 3.4, 1.8, FLIP), _ell(cv, 27.8, by + 2, 3.4, 1.8, FLIP)
        else:
            _ell(cv, 6.0, 24.0 - (1.5 if w else 0), 3.0, 2.0, FLIP), _ell(cv, 26.0, 24.0 - (0 if w else 1.5), 3.0, 2.0, FLIP)
            _ell(cv, 12.0 - (0.6 if w else 0), 29.6, 2.4, 1.4, FLIP), _ell(cv, 20.0 + (0 if w else 0.6), 29.6, 2.4, 1.4, FLIP)
        _ell(cv, 16.0, hy, 7.6, 6.6, BODY)                                  # the head
        _cap(cv, 16.0, hy - 7.0, hy - 2.0, 7.2, int(hy - 3))
        _ell(cv, 16.0, hy - 7.4, 1.9, 1.7, None, flat=POM)
        cv.set(15, int(hy - 8.4), GLINT)
        for ex in (11, 19):                                                 # the eyes
            cv.rect(ex, int(hy), ex + 1, int(hy + 1), INK)
            cv.set(ex, int(hy), GLINT)
            cv.set(ex + 1, int(hy - 1), INK)
        _ell(cv, 16.0, hy + 3.4, 2.8, 1.8, None, flat=BODY[0])               # the muzzle, the nose
        cv.rect(15, int(hy + 2), 16, int(hy + 2), INK)
        cv.set(9, int(hy + 3), PINK), cv.set(22, int(hy + 3), PINK)
        sy = int(hy + 6)
        for x in range(8, 25):                                              # the scarf
            if cv.get(x, sy) is not None:
                cv.set(x, sy, WOOL[1] if x % 3 else WOOL[2])
                cv.set(x, sy + 1, WOOL[2] if x % 3 else WOOL[3])
        for y in range(sy + 1, sy + (4 if slide else 7)):
            cv.set(21 + (1 if w and y > sy + 3 else 0), y, WOOL[2]), cv.set(22 + (1 if w and y > sy + 3 else 0), y, WOOL[3])
    else:                                                                   # the back
        by, hy = (23.0, 15.0) if slide else (20.0, 11.0)
        if not slide:
            _ell(cv, 12.0 + (0.6 if w else 0), 29.6, 2.6, 1.6, FLIP), _ell(cv, 20.0 - (0 if w else 0.6), 29.6, 2.6, 1.6, FLIP)
            _ell(cv, 6.0, 22.0 - (0 if w else 1.5), 3.0, 2.0, FLIP), _ell(cv, 26.0, 22.0 - (1.5 if w else 0), 3.0, 2.0, FLIP)
        else:
            _ell(cv, 4.2, by + 1, 3.4, 1.8, FLIP), _ell(cv, 27.8, by + 1, 3.4, 1.8, FLIP)
        _ell(cv, 16.0, by, 11.4 if slide else 10.2, 6.8 if slide else 9.0, BODY)
        _ell(cv, 16.0, hy, 7.6, 6.6, BODY)
        _cap(cv, 16.0, hy - 7.0, hy + 2.0, 7.4, int(hy + 1))
        _ell(cv, 16.0, hy - 6.4, 1.9, 1.7, None, flat=POM)
        cv.set(15, int(hy - 7.4), GLINT)
        sy = int(hy + 5)
        for x in range(8, 25):
            if cv.get(x, sy) is not None:
                cv.set(x, sy, WOOL[1] if x % 3 else WOOL[2])
                cv.set(x, sy + 1, WOOL[2] if x % 3 else WOOL[3])
        for y in range(sy + 1, sy + (4 if slide else 8)):                    # the scarf's knot hangs down the back
            cv.set(15 - (1 if w and y > sy + 4 else 0), y, WOOL[2]), cv.set(17 + (1 if not w and y > sy + 4 else 0), y, WOOL[3])
    cv.outline(OUT)
    return cv

def seal_icon():
    """The 16x16 head, from the front: the title's player slots, the HUD, the results (the hat in the player's
    colour)."""
    cv = Canvas(16, 16)
    hs.ellipse(cv, 8.0, 9.2, 6.4, 5.8, BODY)
    for y in range(16):                                                     # the hat
        for x in range(16):
            dx, dy = (x + 0.5 - 8) / 6.0, (y + 0.5 - 6.4) / 4.6
            if dx * dx + dy * dy <= 1 and y < 7:
                cv.set(x, y, hs.shade_nd(dx * LD[0] + dy * LD[1], WOOL))
    for x in range(3, 13):
        cv.set(x, 6, WOOL[3] if x % 2 else WOOL[2])
    hs.ellipse(cv, 8.0, 1.6, 1.7, 1.4, None, flat=POM)
    cv.set(7, 1, GLINT)
    for ex in (5, 10):                                                      # eyes, a glint
        cv.rect(ex, 8, ex + 1, 9, INK)
        cv.set(ex, 8, GLINT)
    cv.set(7, 11, PINK), cv.set(8, 11, PINK)
    cv.set(3, 10, PINK), cv.set(12, 10, PINK)
    for x in range(4, 12):                                                  # the scarf
        cv.set(x, 14, WOOL[2] if x % 3 else WOOL[1])
    cv.outline(OUT)
    return cv


def seal_head(frame):
    """Knocked out: the head bobbing in the sea, a ring of water round it."""
    cv = Canvas(16, 16)
    ic = seal_icon()
    oy = frame
    for y in range(16):
        for x in range(16):
            col = ic.get(x, y - oy - 1)
            if col is not None and y < 12:
                cv.set(x, y, col)
    for x in range(16):
        d = abs(x + 0.5 - 8)
        if d < 7.5:
            cv.set(x, 12, WATER_L if (x + frame) % 3 else hs.HOUSE["white"][0])
            if d < 6:
                cv.set(x, 13, WATER)
    return cv


def seal_big(frame):
    """The title's seal (32x32): sitting up, from the front, in its hat and scarf; frame 1 blinks and waves."""
    cv = Canvas(32, 32)
    hs.ellipse(cv, 16.0, 22.5, 10.5, 8.5, BODY)                             # the body
    for fx, fy, k in ((5.5, 24.5, -1), (26.5, 24.5 - (3 if frame else 0), 1)):     # the front flippers
        hs.ellipse(cv, fx, fy, 3.0, 2.0, FLIP)
    hs.ellipse(cv, 16.0, 12.0, 8.0, 7.0, BODY)                              # the head
    for y in range(32):                                                     # the hat
        for x in range(32):
            dx, dy = (x + 0.5 - 16) / 7.6, (y + 0.5 - 8.6) / 5.6
            if dx * dx + dy * dy <= 1 and y < 9:
                cv.set(x, y, hs.shade_nd(dx * LD[0] + dy * LD[1], WOOL))
    for x in range(9, 24):
        cv.set(x, 8, WOOL[3] if x % 2 else WOOL[2])
        cv.set(x, 9, WOOL[2] if x % 2 else WOOL[3])
    hs.ellipse(cv, 16.0, 2.4, 2.4, 2.0, None, flat=POM)
    cv.set(15, 1, GLINT)
    for ex in (12, 19):                                                     # the eyes
        if frame:
            cv.rect(ex, 13, ex + 1, 13, INK)
        else:
            cv.rect(ex, 12, ex + 1, 14, INK)
            cv.set(ex, 12, GLINT)
    cv.rect(15, 15, 16, 15, PINK)
    cv.set(14, 16, INK), cv.set(17, 16, INK), cv.set(15, 17, INK), cv.set(16, 17, INK)
    cv.set(10, 15, PINK), cv.set(21, 15, PINK)
    for x in range(8, 25):                                                  # the scarf, its end hanging
        cv.set(x, 18, WOOL[1] if x % 3 else WOOL[2])
        cv.set(x, 19, WOOL[2] if x % 3 else WOOL[3])
    for y in range(19, 25):
        cv.set(21, y, WOOL[2]), cv.set(22, y, WOOL[3] if y % 2 else WOOL[2])
    cv.outline(OUT)
    return cv


def tag(n):
    """A player's tag over its seal (8x16): the number on a little flag of its colour, a point below."""
    cv = Canvas(8, 16)
    cv.rect(0, 0, 6, 8, WOOL[2])
    cv.rect(0, 0, 6, 0, WOOL[1])
    g = FONT[str(n)]
    for y, row in enumerate(g[:7]):
        for x, ch in enumerate(row):
            if ch == "#":
                cv.set(1 + x, 1 + y, POM)
    for i in range(3):
        cv.rect(1 + i, 9 + i, 5 - i, 9 + i, WOOL[3])
    cv.outline(OUT)
    return cv


def seal_sprites():
    out = {}
    for view in VIEWS:
        for f in range(3):
            out["seal_%s%d" % (view, f)] = seal(view, f)
    out["seal_icon"] = seal_icon()
    out["seal_head0"] = seal_head(0)
    out["seal_head1"] = seal_head(1)
    out["seal_big0"] = seal_big(0)
    out["seal_big1"] = seal_big(1)
    for n in range(1, 5):
        out["tag%d" % n] = tag(n)
    return out


# ---- props (OBJ 2) ------------------------------------------------------------------------------------------------------
def splash(frame):
    """A splash, from above: a ring of water spreading, droplets flying out (the last frame fades: the checker)."""
    cv = Canvas(24, 24)
    r = [4.0, 7.0, 9.5][frame]
    for y in range(24):
        for x in range(24):
            d = math.hypot(x + 0.5 - 12, y + 0.5 - 12)
            if r - 2.2 < d <= r and (frame < 2 or hs.checker(x, y)):
                cv.set(x, y, pc(2) if d > r - 1 and (x + y) % 3 else pc(4))
            elif d <= r - 2.2 and frame == 0:
                cv.set(x, y, pc(5))
    for k in range(8):
        a = k * math.pi / 4 + 0.3
        rr = r + 2 + frame
        x, y = 12 + math.cos(a) * rr, 12 + math.sin(a) * rr
        if 0 <= x < 23 and 0 <= y < 23 and frame < 2:
            cv.set(int(x), int(y), pc(2)), cv.set(int(x) + 1, int(y), pc(4))
    return cv


def droplet(frame):
    cv = Canvas(8, 8)
    hs.ellipse(cv, 4.0, 4.0, 2.0 - frame * 0.6, 2.0 - frame * 0.6, None, flat=pc(4))
    cv.set(3, 3, pc(2))
    return cv


def puff(frame):
    """Snow sprayed by a belly slide."""
    cv = Canvas(8, 8)
    r = [1.8, 2.8][frame]
    for y in range(8):
        for x in range(8):
            d = math.hypot(x + 0.5 - 4, y + 0.5 - 4)
            if d < r and (frame == 0 or hs.checker(x, y)):
                cv.set(x, y, pc(2) if d < r - 1 else pc(4))
    return cv


def chip(frame):
    """Ice chips flying off a chunk that breaks."""
    cv = Canvas(8, 8)
    pts = [(1, 2), (5, 1), (3, 5), (6, 5)] if frame == 0 else [(0, 1), (6, 0), (2, 6), (7, 6)]
    for x, y in pts:
        cv.set(x, y, pc(2)), cv.set(min(7, x + 1), y, pc(4))
    return cv


def stars(frame):
    """Dizzy: two little stars going round."""
    cv = Canvas(24, 8)
    for i, (sx, sy) in enumerate(((3, 3), (12, 4), (20, 3)) if frame == 0 else ((7, 4), (16, 3), (21, 5))):
        for d in (1, 2):
            for dx, dy in ((d, 0), (-d, 0), (0, d), (0, -d)):
                cv.set(sx + dx, sy + dy, pc(8) if d == 2 else pc(15))
        for dx, dy in ((1, 1), (-1, -1), (1, -1), (-1, 1)):
            cv.set(sx + dx, sy + dy, pc(15))
        cv.set(sx, sy, pc(2))
    cv.outline(pc(1))
    return cv


def fish(kind, frame):
    """The fish flopping on the ice: kind 0 the chubby blue one (BIG BELLY), 1 the golden one (MEGA SLIDE)."""
    cv = Canvas(16, 16)
    ramp = [pc(6), pc(6), pc(7), pc(7)] if kind == 0 else [pc(15), pc(8), pc(9), pc(9)]
    ry = 4.2 if kind == 0 else 3.0
    bend = 1.5 if frame else 0.0
    for y in range(16):
        for x in range(16):
            dx = (x + 0.5 - 8.5) / 5.5
            yy = y + 0.5 - 8.5 + bend * dx * dx * 2
            dy = yy / ry
            if dx * dx + dy * dy <= 1:
                cv.set(x, y, hs.shade_nd(dx * LD[0] + dy * LD[1], ramp))
    ty = 8 - (2 if frame else 0)
    for i in range(4):                                                      # the tail
        cv.set(2 - i // 2, ty - 1 - i // 2 + (1 if i % 2 else 0), ramp[2])
        cv.set(2 - i // 2, ty + 1 + i // 2, ramp[2])
    cv.set(1, ty - 2, ramp[2]), cv.set(1, ty + 2, ramp[2])
    cv.set(11, 7, pc(1)), cv.set(11, 6, pc(2))                              # the eye
    cv.set(13, 9, pc(14))
    if kind == 1:
        cv.set(7, 6, pc(2))
    cv.outline(pc(1))
    return cv


def _badge(ramp):
    """A round badge (16x16, the disc 13 px across with its outline) over a seal with a power: the fish's colour, lit
    from the top-left; the symbol goes on it."""
    cv = Canvas(16, 16)
    hs.ellipse(cv, 8.0, 8.0, 6.0, 6.0, ramp)
    return cv


def icon_heavy():
    """BIG BELLY: an iron weight on the blue fish's badge."""
    cv = _badge([pc(4), pc(6), pc(6), pc(7)])
    hs.ellipse(cv, 8.0, 9.8, 4.2, 3.0, [pc(12), pc(12), pc(13), pc(13)])   # the weight
    for x, y in ((6, 3), (7, 3), (8, 3), (9, 3), (10, 3), (5, 4), (6, 4), (10, 4), (11, 4), (5, 5), (11, 5), (5, 6),
                 (11, 6)):                                                  # its handle
        cv.set(x, y, pc(13))
    cv.set(6, 8, pc(2)), cv.set(7, 3, pc(12))
    cv.outline(pc(1))
    return cv


def icon_mega():
    """MEGA SLIDE: a white bolt on the golden fish's badge."""
    cv = _badge([pc(8), pc(8), pc(9), pc(9)])
    bolt = ((9, 2), (10, 2), (8, 3), (9, 3), (7, 4), (8, 4), (6, 5), (7, 5), (5, 6), (6, 6), (7, 6), (8, 6), (9, 6),
            (10, 6), (8, 7), (9, 7), (7, 8), (8, 8), (6, 9), (7, 9), (5, 10), (6, 10), (5, 11))
    for x, y in bolt:
        cv.set(x, y, pc(2))
    for x, y in bolt:                                                       # a dark edge on its right and below
        for dx, dy in ((1, 0), (0, 1)):
            if (x + dx, y + dy) not in bolt and cv.get(x + dx, y + dy) is not None:
                cv.set(x + dx, y + dy, pc(9))
    cv.outline(pc(1))
    return cv


def pip(full):
    cv = Canvas(8, 8)
    if full:
        hs.ellipse(cv, 4.0, 4.0, 3.2, 3.2, [pc(15), pc(8), pc(9), pc(9)])
        cv.set(3, 2, pc(2))
    else:
        for y in range(8):
            for x in range(8):
                d = math.hypot(x + 0.5 - 4, y + 0.5 - 4)
                if 2.2 < d <= 3.2:
                    cv.set(x, y, pc(13))
    cv.outline(pc(1))
    return cv


def disc_quarter(R, Q, kind):
    """A quarter of the round DASH button: a see-through blue disc (the house checker; pressed: solid, lighter; on
    cooldown: grey) with a 2-px cream ring, like the apps' drawn buttons."""
    cv = Canvas(Q, Q)
    for y in range(Q):
        for x in range(Q):
            d = math.hypot(x + 0.5 - Q, y + 0.5 - Q)
            if d <= R - 2:
                if kind == 1:
                    cv.set(x, y, pc(11))
                elif hs.checker(x, y):
                    cv.set(x, y, pc(13) if kind == 2 else pc(10))
            elif d <= R:
                cv.set(x, y, pc(12) if kind == 2 else pc(3))
    for y in range(Q):
        for x in range(Q):
            d = math.hypot(x + 0.5 - Q, y + 0.5 - Q)
            if R < d <= R + 1:
                cv.set(x, y, pc(1))
    return cv


def icon_dash():
    """A seal on its belly, speed lines behind."""
    cv = Canvas(16, 16)
    hs.ellipse(cv, 9.5, 9.0, 4.8, 2.8, None, flat=pc(3))
    hs.ellipse(cv, 13.2, 8.2, 2.2, 2.2, None, flat=pc(3))
    for i, y in enumerate((6, 9, 12)):
        for x in range(1 + i % 2, 4 + i % 2):
            cv.set(x, y, pc(3))
    cv.outline(pc(1))
    return cv


def stick_quarter(R=22):
    Q = ceil8(R + 1)
    cv = Canvas(Q, Q)
    for y in range(Q):
        for x in range(Q):
            d = math.hypot(x + 0.5 - Q, y + 0.5 - Q)
            if R - 2 < d <= R:
                cv.set(x, y, pc(3))
            elif d <= R - 2 and hs.checker(x, y) and (x + y) % 4 == 0:
                cv.set(x, y, pc(10))
    for y in range(Q):
        for x in range(Q):
            d = math.hypot(x + 0.5 - Q, y + 0.5 - Q)
            if R < d <= R + 1:
                cv.set(x, y, pc(1))
    return cv


def knob():
    cv = Canvas(16, 16)
    hs.ellipse(cv, 8.0, 8.0, 6.5, 6.5, [pc(2), pc(3), pc(11), pc(10)])
    cv.outline(pc(1))
    return cv


def props():
    out = {}
    for f in range(3):
        out["splash%d" % f] = splash(f)
    for f in range(2):
        out["drop%d" % f] = droplet(f)
        out["puff%d" % f] = puff(f)
        out["chip%d" % f] = chip(f)
        out["stars%d" % f] = stars(f)
        out["fish_heavy%d" % f] = fish(0, f)
        out["fish_mega%d" % f] = fish(1, f)
    out["icon_heavy"] = icon_heavy()
    out["icon_mega"] = icon_mega()
    out["pip0"] = pip(False)
    out["pip1"] = pip(True)
    for name, k in (("btn", 0), ("btn_on", 1), ("btn_cd", 2)):
        out[name] = disc_quarter(22, 24, k)
    out["icon_dash"] = icon_dash()
    out["stick"] = stick_quarter()
    out["knob"] = knob()
    return out


# ---- penguins (OBJ 7): they watch from the icebergs and cheer at every splash -------------------------------------------
def penguin(frame):
    P = PENG_PAL
    cv = Canvas(16, 16)
    oy = -2 if frame == 2 else 0
    hs.ellipse(cv, 8.0, 10.0 + oy, 4.6, 5.4, [P[2], P[1], P[1], P[1]])     # the body
    hs.ellipse(cv, 8.0, 11.0 + oy, 3.0, 4.0, [P[3], P[3], P[4], P[4]])     # the white front
    hx = 8.0 + (1.0 if frame == 1 else 0.0)
    hs.ellipse(cv, hx, 4.6 + oy, 3.4, 3.0, [P[2], P[1], P[1], P[1]])       # the head
    cv.set(int(hx) - 2, 4 + oy, P[3]), cv.set(int(hx) + 1, 4 + oy, P[3])
    cv.set(int(hx) - 2, 5 + oy, P[0]), cv.set(int(hx) + 1, 5 + oy, P[0])
    cv.rect(int(hx), 6 + oy, int(hx) + 1, 6 + oy, P[5])                    # the beak
    cv.set(int(hx) - 3, 6 + oy, P[7]), cv.set(int(hx) + 2, 6 + oy, P[7])
    if frame == 2:                                                          # cheering: flippers up
        for i in range(4):
            cv.set(3 - i // 2, 8 - i + oy, P[1]), cv.set(12 + i // 2, 8 - i + oy, P[1])
    else:
        for i in range(4):
            cv.set(3, 8 + i, P[1]), cv.set(12, 8 + i, P[1])
    cv.rect(5, 15 + oy, 6, 15 + oy, P[5]), cv.rect(9, 15 + oy, 10, 15 + oy, P[5])   # the feet
    cv.outline(P[0])
    return cv


def penguins():
    return {"peng%d" % f: penguin(f) for f in range(3)}


# ---- the sea panorama (BG3): 1024 x 256, stage x = panorama x - SEA_X0 ---------------------------------------------------
SEA_X0 = 352
SEA = dict(r1=(160, 214, 242), r2=(124, 192, 234), sp=(236, 250, 255),
           i0=(252, 254, 255), i1=(230, 242, 252), i2=(198, 224, 244), i3=(150, 196, 232), i4=(96, 156, 204),
           sk=(120, 196, 226),
           h0=(222, 74, 70), h1=(160, 42, 52), w0=(222, 172, 112), w1=(170, 120, 72), w2=(116, 78, 46),
           c0=(246, 246, 240), c1=(196, 200, 214), g0=(44, 70, 120), o=(42, 30, 52), fl=(255, 236, 120))
# icebergs and ice bits: (stage x, stage y, width, height, seed); the 320 stage shows the first ones, 427 and 520
# columns more; the top corners (y < 44) stay clear for the HUD (draw.c draw_hud) at every width
BERGS = [(6, 186, 40, 28, 1), (-52, 98, 46, 40, 2), (-98, 176, 36, 30, 3), (378, 60, 36, 30, 4)]
BITS = [(282, 40, 12, 7, 5), (300, 210, 10, 6, 6), (18, 60, 12, 7, 7), (-88, 50, 12, 7, 8), (398, 192, 12, 7, 9),
        (408, 118, 10, 6, 10), (-24, 214, 10, 6, 11), (346, 222, 12, 7, 12)]
BOAT = (330, 86)
# the penguins (src/draw.c): stage positions of their feet, on the bergs
PENGUINS = [(18, 196), (32, 198), (-44, 108), (-30, 106), (-17, 110), (-84, 186), (392, 72)]


def berg(cv, X, Y, w, h, seed):
    """An iceberg from above, in the floe's style: a wobbly top lit from the top-left, its side below, the submerged
    skirt round it."""
    rng = random.Random(seed)
    cx, cy = X + w / 2.0, Y + h / 2.0
    ph = [rng.uniform(0, 6.3) for _ in range(3)]

    def inside(x, y, grow=0.0):
        dx, dy = (x - cx) / (w / 2.0 + grow), (y - cy) / (h / 2.0 + grow)
        a = math.atan2(dy, dx)
        r = 1.0 + 0.10 * math.sin(3 * a + ph[0]) + 0.06 * math.sin(5 * a + ph[1])
        return dx * dx + dy * dy <= r * r * 0.92
    for y in range(int(Y) - 4, int(Y + h) + 6):
        for x in range(int(X) - 4, int(X + w) + 4):
            p = (x + 0.5, y + 0.5)
            if inside(*p):
                up, left = not inside(p[0], p[1] - 2), not inside(p[0] - 2, p[1])
                dn, rt = not inside(p[0], p[1] + 2), not inside(p[0] + 2, p[1])
                col = SEA["i0"] if (up or left) else SEA["i3"] if (dn or rt) else SEA["i1"]
                if col == SEA["i1"] and math.sin(x * 0.7 + y * 0.4 + ph[2]) > 0.75:
                    col = SEA["i2"]
                cv.set(x, y, col)
            elif inside(p[0], p[1] - 3):
                cv.set(x, y, SEA["i4"] if inside(p[0], p[1] - 1.5) else SEA["i3"])
            elif inside(p[0], p[1], 1.6):
                cv.set(x, y, SEA["sk"])


def boat(cv, X, Y):
    """A little fishing boat from above, bow up, a line out to a float."""
    W, H = 26, 54
    cx = X + W / 2.0
    for y in range(Y, Y + H):
        for x in range(X, X + W):
            t = (y - Y) / float(H)
            half = (W / 2.0) * (min(1.0, t / 0.35) ** 0.6 if t < 0.35 else 1.0 - 0.15 * max(0.0, t - 0.85) / 0.15)
            dx = abs(x + 0.5 - cx)
            if dx <= half:
                edge = dx > half - 2.2
                col = SEA["h0"] if edge else (SEA["w0"] if (y // 3) % 2 else SEA["w1"])
                if edge and (x + 0.5 > cx):
                    col = SEA["h1"]
                cv.set(x, y, col)
            elif dx <= half + 1:
                cv.set(x, y, SEA["o"])
    cv.rect(X + 7, Y + 30, X + W - 8, Y + 44, SEA["c0"])                      # the cabin
    cv.rect(X + 7, Y + 44, X + W - 8, Y + 45, SEA["c1"])
    cv.rect(X + 10, Y + 32, X + 15, Y + 34, SEA["g0"])
    cv.rect(X + 7, Y + 30, X + W - 8, Y + 30, SEA["i0"])
    for i in range(14):                                                     # the rod's line, the float
        cv.set(X + 4 - i // 2, Y + 20 + i, SEA["c1"])
    hs_x, hs_y = X - 4, Y + 34
    cv.rect(hs_x, hs_y, hs_x + 1, hs_y + 1, SEA["h0"])
    cv.set(hs_x, hs_y, SEA["fl"])
    for i in range(4):                                                      # its wake
        cv.set(X + 4 + i * 5, Y + H + 1 + (i % 2), SEA["r1"])
        cv.set(X + 5 + i * 5, Y + H + 2, SEA["r2"])


def ripple_tiles():
    """Six 8x8 ripple marks (the sea is the backdrop's gradient: tiles are see-through around them)."""
    out = []
    shapes = [[(1, 3), (2, 2), (3, 2), (4, 3)], [(3, 5), (4, 4), (5, 4), (6, 5)], [(0, 6), (1, 5), (2, 5), (3, 6)],
              [(2, 1), (3, 0), (4, 0), (5, 1)], [(4, 4)], [(2, 5), (3, 4), (4, 4), (5, 4), (6, 5)]]
    for k, pts in enumerate(shapes):
        t = Canvas(8, 8)
        for i, (x, y) in enumerate(pts):
            t.set(x, y, SEA["sp"] if (k == 4) else (SEA["r1"] if i in (1, 2) else SEA["r2"]))
        out.append(t)
    return out


def sea():
    W, H = 1024, 256
    cv = Canvas(W, H)
    rng = random.Random(2026)
    rip = ripple_tiles()
    for ty in range(H // 8):
        for tx in range(W // 8):
            if rng.random() < 0.16:
                cv.paste(rip[rng.randrange(len(rip))], tx * 8, ty * 8)
    for (x, y, w, h, seed) in BITS + BERGS:
        X = x + SEA_X0
        for yy in range(y - 6, y + h + 8):                                 # clear the ripples round them
            for xx in range(X - 6, X + w + 6):
                cv.set(xx, yy, None)
        berg(cv, X, y, w, h, seed)
    bx, by = BOAT
    for yy in range(by - 4, by + 64):
        for xx in range(bx + SEA_X0 - 10, bx + SEA_X0 + 32):
            cv.set(xx, yy, None)
    boat(cv, bx + SEA_X0, by)
    return cv


# ---- sheets -------------------------------------------------------------------------------------------------------------
def pack(sprites, width=256):
    """Shelf-pack named canvases into a sheet: {name: (x, y, w, h)}, height."""
    pos, x, y, row = {}, 0, 0, 0
    for name in sorted(sprites, key=lambda n: (-sprites[n].h, n)):
        cv = sprites[name]
        if x + cv.w > width:
            x, y, row = 0, y + row, 0
        pos[name] = (x, y, cv.w, cv.h)
        x += cv.w
        row = max(row, cv.h)
    return pos, y + row


def sheet(sprites, width=256):
    pos, h = pack(sprites, width)
    im = Image.new("RGB", (width, h), hs.MAGENTA)
    for name, (x, y, w, hh) in pos.items():
        im.paste(sprites[name].image(), (x, y))
    return im, pos


def write_all(out, preview=False):
    os.makedirs(os.path.join(out, "tiles"), exist_ok=True)
    files = {"seals.png": sheet(seal_sprites())[0], "props.png": sheet(props())[0],
             "penguins.png": sheet(penguins(), 64)[0], "tiles/sea.png": sea().image()}
    for f, im in files.items():
        im.save(os.path.join(out, f))
        if preview:
            os.makedirs(os.path.join(out, "preview"), exist_ok=True)
            k = 1 if im.width >= 1024 else 4
            im.resize((im.width * k, im.height * k), Image.NEAREST).save(
                os.path.join(out, "preview", os.path.basename(f)))
    return sorted(files)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", default=os.path.join(GAME, "art"))
    ap.add_argument("--preview", action="store_true", help="also write 4x previews in <out>/preview")
    a = ap.parse_args(argv)
    for f in write_all(a.out, a.preview):
        print("wrote %s/%s" % (a.out, f))


if __name__ == "__main__":
    main()

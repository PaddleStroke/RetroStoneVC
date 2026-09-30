#!/usr/bin/env python3
"""Pancake Tower: the code-drawn art, in the 8BCraft house style (games/common/tools/house_style.py, Leady Squid's
look): flat shades (2-4 per material), 1-px dark outlines that are never black, light from the top-left.

The pancakes and toppings of the tower are drawn at run time by src/render.c (any width, pixel-exact cuts) with
the palettes defined here (TOWER_PAL, TOPPING_PAL); everything else is drawn here:
  sprites   the chef (5 poses) and his portrait (4 heads, a plate-shaped frame), butter pats, syrup drips, the
            syrup bottle and stream, splashes, "+1"/"+2", the falling toppings, plaster, dust, roof tiles, a star
            burst, sweat, the ceiling's and the roof's broken edges, birds, a weather balloon, a plane, a
            satellite, the moon and the cow
  near      five 320x256 scenery segments (BG3, the tower's world): the kitchen with the ceiling, the attic and
            the roof; the sky; the clouds; the stratosphere; space (repeats)
  far       a 256x544 far layer (BG4, 1/4 of the camera): the garden and the neighbours through the window,
            the hills, far clouds, the stars

    python3 games/pancaketower/tools/make_art.py [--out games/pancaketower/art]    (PNG sheets for review)
    build_assets.py imports build() and converts the canvases directly.

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pancaketower/LICENSE.
"""
import argparse
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.dirname(HERE)
ROOT = os.path.abspath(os.path.join(GAME, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "games", "common", "tools"))
import house_style as hs  # noqa: E402
from house_style import Canvas  # noqa: E402

H = hs.HOUSE
PK = hs.ACCENTS["pancake"]            # light .. dark
SY = hs.ACCENTS["syrup"]
FOOD_OUT = (62, 30, 20)               # food outline: a dark brown (never black)
NAVY = H["outline"][0]                # characters and props

# ---- the fixed palettes shared with src/render.c (index = colour number) -------------------------------------
TOWER_PAL = [None, FOOD_OUT, PK[3], PK[2], PK[1], PK[0], SY[2], SY[1], SY[0],
             (255, 246, 184), (240, 204, 92), (250, 214, 150),
             (236, 240, 248), (178, 188, 208), (112, 122, 148), (255, 255, 255)]
T = dict(out=1, crust=2, mid=3, light=4, hi=5, sd=6, sm=7, sl=8, butter=9, butter2=10, crumb=11,
         plate=12, plate2=13, plate3=14, white=15)
TOPPING_PAL = [None, (52, 20, 30), (158, 26, 48), (222, 58, 66), (255, 134, 122), (255, 218, 198),
               (38, 40, 108), (74, 84, 172), (146, 164, 232), (252, 242, 196), (234, 208, 128), (150, 108, 58),
               (58, 30, 22), (124, 72, 44), (255, 252, 244), (222, 214, 214)]
P = dict(out=1, sd=2, sm=3, sl=4, core=5, bd=6, bm=7, bl=8, ban=9, ban2=10, seed=11, cd=12, cl=13, cream=14,
         cream2=15)


def tc(i):
    return TOWER_PAL[i]


def pc(i):
    return TOPPING_PAL[i]


# ---- the chef ---------------------------------------------------------------------------------------------------
CHEF = dict(out=NAVY, skin=H["skin"], white=(250, 250, 252), w2=(214, 222, 238), w3=(158, 170, 200),
            red=(226, 72, 66), red2=(150, 36, 44), mous=(128, 78, 44), mous2=(84, 48, 28),
            trou=(92, 104, 138), trou2=(56, 64, 94), ink=H["ink"][0], cheek=(246, 146, 140))


def _ell(cv, cx, cy, rx, ry, fn):
    for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
        for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
            dx, dy = (x + 0.5 - cx) / rx, (y + 0.5 - cy) / ry
            if dx * dx + dy * dy <= 1.0:
                c = fn(dx, dy, x, y)
                if c:
                    cv.set(x, y, c)


def _white(dx, dy, *a):
    nd = dx * hs.LIGHT_DIR[0] + dy * hs.LIGHT_DIR[1]
    return CHEF["white"] if nd > -0.1 else CHEF["w2"] if nd > -0.62 else CHEF["w3"]


def _skin(dx, dy, *a):
    return hs.shade_nd(dx * hs.LIGHT_DIR[0] + dy * hs.LIGHT_DIR[1], CHEF["skin"][0:3] + [CHEF["skin"][2]])


def _arm(cv, a, b, s):
    """a sleeve (white capsule) from the shoulder a to the hand b, the hand a skin circle"""
    hs.capsule(cv, a[0], a[1], b[0], b[1], 2.3 * s, [CHEF["white"], CHEF["white"], CHEF["w2"], CHEF["w3"]])
    _ell(cv, b[0], b[1], 2.2 * s, 2.2 * s, _skin)


def chef(pose, s=1.0, ox=0.0, oy=0.0, w=32, h=48, portrait=False):
    """The chef: chubby, a tall puffy toque, a moustache, a red neckerchief, a round belly in a white jacket.
    pose: idle, blink, cheer, panic, despair. Coordinates in 'chef units' (the 32x48 frame), scaled by s."""
    cv = Canvas(w, h)
    C = CHEF

    def X(v):
        return ox + v * s

    def Y(v):
        return oy + v * s

    bob = {"cheer": -1.0, "despair": 1.5}.get(pose, 0.0)
    # legs and shoes
    if not portrait:
        for lx in (11.5, 17.5):
            cv.rect(int(X(lx)), int(Y(41 + bob / 2)), int(X(lx + 3)), int(Y(45)), C["trou"])
            cv.set(int(X(lx + 3)), int(Y(43)), C["trou2"])
            _ell(cv, X(lx + 1.5 + (1 if lx > 15 else -1)), Y(46), 3.0 * s, 1.8 * s,
                 lambda dx, dy, *a: C["mous2"] if dy > -0.2 else C["mous"])
    # the belly (jacket) and the apron
    by = 31 + bob

    def belly(dx, dy, x, y):
        c = _white(dx, dy)
        if dy > 0.05 and abs(dx) < 0.72:                                   # the apron
            c = C["white"] if dx < 0.25 else C["w2"]
            if abs(dx) > 0.66 or dy > 0.9:
                c = C["w3"]
        if abs(dx - 0.0) < 0.06 and dy < 0.0 and not portrait:              # the jacket's opening
            c = C["w3"]
        return c
    _ell(cv, X(16), Y(by), 11.2 * s, 10.5 * s, belly)
    # double row of buttons
    for bx in (13.3, 18.7):
        for byy in (by - 7, by - 3.5):
            cv.set(int(X(bx)), int(Y(byy)), C["w3"])
    # the neckerchief
    _ell(cv, X(16), Y(by - 9.2), 6.2 * s, 2.4 * s, lambda dx, dy, *a: C["red"] if dy < 0.3 else C["red2"])
    tri = [(16, by - 8), (13.5, by - 4.2), (18.5, by - 4.2)]
    for yy in range(int(Y(by - 8)), int(Y(by - 4)) + 1):
        for xx in range(int(X(13)), int(X(19.5)) + 1):
            u, v = (xx + 0.5 - ox) / s, (yy + 0.5 - oy) / s
            if v >= tri[0][1] and abs(u - 16) < (v - tri[0][1]) * 0.62 + 0.4 and v <= tri[1][1]:
                cv.set(xx, yy, C["red"] if u < 16.5 else C["red2"])
    # arms (behind or in front of the head depending on the pose)
    arms = {
        "idle": [((6.5, by - 5), (6.8, by + 2.5)), ((25.5, by - 5), (25.2, by + 2.5))],
        "blink": [((6.5, by - 5), (6.8, by + 2.5)), ((25.5, by - 5), (25.2, by + 2.5))],
        "cheer": [((7.0, by - 6), (2.6, by - 18)), ((25.0, by - 6), (29.4, by - 18))],
        "panic": [((7.0, by - 6), (8.6, by - 13.5)), ((25.0, by - 6), (23.4, by - 13.5))],
        "despair": [((7.0, by - 6), (9.0, by - 21.5)), ((25.0, by - 6), (23.0, by - 21.5))],
    }[pose]
    front = pose in ("panic", "despair")
    if not front:
        for a, b in arms:
            _arm(cv, (X(a[0]), Y(a[1])), (X(b[0]), Y(b[1])), s)
    # the head
    hy = 16 + bob
    _ell(cv, X(16), Y(hy), 7.4 * s, 6.8 * s, _skin)
    # cheeks
    for cx in (11.2, 20.8):
        _ell(cv, X(cx), Y(hy + 2.2), 1.5 * s, 1.1 * s, lambda *a: C["cheek"])
    # eyes
    ey = hy - 0.8
    for ex in (13.0, 19.0):
        x0, y0 = int(X(ex)), int(Y(ey))
        if pose in ("blink", "despair"):
            cv.rect(x0 - 1, y0 + 1, x0 + int(s), y0 + 1, C["ink"])
        elif pose == "cheer":                                              # happy: ^ ^
            cv.set(x0 - 1, y0 + 1, C["ink"]), cv.set(x0, y0, C["ink"]), cv.set(x0 + 1, y0 + 1, C["ink"])
            if s > 1.3:
                cv.set(x0 + 2, y0 + 2, C["ink"]), cv.set(x0 - 2, y0 + 2, C["ink"])
        elif pose == "panic":                                              # wide
            r = max(1.6, 1.5 * s)
            _ell(cv, X(ex), Y(ey), r, r * 1.2, lambda *a: C["white"])
            cv.set(int(X(ex)), int(Y(ey)), C["ink"])
            if s > 1.3:
                cv.set(int(X(ex)), int(Y(ey)) + 1, C["ink"])
        else:
            cv.rect(x0, y0, x0 + (1 if s > 1.3 else 0), y0 + (2 if s > 1.3 else 1), C["ink"])
            cv.set(x0 + (1 if s > 1.3 else 0), y0, C["white"])
    # the moustache (two round curls) and the mouth
    for sgn in (-1, 1):
        _ell(cv, X(16 + sgn * 2.6), Y(hy + 2.6), 2.9 * s, 1.5 * s,
             lambda dx, dy, *a: C["mous"] if dy < 0.2 else C["mous2"])
        cv.set(int(X(16 + sgn * 5.6)), int(Y(hy + 1.6)), C["mous2"])
    my = hy + 4.6
    if pose == "cheer":
        _ell(cv, X(16), Y(my), 1.9 * s, 1.3 * s, lambda dx, dy, *a: C["red2"] if dy < 0.3 else C["red"])
    elif pose == "panic":
        _ell(cv, X(16), Y(my + 0.3), 1.2 * s, 1.6 * s, lambda *a: C["red2"])
    elif pose == "despair":
        cv.rect(int(X(14.8)), int(Y(my + 0.4)), int(X(17.2)), int(Y(my + 0.4)), C["mous2"])
    # the toque: a band and a puffy top
    ty = hy - 7.2
    for yy in range(int(Y(ty - 3)), int(Y(ty + 1)) + 1):
        for xx in range(int(X(10.2)), int(X(21.8)) + 1):
            cv.set(xx, yy, shade_col((xx + 0.5 - X(10.2)) / (11.6 * s)))
    for (cx, cy, r) in ((12.0, ty - 6.5, 4.6), (20.0, ty - 6.5, 4.6), (16.0, ty - 8.5, 5.4)):
        _ell(cv, X(cx), Y(cy), r * s, r * 0.85 * s, _white)
    # front arms
    if front:
        for a, b in arms:
            _arm(cv, (X(a[0]), Y(a[1])), (X(b[0]), Y(b[1])), s)
    cv.outline(C["out"])
    return cv


def shade_col(f):
    return CHEF["white"] if f < 0.4 else CHEF["w2"] if f < 0.8 else CHEF["w3"]


CHEF_POSES = ["idle", "blink", "cheer", "panic", "despair"]
HEAD_POSES = ["idle", "cheer", "panic", "despair"]


def chef_head(pose):
    """The portrait: head, toque and shoulders at 1.55x in a 32x32 frame."""
    return chef(pose, s=1.55, ox=16 - 16 * 1.55, oy=31 - 30 * 1.55, w=32, h=32, portrait=True)


# ---- food sprites (the tower palette) -------------------------------------------------------------------------------
def butter(w):
    cv = Canvas(w, 8)
    x0, x1 = (w - (w - 4)) // 2, w - 3
    for y in range(3, 8):
        for x in range(x0, x1 + 1):
            if y == 7 and x in (x0, x1):
                continue
            c = tc(T["butter"]) if y < 5 else tc(T["butter2"])
            if y == 3 and x < x0 + (x1 - x0) // 2:
                c = tc(T["white"])
            cv.set(x, y, c)
    cv.set(x0 - 1, 7, tc(T["butter2"])), cv.set(x1 + 1, 7, tc(T["butter2"]))       # melting
    cv.outline(tc(T["out"]))
    return cv


def drip(frame):
    """a syrup drip: forming (0), stretching (1), a falling drop (2), a splat (3)"""
    cv = Canvas(8, 8)
    S = [tc(T["sl"]), tc(T["sm"]), tc(T["sd"])]
    if frame == 0:
        cv.rect(3, 0, 4, 2, S[1]), cv.set(3, 0, S[0])
    elif frame == 1:
        cv.rect(3, 0, 4, 3, S[1]), cv.rect(3, 4, 4, 5, S[1]), cv.set(3, 1, S[0]), cv.set(4, 5, S[2])
    elif frame == 2:
        cv.rect(3, 2, 4, 5, S[1]), cv.set(3, 3, S[0]), cv.set(4, 5, S[2]), cv.set(3, 1, S[1])
    else:
        cv.rect(1, 6, 6, 6, S[1]), cv.rect(2, 5, 5, 5, S[1]), cv.set(2, 5, S[0]), cv.set(0, 6, S[2]), cv.set(7, 6, S[2])
    cv.outline(tc(T["out"]))
    return cv


def bottle(frame):
    """the syrup bottle, tipped over to pour (frame 1 a little more)"""
    cv = Canvas(16, 32)
    ang = math.radians(115 + frame * 12)
    ca, sa = math.cos(ang), math.sin(ang)
    for y in range(32):
        for x in range(16):
            # local: u along the bottle (0 = the bottom, 22 = the cap), v across
            dx, dy = x + 0.5 - 9, y + 0.5 - 20
            u = dx * ca + dy * sa
            v = -dx * sa + dy * ca
            u += 11
            half = 4.2 if u < 13 else 4.2 - (u - 13) * 0.55 if u < 17 else 2.0 if u < 21 else 0
            if 0 <= u < 21 and abs(v) <= half:
                c = tc(T["sm"])
                if v < -1.8:
                    c = tc(T["sl"])
                if v > 2.3:
                    c = tc(T["sd"])
                if 4 <= u < 10 and abs(v) < 3.2:                            # the label
                    c = tc(T["hi"]) if v < 1.5 else tc(T["light"])
                    if 6 <= u < 8 and abs(v) < 1.3:
                        c = tc(T["sd"])
                if u >= 18:                                                   # the cap
                    c = tc(T["plate2"]) if v < 0.5 else tc(T["plate3"])
                if v < -2.8 and 1 < u < 12:
                    c = tc(T["white"])
                cv.set(x, y, c)
    cv.outline(tc(T["out"]))
    return cv


def glaze():
    """the syrup on the top pancake: a glossy amber puddle (the next pancake will slide on it)"""
    cv = Canvas(16, 8)
    for y in range(8):
        for x in range(16):
            dx, dy = (x + 0.5 - 8) / 7.0, (y + 0.5 - 5.5) / 2.6
            if dx * dx + dy * dy <= 1.0:
                cv.set(x, y, tc(T["sl"]) if dy < -0.2 else tc(T["sm"]))
    cv.set(4, 4, tc(T["white"])), cv.set(5, 4, tc(T["hi"])), cv.set(3, 7, tc(T["sm"])), cv.set(12, 7, tc(T["sd"]))
    cv.outline(tc(T["out"]))
    return cv


def stream():
    cv = Canvas(8, 8)
    for y in range(8):
        cv.set(3, y, tc(T["sl"])), cv.set(4, y, tc(T["sm"]))
    return cv


def splash(frame):
    cv = Canvas(16, 16)
    rng = random.Random(40 + frame)
    r = 4.5 + frame * 1.5
    hs.ellipse(cv, 8, 9, r, r * 0.6, [tc(T["sl"]), tc(T["sl"]), tc(T["sm"]), tc(T["sd"])])
    for k in range(5 + frame * 2):
        a = rng.uniform(0, 2 * math.pi)
        d = r + rng.uniform(1, 3)
        cv.set(int(8 + math.cos(a) * d), int(9 + math.sin(a) * d * 0.7), tc(T["sm"]))
    cv.outline(tc(T["out"]))
    return cv


def plus(n):
    cv = Canvas(16, 8)
    G = hs.FONT
    for i, ch in enumerate("+" + str(n)):
        for y, row in enumerate(G[ch][:7]):
            for x, c in enumerate(row):
                if c == "#":
                    cv.set(1 + i * 6 + x, y, tc(T["butter"]) if y < 4 else tc(T["butter2"]))
    cv.outline(tc(T["out"]))
    return cv


def crumbs(frame):
    cv = Canvas(8, 8)
    pts = [(1, 2), (5, 1), (3, 5), (6, 6)] if frame == 0 else [(2, 1), (6, 3), (1, 6), (4, 4)]
    for x, y in pts:
        cv.set(x, y, tc(T["light"])), cv.set(x + 1, y, tc(T["mid"]))
    return cv


def plate_ring():
    """a quarter (top-left) of the 48x64 plate-rimmed frame of the chef's portrait (drawn 4 times, flipped)"""
    cv = Canvas(24, 32)
    for y in range(32):
        for x in range(24):
            # the distance to the inner rectangle [16, 24) x [16, 32): a rounded rectangle of radius 16
            d = math.hypot(max(0.0, 16 - (x + 0.5)), max(0.0, 16 - (y + 0.5))) + 7.5
            if d < 23.5:
                if d > 20.5:
                    c = tc(T["plate"]) if (x + y) < 22 else tc(T["plate2"])
                elif d > 19.3:
                    c = tc(T["plate3"])
                elif d > 18.3:
                    c = tc(T["plate2"])
                else:
                    c = tc(T["crumb"])
                cv.set(x, y, c)
    for y in range(32):
        for x in range(24):
            if cv.p[y][x] is None and math.hypot(max(0.0, 16 - (x + 0.5)), max(0.0, 16 - (y + 0.5))) + 7.5 < 24.6:
                cv.set(x, y, tc(T["out"]))
    return cv


# ---- toppings falling (the topping palette) -----------------------------------------------------------------------------
def topping_clump(kind):
    cv = Canvas(32, 16)
    if kind == 0:                                     # strawberry slices
        for i, (cx, cy) in enumerate(((7, 10), (15, 8), (23, 10), (11, 12), (20, 12))):
            _ell(cv, cx, cy, 4.2, 3.6, lambda dx, dy, *a: pc(P["sl"]) if dx + dy < -0.6 else pc(P["sm"]) if dy < 0.55 else pc(P["sd"]))
            _ell(cv, cx, cy - 0.3, 2.0, 1.6, lambda *a: pc(P["core"]))
            for sx, sy in ((-2, -1), (2, -1), (0, 2)):
                cv.set(cx + sx, cy + sy, pc(P["sd"]))
    elif kind == 1:                                   # blueberries
        for cx, cy in ((6, 11), (11, 9), (16, 11), (21, 9), (26, 11), (14, 6), (19, 6)):
            _ell(cv, cx, cy, 2.9, 2.9, lambda dx, dy, *a: pc(P["bl"]) if dx + dy < -0.8 else pc(P["bm"]) if dy < 0.5 else pc(P["bd"]))
            cv.set(int(cx), int(cy) - 2, pc(P["bd"]))
    elif kind == 2:                                   # banana slices
        for cx, cy in ((8, 10), (16, 8), (24, 10), (12, 12), (20, 12)):
            _ell(cv, cx, cy, 4.4, 3.0, lambda dx, dy, *a: pc(P["ban"]) if dy < 0.45 else pc(P["ban2"]))
            for a in range(6):
                cv.set(int(cx + math.cos(a) * 1.8), int(cy + math.sin(a) * 1.2), pc(P["seed"]))
    elif kind == 3:                                   # chocolate chips
        for cx, cy in ((6, 12), (10, 9), (15, 12), (19, 8), (23, 12), (27, 10), (13, 6), (21, 5)):
            for y in range(4):
                for x in range(-y // 2 - 1, y // 2 + 2):
                    cv.set(cx + x, cy - 2 + y, pc(P["cl"]) if x < 0 else pc(P["cd"]))
    else:                                             # whipped cream dollop
        for (cx, cy, rx, ry) in ((16, 12, 12, 4), (16, 8.5, 9, 3.4), (16, 5.5, 5.5, 2.6), (17, 3, 2.2, 1.8)):
            _ell(cv, cx, cy, rx, ry, lambda dx, dy, *a: pc(P["cream"]) if dy < 0.35 else pc(P["cream2"]))
    cv.outline(pc(P["out"]))
    return cv


# ---- effects ---------------------------------------------------------------------------------------------------------------
FX = dict(out=NAVY, p1=(246, 242, 232), p2=(206, 200, 190), p3=(150, 142, 138), d1=(232, 220, 196), d2=(196, 182, 160),
          r1=(214, 86, 64), r2=(150, 50, 44), y1=(255, 238, 150), y2=(250, 200, 70), wh=(255, 255, 255),
          b1=(170, 220, 255), b2=(96, 160, 226), wood=(150, 100, 60))


def plaster(k):
    cv = Canvas(8, 8)
    shapes = [[(1, 2), (5, 2), (6, 5), (2, 6)], [(2, 1), (6, 3), (4, 6), (1, 4)], [(2, 2), (5, 1), (6, 6), (1, 5)]][k]
    for y in range(8):
        for x in range(8):
            if _inside(shapes, x + 0.5, y + 0.5):
                cv.set(x, y, FX["p1"] if x + y < 7 else FX["p2"] if k != 2 else FX["wood"])
    cv.outline(FX["out"])
    return cv


def _inside(poly, x, y):
    n, ins = len(poly), False
    for i in range(n):
        (x1, y1), (x2, y2) = poly[i], poly[(i + 1) % n]
        if (y1 > y) != (y2 > y) and x < (x2 - x1) * (y - y1) / (y2 - y1) + x1:
            ins = not ins
    return ins


def dust(frame):
    cv = Canvas(16, 16)
    r = [3.5, 5.5, 6.5][frame]
    for (cx, cy, k) in ((8, 9, 1.0), (5, 8, 0.7), (11, 7, 0.7)):
        rr = r * k
        for y in range(16):
            for x in range(16):
                d = math.hypot(x + 0.5 - cx, y + 0.5 - cy)
                if d < rr and (frame < 2 or hs.checker(x, y) or d < rr * 0.6):
                    cv.set(x, y, FX["d1"] if (x - cx) + (y - cy) < 0 else FX["d2"])
    return cv


def roof_tile(k):
    cv = Canvas(8, 8)
    for y in range(1, 7):
        for x in range(1 + (y > 4), 7 - (y > 4)):
            cv.set(x, y, FX["r1"] if y < 4 - k else FX["r2"])
    cv.outline(FX["out"])
    return cv


def burst(frame):
    cv = Canvas(16, 16)
    r = 7 if frame == 0 else 5
    for a in range(8):
        ang = a * math.pi / 4
        L = r if a % 2 == 0 else r * 0.55
        for t in range(int(L)):
            cv.set(int(8 + math.cos(ang) * t), int(8 + math.sin(ang) * t), FX["y1"] if t < L - 2 else FX["y2"])
    cv.set(8, 8, FX["wh"]), cv.set(7, 8, FX["wh"]), cv.set(8, 7, FX["wh"])
    return cv


def sweat():
    cv = Canvas(8, 8)
    _ell(cv, 4, 5, 2.2, 2.2, lambda dx, dy, *a: FX["b1"] if dx + dy < 0 else FX["b2"])
    cv.set(4, 2, FX["b2"]), cv.set(4, 1, FX["b1"])
    cv.outline(FX["out"])
    return cv


def jag(kind):
    """the broken edge of the ceiling (kind 0, 8x24: plaster, joist, boards) or of the roof (kind 1, 8x16) at the
    left side of the hole (flipped for the right side)"""
    h = 24 if kind == 0 else 16
    cv = Canvas(8, h)
    prof = [4, 6, 3, 5, 7, 4, 2, 5, 6, 3, 5, 4, 6, 3, 2, 5, 6, 4, 3, 5, 6, 4, 5, 3]
    for y in range(h):
        for x in range(prof[y % len(prof)]):
            if kind == 0:
                c = FX["p1"] if y < 4 else FX["wood"] if y < 20 else FX["d2"]
            else:
                c = FX["r1"] if (y // 4) % 2 == 0 else FX["r2"]
            cv.set(x, y, c)
    cv.outline(FX["out"])
    return cv


# ---- sky critters -------------------------------------------------------------------------------------------------------------
CR = dict(out=NAVY, w=(250, 250, 250), g1=(206, 212, 226), g2=(140, 148, 170), k=(40, 36, 52), pk=(244, 160, 170),
          pk2=(200, 104, 124), m1=(255, 248, 206), m2=(236, 222, 160), m3=(196, 180, 120), bl=(90, 150, 220),
          rd=(222, 70, 64), gd=(236, 190, 70), hn=(222, 200, 150))


def bird(frame):
    cv = Canvas(16, 16)
    _ell(cv, 8, 9, 4.2, 2.6, lambda dx, dy, *a: CR["w"] if dy < 0.2 else CR["g1"])
    _ell(cv, 12, 7.5, 2.2, 2.0, lambda *a: CR["w"])
    cv.set(14, 7, CR["gd"]), cv.set(15, 8, CR["gd"]), cv.set(12, 7, CR["k"])
    wy = [3, 11][frame]
    for i in range(6):
        y = 8 + (wy - 8) * i // 5
        cv.set(6 + i // 2, y, CR["g2"] if i > 3 else CR["g1"])
        cv.set(7 + i // 2, y, CR["g1"])
    cv.set(3, 9, CR["g1"]), cv.set(2, 10, CR["g2"])
    cv.outline(CR["out"])
    return cv


def balloon():
    cv = Canvas(16, 32)
    _ell(cv, 8, 8, 7, 7.5, lambda dx, dy, *a: CR["w"] if dx + dy < 0.2 else CR["g1"] if dx + dy < 0.9 else CR["g2"])
    for y in range(15, 26):
        cv.set(8, y, CR["g2"])
    cv.rect(6, 26, 10, 29, CR["rd"]), cv.set(6, 26, CR["w"])
    cv.outline(CR["out"])
    return cv


def plane():
    cv = Canvas(32, 16)
    for x in range(3, 29):
        for y in range(6, 10):
            if x > 26 and y == 6:
                continue
            cv.set(x, y, CR["w"] if y < 8 else CR["g1"])
    cv.rect(24, 7, 25, 7, CR["bl"]), cv.rect(20, 7, 21, 7, CR["bl"]), cv.rect(16, 7, 17, 7, CR["bl"])
    for i in range(6):
        cv.set(12 + i, 10 + i // 2, CR["g1"]), cv.set(13 + i, 10 + i // 2, CR["g2"])
    for i in range(5):
        cv.set(3 + i // 2, 5 - i, CR["rd"]), cv.set(4 + i // 2, 5 - i, CR["rd"])
    cv.rect(3, 6, 8, 6, CR["rd"])
    cv.outline(CR["out"])
    return cv


def satellite():
    cv = Canvas(32, 16)
    cv.rect(13, 5, 18, 10, CR["gd"]), cv.rect(13, 5, 15, 7, CR["m1"])
    for px0 in (1, 21):
        for y in range(5, 11):
            for x in range(px0, px0 + 10):
                cv.set(x, y, CR["bl"] if (x + y) % 3 else CR["g2"])
    cv.rect(11, 7, 12, 8, CR["g2"]), cv.rect(19, 7, 20, 8, CR["g2"])
    cv.set(16, 3, CR["g1"]), cv.set(16, 4, CR["g1"]), cv.set(16, 2, CR["rd"])
    cv.outline(CR["out"])
    return cv


def moon():
    cv = Canvas(32, 32)
    _ell(cv, 16, 16, 15, 15, lambda dx, dy, *a: CR["m1"] if dx + dy < -0.3 else CR["m2"] if dx + dy < 0.7 else CR["m3"])
    for cx, cy, r in ((11, 12, 3.2), (20, 9, 2.0), (19, 20, 3.8), (9, 21, 2.0), (24, 16, 1.6)):
        _ell(cv, cx, cy, r, r, lambda dx, dy, *a: CR["m3"] if dx + dy > -0.3 else CR["hn"])
    cv.outline(CR["out"])
    return cv


def cow(frame):
    """the cow jumping over the moon: legs stretched (0) or tucked (1)"""
    cv = Canvas(32, 24)
    # legs
    legs = [((9, 14), (5, 20)), ((13, 14), (10, 21)), ((21, 14), (24, 21)), ((25, 14), (29, 20))] if frame == 0 else \
        [((9, 14), (8, 18)), ((13, 14), (13, 18)), ((21, 14), (21, 18)), ((25, 14), (26, 18))]
    for a, b in legs:
        hs.capsule(cv, a[0], a[1], b[0], b[1], 1.3, [CR["w"], CR["w"], CR["g1"], CR["g1"]])
        cv.set(int(b[0]), int(b[1]) + 1, CR["k"])
    # body with spots, udder
    _ell(cv, 17, 11, 10.5, 5.5, lambda dx, dy, *a: CR["w"] if dy < 0.45 else CR["g1"])
    for cx, cy, r in ((13, 9, 2.4), (20, 12, 2.8), (24, 8, 1.6)):
        _ell(cv, cx, cy, r, r * 0.8, lambda *a: CR["k"])
    _ell(cv, 17, 16, 2.2, 1.4, lambda *a: CR["pk"])
    # head and tail
    _ell(cv, 28, 7, 3.6, 3.2, lambda dx, dy, *a: CR["w"] if dy < 0.3 else CR["g1"])
    _ell(cv, 30.2, 9, 2.0, 1.5, lambda *a: CR["pk"])
    cv.set(31, 9, CR["pk2"]), cv.set(27, 6, CR["k"]), cv.set(26, 3, CR["hn"]), cv.set(29, 3, CR["hn"])
    for i in range(4):
        cv.set(6 - i, 8 - i // 2, CR["g2"])
    cv.set(2, 5, CR["k"])
    cv.outline(CR["out"])
    return cv


# ---- the medals: the house tiers (bronze, silver, gold, pearl), a golden-fork emblem ----------------------------------
def fork_medal(tier):
    U = hs.UI
    dark, light = [(U["bd"], U["bl"]), (U["sd"], U["sl"]), (U["gd"], U["gl"]), (U["pd"], U["pl"])][tier]
    cv = Canvas(24, 24)
    for y in range(24):
        for x in range(24):
            d = math.hypot(x + 0.5 - 12, y + 0.5 - 12)
            if d < 10.5:
                c = dark if d > 9.2 else light
                if d < 9.2 and (x - 12) + (y - 12) < -8:
                    c = U["white"] if tier != 3 else U["pearl"]
                cv.set(x, y, c)
    # the fork: four tines, the neck, the handle (engraved in the dark shade)
    for tx in (8, 10, 13, 15):
        for y in range(5, 10):
            cv.set(tx, y, dark)
    cv.rect(8, 10, 15, 11, dark)
    cv.rect(11, 12, 12, 19, dark)
    cv.set(11, 12, dark), cv.set(12, 13, light)
    if tier == 3:
        for y in range(24):
            for x in range(24):
                if math.hypot(x + 0.5 - 17, y + 0.5 - 17) < 2.6:
                    cv.set(x, y, U["pearl"] if x + y < 34 else U["pl"])
        cv.set(16, 16, U["white"])
    cv.outline(U["out"])
    return cv


# ---- the sprite list: (name, group, frames: [Canvas]) ---------------------------------------------------------------------
def sprites():
    S = []
    S.append(("chef", "chef", [chef(p) for p in CHEF_POSES]))
    S.append(("butter", "food", [butter(16)]))
    S.append(("butter_big", "food", [butter(24)]))
    S.append(("drip", "food", [drip(i) for i in range(4)]))
    S.append(("bottle", "food", [bottle(0), bottle(1)]))
    S.append(("stream", "food", [stream()]))
    S.append(("glaze", "food", [glaze()]))
    S.append(("splash", "food", [splash(0), splash(1)]))
    S.append(("plus", "food", [plus(1), plus(2)]))
    S.append(("crumbs", "food", [crumbs(0), crumbs(1)]))
    S.append(("ring", "food", [plate_ring()]))
    S.append(("topping", "topping", [topping_clump(k) for k in range(5)]))
    S.append(("plaster", "fx", [plaster(k) for k in range(3)]))
    S.append(("dust", "fx", [dust(k) for k in range(3)]))
    S.append(("rooftile", "fx", [roof_tile(0), roof_tile(1)]))
    S.append(("burst", "fx", [burst(0), burst(1)]))
    S.append(("sweat", "fx", [sweat()]))
    S.append(("jag_ceiling", "fx", [jag(0)]))
    S.append(("jag_roof", "fx", [jag(1)]))
    S.append(("bird", "critter", [bird(0), bird(1)]))
    S.append(("balloon", "critter", [balloon()]))
    S.append(("plane", "critter", [plane()]))
    S.append(("satellite", "critter", [satellite()]))
    S.append(("moon", "critter", [moon()]))
    S.append(("cow", "critter", [cow(0), cow(1)]))
    S.append(("medal", "medal", [fork_medal(k) for k in range(4)]))
    return S


# ---- the near scenery (BG3): five 320x256 segments; image row 0 = the segment's top ---------------------------------------
KIT = dict(out=(48, 34, 40), wall=(250, 236, 206), wall2=(236, 214, 178), wall3=(214, 186, 150),
           tile=(214, 236, 240), tile2=(170, 206, 218), tile3=(120, 160, 180), wood=(206, 146, 88),
           wood2=(164, 104, 58), wood3=(112, 66, 36), floor=(220, 120, 86), floor2=(176, 82, 62),
           frame=(250, 250, 246), pan=(90, 96, 116), pan2=(56, 60, 78), copper=(226, 142, 90),
           jar=(206, 232, 214), jar2=(150, 190, 168), red=(214, 80, 70), green=(122, 176, 88), dark=(64, 42, 36),
           attic=(118, 82, 58), attic2=(90, 60, 44), attic3=(64, 42, 32), roof=(206, 84, 66), roof2=(150, 52, 46),
           roof3=(104, 34, 36), glass=None)


def wy_to_row(seg, wy):
    """world y (up) -> image row of segment seg (256 rows, row 0 at the top)"""
    top = -64 + (seg + 1) * 256 - 1
    return top - wy


def kitchen():
    cv = Canvas(320, 256)
    K = KIT
    R = lambda wy: wy_to_row(0, wy)       # noqa: E731

    # wallpaper (40 .. 96): soft vertical stripes with a small diamond every 16 px
    for y in range(R(95), R(40) + 1):
        for x in range(320):
            c = K["wall"] if (x // 8) % 2 == 0 else K["wall2"]
            if (x % 16 == 12 and (y % 16) == 4) or (x % 16 in (11, 13) and (y % 16) in (3, 5)):
                c = K["wall3"]
            cv.set(x, y, c)
    # the backsplash (0 .. 40, and down to the floor beside the counter): 16-px square tiles with grout
    # (patterns on the 8-px grid keep the tile count low: VRAM)
    for y in range(R(39), R(-40) + 1):
        for x in range(320):
            gx, gy = x % 16, (y - R(39)) % 16
            c = K["tile"] if gx + gy < 17 else K["tile2"]
            if gx == 0 or gy == 0:
                c = K["tile3"]
            elif gx == 1 and gy < 8:
                c = K["frame"]
            cv.set(x, y, c)
    # a dado rail between them
    for y in range(R(41), R(39) + 1):
        for x in range(320):
            cv.set(x, y, K["wood"] if y == R(41) else K["wood2"])
    # the window (x 216..296, wy 22..84): a frame, a cross, the glass transparent (the far layer shows)
    wx0, wx1, wy0, wy1 = 216, 296, 22, 84
    for y in range(R(wy1), R(wy0) + 1):
        for x in range(wx0, wx1 + 1):
            edge = x - wx0 < 4 or wx1 - x < 4 or y - R(wy1) < 4 or R(wy0) - y < 4
            cross = abs(x - (wx0 + wx1) // 2) <= 1 or abs(y - (R(wy1) + R(wy0)) // 2) <= 1
            if edge or cross:
                cv.set(x, y, K["frame"] if (x - wx0 < 2 or y - R(wy1) < 2) or cross and (x + y) % 2 == 0 else K["tile2"])
            else:
                cv.p[y][x] = None
    for x in range(wx0 - 4, wx1 + 5):                     # the sill
        for y in range(R(wy0) + 1, R(wy0) + 4):
            cv.set(x, y, K["wood"] if y == R(wy0) + 1 else K["wood2"])
    # a pot of herbs on the sill
    for y in range(R(wy0) - 7, R(wy0) + 1):
        for x in range(226, 236):
            cv.set(x, y, K["floor"] if y > R(wy0) - 4 else K["green"] if (x + y) % 3 else K["dark"])
    # the shelf (wy 50) with jars, and a pan rail (wy 88) with pans on the left
    for x in range(24, 104):
        cv.set(x, R(50), K["wood"]), cv.set(x, R(49), K["wood2"]), cv.set(x, R(48), K["wood3"])
    for i, (jx, jh, lid) in enumerate(((30, 14, K["red"]), (46, 11, K["green"]), (60, 16, K["copper"]), (78, 12, K["red"]))):
        for y in range(R(51 + jh), R(51) + 1):
            for x in range(jx, jx + 11):
                c = K["jar"] if x < jx + 5 else K["jar2"]
                if y <= R(51 + jh) + 2:
                    c = lid
                cv.set(x, y, c)
        cv.set(jx + 2, R(51 + jh) + 4, K["frame"])
    for x in range(20, 112):
        cv.set(x, R(88), K["pan"])
    for i, (px, r) in enumerate(((34, 7), (58, 9), (86, 6))):
        for y in range(R(87), R(80) + 1):
            cv.set(px, y, K["pan"])
        _ell(cv, px, R(80) + r, r, r * 0.95, lambda dx, dy, *a: K["copper"] if dx + dy < -0.2 else K["floor2"] if dy < 0.6 else K["wood3"])
    # a clock above the window
    _ell(cv, 256, R(91), 5.5, 5.5, lambda dx, dy, *a: K["frame"] if dx * dx + dy * dy < 0.6 else K["wood2"])
    cv.set(256, R(91) - 2, K["dark"]), cv.set(256, R(91) - 1, K["dark"]), cv.set(257, R(91), K["dark"])
    # the counter: the top (wy -10 .. -6) and the cabinets down to the floor (-40)
    for y in range(R(-6), R(-10) + 1):
        for x in range(52, 268):
            cv.set(x, y, K["wood"] if y == R(-6) else K["wood2"])
    for y in range(R(-11), R(-40) + 1):
        for x in range(56, 264):
            dx = (x - 64) % 48                            # four 48-px doors from x 64, side panels
            side = x < 64 or x >= 256
            c = K["tile"] if side or dx not in (0, 47) else K["tile3"]
            if y == R(-11):
                c = K["tile3"]
            elif (not side and dx in (1, 2)) or y == R(-11) + 1:
                c = K["frame"]
            if not side and dx in (21, 22, 23, 24, 25, 26) and y in (R(-15), R(-16)):
                c = K["pan"]                              # handles
            cv.set(x, y, c)
    # the plate on the counter (wy -6 .. -1), the tower's base
    for y in range(R(-1), R(-6) + 1):
        d = R(-1) - y + 5                                 # 5 at the top row .. 0 at the bottom
        half = 58 if d >= 3 else 58 - (3 - d) * 3
        for x in range(160 - half, 160 + half):
            c = tc(T["plate"]) if d >= 4 else tc(T["plate2"]) if d >= 2 else tc(T["plate3"])
            if d == 5 and (x - 160 + half) < 6:
                c = tc(T["white"])
            cv.set(x, y, c)
    # the floor (wy -40 .. -64): terracotta tiles, a checker on the 8-px grid
    for y in range(R(-41), 256):
        for x in range(320):
            row = (y - R(-41)) // 8
            c = K["floor"] if ((x // 16) + row) % 2 == 0 else K["floor2"]
            if (y - R(-41)) % 8 == 0:
                c = K["wood3"]
            cv.set(x, y, c)
    # the ceiling slab (96 .. 120): a cornice, plaster, the joists, the attic floorboards
    for y in range(R(119), R(96) + 1):
        for x in range(320):
            d = R(96) - y                                 # 0 at the bottom
            if d < 3:
                c = K["frame"] if d == 0 else K["wall2"]
            elif d < 6:
                c = K["wall"]
            elif d < 20:
                c = K["wood2"] if (x % 32) < 26 else K["wood3"]
                if (x % 32) in (26, 27):
                    c = K["dark"]
                if d == 6 or d == 19:
                    c = K["wood3"]
            else:
                c = K["wood"] if (x // 24) % 2 else K["wood2"]
                if x % 24 == 0:
                    c = K["wood3"]
            cv.set(x, y, c)
    # the attic (120 .. 168): plank wall (16-px planks), a round window, boxes and a cobweb
    for y in range(R(167), R(120) + 1):
        for x in range(320):
            c = K["attic2"] if (x // 16) % 2 else K["attic"]
            if x % 16 == 0:
                c = K["attic3"]
            cv.set(x, y, c)
    _ell(cv, 44, R(146), 9, 9, lambda dx, dy, *a: K["wood2"] if dx * dx + dy * dy > 0.55 else None)
    for y in range(R(146) - 8, R(146) + 9):                 # the round window's glass: see the far layer
        for x in range(36, 53):
            if math.hypot(x + 0.5 - 44, y + 0.5 - R(146)) < 6.5:
                cv.p[y][x] = None
    for bx, bw, bh in ((236, 22, 14), (260, 16, 10), (246, 14, 9)):
        by0 = 121 if bx != 246 else 135
        for y in range(R(by0 + bh), R(by0) + 1):
            for x in range(bx, bx + bw):
                c = K["copper"] if y < R(by0 + bh) + 2 else K["wood"] if x < bx + bw // 2 else K["wood2"]
                if x == bx + bw // 2 and y < R(by0 + bh) + 5:
                    c = K["wood3"]
                cv.set(x, y, c)
    for i in range(10):                                   # a cobweb in the top-left corner (under the roof)
        cv.set(64 + i, R(160) + i // 2, K["frame"]), cv.set(64, R(160) + i, K["frame"])
    # the roof (an inverted V, the peak at the centre, 10 px of tiles) and the sky above it (transparent)
    for y in range(R(191), R(120) + 1):
        wy = 191 - y
        for x in range(320):
            top = 190 - abs(x + 0.5 - 160) * 0.5
            if wy > top:
                cv.p[y][x] = None
            elif wy > top - 11:
                k = int((top - wy) // 3.6)
                c = [K["roof"], K["roof2"], K["roof"]][min(k, 2)]
                if int(top - wy) % 4 == 0:
                    c = K["roof3"]
                if (x // 8) % 2 == 0 and int(top - wy) % 4 == 1:
                    c = K["roof2"]
                cv.set(x, y, c)
            elif wy > top - 14:
                cv.set(x, y, K["attic3"])
    # a chimney on the right, sticking out of the roof
    for y in range(R(176), R(150) + 1):
        for x in range(262, 278):
            top = 190 - abs(x + 0.5 - 160) * 0.5
            wy = 191 - y
            if wy > top - 8:
                c = K["floor"] if ((x - 262) // 4 + (y // 3)) % 2 else K["floor2"]
                if y <= R(176) + 1:
                    c = K["dark"]
                cv.set(x, y, c)
    cv.outline(K["out"])
    return cv


def sky_seg():
    cv = Canvas(320, 256)
    # a TV antenna on the peak's right, small puffs
    for (cx, cy, r) in ((40, 60, 5), (52, 58, 7), (64, 61, 5), (262, 140, 4), (272, 137, 6), (283, 140, 4)):
        _ell(cv, cx, cy, r * 1.4, r, lambda dx, dy, *a: (250, 250, 252) if dy < 0.3 else (214, 230, 246))
    return cv


def cloud(cv, cx, cy, s, rng):
    parts = [(0, 0, 1.0)] + [(rng.uniform(-1.6, 1.6), rng.uniform(-0.5, 0.2), rng.uniform(0.55, 0.85)) for _ in range(4)]
    for dx, dy, k in parts:
        _ell(cv, cx + dx * s * 10, cy + dy * s * 8, s * 11 * k, s * 8 * k,
             lambda ex, ey, *a: (255, 255, 255) if ex + ey < -0.5 else (240, 246, 252) if ey < 0.45 else (206, 222, 242))


def clouds_seg():
    cv = Canvas(320, 256)
    rng = random.Random(77)
    for (cx, cy, s) in ((40, 200, 1.4), (290, 150, 1.6), (70, 90, 1.1), (270, 40, 1.2), (150, 236, 0.8), (190, 20, 0.9)):
        cloud(cv, cx, cy, s, rng)
    cv.outline((150, 170, 206))
    return cv


def strato_seg():
    cv = Canvas(320, 256)
    rng = random.Random(5)
    for k in range(9):                    # thin streaks on the 8-px grid (their tiles repeat: VRAM)
        y = (k * 27 + rng.randint(0, 8)) // 8 * 8 + 3
        x0 = rng.randint(-5, 32) * 8
        L = rng.randint(6, 14) * 8
        for i in range(L):
            x = x0 + i
            end = i < 8 or i >= L - 8
            if 0 <= x < 320 and (not end or hs.checker(x, y)):
                cv.set(x, y, (206, 214, 246) if i % 8 not in (0, 4) else (160, 176, 228))
                if not end and i % 16 == 5:
                    cv.set(x, y - 1, (160, 176, 228))
    return cv


STAR_SPOTS = [(2, 3), (5, 1), (4, 6), (1, 5)]         # stars sit at a few spots of their 8x8 cell (VRAM: few tiles)


def stars(cv, n, seed, dens=1.0, big=True):
    rng = random.Random(seed)
    for _ in range(n):
        sx, sy = STAR_SPOTS[rng.randrange(len(STAR_SPOTS))]
        x, y = rng.randrange(cv.w // 8) * 8 + sx, rng.randrange(cv.h // 8) * 8 + sy
        k = rng.random()
        if k < 0.7:
            cv.set(x, y, (200, 206, 240))
        elif k < 0.93 or not big:
            cv.set(x, y, (255, 255, 255)), cv.set(x + 1, y, (170, 176, 220))
        else:
            for d in (-1, 1):
                cv.set(x + d, y, (220, 226, 255)), cv.set(x, y + d, (220, 226, 255))
            cv.set(x, y, (255, 255, 255))


def space_seg():
    cv = Canvas(320, 256)
    stars(cv, 70, 11)
    return cv


# ---- the far layer (BG4): 256 wide (repeats), 544 tall; row 0 = the top (far y 543), the bottom = far y 0 --------------
FAR_H = 544


def far():
    cv = Canvas(256, FAR_H)
    R = lambda fy: FAR_H - 1 - fy       # noqa: E731
    G = dict(h1=(150, 196, 120), h2=(116, 166, 100), h3=(84, 130, 86), roof=(196, 104, 84), wall=(240, 226, 206),
             tree=(70, 118, 74), tree2=(52, 92, 64), trunk=(110, 76, 56), hedge=(98, 150, 84), cl=(250, 252, 255),
             cl2=(222, 234, 248), mt=(168, 186, 212), win=(110, 150, 190))
    # far hills (horizon at far y 118), then nearer hills, the garden at the bottom
    for x in range(256):
        h1 = 118 + 10 * math.sin(x / 256.0 * 2 * math.pi * 2 + 0.4) + 5 * math.sin(x / 256.0 * 2 * math.pi * 5)
        h2 = 96 + 8 * math.sin(x / 256.0 * 2 * math.pi * 3 + 1.3)
        for fy in range(0, int(h1)):
            c = G["mt"] if fy > h2 + 4 else G["h1"] if fy > h2 - 12 else G["h2"] if fy > 50 else G["h3"]
            cv.set(x, R(fy), c)
    rng = random.Random(3)
    # houses of the town on the hills
    for i in range(5):
        hx = 14 + i * 50 + rng.randint(-4, 4)
        base = 88 + rng.randint(-6, 6)
        for fy in range(base, base + 9):
            for x in range(hx, hx + 12):
                cv.set(x, R(fy), G["wall"] if fy < base + 6 else G["roof"])
        for k in range(3):
            for x in range(hx + k + 1, hx + 12 - k - 1):
                cv.set(x, R(base + 9 + k), G["roof"])
        cv.set(hx + 3, R(base + 3), G["win"]), cv.set(hx + 8, R(base + 3), G["win"])
    # trees
    for i in range(6):
        tx = rng.randrange(256)
        ty = 60 + rng.randint(0, 20)
        _ell(cv, tx, R(ty + 10), 6, 7, lambda dx, dy, *a: G["tree"] if dx + dy < 0.2 else G["tree2"])
        for fy in range(ty, ty + 4):
            cv.set(tx, R(fy), G["trunk"])
    # far clouds (far y 230 .. 320)
    for (cx, cy, s) in ((40, 250, 0.7), (150, 285, 0.9), (220, 240, 0.6), (95, 315, 0.5)):
        for dx, dy, k in ((0, 0, 1.0), (-1.2, -0.2, 0.7), (1.3, -0.1, 0.75)):
            _ell(cv, cx + dx * 10 * s, R(cy) + dy * 8 * s, 12 * s * k, 6 * s * k,
                 lambda ex, ey, *a: G["cl"] if ey < 0.3 else G["cl2"])
    # stars, sparse lower (far y 330 ..), dense in the repeating top 256 rows (far y 288 .. 543)
    top = Canvas(256, 256)
    stars(top, 60, 21, big=False)
    for y in range(256):
        for x in range(256):
            fy = FAR_H - 1 - y
            if top.p[y][x] is not None and fy >= 288:
                keep = fy >= 360 or (x * 7 + y * 13) % 5 == 0
                if keep:
                    cv.set(x, y, top.p[y][x])
    return cv


# ---- everything --------------------------------------------------------------------------------------------------------------
def build():
    return dict(sprites=sprites(), near=[kitchen(), sky_seg(), clouds_seg(), strato_seg(), space_seg()], far=far())


def sheet(entries):
    """the sprites on one review sheet (magenta background)"""
    rows, y, W = [], 0, 256
    x, rh, pos = 0, 0, []
    for name, g, frames in entries:
        for f in frames:
            if x + f.w > W:
                x, y, rh = 0, y + rh + 2, 0
            pos.append((f, x, y))
            x += f.w + 2
            rh = max(rh, f.h)
    cv = Canvas(W, y + rh)
    for f, fx, fy in pos:
        cv.paste(f, fx, fy)
    return cv


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(GAME, "art"))
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    b = build()
    sheet(b["sprites"]).image().save(os.path.join(a.out, "sprites.png"))
    for i, seg in enumerate(b["near"]):
        seg.image().save(os.path.join(a.out, "near%d.png" % i))
    b["far"].image().save(os.path.join(a.out, "far.png"))
    print("art -> %s" % a.out)


if __name__ == "__main__":
    main()

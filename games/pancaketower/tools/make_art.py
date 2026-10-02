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
# the effects' colours: the debris are the materials of the house exactly (KIT: the plaster, the joists' wood, the
# roof tiles), so a chunk that flies out of the hole has the colours of the hole's edge
FX = dict(out=NAVY, p1=(242, 238, 228), p2=(214, 206, 194), d1=(232, 220, 196), d2=(196, 182, 160),
          r1=(206, 84, 66), r2=(150, 52, 46), y1=(255, 238, 150), y2=(250, 200, 70), wh=(255, 255, 255),
          b1=(170, 220, 255), b2=(96, 160, 226), wood=(164, 104, 58), wood2=(112, 66, 36), smoke=(176, 172, 180))


def plaster(k):
    """plaster chunks (0, 1) and a broken joist end (2)"""
    cv = Canvas(8, 8)
    shapes = [[(1, 2), (5, 2), (6, 5), (2, 6)], [(2, 1), (6, 3), (4, 6), (1, 4)], [(2, 2), (5, 1), (6, 6), (1, 5)]][k]
    for y in range(8):
        for x in range(8):
            if _inside(shapes, x + 0.5, y + 0.5):
                if k == 2:
                    c = FX["wood"] if x + y < 7 else FX["wood2"]
                else:
                    c = FX["p1"] if x + y < 7 else FX["p2"]
                cv.set(x, y, c)
    cv.outline(FX["out"])
    return cv


def splinter(k):
    """a wood splinter, two angles"""
    cv = Canvas(8, 8)
    for i in range(6):
        x, y = (1 + i, 5 - i // 2) if k == 0 else (2 + i // 2, 1 + i)
        cv.set(x, y, FX["wood"]), cv.set(x + (k == 1), y + (k == 0), FX["wood2"])
    cv.outline(FX["out"])
    return cv


def smoke(frame):
    """chimney smoke: a puff growing and thinning as it rises (frames 0..3)"""
    cv = Canvas(16, 16)
    r = [4.0, 5.5, 6.5, 7.5][frame]
    for (cx, cy, k) in ((8, 9, 1.0), (5.5, 8, 0.7), (10.5, 7, 0.7)):
        rr = r * k
        for y in range(16):
            for x in range(16):
                d = math.hypot(x + 0.5 - cx, y + 0.5 - cy)
                if d < rr and (frame < 2 or hs.checker(x, y, frame) or d < rr * (0.7 if frame == 2 else 0.45)):
                    cv.set(x, y, FX["wh"] if (x - cx) + (y - cy) < -1 else FX["d1"] if (y - cy) < 1.5 else FX["smoke"])
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
    S.append(("splinter", "fx", [splinter(0), splinter(1)]))
    S.append(("smoke", "fx", [smoke(k) for k in range(4)]))
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
           attic=(118, 82, 58), attic2=(90, 60, 44), attic3=(64, 42, 36), roof=(206, 84, 66), roof2=(150, 52, 46),
           roof3=(104, 34, 36), roofhi=(234, 128, 98), plaster=(242, 238, 228), plaster2=(214, 206, 194),
           bulb=(255, 240, 170), glass=None)
SKY_W, SKY_2, SKY_3 = (255, 255, 255), (214, 228, 246), (150, 170, 206)     # clouds, streaks and stars

# The near scenery's four BG palettes (build_assets.py fits every 8x8 cell into one of them exactly, no quantising: a
# colour that is in none of a cell's candidates is an error). The kitchen (segment 0, the title's scenery) uses only
# the first two: the fourth is loaded into the title logo's BG palette once the title is gone.
NEAR_PALETTES = [
    [KIT[n] for n in ("wall", "wall2", "wall3", "plaster", "plaster2", "frame", "tile", "tile2", "tile3", "wood",
                      "wood2", "wood3", "dark", "out", "pan")],                         # the kitchen
    [KIT[n] for n in ("wall", "wall2", "wall3", "frame", "red", "floor", "floor2", "copper", "green", "dark", "wood",
                      "wood2", "wood3", "pan", "bulb")],                                 # its things
    [KIT[n] for n in ("attic", "attic2", "dark", "wood", "wood2", "wood3", "out", "copper", "pan", "wall2", "wall3",
                      "plaster", "plaster2", "bulb", "frame")],                          # the attic
    [KIT[n] for n in ("roof", "roof2", "roof3", "roofhi", "floor", "floor2", "wall3", "wood3", "pan", "pan2", "out",
                      "dark")] + [SKY_W, SKY_2, SKY_3],                                  # the roof, the sky
]

# ---- the house, bottom to top (world y, up; tools/../src/tuning.h has the same numbers) ----------------------------
#   -64 .. -41  the kitchen floor          -40 .. 151  the kitchen (the counter, the plate at 0, the window, a lamp)
#   152 .. 191  the kitchen CEILING (cornice, plaster, joists, the attic's floorboards): CEILING_Y .. CEILING_TOP
#   192 .. 287  the attic (planks, a round window, boxes, a bare bulb, the sloping roof in the corners)
#   288 .. 335  the ROOF in section (boards, rafters and insulation, battens, the tiles): ROOF_Y .. ROOF_TOP
#   336 .. 395  the roof seen from above (rows of tiles up to the ridge), the chimney and its smoke, an aerial
#   396 ..      the open sky (the neighbourhood's rooftops are on the far layer, below)
HOUSE_H = 512                         # world -64 .. 447: near segments 0 and 1
CEILING_Y, CEILING_TOP, ROOF_Y, ROOF_TOP, RIDGE_Y = 152, 192, 288, 336, 396   # the open sky above RIDGE_Y
LAMP_X = 100                          # the kitchen lamp hangs from the ceiling here
BULB_X = 244                          # the attic's bare bulb
CHIMNEY_X, CHIMNEY_TOP = 224, 428      # the chimney's centre and its flue (the smoke rises from it: draw.c)
AERIAL_X = 88


def wy_to_row(seg, wy):
    """world y (up) -> image row of segment seg (256 rows, row 0 at the top)"""
    top = -64 + (seg + 1) * 256 - 1
    return top - wy


def house():
    """The house in one 320 x 512 canvas (row 0 = world y 447): the kitchen with its ceiling, the attic, the roof."""
    cv = Canvas(320, HOUSE_H)
    K = KIT
    R = lambda wy: 447 - wy               # noqa: E731

    def band(wy0, wy1, fn):
        for wy in range(wy0, wy1 + 1):
            for x in range(320):
                c = fn(x, wy)
                if c is not None:
                    cv.set(x, R(wy), c)

    # ---- the kitchen ----------------------------------------------------------------------------------------------
    # the wallpaper (43 .. 151): soft vertical stripes with a small diamond every 16 px
    def wallpaper(x, wy):
        y = R(wy)
        c = K["wall"] if (x // 8) % 2 == 0 else K["wall2"]
        if (x % 16 == 12 and (y % 16) == 4) or (x % 16 in (11, 13) and (y % 16) in (3, 5)):
            c = K["wall3"]
        return c
    band(43, 151, wallpaper)

    # the backsplash (-40 .. 39): 16-px square tiles with grout (patterns on the 8-px grid: few tiles, VRAM)
    def tiles(x, wy):
        gx, gy = x % 16, (39 - wy) % 16
        c = K["tile"] if gx + gy < 17 else K["tile2"]
        if gx == 0 or gy == 0:
            c = K["tile3"]
        elif gx == 1 and gy < 8:
            c = K["frame"]
        return c
    band(-40, 39, tiles)
    band(40, 42, lambda x, wy: K["wood"] if wy == 42 else K["wood2"])           # the dado rail
    # the window (x 216..296, wy 52..128): a frame, a cross, the glass transparent (the far layer shows: the garden,
    # the neighbours' house), curtains tied at its sides
    wx0, wx1, wy0, wy1 = 216, 296, 52, 128
    for wy in range(wy0, wy1 + 1):
        for x in range(wx0, wx1 + 1):
            y = R(wy)
            edge = x - wx0 < 4 or wx1 - x < 4 or wy1 - wy < 4 or wy - wy0 < 4
            cross = abs(x - (wx0 + wx1) // 2) <= 1 or abs(wy - (wy0 + wy1) // 2) <= 1
            if edge or cross:
                cv.set(x, y, K["frame"] if (x - wx0 < 2 or wy1 - wy < 2) or cross and (x + y) % 2 == 0 else K["wall3"])
            else:
                cv.p[y][x] = None
    for x in range(wx0 - 4, wx1 + 5):                     # the sill
        for wy in range(wy0 - 3, wy0):
            cv.set(x, R(wy), K["wood"] if wy == wy0 - 1 else K["wood2"])
    for side in (0, 1):                                   # the curtains, gathered by a tie
        for wy in range(wy0 + 6, wy1 + 4):
            t = (wy1 + 3 - wy) / float(wy1 + 3 - wy0 - 6)  # 0 at the top
            wdt = 7 - int(4 * math.sin(min(1.0, t * 1.6) * math.pi * 0.5)) if wy > wy0 + 26 else 3 + (wy0 + 26 - wy) // 4
            wdt = max(3, min(8, wdt))
            for k in range(wdt):
                x = wx0 - 1 - k if side == 0 else wx1 + 1 + k
                c = K["red"] if (k + (wy // 2)) % 4 else K["floor2"]
                cv.set(x, R(wy), c)
        tie = wy0 + 26
        for k in range(0, 8):
            x = wx0 - 1 - k if side == 0 else wx1 + 1 + k
            cv.set(x, R(tie), K["copper"])
    # the shelf (wy 64) with jars, a pan rail (wy 112) with two pans, on the left
    for x in range(18, 82):
        cv.set(x, R(64), K["wood"]), cv.set(x, R(63), K["wood2"]), cv.set(x, R(62), K["wood3"])
    for x in (24, 74):
        for wy in range(56, 62):
            cv.set(x, R(wy), K["wood3"]), cv.set(x + 1, R(wy), K["wood2"])
    for jx, jh, lid in ((22, 14, K["red"]), (38, 11, K["green"]), (52, 16, K["wood3"]), (68, 12, K["red"])):
        for wy in range(65, 65 + jh + 1):
            for x in range(jx, jx + 11):
                c = K["copper"] if x < jx + 5 else K["floor2"]
                if wy >= 65 + jh - 2:
                    c = lid
                cv.set(x, R(wy), c)
        cv.set(jx + 2, R(65 + jh - 4), K["frame"])
    for x in range(16, 70):
        cv.set(x, R(112), K["pan"])
    for px, r in ((30, 8), (56, 10)):
        for wy in range(104, 112):
            cv.set(px, R(wy), K["pan"])
        _ell(cv, px, R(104) + r, r, r * 0.95,
             lambda dx, dy, *a: K["copper"] if dx + dy < -0.2 else K["floor2"] if dy < 0.6 else K["wood3"])
    # a clock above the window
    _ell(cv, 256, R(140), 5.5, 5.5, lambda dx, dy, *a: K["frame"] if dx * dx + dy * dy < 0.6 else K["wood2"])
    cv.set(256, R(140) - 2, K["dark"]), cv.set(256, R(140) - 1, K["dark"]), cv.set(257, R(140), K["dark"])
    # the counter: the top (wy -10 .. -6) and the cabinets down to the floor (-40)
    band(-10, -6, lambda x, wy: (K["wood"] if wy == -6 else K["wood2"]) if 52 <= x < 268 else None)

    def cabinets(x, wy):
        if not 56 <= x < 264:
            return None
        dx = (x - 64) % 48                                # four 48-px doors from x 64, side panels
        side = x < 64 or x >= 256
        c = K["tile"] if side or dx not in (0, 47) else K["tile3"]
        if wy == -11:
            c = K["tile3"]
        elif (not side and dx in (1, 2)) or wy == -12:
            c = K["frame"]
        if not side and dx in (21, 22, 23, 24, 25, 26) and wy in (-15, -16):
            c = K["pan"]                                  # handles
        return c
    band(-40, -11, cabinets)
    # the plate on the counter (wy -6 .. -1), the tower's base
    for wy in range(-6, 0):
        d = wy + 6 - 1                                    # 5 at the top row .. 0 at the bottom
        half = 58 if d >= 3 else 58 - (3 - d) * 3
        for x in range(160 - half, 160 + half):
            c = K["frame"] if d >= 4 else K["tile2"] if d >= 2 else K["tile3"]
            if d == 5 and (x - 160 + half) < 6:
                c = K["tile"]
            cv.set(x, R(wy), c)
    # the floor (wy -64 .. -41): terracotta tiles, a checker on the 8-px grid
    band(-64, -41, lambda x, wy: K["wood3"] if (-41 - wy) % 8 == 0 else
         K["floor"] if ((x // 16) + (-41 - wy) // 8) % 2 == 0 else K["floor2"])

    # ---- the kitchen ceiling (152 .. 191): a cornice, the plaster, the joists' cut ends, the attic's floorboards ----
    def ceiling(x, wy):
        d = wy - CEILING_Y                                # 0 at the bottom (the ceiling seen from the kitchen)
        if d < 6:                                         # the cornice: a stepped moulding
            return [K["wall3"], K["frame"], K["plaster2"], K["plaster"], K["frame"], K["plaster2"]][d]
        if d < 20:                                        # the plaster
            return K["plaster2"] if d == 19 else K["plaster"]
        if d < 36:                                        # joists (cut ends, every 32 px) and the dark void
            jx = x % 32
            if jx < 12:
                if d == 20 or d == 35 or jx in (0, 11):
                    return K["wood3"]
                return K["wood2"] if (jx + d) % 7 else K["wood"]
            return K["dark"] if d > 21 else K["wood3"]
        return K["wood3"] if x % 24 == 0 or d == 36 else K["wood"] if (x // 24) % 2 else K["wood2"]   # floorboards
    band(CEILING_Y, CEILING_TOP - 1, ceiling)
    # the lamp: a cord from a ceiling rose, an enamel shade, the bulb's glow under it
    for k in range(-5, 6):
        cv.set(LAMP_X + k, R(CEILING_Y + 6), K["plaster2"] if abs(k) == 5 else K["frame"])
    for k in range(-3, 4):
        cv.set(LAMP_X + k, R(CEILING_Y + 7), K["plaster2"])
    for wy in range(126, CEILING_Y + 6):
        cv.set(LAMP_X, R(wy), K["pan"])
    for wy in range(114, 127):
        h = wy - 114                                      # 0 = the rim
        half = 12 - (h * h) // 16 if h < 12 else 3
        for x in range(LAMP_X - half, LAMP_X + half + 1):
            c = K["red"] if x < LAMP_X + half // 3 else K["floor2"]
            if h == 0:
                c = K["floor2"]
            if h == 12:
                c = K["pan"]
            cv.set(x, R(wy), c)
    for x in range(LAMP_X - 4, LAMP_X + 5):
        for wy in range(110, 114):
            if (x - LAMP_X) ** 2 * 4 + (wy - 113) ** 2 * 9 < 64:
                cv.set(x, R(wy), K["bulb"] if wy > 110 else K["frame"])

    # ---- the attic (192 .. 287) -------------------------------------------------------------------------------------
    def planks(x, wy):
        c = K["attic2"] if (x // 16) % 2 else K["attic"]
        if x % 16 == 0:
            c = K["attic3"]
        if x % 16 in (3, 12) and (wy - 196) % 48 == 0:
            c = K["attic3"]                               # nail heads
        return c
    band(CEILING_TOP, ROOF_Y - 1, planks)
    # the roof slopes down into the corners: its underside, boards parallel to the slope (a 45-degree pattern on the
    # 8-px grid: few tiles), a rafter along the edge
    for wy in range(CEILING_TOP + 40, ROOF_Y):
        reach = wy - (CEILING_TOP + 40)                   # 45 degrees: from 0 px at wy 232 to 55 px at 287
        for side in (0, 1):
            for k in range(reach + 1):
                x = k if side == 0 else 319 - k
                e = reach - k                             # distance to the rafter, along x
                if e < 2:
                    c = K["attic3"]
                elif e < 5:
                    c = K["wood2"]
                else:
                    c = K["wood3"] if (x + wy) % 8 == 0 else K["attic"] if (x + wy) % 16 < 8 else K["attic2"]
                cv.set(x, R(wy), c)
    # a round window (the far layer shows the neighbours' rooftops)
    wcx, wcy = 40, 222
    _ell(cv, wcx, R(wcy), 10, 10, lambda dx, dy, *a: K["wood2"] if dx + dy < 0.3 else K["wood3"])
    for wy in range(wcy - 9, wcy + 10):
        for x in range(wcx - 9, wcx + 10):
            d = math.hypot(x + 0.5 - wcx, wy + 0.5 - wcy)
            if d < 7.2 and abs(x + 0.5 - wcx) > 0.8 and abs(wy + 0.5 - wcy) > 0.8:
                cv.p[R(wy)][x] = None
    # a travel trunk under it, boxes on the right, a cobweb in the corner
    for wy in range(192, 209):
        for x in range(14, 64):
            c = K["copper"] if wy in (200, 201) else K["wood"] if x < 38 else K["wood2"]
            if wy >= 206:
                c = K["wood3"] if wy == 208 else K["copper"]
            if x in (14, 63) or wy == 192:
                c = K["wood3"]
            if x in (36, 37) and 198 <= wy <= 203:
                c = K["pan"]
            cv.set(x, R(wy), c)
    for bx, bw, by0, bh in ((228, 28, 192, 18), (258, 24, 192, 14), (236, 20, 211, 13), (262, 16, 207, 10)):
        for wy in range(by0, by0 + bh):
            for x in range(bx, bx + bw):
                c = K["wall3"] if wy >= by0 + bh - 2 else K["wood"] if x < bx + bw // 2 else K["wood2"]
                if x == bx + bw // 2 and wy >= by0 + bh - 6:
                    c = K["wall2"]                        # the tape
                if x in (bx, bx + bw - 1) or wy == by0:
                    c = K["wood3"]
                cv.set(x, R(wy), c)
    for i in range(12):                                   # the cobweb in the slope's corner
        cv.set(58 + i, R(ROOF_Y - 2 - i // 2), K["plaster2"])
        cv.set(58 + i // 2, R(ROOF_Y - 2 - i), K["plaster2"])
    cv.set(64, R(ROOF_Y - 6), K["plaster"])
    # the bare bulb on its cord
    for wy in range(266, ROOF_Y):
        cv.set(BULB_X, R(wy), K["pan"])
    for wy in range(262, 266):
        for x in range(BULB_X - 2, BULB_X + 3):
            cv.set(x, R(wy), K["pan"] if wy == 265 else K["bulb"])
    for x in range(BULB_X - 3, BULB_X + 4):
        for wy in range(255, 262):
            if (x - BULB_X) ** 2 * 3 + (wy - 258) ** 2 * 3 < 30:
                cv.set(x, R(wy), K["bulb"] if (x - BULB_X) + (258 - wy) < 2 else K["frame"])

    # ---- the roof in section (288 .. 335): the attic's ceiling boards, rafters and insulation, battens, tiles ----------
    def roof_section(x, wy):
        d = wy - ROOF_Y
        if d < 4:
            return K["wood3"] if d == 0 or x % 24 == 0 else K["wood"] if (x // 24) % 2 else K["wood2"]
        if d < 28:                                        # rafters (cut, every 40 px) and the insulation
            rx = x % 40
            if rx < 10:
                if rx in (0, 9) or d in (4, 27):
                    return K["attic3"]
                return K["attic"] if (rx + d) % 9 else K["attic2"]
            k = ((x % 8) * 7 + (d // 3) * 13) % 11       # a fluffy pattern of period 8: few tiles (VRAM)
            return K["wall2"] if k < 6 else K["wall3"] if k < 9 else K["plaster"]
        if d < 32:                                        # the battens
            return K["dark"] if d == 28 or x % 16 >= 12 else K["wood3"]
        # side view: a dark eave and an overlapping terracotta tile lip
        ty = d - 32
        if ty < 8:
            return K["wood3"] if ty == 0 else K["wood2"]
        if ty < 10:
            return K["roof3"]
        if ty == 15:
            return K["roofhi"]
        return K["roof2"] if x % 16 == 0 else K["roof"]
    band(ROOF_Y, ROOF_TOP - 1, roof_section)

    # The roof is seen from the side. Above the tile lip is open sky,
    # rather than a second, perspective roof glued on top of the section.
    # The chimney stays as a familiar silhouette while the roof drops below.
    # the chimney (it stands on the roof and rises above the ridge), smoke is a sprite
    cx0, cx1, cy0, cy1 = CHIMNEY_X - 12, CHIMNEY_X + 12, 350, CHIMNEY_TOP - 4
    for wy in range(cy0, cy1 + 1):
        for x in range(cx0, cx1):
            course = (wy - cy0) // 4
            bx = (x - cx0 + (4 if course % 2 else 0)) % 8
            c = K["floor"] if x < cx0 + 16 else K["floor2"]
            if (wy - cy0) % 4 == 3 or bx == 7:
                c = K["wall3"]
            if wy >= cy1 - 5:                             # the cap
                c = K["pan"] if wy >= cy1 - 1 or x < cx0 + 12 else K["pan2"]
            if x in (cx0, cx1 - 1) and wy < cy1 - 5:
                c = K["wood3"]
            cv.set(x, R(wy), c)
    for x in range(cx0 - 2, cx1 + 2):                     # the cap overhangs
        for wy in (cy1 - 5, cy1 - 4):
            cv.set(x, R(wy), K["pan2"] if wy == cy1 - 5 else K["pan"])
    for x in range(cx0 + 3, cx1 - 3):                     # the flue
        cv.set(x, R(cy1 + 1), K["pan2"])
    for x in range(cx0 - 3, cx1 + 3):                     # flashing where it meets the roof
        cv.set(x, R(cy0), K["pan"]), cv.set(x, R(cy0 + 1), K["pan2"])
    # a TV aerial on the left
    for wy in range(ROOF_TOP + 2, 440):
        cv.set(AERIAL_X, R(wy), K["pan2"]), cv.set(AERIAL_X + 1, R(wy), K["pan"])
    for wy, half in ((436, 14), (428, 11), (420, 8)):
        for x in range(AERIAL_X - half, AERIAL_X + 2 + half):
            cv.set(x, R(wy), K["pan"])
        for x in (AERIAL_X - half, AERIAL_X + 1 + half):
            cv.set(x, R(wy - 1), K["pan2"])
    cv.outline(K["out"])
    return cv


def house_segments():
    """the house canvas -> near segments 0 (world -64..191) and 1 (192..447)"""
    hv = house()
    segs = []
    for top in (256, 0):
        cv = Canvas(320, 256)
        for y in range(256):
            cv.p[y] = list(hv.p[top + y])
        segs.append(cv)
    return segs


def cloud(cv, cx, cy, s, rng):
    parts = [(0, 0, 1.0)] + [(rng.uniform(-1.6, 1.6), rng.uniform(-0.5, 0.2), rng.uniform(0.55, 0.85)) for _ in range(4)]
    for dx, dy, k in parts:
        _ell(cv, cx + dx * s * 10, cy + dy * s * 8, s * 11 * k, s * 8 * k,
             lambda ex, ey, *a: SKY_W if ey < 0.45 else SKY_2)


def clouds_seg():
    """two cloud shapes, each stamped three times on the 8-px grid (their tiles are shared: VRAM)"""
    cv = Canvas(320, 256)
    shapes = []
    for seed, sc in ((77, 1.4), (78, 1.0)):
        sh = Canvas(80, 48)
        cloud(sh, 40, 28, sc, random.Random(seed))
        shapes.append(sh)
    for (x, y, k) in ((0, 176, 0), (232, 120, 0), (48, 64, 1), (224, 16, 1), (120, 208, 1), (152, 0, 0)):
        sh = shapes[k]
        for yy in range(sh.h):
            for xx in range(sh.w):
                if sh.p[yy][xx] is not None and 0 <= x + xx < 320 and 0 <= y + yy < 256:
                    cv.p[y + yy][x + xx] = sh.p[yy][xx]
    cv.outline(SKY_3)
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
                cv.set(x, y, SKY_2 if i % 8 not in (0, 4) else SKY_3)
                if not end and i % 16 == 5:
                    cv.set(x, y - 1, SKY_3)
    return cv


STAR_SPOTS = [(2, 3), (5, 1), (4, 6), (1, 5)]         # stars sit at a few spots of their 8x8 cell (VRAM: few tiles)


def stars(cv, n, seed, dens=1.0, big=True):
    rng = random.Random(seed)
    for _ in range(n):
        sx, sy = STAR_SPOTS[rng.randrange(len(STAR_SPOTS))]
        x, y = rng.randrange(cv.w // 8) * 8 + sx, rng.randrange(cv.h // 8) * 8 + sy
        k = rng.random()
        if k < 0.7:
            cv.set(x, y, SKY_2)
        elif k < 0.93 or not big:
            cv.set(x, y, SKY_W), cv.set(x + 1, y, SKY_3)
        else:
            for d in (-1, 1):
                cv.set(x + d, y, SKY_2), cv.set(x, y + d, SKY_2)
            cv.set(x, y, SKY_W)


def space_seg():
    cv = Canvas(320, 256)
    stars(cv, 70, 11)
    return cv


# ---- the far layer (BG4): 256 wide (repeats), FAR_H tall; row 0 = the top, the bottom = far y 0 ---------------------
# It moves at 1/4 of the camera (far y = (camera - CAM0) / 4 + 239 - screen line), so each band is seen where it belongs:
#   far   0 .. 103  the lawn (behind the kitchen wall)
#       104 .. 181  the garden: a fence, the neighbours' houses, trees (through the kitchen window)
#       182 .. 225  the neighbourhood's rooftops (through the attic window, and below the roof once the tower is out)
#       226 .. 249  the hills on the horizon          300 .. 390  far clouds          400 ..  stars (the top 256 repeat)
FAR_H = 704
FG = dict(h1=(150, 196, 120), h2=(116, 166, 100), h3=(84, 130, 86), mt=(168, 186, 212), roof=(196, 104, 84),
          roof2=(150, 80, 70), slate=(110, 120, 150), wall=(240, 226, 206), wall2=(206, 190, 170), win=(110, 150, 190),
          tree=(70, 118, 74), tree2=(52, 92, 64), cl=(250, 252, 255), cl2=(222, 234, 248), dark=(88, 64, 60))


def _far_house(cv, R, x0, base, w, wall_h, roof_h, roof, roof_dark, wall, chimney=False, door=True):
    """a house seen from the front: the walls (base .. base + wall_h), a gable roof, windows on the 8-px grid"""
    G = FG
    for fy in range(base, base + wall_h):
        for x in range(x0, x0 + w):
            c = wall if x < x0 + w - 3 else G["wall2"]
            k, wx = fy - base, (x - x0) % 16
            if wall_h > 12 and 6 <= k < 14 and 4 <= wx < 12:
                c = G["win"] if not (wx == 7 or k == 9) else G["wall"]
            elif wall_h <= 12 and 2 <= k < 5 and 5 <= wx < 10:
                c = G["win"]
            if door and k < 11 and x0 + w // 2 - 3 <= x < x0 + w // 2 + 3:
                c = G["dark"]
            cv.set(x, R(fy), c)
    for k in range(roof_h):
        inset = k * (w // 2 + 2) // roof_h
        for x in range(x0 - 2 + inset, x0 + w + 2 - inset):
            c = roof if x < x0 + w // 2 else roof_dark
            if k == 0:
                c = roof_dark
            cv.set(x, R(base + wall_h + k), c)
    if chimney:
        for fy in range(base + wall_h + roof_h // 3, base + wall_h + roof_h * 2 // 3 + 4):
            for x in range(x0 + w - 12, x0 + w - 7):
                cv.set(x, R(fy), G["roof2"] if x < x0 + w - 9 else G["dark"])


def _far_tree(cv, R, cx, base, r):
    G = FG
    for fy in range(base, base + 4):
        cv.set(cx, R(fy), G["dark"])
    _ell(cv, cx, R(base + 3 + r), r, r * 1.1, lambda dx, dy, *a: G["tree"] if dx + dy < 0.1 else G["tree2"])


def far():
    cv = Canvas(256, FAR_H)
    R = lambda fy: FAR_H - 1 - fy       # noqa: E731
    G = FG
    # the hills on the horizon (226 .. 249) and the land below them
    for x in range(256):
        h = 238 + 6 * math.sin(x / 64.0 * 2 * math.pi + 0.4)            # a 64-px period: few tiles (VRAM)
        for fy in range(0, int(h)):
            c = G["mt"] if fy > 222 else G["h1"] if fy > 120 else G["h2"]
            if fy <= 103:
                c = G["h3"] if fy < 96 else G["h2"]
            cv.set(x, R(fy), c)
    # the neighbourhood's rooftops: a back row (smaller, slate, fainter) and a front row (red and brown roofs)
    # (stamps repeated on the 8-px grid: their tiles are shared)
    for i, x0 in enumerate(range(20, 256, 32)):
        _far_house(cv, R, x0, 208, 24, 6, 8, G["slate"], G["mt"], G["wall2"], chimney=i % 2 == 1, door=False)
    for i, x0 in enumerate(range(4, 256, 32)):
        roof = (G["roof"], G["roof2"]) if i % 2 == 0 else (G["roof2"], G["dark"])
        _far_house(cv, R, x0, 184, 24, 9, 12, roof[0], roof[1], G["wall"], chimney=i % 4 == 0, door=False)
    for cx in (32, 160):
        _far_tree(cv, R, cx, 182, 6)
    # the garden (104 .. 181): the lawn, the neighbours' houses, trees, a fence in front
    for fy in range(104, 124):
        for x in range(256):
            cv.set(x, R(fy), G["h1"] if (x // 8 + fy // 8) % 2 else G["h2"])
    _far_house(cv, R, 16, 120, 48, 28, 22, G["roof"], G["roof2"], G["wall"], chimney=True)
    _far_house(cv, R, 144, 120, 48, 24, 20, G["slate"], G["dark"], G["wall2"])
    _far_tree(cv, R, 100, 118, 14)
    _far_tree(cv, R, 228, 118, 15)
    for fy in range(108, 122):                            # the fence
        for x in range(256):
            px = x % 8
            if fy >= 119 and px > 4:
                continue
            c = G["wall"] if px < 5 else G["wall2"]
            if fy in (112, 113):
                c = G["wall2"]
            cv.set(x, R(fy), c)
    # far clouds
    for (cx, cy, s) in ((40, 330, 0.7), (150, 362, 0.9), (220, 312, 0.6), (95, 385, 0.5)):
        for dx, dy, k in ((0, 0, 1.0), (-1.2, -0.2, 0.7), (1.3, -0.1, 0.75)):
            _ell(cv, cx + dx * 10 * s, R(cy) + dy * 8 * s, 12 * s * k, 6 * s * k,
                 lambda ex, ey, *a: G["cl"] if ey < 0.3 else G["cl2"])
    # stars: sparse from 400, dense in the repeating top 256 rows (FAR_H - 256 ..)
    top = Canvas(256, 256)
    rng = random.Random(21)
    for _ in range(64):
        sx, sy = STAR_SPOTS[rng.randrange(len(STAR_SPOTS))]
        x, y = rng.randrange(32) * 8 + sx, rng.randrange(32) * 8 + sy
        k = rng.random()
        top.set(x, y, G["cl"] if k < 0.4 else G["cl2"] if k < 0.75 else G["mt"])
    for y in range(256):
        for x in range(256):
            if top.p[y][x] is None:
                continue
            for fy in (FAR_H - 1 - y, FAR_H - 1 - y - 256):
                if fy >= FAR_H - 256 or (fy >= 400 and (x * 7 + y * 13) % 3 == 0):
                    cv.set(x, R(fy), top.p[y][x])
    return cv


# ---- everything --------------------------------------------------------------------------------------------------------------
def build():
    return dict(sprites=sprites(), near=house_segments() + [clouds_seg(), strato_seg(), space_seg()], far=far())


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

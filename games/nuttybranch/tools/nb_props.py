#!/usr/bin/env python3
"""Nutty Branch: the world's props, traps and decorations, drawn by nb_render.py in the game's own view at every
pre-drawn size (LODS, px per world unit: draw.c picks the nearest to the projected scale) and, for the things that
sit at an angle round the branch, rolled with the bark (TILTS degrees; the left side is the mirror).

Each function returns a list of frames (Canvas, ax, ay), the anchor being the point on the bark the game projects.
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE.
"""
import math
import random

import numpy as np

import house_style as hs
import nb_render as nr
from nb_pal import (BIRD, COL, WOOD, R_ACORN, R_BARK, R_BARKD, R_BLACK, R_CAP, R_CAPA, R_DARK, R_GOLD, R_GREEN,
                    R_GREY, R_HAZEL, R_LEAF, R_MOSS, R_OWL, R_SAP, R_SNOW, R_WOOD, R_YEL)

Canvas = hs.Canvas
LODS = [1.6, 1.15, 0.83, 0.6, 0.43, 0.31, 0.22, 0.16]
TILTS = [0, 18, 36]
BR = 64.0                                             # the branch radius
Ell = nr.Ell


def to_canvas(px, anchor, size):
    """The renderer's pixels -> a Canvas sized to multiples of 8 (the anchor kept)."""
    if not px:
        return Canvas(8, 8), 4, 4
    x0, x1 = min(x for x, y in px), max(x for x, y in px) + 1
    y0, y1 = min(y for x, y in px), max(y for x, y in px) + 1
    W, H = x1 - x0, y1 - y0
    W8, H8 = max(8, (W + 7) // 8 * 8), max(8, (H + 7) // 8 * 8)
    ox, oy = (W8 - W) // 2 - x0, H8 - H - y0
    cv = Canvas(W8, H8)
    for (x, y), c in px.items():
        cv.set(x + ox, y + oy, c)
    return cv, int(math.floor(anchor[0] + ox)), int(math.floor(anchor[1] + oy))


def draw(prims, k, roll=0.0, shadow=None, out=WOOD["out"], shadow_col=WOOD["dark"], extra=None, crease=True):
    px, anc, size = nr.render(prims, k, math.radians(roll), shadow, out_col=None, shadow_col=shadow_col,
                              crease=crease)
    if extra:
        extra(px, anc, size, k, roll)
    if out is not None:                                   # the house outline, after the extras
        W, H = size
        add = {}
        for (x, y), c in px.items():
            if c == shadow_col and (x + y) % 2 == 0 and shadow:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                q = (x + dx, y + dy)
                if q not in px and 0 <= q[0] < W and 0 <= q[1] < H:
                    add[q] = out
        px.update(add)
    return to_canvas(px, anc, size)


def surf(a_deg, h=0.0):
    """The point of the cylinder's surface at angle a (from the top), raised by h, in the frame whose origin is the
    top of the branch."""
    a = math.radians(a_deg)
    return ((BR + h) * math.sin(a), (BR + h) * math.cos(a) - BR, 0.0)


def leaf_cluster(c, n, spread, rng, mat=R_LEAF, size=3.0):
    out = []
    for _ in range(n):
        p = np.array(c) + np.array([rng.uniform(-1, 1), rng.uniform(-0.6, 1), rng.uniform(-1, 1)]) * spread
        Rm = nr.rot_z(rng.uniform(-1.2, 1.2)) @ nr.rot_x(rng.uniform(-1.3, 0.3)) @ nr.rot_y(rng.uniform(0, 6.3))
        out.append(Ell(p, (size, size * 0.22, size * 0.62), mat, Rm))
    return out


def ring_tex(rings):
    """rings round the local y axis (Pl: the unit-sphere point)"""
    def f(Pl, n):
        d = np.sqrt(np.nan_to_num(Pl[..., 0] ** 2 + Pl[..., 2] ** 2, posinf=0))
        return (d * rings).astype(int) % 2
    return f


def stripe_tex(axis, n):
    """n bands across the unit sphere along an axis"""
    def f(Pl, nn):
        return ((np.nan_to_num(Pl[..., axis], posinf=0, neginf=0) + 1) * n).astype(int) % 2
    return f


# ---- nuts (OBJ 6) ---------------------------------------------------------------------------------------------------
def acorn(k):
    prims = [Ell((0, 3.6, 0), (3.2, 3.6, 3.2), R_ACORN), Ell((0, 6.4, 0), (3.6, 1.8, 3.6), R_CAPA, tex=stripe_tex(0, 3)),
             Ell((0.3, 8.4, 0), (0.7, 1.0, 0.7), R_CAPA)]
    return draw(prims, k, 0, (0, 0, 3.6, 2.6), COL["out"], COL["out"])


def hazel(k):
    prims = [Ell((0, 3.8, 0), (3.8, 3.8, 3.8), R_HAZEL), Ell((0, 2.0, 0), (3.6, 1.4, 3.6), R_CAPA)]
    return draw(prims, k, 0, (0, 0, 4, 3), COL["out"], COL["out"])


def walnut(k):
    prims = [Ell((0, 4.6, 0), (4.6, 4.4, 4.6), R_GOLD, tex=stripe_tex(0, 3))]
    return draw(prims, k, 0, (0, 0, 5, 3.4), COL["out"], COL["out"])


# ---- the traps on the bark (OBJ 2 unless said) --------------------------------------------------------------------------
TWIG_WOOD = [WOOD["b0"], WOOD["b0"], WOOD["b1"], WOOD["b2"]]


def twig(k, long=True):
    """A side shoot growing out of the bark at 74 degrees on the right, arching over the path (to -6 degrees when long,
    30 when short) a few units up, a few leaves on it. The anchor is the top of the branch."""
    rng = random.Random(4 if long else 5)
    tip = -6.0 if long else 30.0
    pts, prims = [], []
    n = 16
    for i in range(n + 1):
        t = i / float(n)
        a = 74.0 + (tip - 74.0) * t
        h = 9.0 * math.sin(math.pi * min(1.0, t * 1.15) * 0.5) + 1.0
        pts.append(surf(a, h))
    for i in range(n):
        r = 1.9 - 1.1 * i / n
        prims += nr.capsule(pts[i], pts[i + 1], r, r - 1.1 / n, TWIG_WOOD, n=3)
    for t in (0.45, 0.7, 1.0):                           # leaves on short stalks
        p = np.array(pts[int(t * n)]) + np.array([0, 2.0, rng.uniform(-2, 2)])
        prims += leaf_cluster(p, 2, 2.0, rng, size=3.2)
    sx = (surf(74)[0] + surf(tip)[0]) / 2
    return draw(prims, k, 0, (sx, 0, abs(surf(74)[0] - surf(tip)[0]) / 2 + 2, 4))


def burl(k, roll):
    """A raised knot: a knobbly bulge of the branch's own bark, rings round it."""
    rng = random.Random(9)
    prims = [Ell((0, 4.0, 0), (17.5, 9.0, 12.0), R_BARKD, tex=ring_tex(4)),     # a low swelling of the bark
             Ell((0, 12.0, 0), (11.0, 10.0, 8.0), R_BARK, tex=ring_tex(3))]       # the knobbly crown
    for i in range(6):
        a = rng.uniform(0, 2 * math.pi)
        prims.append(Ell((9 * math.cos(a), 15 + 5 * math.sin(a) * 0.6, 4 * math.sin(a)), (3.4, 3.4, 3.0), R_BARK,
                         tex=ring_tex(2)))
    prims.append(Ell((-2, 22, -2), (2.4, 2.0, 2.0), R_DARK))
    return draw(prims, k, roll, (0, 0, 17, 11))


def web(k, roll):
    """A spider web strung across between two thin shoots at +-22 degrees."""
    xs = BR * math.sin(math.radians(22))
    prims = nr.capsule((-xs, 0, 0), (-xs - 1, 40, 0), 1.1, 0.6, R_CAPA, n=10) + \
        nr.capsule((xs, 0, 0), (xs + 1, 40, 0), 1.1, 0.6, R_CAPA, n=10)

    def silk(px, anc, size, kk, rr):
        ax, ay = anc
        W, H = size
        hub = (0.0, 20.0)
        rm = nr.rot_z(-math.radians(rr))

        def put(x, y, c):
            p = rm @ np.array([x, y, 0.0])
            X, Y = int(ax + p[0] * kk), int(ay - p[1] * nr.CE * kk)
            if 0 <= X < W and 0 <= Y < H and (X, Y) not in px:
                px[(X, Y)] = c
        for i in range(10):                              # spokes
            a = i * math.pi / 5 + 0.2
            for s in np.arange(0, 1, 0.5 / max(1, kk * 20)):
                put(hub[0] + math.cos(a) * s * (xs - 1), hub[1] + math.sin(a) * s * 19, COL["w1"])
        for r in (0.3, 0.55, 0.8, 1.0):                  # the spiral, as rings
            for t in np.arange(0, 2 * math.pi, 0.5 / max(1, kk * r * 18)):
                put(hub[0] + math.cos(t) * r * (xs - 1), hub[1] + math.sin(t) * r * 19,
                    COL["w1"] if r < 0.9 else COL["w2"])
    return draw(prims, k, roll, (0, 0, xs + 2, 3), COL["out"], COL["out"], extra=silk)


def sap(k, roll):
    """A run of amber sap on the bark (a 28-unit piece of a patch), glossy, with drips."""
    rng = random.Random(3)
    prims = [Ell((0, 0.2, 0), (26, 1.0, 14), R_SAP)]
    for _ in range(5):
        prims.append(Ell((rng.uniform(-22, 22), 0.4, rng.uniform(-10, 10)), (3, 1.4, 3), R_SAP))
    return draw(prims, k, roll, None, COL["out"], COL["out"])


def hole(k, roll):
    """The woodpecker's hole: dark and sunk, chips of pale wood round it."""
    rng = random.Random(8)
    prims = [Ell((0, 0.0, 0), (13, 0.4, 9), R_DARK), Ell((0, 0.15, 3.5), (10, 0.3, 4.5), [WOOD["b4"]] * 4)]
    for i in range(10):
        a = i / 10.0 * 2 * math.pi + rng.uniform(-0.2, 0.2)
        prims.append(Ell((15 * math.cos(a), 0.6, 11 * math.sin(a)), (1.8, 0.9, 1.2), R_WOOD))
    return draw(prims, k, roll, None)


def pinecone(k, roll):
    prims = [Ell((0, 7.5, 0), (4.8, 7.5, 4.8), [WOOD["wm"], WOOD["b2"], WOOD["b3"], WOOD["b4"]], tex=stripe_tex(1, 5))]
    return draw(prims, k, roll, (0, 0, 5.5, 4))


def shadow(k, rx, rz, col=WOOD["dark"]):
    return draw([], k, 0, (0, 0, rx, rz), None, col)


def caterpillar(k, roll, frame):
    prims = []
    for i in range(7):
        hump = 1.6 if (i + frame) % 3 == 0 else 0
        mat = R_GREEN
        prims.append(Ell((-9 + i * 3.2, 2.4 + hump, 0), (2.4, 2.4, 2.4), mat, tex=stripe_tex(1, 2) if i % 2 else None))
    prims.append(Ell((11.6, 3.4, 0), (2.8, 2.8, 2.6), R_GREEN))
    prims.append(Ell((13.4, 4.4, -1.6), (0.7, 0.7, 0.7), R_BLACK))
    return draw(prims, k, roll, (1, 0, 13, 3), BIRD["out"], BIRD["out"])


def snake(k, roll, frame):
    """Slithering across to the right: an S of green and black bands, the head raised."""
    prims = []
    n = 18
    for i in range(n):
        t = i / (n - 1.0)
        x = -22 + 44 * t
        z = 6 * math.sin(t * 2.6 * math.pi + frame * math.pi)
        mat = R_BLACK if i % 3 == 0 else R_GREEN
        prims.append(Ell((x, 2.2, z), (2.4, 2.2, 2.4), mat))
    prims.append(Ell((24.5, 4.0, 6 * math.sin(2.6 * math.pi + frame * math.pi)), (3.4, 2.6, 2.8), R_GREEN))
    prims.append(Ell((26.4, 5.4, 6 * math.sin(2.6 * math.pi + frame * math.pi) - 1.8), (0.8, 0.8, 0.8), R_YEL))
    return draw(prims, k, roll, (1, 0, 25, 7), BIRD["out"], BIRD["out"])


def snow(k, roll):
    rng = random.Random(6)
    prims = [Ell((0, 5, 0), (11, 6.5, 8), R_SNOW)]
    for _ in range(4):
        prims.append(Ell((rng.uniform(-8, 8), rng.uniform(6, 10), rng.uniform(-4, 4)), (4.5, 3.6, 4), R_SNOW))
    return draw(prims, k, roll, (0, 0, 13, 9), COL["out"], COL["out"])


def broken_end(k):
    """A jagged broken branch end, seen from behind: pale splinters rising from the end of the bark."""
    rng = random.Random(12)
    prims = []
    for a in range(-66, 67, 11):
        h = rng.uniform(4, 15) * (1 - abs(a) / 120.0)
        base = np.array(surf(a + rng.uniform(-3, 3)))
        n = np.array([math.sin(math.radians(a)), math.cos(math.radians(a)), 0])
        top = base + n * h + np.array([rng.uniform(-2, 2), 0, rng.uniform(1, 5)])
        prims += nr.capsule(base, top, rng.uniform(2.2, 3.4), 0.5, R_WOOD, n=6)
    for a in range(-80, 81, 8):                          # the torn bark lip
        prims.append(Ell(surf(a, 0.5), (3.4, 1.6, 1.6), R_BARKD))
    return draw(prims, k, 0, None)


def gap_face(k):
    """The far side of a gap: the broken face of the branch facing us, rings of pale wood, splinters on top."""
    rng = random.Random(14)
    teeth = [rng.uniform(0, 7) for _ in range(24)]

    def face(px, anc, size, kk, rr):
        ax, ay = anc
        W, H = size
        for Y in range(H):
            for X in range(W):
                x = (X + 0.5 - ax) / kk
                y = (ay - (Y + 0.5)) / (kk * nr.CE)
                r = math.hypot(x, y + BR)
                ti = int((x / BR + 1) * 11.99)
                top = teeth[ti] * (1 - abs(x) / BR) if 0 <= ti < 24 else 0
                if r > BR or y > top or y < -BR * 0.22:
                    continue
                if y < -BR * 0.12 and not hs.dither_ok(X, Y, int(16 * (y + BR * 0.22) / (BR * 0.1))):
                    continue
                if r > BR - 3.5:
                    c = WOOD["b4"] if r < BR - 1.5 else WOOD["out"]
                elif y > -1.2:
                    c = WOOD["wl"]
                else:
                    c = WOOD["wl"] if int(r / 4.5) % 2 == 0 else WOOD["wm"]
                px[(X, Y)] = c
    return draw([], k, 0, (0, -BR * 0.22 / nr.SE * nr.CE, BR, BR * 0.3 / nr.SE), None, None, extra=face)


# ---- birds and beasts (OBJ 7) -------------------------------------------------------------------------------------------
def crow(k, frame):
    """Seen from behind: 0 standing, 1 pecking, 2 flying off (wings up)."""
    peck = 1 if frame == 1 else 0
    prims = [Ell((0, 6, 0), (4.2, 4.8, 6), R_BLACK), Ell((0, 11 - 3 * peck, 4 + 2 * peck), (3.2, 3.0, 3.2), R_BLACK),
             Ell((0, 4, -6), (2.4, 1.2, 4), R_BLACK), Ell((0, 10 - 3 * peck, 7.4 + 2 * peck), (0.9, 0.9, 1.8), R_GREY)]
    if frame == 2:
        prims += [Ell((-8, 10, 0), (8, 1.4, 4), R_BLACK, nr.rot_z(-0.5)), Ell((8, 10, 0), (8, 1.4, 4), R_BLACK, nr.rot_z(0.5))]
    else:
        prims += [Ell((-1.6, 1.2, 0), (0.6, 1.6, 0.6), R_GREY), Ell((1.6, 1.2, 0), (0.6, 1.6, 0.6), R_GREY)]
    return draw(prims, k, 0, (0, 0, 5, 6), BIRD["out"], BIRD["out"])


def bee(frame):
    cv = Canvas(8, 8)
    hs.ellipse(cv, 4, 4.5, 2.6, 1.8, None, flat=BIRD["yellow"])
    cv.set(3, 4, BIRD["black"]), cv.set(3, 5, BIRD["black"]), cv.set(5, 4, BIRD["black"]), cv.set(5, 5, BIRD["black"])
    wy = 2 if frame == 0 else 3
    cv.set(3, wy, BIRD["white"]), cv.set(5, wy, BIRD["white"]), cv.set(2, wy, BIRD["white"])
    cv.outline(BIRD["out"])
    return cv, 4, 4


# ---- weather, swinging branches, decorations (OBJ 2) ------------------------------------------------------------------
SWING_ANGS = [-45, -30, -15, 0, 15, 30, 45]
SWING_LEN = 64.0


def swing(k, ang):
    """A leafy branch end hanging from a limb above, swinging across the path; the anchor is its pivot, 70 units over
    the bark. Its lower end (the leaves) sweeps at 6 to 26 units over the bark."""
    rng = random.Random(21)
    a = math.radians(ang)
    pivot = np.array([0.0, 0.0, 0.0])
    end = pivot + np.array([SWING_LEN * math.sin(a), -SWING_LEN * math.cos(a), 0])
    prims = nr.capsule(pivot, end, 2.2, 1.4, R_BARK, n=18)
    prims += leaf_cluster(end + np.array([0, -4, 0]), 14, 7.0, rng, size=4.2)
    prims += leaf_cluster(pivot + (end - pivot) * 0.55, 5, 4.0, rng, size=3.4)
    return draw(prims, k, 0, None)


def side_branch(k, kind):
    """A limb growing out of the side of the branch (the root is the anchor), outward, up and on: 0 leafy, 1 bare with
    a tuft, 2 with a bird's nest; drawn for the right side (the left is the mirror)."""
    rng = random.Random(30 + kind)
    root = np.array([0.0, 0.0, 0.0])
    if kind == 0:
        mid, end = np.array([46, 22, 30]), np.array([96, 44, 64])
    elif kind == 1:
        mid, end = np.array([40, 30, 20]), np.array([70, 64, 40])
    else:
        mid, end = np.array([50, 14, 36]), np.array([90, 26, 70])
    prims = nr.capsule(root, mid, 10, 6.5, R_BARKD) + \
        nr.capsule(mid, end, 6.5, 2.2, R_BARKD)
    if kind == 0:
        prims += leaf_cluster(end, 26, 13, rng, size=5.2) + leaf_cluster(mid + np.array([0, 10, 0]), 12, 9, rng, size=4.6)
    elif kind == 1:
        prims += nr.capsule(end, end + np.array([10, 16, 6]), 2.0, 0.8, R_BARK) + \
            nr.capsule(end, end + np.array([16, 4, 4]), 2.0, 0.8, R_BARK)
        prims += leaf_cluster(end + np.array([12, 14, 6]), 10, 7, rng, size=4.2)
    else:
        nest = mid + np.array([12, 8, 4])
        for i in range(14):                                # a ring of twigs and straw
            t = i / 14.0 * 2 * math.pi
            prims.append(Ell(nest + np.array([7 * math.cos(t), 0, 6 * math.sin(t)]), (3.2, 2.6, 3.0), R_WOOD))
        prims += [Ell(nest + np.array([-1.5, 1.5, 0]), (2.0, 2.4, 2.0), R_CAP), Ell(nest + np.array([2, 1.5, 1]), (2.0, 2.4, 2.0), R_CAP)]
        prims += leaf_cluster(end, 14, 9, rng, size=4.6)
    return draw(prims, k, 0, None)


def overhead(k, kind):
    """Leaves passing above the camera: a hanging clump with strands of moss or vines (the anchor is its top)."""
    rng = random.Random(40 + kind)
    prims = nr.capsule((0, -4, 0), (8 * (kind * 2 - 1), 90, 8), 2.4, 3.4, R_BARK, n=20)   # the limb it hangs from
    prims += leaf_cluster((0, -10, 0), 30, 15, rng, size=5.4)
    for i in range(4 if kind == 0 else 7):
        x = rng.uniform(-14, 14)
        L = rng.uniform(14, 34)
        prims += nr.capsule((x, -12, rng.uniform(-4, 4)), (x + rng.uniform(-3, 3), -12 - L, 0), 0.9, 0.6,
                            R_MOSS if kind == 0 else R_LEAF, n=8)
        if kind == 1:
            prims += leaf_cluster((x, -12 - L, 0), 2, 2.5, rng, size=2.6)
    return draw(prims, k, 0, None)


def mushrooms(k, roll):
    """Bracket fungi on the side of the bark (drawn at 72 degrees, rolled with the surface)."""
    prims = []
    for i, (x, y, z, r) in enumerate(((0, 2, 0, 6), (-2, 7, 6, 4.6), (1, -3, -6, 4))):
        prims.append(Ell((x, y, z), (r, r * 0.35, r * 0.9), R_CAP, tex=ring_tex(2)))
        prims.append(Ell((x, y - r * 0.3, z), (r * 0.9, r * 0.2, r * 0.8), [WOOD["cream"], WOOD["cream"], WOOD["wm"], WOOD["wm"]]))
    return draw(prims, k, roll, None)


def hive(k):
    """A wild beehive hanging under a limb: ribbed straw-coloured comb, a dark entrance."""
    prims = nr.capsule((0, 0, 0), (0, -8, 0), 1.0, 1.0, R_BARK, n=4)
    for i in range(5):
        prims.append(Ell((0, -12 - i * 4.2, 0), (7.5 - abs(i - 2) * 1.2, 2.6, 6.5 - abs(i - 2)), R_WOOD))
    prims.append(Ell((0, -24, 5.5), (2.0, 1.8, 0.8), R_DARK))
    return draw(prims, k, 0, None)


def fg_leaf(frame):
    """A big leaf sweeping past the screen's edge (screen space, 48x48)."""
    cv = Canvas(48, 48)
    a = [0.4, 1.2, 2.1][frame]
    for y in range(48):
        for x in range(48):
            dx, dy = x + 0.5 - 24, y + 0.5 - 24
            u = dx * math.cos(a) + dy * math.sin(a)
            v = -dx * math.sin(a) + dy * math.cos(a)
            w = 9 * (1 - (u / 21.0) ** 2)
            if abs(u) < 21 and abs(v) < w:
                c = WOOD["l1"] if v < 0 else WOOD["l2"]
                if abs(v) < 0.8:
                    c = WOOD["l0"]
                if (abs(v) + abs(u * 0.5)) % 7 < 0.8 and abs(v) > 1:
                    c = WOOD["l0"]
                cv.set(x, y, c)
    cv.outline(WOOD["out"])
    return cv, 24, 24


def rain(frame):
    cv = Canvas(8, 16)
    for i in range(10):
        x = 5 - i // 3 + (frame % 2)
        cv.set(x, i + frame * 3, COL["w2"] if i < 7 else COL["w1"])
    return cv, 4, 8

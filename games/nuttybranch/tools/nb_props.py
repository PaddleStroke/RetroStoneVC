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
                    R_GREY, R_HAZEL, R_LEAF, R_MOSS, R_SAP, R_SNOW, R_WOOD, R_YEL)

Canvas = hs.Canvas
LODS = [round(1.6 / 1.18 ** i, 3) for i in range(16)]   # 1.6 .. 0.13: steps of 18%
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


# ---- branches: one system for the ones that cross the path (obstacles) and the ones that grow outward (scenery) --------
# The crossing ones follow the same geometry as src/play.c limb_at(): a limb growing out of the flank at 82 degrees,
# arching over the path; its centre at angle side * (82 - SPAN * t) degrees and HC(t) units over the bark, radius R(t).
LIMB_ROOT = 82.0
LIMB_LOW = dict(span=122.0, h0=0.5, h1=2.0, r0=7.5, r1=3.8)      # lying low over the path: jump it
LIMB_ARCH = dict(span=137.0, h0=4.0, h1=30.0, r0=8.0, r1=4.5)    # a high arch: run under it


def limb_hc(g, t):
    if g is LIMB_LOW:                                     # (src/play.c limb_point: the same shape)
        return limb_r(g, t) + g["h0"] + g["h1"] * math.sin(max(0.0, min(1.0, t * 2)) * math.pi / 2)
    return g["h0"] + g["h1"] * math.sin(math.pi * t)


def limb_r(g, t):
    return g["r0"] + (g["r1"] - g["r0"]) * t


def bark_tex(Pl, n):
    """rough bark on a limb: a speckle of darker and lighter flecks"""
    x = np.nan_to_num(Pl[..., 0], posinf=0, neginf=0)
    y = np.nan_to_num(Pl[..., 1], posinf=0, neginf=0)
    z = np.nan_to_num(Pl[..., 2], posinf=0, neginf=0)
    h = ((x * 7.3 + 9).astype(int) * 3 + (y * 5.1 + 9).astype(int) * 7 + (z * 6.7 + 9).astype(int) * 11) % 7
    return np.where(h == 0, 1, np.where(h == 6, -1, 0))


def limb_chain(pts, r0, r1, mat=None):
    """a tapering limb through points (a polyline smoothed by its density): spheres along it"""
    mat = mat or R_BARKD
    out = []
    n = len(pts)
    for i in range(n):
        t = i / float(n - 1)
        r = r0 + (r1 - r0) * t
        out.append(Ell(pts[i], (r, r, r), mat, tex=bark_tex))
    return out


def bezier(p0, p1, p2, n):
    p0, p1, p2 = (np.array(p, float) for p in (p0, p1, p2))
    return [tuple((1 - t) ** 2 * p0 + 2 * (1 - t) * t * p1 + t * t * p2) for t in (i / float(n - 1) for i in range(n))]


def twigs_and_leaves(base, direction, rng, n, length, size):
    """small sub-branches off a point, each ending in a leaf cluster"""
    prims = []
    for _ in range(n):
        d = np.array(direction, float) + np.array([rng.uniform(-0.6, 0.6), rng.uniform(-0.2, 0.7), rng.uniform(-0.6, 0.6)])
        d = d / np.linalg.norm(d)
        mid = np.array(base) + d * length * 0.5 + np.array([0, rng.uniform(0, 3), 0])
        end = np.array(base) + d * length
        prims += limb_chain(bezier(base, mid, end, 10), 1.6, 0.6)
        prims += leaf_cluster(end, 8, size * 1.6, rng, size=size)
    return prims


def crossing_limb(k, arch):
    """A branch growing out of the right flank and crossing the path (the left is the mirror). The anchor is the top
    of the main branch. It meanders a little (its centre line stays within a unit or two of the shape the game
    collides with: src/play.c limb_point), has knobs and stubs, sub-branches with leaf clusters along it and a full
    spray of leaves past its tip; a low one lies on the bark with a contact shadow along it."""
    g = LIMB_ARCH if arch else LIMB_LOW
    rng = random.Random(51 if arch else 52)
    pts = []
    n = 44
    t0 = -0.1                                             # the root: from under the flank (a collar), then over
    ts = [t0 + (1 - t0) * i / (n - 1.0) for i in range(n)]
    for t in ts:
        x, y, z = surf(LIMB_ROOT - g["span"] * t, limb_hc(g, max(0.0, t)) - (6 * -t if t < 0 else 0))
        wob = math.sin(t * 9.0 + 0.7)                     # meandering along and up
        pts.append((x, y + 0.8 * wob * (1 if arch else 0.4), z + 2.2 * math.sin(t * 6.0 + 1.3)))
    prims = []
    for i, (t, p) in enumerate(zip(ts, pts)):
        r = limb_r(g, max(0.0, t)) * (1 + 0.35 * max(0.0, (0.06 - t) / 0.16))    # flaring into the flank
        r *= 1 + 0.08 * math.sin(i * 1.7)                  # knobbly
        prims.append(Ell(p, (r, r * 0.95, r), R_BARKD, tex=bark_tex))
        if not arch and t >= 0:                           # the contact shadow on the bark under it
            a = LIMB_ROOT - g["span"] * t
            prims.append(Ell(surf(a, 0.2) + np.array([0, 0, -r * 0.25 + p[2]]), (r * 1.05, 0.4, r * 0.75),
                             [WOOD["b4"]] * 4))
    for frac, h in ((0.22, 1.0), (0.5, 0.9), (0.78, 1.1)):  # knobs and broken stubs
        q = np.array(pts[int(frac * (n - 1))])
        r = limb_r(g, frac)
        prims.append(Ell(q + np.array([0, r * 0.7, 0]), (r * 0.55, r * 0.5 * h, r * 0.55), R_BARKD, tex=bark_tex))
    end = np.array(pts[-1])
    out = np.array([-1.0, 0.3, 0.4])                     # past the far side: twigs and leaves hanging over
    prims += twigs_and_leaves(end, out, rng, 5, 20, 4.6)
    prims += leaf_cluster(end + np.array([-7, 5, 4]), 26, 11, rng, size=5.0)
    subs = ((0.3, [0.3, 1.0, 0.7], 3), (0.55, [0.0, 1.0, -0.5], 2), (0.75, [-0.4, 1.0, 0.4], 3))
    if not arch:                                          # a low one: sideways, not up
        subs = ((0.25, [0.3, 0.4, 1.0], 2), (0.7, [-0.3, 0.4, -1.0], 2))
    for frac, d, m in subs:
        prims += twigs_and_leaves(np.array(pts[int(frac * (n - 1))]), d, rng, m, 13 if arch else 11, 4.2)
    return draw(prims, k, 0, None)


def outward_limb(k, kind):
    """A big branch growing out of the right flank (the root is the anchor), outward, up and on, curving and tapering,
    with sub-branches and full leaf clusters: 0 reaching up, 1 long and low, 2 with a bird's nest."""
    rng = random.Random(60 + kind)
    if kind == 0:
        p1, p2 = (40, 26, 20), (78, 70, 44)
    elif kind == 1:
        p1, p2 = (60, 6, 30), (118, 22, 70)
    else:
        p1, p2 = (46, 18, 26), (96, 40, 56)
    pts = bezier((0, 0, 0), p1, p2, 36)
    prims = limb_chain(pts, 11, 3.0)
    for frac in (0.35, 0.6):                               # sub-branches forking off
        base = np.array(pts[int(frac * 35)])
        d = np.array(p2, float) - np.array(p1, float)
        d = d / np.linalg.norm(d)
        side = np.array([rng.uniform(-0.3, 0.3), 1.0, rng.uniform(-0.5, 0.5)])
        tip = base + (d * 0.4 + side * 0.8) * 34
        prims += limb_chain(bezier(base, base + side * 14 + d * 8, tip, 14), 4.0, 1.4)
        prims += leaf_cluster(tip, 22, 10, rng, size=5.0)
    prims += leaf_cluster(np.array(pts[-1]) + np.array([4, 6, 4]), 30, 14, rng, size=5.4)
    if kind == 2:
        nest = np.array(pts[20]) + np.array([0, 7, 0])
        for i in range(14):
            t = i / 14.0 * 2 * math.pi
            prims.append(Ell(nest + np.array([7 * math.cos(t), 0, 6 * math.sin(t)]), (3.2, 2.6, 3.0), R_WOOD))
        prims += [Ell(nest + np.array([-1.5, 1.6, 0]), (2.0, 2.4, 2.0), R_CAP), Ell(nest + np.array([2, 1.6, 1]), (2.0, 2.4, 2.0), R_CAP)]
    return draw(prims, k, 0, None)


KNOTHOLE_RX, KNOTHOLE_RZ = 15.0, 13.0                # (src/tuning.h KNOTHOLE_HALF, KNOTHOLE_LEN)


def knothole(k, roll, frame):
    """A knot-hole sunk into the curved bark (rolled with it, so foreshortened toward the sides): a swollen lip of bark
    lit from above, the far inner wall seen through the opening (lit at its rim, darkening down), the dark depth
    below it; frame 1: a mouse peeking out over the near rim."""
    rng = random.Random(70)
    rx, rz, depth, h = KNOTHOLE_RX, KNOTHOLE_RZ, 4.5, 0.6
    prims = []
    for i in range(22):                                   # the lip: a ring of bark swellings
        a = i / 22.0 * 2 * math.pi
        prims.append(Ell((rx * 1.12 * math.cos(a), 1.0, rz * 1.12 * math.sin(a)),
                         (4.4 + rng.uniform(0, 1.4), 2.6, 3.8), R_BARKD, tex=bark_tex))

    def inside(px, anc, size, kk, rr):
        ax, ay = anc
        W, H = size
        c, s_ = math.cos(math.radians(rr)), math.sin(math.radians(rr))

        def plane(u, w):                                  # screen (units) -> the bark plane (x across, z along)
            x = (u - s_ * h) / c
            return x, (w - (-s_ * x + c * h) * nr.CE) / nr.SE

        def screen(x, z, up=0.0):                         # the bark plane (raised by up) -> pixel
            u = c * x + s_ * (h + up)
            w = (-s_ * x + c * (h + up)) * nr.CE + z * nr.SE
            return ax + u * kk, ay - w * kk
        for Y in range(H):
            for X in range(W):
                x, z = plane((X + 0.5 - ax) / kk, (ay - (Y + 0.5)) / kk)
                if (x / rx) ** 2 + (z / rz) ** 2 > 1.0:
                    continue
                ze = rz * math.sqrt(max(0.0, 1 - (x / rx) ** 2))          # the far rim on this line across
                f = (ze - z) * nr.SE / max(0.01, depth * nr.CE * math.sqrt(max(0.05, 1 - (x / rx) ** 2)))
                if f < 0.2:
                    col = WOOD["b2"]                      # the far wall: lit at its rim ...
                elif f < 0.55:
                    col = WOOD["b3"]
                elif f < 1.0:
                    col = WOOD["b4"]                      # ... darkening down
                elif f < 1.3 and (X + Y) % 2 == 0:
                    col = WOOD["b4"]
                else:
                    col = WOOD["dark"]                    # the depth
                px[(X, Y)] = col
        if frame == 1 and kk > 0.45:                      # a mouse peeking out over the near rim
            zc = -rz * 0.35
            for (cx, cz, up, r, col) in ((-2.6, 0, 2.6, 1.6, WOOD["wm"]), (2.6, 0, 2.6, 1.6, WOOD["wm"]),
                                         (-2.6, 0, 2.6, 0.8, WOOD["cap"]), (2.6, 0, 2.6, 0.8, WOOD["cap"]),
                                         (0, 0, 0, 3.1, WOOD["wm"])):
                X0, Y0 = screen(cx, zc + cz, up)
                rp = r * kk
                for Y in range(int(Y0 - rp) - 1, int(Y0 + rp) + 2):
                    for X in range(int(X0 - rp) - 1, int(X0 + rp) + 2):
                        if (X + 0.5 - X0) ** 2 + ((Y + 0.5 - Y0) * 1.15) ** 2 <= rp * rp and 0 <= X < W and 0 <= Y < H:
                            px[(X, Y)] = col
            for ex in (-1.2, 1.2):                        # eyes and a nose
                X, Y = (int(q) for q in screen(ex, zc, 0.5))
                px[(X, Y)] = WOOD["out"]
                if kk > 1.0:
                    px[(X + (1 if ex > 0 else -1), Y)] = WOOD["out"]
                    px[(X, Y - 1)] = WOOD["cream"]
            X, Y = (int(q) for q in screen(0, zc, -1.4))
            px[(X, Y)] = WOOD["cap"]
    return draw(prims, k, roll, None, extra=inside)


def broken_near(k):
    """The near side of a gap: the broken end of the bark, a few jagged shards of bark and torn fibres."""
    rng = random.Random(81)
    prims = []
    for a in range(-62, 63, 9):
        a2 = a + rng.uniform(-3, 3)
        base = np.array(surf(a2, -1.0))
        n = np.array([math.sin(math.radians(a2)), math.cos(math.radians(a2)), 0])
        h = rng.uniform(1.5, 6.0) * (1 - abs(a2) / 110.0)
        top = base + n * h + np.array([0, 0, rng.uniform(1, 4)])
        prims += limb_chain([tuple(base + (top - base) * i / 5.0) for i in range(6)], rng.uniform(2.0, 3.2), 0.7,
                            R_BARKD if rng.random() < 0.75 else [WOOD["wm"], WOOD["wm"], WOOD["b3"], WOOD["b4"]])
    return draw(prims, k, 0, None)


#!/usr/bin/env python3
"""Nutty Branch: the branch texture (the affine layer BG2, 512x256 texels, palette BG 1: nb_pal.BARK0).

Seen from above and behind, the branch is a cylinder: a texel column is a place across its width, x = R sin(angle),
so everything on the bark is laid out by its ANGLE round the branch and squeezed toward the silhouette:
  - the bark's furrows run ALONG the branch, evenly spaced in angle (every 10 degrees): wide apart on the top,
    bunching up toward the sides; in the perspective they all run to the vanishing point;
  - the ridges between them are broken into plates of varied lengths by short cross-cracks (the subtle detail
    that scrolls toward the camera with the speed);
  - the shading follows the light from above: n . L with n = (sin a, cos a): a light band on the top, darkening
    gradually to both sides (7 shades), the underside darkest, a dark rim at the silhouette, a slightly ragged edge
    and the odd bump;
  - variety along the length: three 64-row bark patches (plain with lichen, a burl, a scar with moss), picked per
    metre by the game (the plain one most of the time).

Rows: 0-63 the three patches side by side (centres TEX_VAR_CX), 64-239 the fork rows (two sections, centre to centre
16 * (j + 1), j = 0..21, 8 identical rows each), 240-247 the end grain (the broken face of a gap's far side),
248-255 the jagged edge (a gap's near side).
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE.
"""
import math
import random

from nb_pal import BK

TEX_W, TEX_H = 512, 256
R = 64
VAR_CX = [85, 256, 427]
FORK_STEPS = 22
LIGHT = (-0.2, 0.98)
SHADE_TH = [0.965, 0.88, 0.74, 0.55, 0.32, 0.06]          # nd above: b0 .. b5, else b6
SHADES = [BK["b0"], BK["b1"], BK["b2"], BK["b3"], BK["b4"], BK["b5"], BK["b6"]]
FURROW_DEG = 10.0


def shade_of(a):
    """The bark shade index (0 = the sunlit top .. 6 = the underside) at angle a (radians)."""
    n = math.hypot(*LIGHT)
    nd = (math.sin(a) * LIGHT[0] + math.cos(a) * LIGHT[1]) / n
    for i, t in enumerate(SHADE_TH):
        if nd > t:
            return i
    return 6


class Patch:
    """One 64-row bark patch: plate lengths per ridge, furrow depths, features."""

    def __init__(self, seed, features):
        rng = random.Random(seed)
        self.offset = rng.uniform(0, FURROW_DEG)
        self.plates = {}
        for k in range(-12, 13):                          # per ridge: plate boundaries (rows, multiples of 8)
            rows, v = [], rng.choice([0, 8, 16])
            while v < 64:
                rows.append(v)
                v += rng.choice([16, 24, 24, 32, 40])
            self.plates[k] = set(rows)
        self.deep = {k: rng.random() < 0.55 for k in range(-12, 13)}
        self.features = features
        self.edge = [rng.choice([0, 0, 1, 2]) for _ in range(8)]   # the ragged silhouette, per 8 rows

    def texel(self, u, v, plain=False):
        if abs(u) >= 1.0:
            return 0
        a = math.asin(u)
        deg = math.degrees(a)
        s = shade_of(a)
        if not plain:
            cut = 1.0 - 0.012 * self.edge[v // 8]
            if abs(u) > cut:
                return 0
        if abs(u) > 0.985:
            return BK["rim"]
        if abs(deg) > 80:
            return BK["b6"]
        # furrows along the branch, evenly spaced in angle (so they bunch toward the sides)
        f = (deg - self.offset) / FURROW_DEG
        k = int(math.floor(f + 0.5))
        d = abs(f - k) * FURROW_DEG                       # degrees to the nearest furrow
        deep = self.deep.get(k, True)
        if d < (1.5 if deep else 1.0):
            return SHADES[min(6, s + 2)]
        if d < (2.3 if deep else 1.7):
            return SHADES[min(6, s + 1)]
        # the plate's lit left edge (relief), the cross-cracks at the plate ends (on the ridge only)
        ridge = int(math.floor(f))
        if not plain and v in self.plates.get(ridge, ()) and d > 2.6:
            return SHADES[min(6, s + 2)]
        if not plain:
            for feat in self.features:
                c = feat(deg, v, s)
                if c:
                    return c
        if (f - math.floor(f)) * FURROW_DEG < 3.2 and f - math.floor(f) < 0.5:
            return SHADES[max(0, s - 1)]
        return SHADES[s]


def moss(c_deg, c_v, r_deg, r_v, seed):
    rng = random.Random(seed)
    holes = {(rng.randint(-8, 8), rng.randint(-10, 10)) for _ in range(9)}

    def f(deg, v, s):
        dx, dy = (deg - c_deg) / r_deg, (v - c_v) / r_v
        if dx * dx + dy * dy > 1:
            return 0
        if (int(deg) // 3 - int(c_deg) // 3, (v - c_v) // 3) in holes:
            return 0
        return BK["m1"] if s <= 2 and (int(deg) + v) % 5 else BK["m2"]
    return f


def lichen(c_deg, c_v, r_deg, r_v):
    def f(deg, v, s):
        dx, dy = (deg - c_deg) / r_deg, (v - c_v) / r_v
        if dx * dx + dy * dy <= 1 and (int(deg * 2) * 7 + v * 3) % 9 == 0:
            return BK["lichen"]
        return 0
    return f


def burl(c_deg, c_v, r_deg, r_v):
    """A knobbly bulge in the bark: rings round a dark knot, lit from the top-left."""
    def f(deg, v, s):
        dx, dy = (deg - c_deg) / r_deg, (v - c_v) / r_v
        d = math.hypot(dx, dy)
        if d > 1:
            return 0
        if d < 0.28:
            return BK["knot"]
        nd = -dx * 0.6 - dy * 0.8
        base = max(0, min(6, s + (-1 if nd > 0.3 else 1 if nd < -0.3 else 0)))
        if int(d * 6) % 2 == 0:
            base = min(6, base + 1)
        return SHADES[base]
    return f


def scar(d0, d1, v0, v1):
    """An old wound: pale wood with grain lines along the branch, dark lips."""
    def f(deg, v, s):
        if not (v0 <= v < v1):
            return 0
        t = (v - v0) / float(v1 - v0)
        half = (d1 - d0) / 2.0 * math.sin(math.pi * t) ** 0.6
        c = (d0 + d1) / 2.0
        if abs(deg - c) > half + 0.8:
            return 0
        if abs(deg - c) > half - 0.6:
            return BK["b6"]
        return BK["wm"] if int((deg - c) * 2) % 3 == 0 else BK["wl"]
    return f


PATCHES = [
    Patch(11, [lichen(40, 44, 12, 9)]),
    Patch(22, [burl(20, 24, 9, 8), lichen(-34, 50, 10, 7)]),
    Patch(33, [scar(-22, -9, 18, 50), moss(44, 56, 10, 6, 5)]),
]


def branch_texture():
    """Indices (0 = transparent) of the whole texture."""
    T = [[0] * TEX_W for _ in range(TEX_H)]
    for p, cx in zip(PATCHES, VAR_CX):
        for v in range(64):
            for x in range(cx - R - 4, cx + R + 4):
                u = (x + 0.5 - cx) / R
                T[v][x] = p.texel(u, v)
        if p is PATCHES[1]:                                # a bump on the left silhouette
            for v in range(40, 56):
                bulge = 3.2 * math.sin(math.pi * (v - 40) / 16.0)
                for x in range(cx - R - 4, cx - R + 2):
                    u = (x + 0.5 - cx) / R
                    if -1.0 - bulge / R < u <= -0.97:
                        T[v][x] = BK["rim"] if u < -1.0 - bulge / R + 1.5 / R else BK["b6"]
    base = PATCHES[0]
    for j in range(FORK_STEPS):                           # the fork rows
        S = 16 * (j + 1)
        row = [0] * TEX_W
        for x in range(TEX_W):
            ul = (x + 0.5 - (256 - S / 2.0)) / R
            ur = (x + 0.5 - (256 + S / 2.0)) / R
            inl, inr = abs(ul) < 1.0, abs(ur) < 1.0
            if not inl and not inr:
                continue
            if inl and inr:
                if abs(abs(ul) - abs(ur)) < 0.05:
                    row[x] = BK["b6"]
                    continue
                u = ul if abs(ul) < abs(ur) else ur
            else:
                u = ul if inl else ur
            row[x] = base.texel(u, 1, plain=True)
        for r in range(8):
            T[64 + j * 8 + r] = list(row)
    for r in range(8):                                    # the end grain: the broken face of a gap's far side
        for x in range(256 - R, 256 + R):
            u = (x + 0.5 - 256) / R
            if abs(u) > 0.93:
                T[240 + r][x] = BK["rim"] if abs(u) > 0.975 else BK["b5"]
            elif r == 0:
                T[240][x] = BK["b4"] if (x // 3) % 2 else BK["b3"]
            else:
                ring = int(abs(u) * 7 + r * 0.45) % 2
                T[240 + r][x] = BK["wl"] if ring == 0 else BK["wm"]
    rng = random.Random(7)                                # the jagged edge: the near side of a gap
    jag = [rng.randint(0, 7) for _ in range(2 * R)]
    for i in range(1, len(jag) - 1):
        jag[i] = max(jag[i], (jag[i - 1] + jag[i + 1]) // 2 - 1)
    for r in range(8):
        for i, x in enumerate(range(256 - R, 256 + R)):
            d = jag[i]
            u = (x + 0.5 - 256) / R
            if r < 7 - d:
                T[248 + r][x] = 0
            elif r == 7 - d:
                T[248 + r][x] = BK["wl"]
            else:
                T[248 + r][x] = base.texel(u, 0, plain=True)
    return T

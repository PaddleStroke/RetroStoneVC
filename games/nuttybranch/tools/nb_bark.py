#!/usr/bin/env python3
"""Nutty Branch: the branch texture (the affine layer BG2, 512x256 texels, palette BG 1: nb_pal.BARK0).

Seen from above and behind, the branch is a cylinder: a texel column is a place across its width, x = R sin(angle),
so everything on the bark is laid out by its ANGLE round the branch and squeezed toward the silhouette:
  - the bark's furrows run ALONG the branch, evenly spaced in angle (every 9 degrees): wide apart on the top,
    bunching up and thinning toward the sides (tests/furrow_test.sh measures it on screen); in the perspective they
    all run to the vanishing point; each wanders and swells a little along the branch (braided, irregular bark);
  - the ridges between them are rough, split here and there, rarely cracked across (the detail that scrolls
    toward the camera with the speed); the pattern repeats every 32 rows (its tiles deduplicate);
  - the shading follows the light from above: n . L with n = (sin a, cos a): a narrow highlight on the top, darkening
    gradually to both sides (7 shades), the underside darkest, a dark rim at the silhouette, a slightly ragged edge
    and the odd bump;
  - variety along the length: three 64-row bark patches (plain with lichen, a burl, a scar with moss), picked per
    metre by the game (the plain one most of the time).

Rows: 0-63 the three patches side by side (centres TEX_VAR_CX), 64-239 the fork rows (two sections, centre to centre
16 * (j + 1), j = 0..21, 8 identical rows each), 240-247 a gap's far end (torn teeth of bark, the dark broken face
between them), 248-255 its near end (the jagged broken bark). Beside the first fork rows (rows 64-127, columns 0-127):
the far end's broken face, seen end-on (src/road.c stretches it down the gap's empty lines).
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
SHADE_TH = [0.985, 0.92, 0.78, 0.58, 0.34, 0.07]          # nd above: b0 .. b5, else b6
SHADES = [BK["b0"], BK["b1"], BK["b2"], BK["b3"], BK["b4"], BK["b5"], BK["b6"]]
FURROW_DEG = 9.0


def shade_of(a):
    """The bark shade index (0 = the sunlit top .. 6 = the underside) at angle a (radians)."""
    n = math.hypot(*LIGHT)
    nd = (math.sin(a) * LIGHT[0] + math.cos(a) * LIGHT[1]) / n
    for i, t in enumerate(SHADE_TH):
        if nd > t:
            return i
    return 6


def _hash(*v):
    h = 2166136261
    for x in v:
        h = ((h ^ (x & 0xffffffff)) * 16777619) & 0xffffffff
    return h


class Patch:
    """One 64-row bark patch: furrows evenly spaced in ANGLE (so on screen they bunch up and thin out toward the
    sides: the cylinder), each wandering and swelling along the branch (Patch.furrow), their cores deepest on the lit
    top; rough ridges between them, split here and there, specks of roughness (repeating every 8 rows), rare short
    cross-cracks at the plate ends, the ragged silhouette."""

    def __init__(self, seed, features):
        rng = random.Random(seed)
        self.seed = seed
        self.offset = rng.uniform(0, FURROW_DEG)
        self.plates = {}
        for k in range(-14, 15):                          # per ridge: plate boundaries (rows, multiples of 8)
            rows, v = [], rng.choice([0, 8, 16, 24])
            while v < 64:
                rows.append(v)
                v += rng.choice([16, 24, 24, 32, 40])
            self.plates[k] = sorted(rows)
        self.features = features
        self.edge = [rng.choice([0, 0, 1, 2]) for _ in range(8)]   # the ragged silhouette, per 8 rows

    def plate_of(self, k, v):
        """the plate (its start row) of furrow k's left ridge at row v"""
        start = 0
        for r in self.plates.get(k, [0]):
            if r <= v:
                start = r
        return start

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
        vv = 0 if plain else v
        f0 = (deg - self.offset) / FURROW_DEG
        best, bw, bd, bc = None, 0, 99.0, 0.0
        for k in (int(math.floor(f0)) - 1, int(math.floor(f0)), int(math.floor(f0)) + 1):
            c, wdt = self.furrow(k, vv)
            dd = abs(deg - c)
            if dd < bd:
                best, bw, bd, bc = k, wdt, dd, c
        if bd < bw * 0.45:                                 # the groove: its core deep (deepest on the lit top) ...
            return SHADES[min(6, s + (4 if s <= 1 else 3))]
        if bd < bw:
            return SHADES[min(6, s + 2)]                   # ... its walls
        if bd < bw + 0.8:
            return SHADES[min(6, s + 1)]
        ridge = best if deg > bc else best - 1             # the ridge right of furrow `ridge`
        cl, _ = self.furrow(ridge, vv)
        cr, _ = self.furrow(ridge + 1, vv)
        mid = (cl + cr) / 2.0 + 0.9 * math.sin(vv * 2 * math.pi / 32 + ridge * 1.9)
        h16 = _hash(self.seed, ridge, (vv % 32) // 16)
        if h16 % 3 == 0 and abs(deg - mid) < 0.45 + 0.25 * ((h16 >> 4) & 1):
            return SHADES[min(6, s + 2)]                   # a split down the ridge (braided, irregular)
        if not plain:
            plates = self.plates.get(ridge, [])
            if v % 32 in plates and _hash(self.seed, ridge, v % 32) % 6 == 0 and bd > bw + 1.4 and abs(deg - mid) < 2.6:
                return SHADES[min(6, s + 2)]                # a short cross-crack (rare)
            for feat in self.features:
                c = feat(deg, v, s)
                if c:
                    return c
        col = int(round(u * 64))
        sp = _hash(self.seed, col, (vv % 8), ridge) % 100
        if sp < 22:
            return SHADES[min(6, s + 1)]                    # rough specks
        if sp > 93 and s > 0:
            return SHADES[s - 1]                            # flecks of light on the ridges
        if deg - bc > 0 and deg - bc < bw + 2.2 and s > 0 and sp > 60:
            return SHADES[s - 1]                            # the ridge's lit left flank, broken
        return SHADES[s]

    def furrow(self, k, v):
        """furrow k's centre (degrees) and half width at row v: evenly spaced in angle, each wandering a little along
        the branch (periodic in 32 rows: the tiles repeat) and swelling and thinning"""
        hh = _hash(self.seed, k, 991)
        ph1, ph2 = (hh & 255) / 255.0 * 6.283, ((hh >> 8) & 255) / 255.0 * 6.283
        m = 1 + ((hh >> 16) & 1)
        c = self.offset + k * FURROW_DEG + 1.1 * math.sin(v * 2 * math.pi * m / 32 + ph1) \
            + 0.35 * math.sin(v * 2 * math.pi / 16 + ph2)
        base = 1.25 + ((hh >> 20) & 255) / 255.0 * 1.0
        w = base * (1 + 0.3 * math.sin(v * 2 * math.pi / 32 + ph2))
        return c, w


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
    """An old wound: bare wood with grain lines along the branch, dark lips."""
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
        return BK["b2"] if int((deg - c) * 2) % 3 == 0 else BK["wm"]
    return f


PATCHES = [
    Patch(11, [lichen(40, 44, 12, 9)]),
    Patch(22, [burl(20, 24, 9, 8), lichen(-34, 50, 10, 7)]),
    Patch(33, [scar(-22, -9, 18, 50), moss(44, 56, 10, 6, 5)]),
]


def teeth(n, seed, lo, hi):
    """a jagged edge across n columns: pointed teeth of varied widths and lengths (rows lo..hi)"""
    rng = random.Random(seed)
    out = []
    while len(out) < n:
        w = rng.randint(4, 11)
        peak = rng.randint(lo + 1, hi) if rng.random() < 0.8 else lo + 1
        valley = rng.randint(lo, lo + 1)
        for i in range(w):
            t = 1.0 - abs((i + 0.5) / w * 2 - 1)            # a triangle: 0 at the sides, 1 in the middle
            out.append(int(round(valley + (peak - valley) * t ** 1.4)))
    out = out[:n]
    for i in (0, 1, n - 2, n - 1):                        # the silhouette's ends stay short
        out[i] = lo
    return out


def face_texel(u, v, x):
    """the broken face seen end-on: u across (-1..1), v down (0 the far edge .. 1 the bottom). The upper half is as
    wide as the branch's strip, the lower half rounds off; dark torn wood in its own shade, faint growth rings round
    the pith, splits from it, torn fibres, the bark round it (symmetric: the tiles mirror)."""
    w = 2 * v - 1
    if abs(u) >= 1.0:
        return 0
    r = math.hypot(u, w * math.sqrt(max(0.0, 1 - u * u))) if w <= 0 else math.hypot(u, w)
    # the torn outline: splinters and bites along the sides and the bottom (by place round the edge, in steps)
    if w > -0.85:
        if w > 0:                                         # the bottom: bites and splinters
            h = _hash(77, int((math.atan2(w, abs(u)) + 1.6) * 14)) % 100
            lim = 1.0 - (0.0 if h < 45 else 0.035 if h < 75 else 0.075 if h < 92 else 0.12)
        else:                                             # the sides: a finely ragged edge
            h = _hash(78, int(v * 64)) % 100
            lim = 1.0 - (0.0 if h < 60 else 0.02 if h < 90 else 0.04)
        if r > lim:
            return 0
        r = r / lim
    if r > 1.0:
        return 0
    col = int(abs(u) * 64)
    hh = _hash(41, col, int(v * 64)) % 100
    if r > 0.975:
        return BK["rim"]
    side = w > -0.5 or abs(u) > 0.75                       # (the top's bark is the far end's teeth)
    if r > 0.88 and side:                                 # the bark round it
        return BK["b5"] if hh < 70 else BK["b6"]
    if r > 0.84 and side:
        return BK["b6"]                                   # the cambium line
    ang = math.atan2(w, abs(u))
    for c in (-1.2, -0.35, 0.45, 1.25):                    # splits from the pith
        if r > 0.12 and abs(ang - c) * r < 0.022 + 0.02 * r:
            return BK["knot"]
    if r < 0.08:
        return BK["knot"]
    ring = int((r + 0.025 * math.sin(ang * 5 + 1)) / 0.13)
    c = BK["b4"] if ring % 2 == 0 else BK["b5"]
    if hh < 9:
        return BK["b6"]                                   # torn pits
    if hh > 93 or (w < -0.4 and hh > 84):
        return BK["b3"]                                   # torn fibres catching the light (more toward the top)
    return c


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
    # a gap's two broken ends, 8 rows each over 24 units (src/road.c: 8 on the bark's side of the break, 16 into the
    # gap; row 0 the nearest): pointed teeth of torn bark of all lengths, in the bark's own shades
    near_e = teeth(2 * R, 7, 2, 7)                        # the near end: the bark reaches row e (the break is row 3)
    far_e = teeth(2 * R, 23, 2, 6)                        # the far end: the bark starts at row 7 - e (the break row 5)
    for i, x in enumerate(range(256 - R, 256 + R)):
        u = (x + 0.5 - 256) / R
        e = near_e[i]
        for r in range(8):
            if r < e - 1:
                T[248 + r][x] = base.texel(u, (r * 3) % 64, plain=True)
            elif r == e - 1:
                T[248 + r][x] = SHADES[max(0, shade_of(math.asin(u)) - 1)] if abs(u) < 0.985 else BK["rim"]   # the lit lip
            elif r == e:
                T[248 + r][x] = BK["rim"]                  # the torn edge
        d = 7 - far_e[i]
        for r in range(8):
            if r > d:
                T[240 + r][x] = base.texel(u, (r * 5) % 64, plain=True)
            elif r == d:
                T[240 + r][x] = SHADES[max(0, shade_of(math.asin(u)) - 2)] if abs(u) < 0.985 else BK["rim"]   # its lit lip
            elif r == d - 1:
                T[240 + r][x] = BK["rim"]                  # the shadow under it
            else:
                T[240 + r][x] = BK["b6"]                   # the broken face's top, deep between the teeth
    # the far side's broken face (src/road.c draws it on the gap's empty lines): rows 64-127, columns 0-127
    for r in range(64):
        for x in range(128):
            T[64 + r][x] = face_texel((x + 0.5 - 64) / 64.0, (r + 0.5) / 64.0, x)
    return T

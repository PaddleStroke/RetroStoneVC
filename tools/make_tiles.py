#!/usr/bin/env python3
"""Code-drawn terrain tileset for Bomber Mole ("tileset A"): proper 16x16 pixel art.

Each material gets a 4-shade ramp per season, taken from the colours of the owner's AI tiles
(games/bombermole/art/incoming/tile_<season>_<name>.png; the placeholders' colours when a strip is
missing). The tiles are drawn with clear shapes instead of per-pixel noise: grass with a few blade
clusters, dirt blocks with a lit top and a few pebbles, bevelled boulders, bricks, twisting roots, a
2-frame water shimmer. Common tiles get 2 extra variants (the game picks one per cell with a hash), and
there are transition tiles (the water bank).

    make_tiles.py [--out games/bombermole/art/tilesets/code] [--preview]

Writes tiles.png (the tiles.png layout of docs/art-sheets.md) and tiles_extra.png (one row per season:
the columns of EXTRA below). MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import argparse
import colorsys
import os
import random
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import sheets  # noqa: E402
import make_placeholders as mp  # noqa: E402

EXTRA = [n for n, _ in sheets.TILE_EXTRAS]
MAG = (255, 0, 255)


# ---- colours ------------------------------------------------------------------------------------
def lum(c):
    return 0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2]


def ramp_from_image(path, n=4):
    """n shades (dark to light) of the dominant colours of an AI tile strip."""
    import numpy as np
    im = Image.open(path).convert("RGB")
    im = im.resize((96, max(1, 96 * im.height // im.width)), Image.NEAREST)
    a = np.asarray(im).reshape(-1, 3).astype(np.float32)
    a = a[~((a[:, 0] > 200) & (a[:, 2] > 200) & (a[:, 1] < 80))]
    if len(a) < 50:
        return None
    hsv = np.array([colorsys.rgb_to_hsv(*(p / 255.0)) for p in a])
    # the material's own colour: the most common hue (leaves, flowers and other accents dropped)
    sat = hsv[:, 1] > 0.18
    if sat.mean() > 0.4:
        hist, edges = np.histogram(hsv[sat, 0], bins=24, range=(0, 1))
        hc = (edges[hist.argmax()] + edges[hist.argmax() + 1]) / 2
        dh = np.abs(hsv[:, 0] - hc)
        keep = np.minimum(dh, 1 - dh) < 0.07
    else:
        keep = hsv[:, 1] < 0.35
    a = a[keep] if keep.sum() > 30 else a
    L = a @ np.array([0.299, 0.587, 0.114], np.float32)
    a = a[np.argsort(L)]
    a = a[int(len(a) * 0.06):len(a) - int(len(a) * 0.03)]      # outlines and glints out
    bins = np.array_split(a, n)
    cols = [tuple(int(v) for v in np.median(b, axis=0)) for b in bins]
    return spread(cols)


def spread(cols, step=0.13):
    """Make the 4 shades clearly distinct: value steps of at least `step` around their mean,
    each shade keeping its own hue and saturation (so a season's colours stay as drawn)."""
    hsv = [colorsys.rgb_to_hsv(*(x / 255.0 for x in c)) for c in sorted(cols, key=lum)]
    vs = [v for _, _, v in hsv]
    mean = sum(vs) / len(vs)
    k = max(step, max(b - a for a, b in zip(vs, vs[1:])))
    out = []
    for i, (h, s, v) in enumerate(hsv):
        tv = mean + (i - (len(vs) - 1) / 2) * k
        if abs(v - mean) < abs(tv - mean):
            v = tv
        v = max(0.12, min(1.0, v))
        s = min(1.0, s * (1.12 if i == 0 else 1.0))      # dark shades slightly richer
        out.append(tuple(int(round(x * 255)) for x in colorsys.hsv_to_rgb(h, s, v)))
    return out


def scale_ramp(r, k):
    return [shade(c, k) for c in r]


def shade(c, k):
    return tuple(max(0, min(255, int(x * k))) for x in c)


FALLBACK = {   # when an AI strip is missing: the placeholders' colours
    "grass": ("grass_d", "grass", "grass_l"), "soft_dirt": ("dirt_d", "dirt", "dirt_l"),
    "hard_rock": ("rock_d", "rock", "rock_l"), "stone": ("stone_d", "stone", "stone_l"),
    "tunnel": ("tunnel_d", "tunnel", "tunnel_l"), "roots": ("dirt_d", "root", "root_l"),
    "ladder": ("wood_d", "wood", "wood"), "water": ("water_d", "water", "water_l"),
    "leaves": ("leaf_r", "leaf_o", "leaf_y"), "frozen_dirt": ("frozen", "ice", "snow"),
}


def ramps_for(season_i, incoming):
    season = sheets.SEASONS[season_i]
    P = dict(mp.BASE)
    P.update(mp.SEASON_TWEAKS[season])
    R = {}
    for mat, keys in FALLBACK.items():
        path = os.path.join(incoming, "tile_%s_%s.png" % (season, mat))
        # wood: the AI ladder and plank tiles are mostly the floor around them, so the wood keeps
        # the placeholders' brown in every season
        r = ramp_from_image(path) if os.path.exists(path) and mat != "ladder" else None
        if r is None:
            c = [P[k] for k in keys]
            r = spread([shade(c[0], 0.75), c[0], c[1], shade(c[2], 1.2)])
        R[mat] = r
    # the tunnel floor must read as lower than the soil blocks: at most 80% of their brightness
    k = 0.8 * lum(R["soft_dirt"][1]) / max(1.0, lum(R["tunnel"][2]))
    if k < 1:
        R["tunnel"] = scale_ramp(R["tunnel"], k)
    R["gold"] = (255, 214, 72)
    R["black"] = (16, 10, 8)
    R["outline"] = shade(R["hard_rock"][0], 0.55)
    return R


# ---- drawing helpers ----------------------------------------------------------------------------------
class Tile:
    def __init__(self, fill):
        self.p = [[fill] * 16 for _ in range(16)]

    def set(self, x, y, c, wrap=True):
        if wrap:
            self.p[y % 16][x % 16] = c
        elif 0 <= x < 16 and 0 <= y < 16:
            self.p[y][x] = c

    def rect(self, x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.set(x, y, c, wrap=False)

    def image(self):
        im = Image.new("RGB", (16, 16))
        im.putdata([c for row in self.p for c in row])
        return im


def grass(R, seed):
    g = R["grass"]
    t = Tile(g[1])
    rng = random.Random(seed)
    # a few blade clusters: a dark root pixel, two blades, a light tip (wraps: seamless)
    for _ in range(4):
        x, y = rng.randrange(16), rng.randrange(16)
        t.set(x, y, g[0])
        t.set(x - 1, y - 1, g[0])
        t.set(x + 1, y - 1, g[0])
        t.set(x - 1, y - 2, g[2])
        t.set(x + 1, y - 2, g[3])
    for _ in range(3):                 # soft lighter patches
        x, y = rng.randrange(16), rng.randrange(16)
        for dx in range(3):
            t.set(x + dx, y, g[2])
    return t


def grass_edge(R):
    t = grass(R, 11)
    g = R["grass"]
    for x in range(16):
        t.set(x, 0, g[0])
        t.set(x, 1, g[0] if x % 4 else g[1])
        t.set(x, 2, g[1] if x % 3 else g[0])
    return t


def block(ramp, seed, pebbles=3, top=None):
    """A raised block: lit top and left, shadowed bottom and right, a few pebbles."""
    d, m, l, h = ramp
    t = Tile(m)
    t.rect(0, 0, 15, 2, l)
    t.rect(0, 0, 15, 0, h)
    t.rect(0, 0, 0, 15, l)
    t.rect(0, 13, 15, 15, d)
    t.rect(15, 0, 15, 15, d)
    rng = random.Random(seed)
    for _ in range(pebbles):
        x, y = rng.randint(3, 11), rng.randint(5, 10)
        t.set(x, y, l, wrap=False)
        t.set(x + 1, y, l, wrap=False)
        t.set(x, y + 1, d, wrap=False)
        t.set(x + 1, y + 1, d, wrap=False)
    if top:
        t.rect(0, 0, 15, 2, top[1])
        t.rect(0, 0, 15, 0, top[2])
        for x in range(0, 16, 3):
            t.set(x, 3, top[1], wrap=False)
    return t


def contact_shadow(t, c):
    """A soft 1-px contact shadow: dithered dark pixels under the lowest pixel of each column."""
    for x in range(16):
        ys = [y for y in range(16) if t.p[y][x] != MAG]
        if ys and ys[-1] < 15 and (x + ys[-1]) % 2 == 0:
            t.p[ys[-1] + 1][x] = c
    return t


def ellipse_obj(ramp, o, cx=7.5, cy=8.5, rx=7.2, ry=6.6, light=-0.55, dark=0.65):
    """A rounded object on transparent ground: lit top-left, shaded bottom-right, dark outline."""
    d, m, l, h = ramp
    t = Tile(MAG)
    for y in range(16):
        for x in range(16):
            dx, dy = (x - cx) / rx, (y - cy) / ry
            r = dx * dx + dy * dy
            if r <= 1.0:
                c = m
                if dx + dy < light:
                    c = h if r < 0.5 else l
                elif dx + dy > dark:
                    c = d
                t.p[y][x] = c
            elif r <= 1.3:
                t.p[y][x] = o
    return t


def boulder(R):
    t = ellipse_obj(R["hard_rock"], R["outline"], cy=8.0, rx=7.3, ry=6.8)
    d = R["hard_rock"][0]
    for (x, y) in ((9, 6), (10, 7), (10, 8), (11, 9)):  # a crack
        t.set(x, y, d, wrap=False)
    return contact_shadow(t, R["outline"])


def bricks(R):
    """Unbreakable stone: a block of bricks filling the cell (its corners rounded off)."""
    d, m, l, h = R["stone"]
    t = Tile(m)
    for row in range(2):
        y0 = row * 8
        off = 0 if row == 0 else 4
        t.rect(0, y0 + 7, 15, y0 + 7, R["black"])
        for bx in range(-8, 16, 8):
            x0 = bx + off
            t.rect(x0, y0, x0 + 7, y0, h)                # lit top of each brick
            for y in range(y0, y0 + 7):
                t.set(x0, y, l)                          # lit left
                t.set(x0 + 7, y, R["black"])             # joint
            t.rect(x0 + 1, y0 + 6, x0 + 6, y0 + 6, d)    # shaded bottom
    for (x, y) in ((0, 0), (15, 0), (0, 15), (15, 15)):
        t.p[y][x] = MAG
    return t


def floor(ramp, seed, pebbles=True):
    d, m, l, h = ramp
    t = Tile(m)
    rng = random.Random(seed)
    for _ in range(5):                                   # a few darker patches (seamless)
        x, y = rng.randrange(16), rng.randrange(16)
        t.set(x, y, d)
        t.set(x + 1, y, d)
    if pebbles:
        for _ in range(2):
            x, y = rng.randrange(16), rng.randrange(16)
            t.set(x, y, l)
            t.set(x + 1, y, l)
            t.set(x, y + 1, d)
            t.set(x + 1, y + 1, d)
    return t


def roots(R):
    """Thick twisting roots across the cell, the ground showing between them."""
    t = Tile(MAG)
    d, m, l, h = R["roots"]
    o = R["outline"]
    import math
    for y in range(16):                                   # a vertical root (drawn first, below)
        x = 6 + int(round(1.5 * math.sin(y * math.pi / 8)))
        t.set(x - 2, y, o)
        t.set(x - 1, y, l)
        t.set(x, y, m)
        t.set(x + 1, y, d)
        t.set(x + 2, y, o)
    for k, (y0, amp, ph) in enumerate(((3, 2.0, 0.0), (11, 1.6, 2.0))):   # two thick twisting roots
        for x in range(16):
            y = int(round(y0 + amp * math.sin(ph + x * math.pi / 8)))
            t.set(x, y - 2, o)
            t.set(x, y - 1, h if x % 5 == 0 else l)
            t.set(x, y, m)
            t.set(x, y + 1, d)
            t.set(x, y + 2, o)
    return t


def hole(R, up=False):
    t = Tile(MAG)
    d, m, l, h = R["soft_dirt"]
    for y in range(16):
        for x in range(16):
            r = ((x - 7.5) ** 2 + (y - 7.5) ** 2) ** 0.5
            if up:
                if r < 6.5:
                    t.set(x, y, (255, 244, 196) if r < 4 else (240, 214, 150), wrap=False)
            else:
                if r < 5.2:
                    t.set(x, y, R["black"], wrap=False)
                elif r < 7.0:
                    t.set(x, y, l if y < 8 else d, wrap=False)
                elif r < 7.6:
                    t.set(x, y, R["outline"], wrap=False)
    if up:
        for (x, y) in ((1, 1), (14, 1), (1, 14), (14, 14), (2, 2), (13, 2), (2, 13), (13, 13)):
            t.set(x, y, (240, 214, 150), wrap=False)
    return t


def ladder(R):
    t = Tile(MAG)
    d, m, l, h = R["ladder"]
    o = R["outline"]
    for x in (3, 12):
        t.rect(x - 1, 0, x - 1, 15, o)
        t.rect(x, 0, x, 15, l)
        t.rect(x + 1, 0, x + 1, 15, d)
    for y in (2, 7, 12):
        t.rect(4, y, 11, y, l)
        t.rect(4, y + 1, 11, y + 1, d)
        t.rect(4, y + 2, 11, y + 2, o)                   # the rung's shadow on the floor
    return t


def thin_floor(R):
    d, m, l, h = R["ladder"]
    t = Tile(m)
    for i, y in enumerate((0, 5, 10)):                    # three planks, lit top, dark bottom gap
        t.rect(0, y, 15, y, l)
        t.rect(0, y + 3, 15, y + 3, d)
        t.rect(0, y + 4, 15, y + 4, R["outline"])
        sx = (3, 11, 7)[i]                                 # the plank ends, staggered
        for yy in range(y, y + 4):
            t.set(sx, yy, R["outline"])
            t.set(sx + 1, yy, l)
    for (x, y) in ((5, 1), (6, 2), (6, 3), (13, 6), (12, 7), (12, 8), (3, 11), (4, 12)):
        t.set(x, y, R["black"], wrap=False)                # cracks
    return t


def mound(R, open_):
    """The exit molehill: a mound of soil on the ground (transparent around it)."""
    t = Tile(MAG)
    d, m, l, h = R["soft_dirt"]
    o = R["outline"]
    for y in range(16):
        for x in range(16):
            dx, dy = (x - 7.5) / 7.0, (y - 10.0) / 5.8
            r = dx * dx + dy * dy
            if r <= 1.0 and y >= 3:
                t.set(x, y, l if dx + dy < -0.3 else (d if dx + dy > 0.6 else m), wrap=False)
            elif r <= 1.25 and y >= 3:
                t.set(x, y, o, wrap=False)
    if open_:
        for y in range(7, 13):
            for x in range(4, 12):
                if ((x - 7.5) / 3.6) ** 2 + ((y - 9.8) / 2.6) ** 2 <= 1.0:
                    t.set(x, y, R["black"], wrap=False)
        for (x, y) in ((7, 1), (8, 1), (2, 3), (13, 3), (1, 8), (14, 8)):
            t.set(x, y, R["gold"], wrap=False)
    else:
        for i in range(4):
            t.set(6 + i, 8 + i, d, wrap=False)
            t.set(9 - i, 8 + i, d, wrap=False)
    return t


def dirt_mound(R, seed, cracked=False):
    """Soft dirt on the SURFACE: a rounded mound of soil sitting on the grass or snow."""
    t = ellipse_obj(R["soft_dirt"], R["outline"], cy=8.2, rx=7.4, ry=7.0, light=-0.5, dark=0.6)
    d, m, l, h = R["soft_dirt"]
    rng = random.Random(seed)
    for _ in range(3):                                   # a few pebbles
        x, y = rng.randint(4, 10), rng.randint(6, 10)
        t.set(x, y, l, wrap=False)
        t.set(x, y + 1, d, wrap=False)
    for x in range(3, 13):                               # a few crumbs of the top soil
        if t.p[3][x] == m and x % 3 == 0:
            t.p[3][x] = h
    if cracked:
        for (x, y) in ((4, 4), (5, 5), (6, 6), (6, 7), (7, 8), (9, 8), (10, 9), (11, 10), (7, 9), (6, 10), (6, 11)):
            t.set(x, y, R["black"] if (x + y) % 2 else d, wrap=False)
    return contact_shadow(t, R["outline"])


def puddle(R):
    t = Tile(MAG)
    d, m, l, h = R["water"]
    for y in range(16):
        for x in range(16):
            r = ((x - 7.5) / 6.5) ** 2 + ((y - 8.5) / 4.5) ** 2
            if r <= 1.0:
                t.set(x, y, m if r > 0.35 else l, wrap=False)
            elif r <= 1.2:
                t.set(x, y, d, wrap=False)
    t.rect(4, 6, 6, 6, h)
    return t


def frozen(R):
    """Frozen soil: a frosty mound of soil under a snow cap (bombs only)."""
    t = ellipse_obj(R["frozen_dirt"], R["outline"], cy=8.4, rx=7.4, ry=7.0)
    d, m, l, h = R["frozen_dirt"]
    for (x0, y0) in ((3, 10), (8, 12)):                    # ice streaks
        for i in range(4):
            if t.p[y0 - i // 2][x0 + i] != MAG:
                t.set(x0 + i, y0 - i // 2, h, wrap=False)
    snow, snow_s = (240, 246, 255), (196, 212, 236)
    for x in range(16):                                    # a snow cap with a wavy lower edge
        ys = [y for y in range(16) if t.p[y][x] != MAG]
        if not ys:
            continue
        depth = 4 + (1 if x % 5 in (1, 2) else 0) - (1 if x % 7 == 4 else 0)
        for y in range(ys[0], min(ys[-1], ys[0] + depth)):
            t.p[y][x] = snow if y < ys[0] + depth - 1 else snow_s
    return contact_shadow(t, R["outline"])


def leaves(R):
    """A pile of fallen leaves on the ground (transparent between the leaves)."""
    t = Tile(MAG)
    cols = R["leaves"]
    o = R["outline"]
    spots = [(3, 4), (8, 3), (12, 5), (5, 8), (10, 8), (2, 11), (7, 12), (12, 11), (8, 7), (4, 6), (11, 2)]
    for i, (x, y) in enumerate(spots):
        c = cols[1 + i % 3]
        t.set(x - 1, y + 1, o, wrap=False)
        t.set(x + 3, y, o, wrap=False)
        t.rect(x, y, x + 2, y + 1, c)
        t.set(x + 1, y + 2, cols[0], wrap=False)
    return t


def water(R, frame=0, bank=False):
    d, m, l, h = R["water"]
    t = Tile(m)
    for row, y in enumerate((3, 9, 14)):
        x0 = (row * 6 + frame * 3) % 16
        for i in range(5):
            t.set(x0 + i, y - (1 if i in (1, 2, 3) else 0), l)
        t.set(x0 + 2, y - 2, h)
        t.set(x0 + 8, y + 1, d)
        t.set(x0 + 9, y + 1, d)
    if bank:
        g = R["grass"]
        t.rect(0, 0, 15, 2, g[1])
        for x in range(0, 16, 3):
            t.set(x, 1, g[2], wrap=False)
        t.rect(0, 3, 15, 3, g[0])
        t.rect(0, 4, 15, 4, d)
    return t


# ---- props and sprites the code set redraws (tilesets/code/<entry>.png) --------------------------------
WOOD = [(96, 60, 32), (140, 94, 52), (184, 132, 76), (220, 176, 112)]
WOOD_O = (48, 30, 18)


def bridge(vertical):
    """Planks across the way, rails along it, posts at the ends on the banks; the water shows on the
    two open sides (transparent). Drawn crossed up-down, turned for left-right."""
    d, m, l, h = WOOD
    t = Tile(MAG)
    for y in range(16):                                   # planks: 3 px each, a dark gap
        k = y % 4
        c = h if k == 0 else l if k == 1 else m if k == 2 else d
        t.rect(3, y, 12, y, c)
    for x in (1, 14):                                     # rails along the way
        t.rect(x, 0, x, 15, WOOD_O)
    t.rect(2, 0, 2, 15, l)
    t.rect(13, 0, 13, 15, d)
    for y in (0, 1, 14, 15):                              # posts at the ends
        for x in (1, 2, 13, 14):
            t.p[y][x] = d if y in (0, 15) else m
    if not vertical:
        t.p = [[t.p[x][y] for x in range(16)] for y in range(16)]
    return t


def crate():
    d, m, l, h = WOOD
    t = Tile(MAG)
    t.rect(1, 1, 14, 13, m)
    t.rect(1, 1, 14, 1, h)
    t.rect(1, 1, 1, 13, l)
    t.rect(1, 13, 14, 13, d)
    t.rect(14, 1, 14, 13, d)
    for i in range(12):                                   # the diagonal brace
        t.set(2 + i, 2 + i * 10 // 11, d, wrap=False)
        t.set(3 + i, 2 + i * 10 // 11, l, wrap=False)
    for x in range(0, 16):                                # outline
        for y in (0, 14):
            if 1 <= x <= 14:
                t.p[y][x] = WOOD_O
    for y in range(1, 14):
        t.p[y][0] = t.p[y][15] = WOOD_O
    return contact_shadow(t, WOOD_O)


def log():
    """The floating log: bark with grain lines, BOTH ends cut (light wood with rings): no dark end
    that could read as a head."""
    d, m, l, h = WOOD
    ring, cut = (170, 120, 64), (232, 196, 140)
    t = Tile(MAG)
    t.rect(3, 4, 12, 12, m)
    t.rect(3, 4, 12, 4, WOOD_O)
    t.rect(3, 12, 12, 12, WOOD_O)
    t.rect(3, 5, 12, 5, l)
    t.rect(3, 11, 12, 11, d)
    for y, x0, x1 in ((7, 4, 7), (8, 7, 11), (10, 5, 9)):  # bark grain
        t.rect(x0, y, x1, y, d)
    for cx in (2, 13):                                    # the two cut ends
        for y in range(4, 13):
            for x in range(cx - 2, cx + 3):
                r = ((x - cx) / 2.3) ** 2 + ((y - 8) / 4.6) ** 2
                if r <= 1.0:
                    t.p[y][x] = cut if r > 0.25 else ring
                    if 0.45 < r < 0.7:
                        t.p[y][x] = ring
                elif r <= 1.35:
                    t.p[y][x] = WOOD_O
    return t


def season_tiles(R):
    tiles = {
        "grass": grass(R, 1), "grass_edge": grass_edge(R), "soft_dirt": block(R["soft_dirt"], 3),
        "hard_rock": boulder(R), "stone": bricks(R), "roots": roots(R), "tunnel": floor(R["tunnel"], 5),
        "hole_down": hole(R), "hole_up": hole(R, up=True), "ladder": ladder(R), "thin_floor": thin_floor(R),
        "exit_closed": mound(R, False), "exit_open": mound(R, True), "puddle": puddle(R),
        "frozen_dirt": frozen(R), "leaves": leaves(R), "water": water(R),
    }
    crack = block(R["soft_dirt"], 3)
    d = R["soft_dirt"][0]
    for (x, y) in ((3, 3), (4, 4), (5, 5), (5, 6), (6, 7), (8, 7), (9, 8), (10, 9), (10, 10), (6, 8), (6, 9), (5, 10), (5, 11)):
        crack.set(x, y, R["black"] if (x + y) % 2 else d, wrap=False)
    tiles["dirt_crack"] = crack
    extra = {
        "grass_v2": grass(R, 2), "grass_v3": grass(R, 3), "soft_dirt_v2": block(R["soft_dirt"], 4, pebbles=2),
        "soft_dirt_v3": block(R["soft_dirt"], 5, pebbles=4), "tunnel_v2": floor(R["tunnel"], 6),
        "tunnel_v3": floor(R["tunnel"], 8, pebbles=False), "water_f2": water(R, frame=1),
        "water_edge": water(R, bank=True), "dirt_mound": dirt_mound(R, 7),
        "dirt_mound_crack": dirt_mound(R, 7, cracked=True),
    }
    return tiles, extra


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", default=os.path.join(ROOT, "games", "bombermole", "art", "tilesets", "code"))
    ap.add_argument("--incoming", default=os.path.join(ROOT, "games", "bombermole", "art", "incoming"))
    ap.add_argument("--preview", action="store_true")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    W, H = sheets.sheet_size("tiles")
    sheet = Image.new("RGB", (W, H), MAG)
    extra_img = Image.new("RGB", (16 * len(EXTRA), 64), MAG)
    for s in range(4):
        R = ramps_for(s, a.incoming)
        tiles, extra = season_tiles(R)
        for e in sheets.entries("tiles"):
            x, y, w, h = sheets.frame_rects(e, s)[0]
            sheet.paste(tiles[e.name].image(), (x, y))
        for i, name in enumerate(EXTRA):
            extra_img.paste(extra[name].image(), (i * 16, s * 16))
    sheet.save(os.path.join(a.out, "tiles.png"))
    extra_img.save(os.path.join(a.out, "tiles_extra.png"))
    props = {"bridge": bridge(False), "bridge_v": bridge(True), "crate": crate(), "log": log()}
    for name, t in props.items():
        t.image().save(os.path.join(a.out, name + ".png"))
    print("wrote", os.path.join(a.out, "tiles.png"), "tiles_extra.png and", ", ".join(n + ".png" for n in props))
    if a.preview:
        both = Image.new("RGB", (W + 16 * len(EXTRA) + 8 + 16 * len(props) + 8, H), (24, 24, 32))
        both.paste(sheet, (0, 0))
        both.paste(extra_img, (W + 8, 0))
        for i, t in enumerate(props.values()):
            both.paste(t.image(), (W + 16 * len(EXTRA) + 16 + i * 16, 0))
        both.resize((both.width * 4, both.height * 4), Image.NEAREST).save(os.path.join(a.out, "preview_4x.png"))


if __name__ == "__main__":
    main()

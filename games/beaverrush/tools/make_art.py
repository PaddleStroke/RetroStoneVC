#!/usr/bin/env python3
"""Beaver Rush: the code-drawn art, in the 8BCraft house style (games/common/tools/house_style.py,
docs/art-direction.md): flat shades (2-4 per material), 1-px dark outlines that are never black, light from
the top-left, readable at 1x.

Every colour comes from the explicit 16-colour palettes below (PAL_*): the C side swaps the season ramps
(foliage, grass, snow) by palette index and tints everything by the time of day, so the art never picks its
own colours. build_assets.py turns the canvases into tiles, maps and sprites.

    python3 games/beaverrush/tools/make_art.py --out DIR      writes the sheets as PNG (review, x3)

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/beaverrush/LICENSE.
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

# ---- seasons (0 summer, 1 autumn, 2 winter, 3 spring): the swapped ramps ------------------------------------
LEAF = [hs.HOUSE["leaf"],                                                      # summer: the house leaf
        [(255, 206, 96), (236, 142, 52), (192, 84, 42), (124, 48, 32)],         # autumn
        [(252, 252, 255), (222, 232, 246), (176, 194, 220), (118, 136, 170)],   # winter: snow on bare twigs
        [(214, 238, 140), (160, 210, 90), (98, 162, 60), (54, 106, 40)]]        # spring: fresh green
ACCENT = [(246, 226, 110), (206, 60, 52), (255, 255, 255), (255, 176, 206)]    # flowers, berries, snow, blossom
GRASS = [[(170, 212, 92), (118, 172, 60), (76, 128, 42), (44, 84, 34)],
         [(212, 196, 98), (176, 150, 62), (128, 108, 46), (86, 70, 36)],
         [(250, 252, 255), (216, 228, 244), (170, 188, 214), (112, 128, 160)],
         [(188, 230, 110), (130, 196, 70), (80, 146, 48), (46, 96, 36)]]
FOREST = [[(92, 146, 110), (64, 110, 90), (42, 78, 72)],
          [(170, 120, 80), (130, 86, 62), (90, 62, 56)],
          [(206, 218, 232), (150, 168, 192), (96, 112, 142)],
          [(118, 170, 116), (82, 132, 96), (52, 94, 78)]]

OUT_WOOD = (52, 30, 22)
BARK = [(214, 164, 104), (178, 124, 72), (134, 88, 48), (92, 56, 30), (62, 36, 20)]
CUT = [(240, 212, 150), (214, 172, 108), (160, 110, 60)]
GOLDB = [(255, 244, 170), (252, 216, 72), (222, 168, 40), (186, 128, 22), (130, 84, 14)]
GOLDC = [(255, 250, 214), (250, 222, 120), (200, 150, 40)]
RIBBON = (222, 52, 56)


def pal_trunk(s, gold=False):
    b, c = (GOLDB, GOLDC) if gold else (BARK, CUT)
    return [None, (90, 56, 10) if gold else OUT_WOOD] + b + c + LEAF[s] + [ACCENT[s], RIBBON]


def pal_ground(s):
    return [None, (40, 30, 22)] + GRASS[s] + [(170, 120, 76), (130, 86, 52), (90, 58, 36), (170, 170, 160),
                                               (110, 110, 108), BARK[1], BARK[2], BARK[3], CUT[0], CUT[2]]


def pal_dam(s):
    return [None, (46, 28, 20), (196, 140, 84), (150, 100, 56), (104, 64, 34), (232, 196, 138), (160, 112, 62),
            (150, 112, 76), (116, 84, 56), (80, 58, 40), GRASS[s][1], GRASS[s][2], (230, 245, 255), ACCENT[s],
            (126, 90, 54)]


def pal_banks(s):
    return [None, (30, 34, 30), (150, 110, 80), (96, 68, 50)] + LEAF[s] + [(150, 120, 80), (116, 90, 60),
                                                                          (80, 62, 44), GRASS[s][1], GRASS[s][2],
                                                                          (140, 140, 130), (96, 96, 92)]


CLOUDS = [(252, 252, 255), (226, 232, 246), (196, 206, 230), (160, 172, 206)]


def pal_far(s):
    """the far layer: mountains, the far forest, the river's ripples and the clouds (entries 11..14)"""
    return [None, (150, 160, 190), (116, 126, 160), (88, 96, 130), (240, 244, 252), (196, 206, 228)] + FOREST[s] + \
        [(220, 240, 255), (150, 200, 230)] + CLOUDS


PAL_CLOUDS = [None] + CLOUDS

BEAVER_FUR = hs.ACCENTS["beaver"]
BEAVER2_FUR = [(222, 150, 118), (180, 100, 70), (130, 62, 46), (82, 36, 30)]


def pal_beaver(p2=False):
    pal = [None, (46, 24, 16)] + BEAVER_FUR + [(242, 214, 168), (214, 176, 128), (110, 82, 70), (74, 52, 44),
                                               (255, 190, 80), (214, 120, 34), (250, 250, 250), (18, 8, 26),
                                               (64, 30, 30), (214, 96, 96)]
    if p2:      # player 2: the house palette swap, the fur's hue turned to a darker red-brown
        pal = [None] + hs.hue_swap(pal[1:6], 0.02, 0.16, -0.045, sat_min=0.2, sat_mul=1.15) + pal[6:]
    return pal


PAL_FX = [None, (22, 18, 40), (250, 250, 250), (196, 238, 255), (112, 184, 226), (60, 120, 180), (255, 240, 150),
          (255, 200, 70), (240, 150, 50), (250, 246, 220), (200, 200, 180), (220, 255, 120), (255, 250, 200),
          (170, 200, 255), (255, 220, 60)]
PAL_BIRD = [None, (22, 18, 40), (40, 40, 52), (240, 240, 240), (150, 150, 160), (230, 60, 50), (160, 30, 30),
            (240, 200, 90), (170, 120, 50)]


def pal_particle(s):
    if s == 3:                                      # spring: blossom petals
        return [None, (120, 40, 70), (255, 214, 230), (255, 176, 206), (214, 110, 150), (255, 255, 255)]
    return [None, (40, 30, 30), LEAF[s][0], LEAF[s][1], LEAF[s][3], ACCENT[s]]


# BG palettes (docs/art-direction.md: 0 the UI, 1-4 the playfield, 5 the title logo, 6-7 the mid and far layers):
# 1 trunk, 2 golden trunk, 3 near bank, 4 dam, 6 banks and framing trees, 7 far (mountains, forest, clouds).
# OBJ: 0 beaver, 1 beaver 2 (hue-rotated), 2 wood (logs, chips), 3 the kit, 4 fx, 5 golden wood, 6 bird, 7 particles.
def bg_palettes(s):
    return {1: pal_trunk(s), 2: pal_trunk(s, True), 3: pal_ground(s), 4: pal_dam(s), 6: pal_banks(s), 7: pal_far(s)}


def obj_palettes(s):
    return {0: pal_beaver(), 1: pal_beaver(True), 2: pal_trunk(s), 3: KIT, 4: PAL_FX, 5: pal_trunk(s, True), 6: PAL_BIRD,
            7: pal_particle(s)}


# drawing uses the summer palettes (season 0); the other seasons are the same indices
T = pal_trunk(0)
G = pal_ground(0)
D = pal_dam(0)
K = pal_banks(0)
F = pal_far(0)
B = pal_beaver()
X = PAL_FX
W = PAL_BIRD
P = pal_particle(0)
LIGHT = hs.LIGHT_DIR


def lit(dx, dy):
    """the house light: a dot product with the top-left direction"""
    return dx * LIGHT[0] + dy * LIGHT[1]


def shade4(nd, r4):
    return hs.shade_nd(nd, r4)


# ---- the trunk (BG2) ---------------------------------------------------------------------------------------------
SEG_W, SEG_H = 48, 24


def segment(look):
    """one 48x24 trunk segment: cylinder shading (light on the left), vertical bark fissures, plates, and a knot
    (look 2) or lichen (look 3). Tiles vertically with any other look."""
    cv = Canvas(SEG_W, SEG_H)
    rng = random.Random(100 + look)
    fis = [5 + look, 15 - look % 2, 24 + look % 3, 33, 41 - look % 2]
    for y in range(SEG_H):
        for x in range(SEG_W):
            if x == 0 or x == SEG_W - 1:
                cv.set(x, y, T[1])
                continue
            f = (x - 1) / 45.0
            k = 2 if f < 0.10 else 3 if f < 0.38 else 4 if f < 0.76 else 5
            cv.set(x, y, T[k])
    for i, fx in enumerate(fis):
        ph = rng.uniform(0, 6.28)
        for y in range(SEG_H):
            x = fx + int(round(1.2 * math.sin(2 * math.pi * y / 24.0 * (1 + i % 2) + ph)))
            base = cv.get(x, y)
            dark = T[6] if base in (T[4], T[5]) else T[5]
            cv.set(x, y, dark)
            if base == T[2] or base == T[3]:
                cv.set(x - 1, y, T[2])            # a lit edge on the left of each fissure
        # plates: short horizontal breaks between two fissures
        for yy in (rng.randrange(2, 10), rng.randrange(13, 22)):
            x0 = fx + 2
            for x in range(x0, min(x0 + rng.randrange(3, 6), SEG_W - 2)):
                c = cv.get(x, yy)
                cv.set(x, yy, T[6] if c in (T[4], T[5]) else T[5])
    if look == 2:                                  # a knot
        for y in range(SEG_H):
            for x in range(SEG_W):
                d = math.hypot((x + 0.5 - 30) / 3.6, (y + 0.5 - 12) / 2.8)
                if d < 1.0:
                    cv.set(x, y, T[9] if d > 0.62 else T[8] if d > 0.3 else T[6])
                elif d < 1.35:
                    cv.set(x, y, T[5])
    if look == 3:                                  # lichen (takes the season's leaf colours)
        for (x, y) in [(20, 5), (21, 5), (21, 6), (22, 6), (20, 6), (19, 7), (22, 7), (36, 16), (37, 16), (37, 17)]:
            cv.set(x, y, T[11] if (x + y) % 3 else T[12])
    return cv


def branch(side, stolen=False):
    """a 40x24 branch on the `side` of the trunk ('L': it grows to the left, its base at x = 39): a tapered
    limb bending up, a twig, leaf clusters (the season's ramp: bare twigs with snow in winter)."""
    cv = Canvas(40, SEG_H)
    m = (lambda x: x) if side == "L" else (lambda x: 39 - x)
    wood = [T[2], T[3], T[4], T[5]]
    # the limb: from the trunk (x 40, y 15) to the tip (x 4, y 9), radius 3.6 -> 1.8
    n = 60
    for i in range(n + 1):
        t = i / float(n)
        x = 40 - 36 * t
        y = 15 - 5 * t - 2.0 * math.sin(math.pi * t) * 0.5
        r = 3.6 - 1.8 * t
        for yy in range(int(y - r) - 1, int(y + r) + 2):
            for xx in range(int(x - r) - 1, int(x + r) + 2):
                dx, dy = xx + 0.5 - x, yy + 0.5 - y
                if dx * dx + dy * dy <= r * r:
                    cv.set(m(xx), yy, wood[0 if dy < -r * 0.45 else 1 if dy < 0 else 2 if dy < r * 0.5 else 3])
    # a twig
    for i in range(12):
        t = i / 11.0
        cv.set(m(int(round(20 - 6 * t))), int(round(10 - 6 * t)), wood[2])
        if t < 0.5:
            cv.set(m(int(round(20 - 6 * t))), int(round(11 - 6 * t)), wood[3])
    leaves = [T[10], T[11], T[12], T[13]]
    blobs = [(7, 6, 5.2, 4.2), (3.5, 10, 3.6, 3.4), (12, 4, 3.8, 3.2), (15, 2.8, 3.0, 2.4), (10, 10, 3.6, 2.8)]
    for (bx, by, rx, ry) in blobs:
        for yy in range(int(by - ry) - 1, int(by + ry) + 2):
            for xx in range(int(bx - rx) - 1, int(bx + rx) + 2):
                dx, dy = (xx + 0.5 - bx) / rx, (yy + 0.5 - by) / ry
                if dx * dx + dy * dy <= 1.0:
                    nd = lit(dx if side == "L" else -dx, dy)     # shaded in screen space
                    cv.set(m(xx), yy, shade4(nd, leaves))
    # leaf texture: a few lighter and darker dots, and the accent (flowers, berries, blossoms)
    rng = random.Random(7)
    for _ in range(26):
        xx, yy = rng.randrange(0, 20), rng.randrange(0, 14)
        c = cv.get(m(xx), yy)
        if c in leaves:
            i = leaves.index(c)
            cv.set(m(xx), yy, leaves[max(0, i - 1)] if rng.random() < 0.5 else leaves[min(3, i + 1)])
    for (xx, yy) in [(5, 4), (11, 2), (2, 10), (9, 9)]:
        if cv.get(m(xx), yy) in leaves:
            cv.set(m(xx), yy, T[14])
    if stolen:                                      # the rival's red ribbon, tied round the limb
        for yy in range(8, 20):
            for xx in (24, 25):
                c = cv.get(m(xx), yy)
                if c in wood:
                    cv.set(m(xx), yy, T[15])
        for (xx, yy) in [(23, 19), (22, 20), (22, 21), (26, 19), (27, 20), (27, 21), (21, 21), (28, 21)]:
            cv.set(m(xx), yy, T[15])
    cv.outline(T[1])
    # the base against the trunk: no outline column there
    for yy in range(SEG_H):
        x = m(39)
        if cv.get(x, yy) == T[1] and cv.get(m(38), yy) not in (None, T[1]):
            cv.set(x, yy, cv.get(m(38), yy))
    return cv


# ---- the log (sprites): tumbling, floating, chips ------------------------------------------------------------------
def log_frame(angle, size=48, L=22.0, R=10.0):
    """the gnawed log in flight, rotated in the screen plane (degrees), with a cut face showing its rings at
    each end (a hint of 3D). OBJ palette 2 (5: golden)."""
    cv = Canvas(size, size)
    a = math.radians(angle)
    ca, sa = math.cos(a), math.sin(a)
    c0 = size / 2.0
    wood = [T[2], T[3], T[4], T[5]]
    for y in range(size):
        for x in range(size):
            px, py = x + 0.5 - c0, y + 0.5 - c0
            u = px * ca + py * sa
            v = -px * sa + py * ca
            if abs(v) > R:
                continue
            end = L - 3.5 * math.sqrt(max(0.0, 1 - (v / R) ** 2))     # the face bulges: an ellipse
            if abs(u) <= end:
                # bark: shaded across the log in SCREEN space (light from the top-left)
                nd = lit(-sa * v / R, ca * v / R)
                col = shade4(nd * 1.1, wood)
                # bark fissures along the log, broken into plates
                for fv, ph in ((-5.5, 0), (-1.0, 7), (3.5, 3), (7.5, 11)):
                    if abs(v - fv - 0.8 * math.sin(u / 3.0 + ph)) < 0.55 and int(u + 40 + ph) % 13 < 10:
                        col = T[5] if col != T[5] else T[6]
                cv.set(x, y, col)
            elif abs(u) <= L + 3.5:
                # the cut face: rings
                du = (abs(u) - L) / 3.5
                r = math.hypot(du, v / R)
                if r <= 1.0:
                    ring = int(r * 4.5)
                    cv.set(x, y, T[9] if ring % 2 else (T[7] if r < 0.5 else T[8]))
    cv.outline(T[1])
    return cv


def crop_center(cv, w, h):
    out = Canvas(w, h)
    ox, oy = (cv.w - w) // 2, (cv.h - h) // 2
    for y in range(h):
        for x in range(w):
            out.p[y][x] = cv.get(ox + x, oy + y)
    return out


def log_frames():
    """0 and 90 degrees in 48x24 and 24x48 cells (VRAM), 30 and 60 in 48x48; 120 and 150 are 60 and 30 flipped"""
    return [crop_center(log_frame(0), 48, 24), log_frame(30), log_frame(60), crop_center(log_frame(90), 24, 48)]


def chip(k):
    cv = Canvas(8, 8)
    shapes = [[(2, 3), (3, 2), (4, 2), (5, 3), (4, 4), (3, 4), (5, 4)],
              [(2, 2), (3, 3), (4, 3), (5, 4), (4, 5), (3, 4)],
              [(3, 2), (4, 3), (4, 4), (3, 5), (2, 4), (3, 3)]]
    for i, (x, y) in enumerate(shapes[k]):
        cv.set(x, y, T[7] if i % 3 else T[8])
    cv.set(*shapes[k][0], T[3])
    cv.outline(T[1])
    return cv


def floater(small):
    """a log floating to the dam: 16x8 (near) or 8x8 (far)"""
    cv = Canvas(8 if small else 16, 8)
    w = 5 if small else 11
    x0 = (cv.w - w) // 2
    for x in range(x0, x0 + w):
        for y in (3, 4, 5) if not small else (3, 4):
            cv.set(x, y, T[3] if y == 3 else T[4])
    cv.set(x0 + w - 1, 3, T[7]), cv.set(x0 + w - 1, 4, T[8])
    if not small:
        cv.set(x0 + w - 1, 5, T[9])
    cv.outline(T[1])
    return cv


def branch_piece(rot):
    """a broken-off branch tumbling away (32x32), rotated by rot quarter turns"""
    src = branch("L")
    cv = Canvas(32, 32)
    for y in range(24):
        for x in range(40):
            c = src.get(x, y)
            if c is None or x < 8:
                continue
            u, v = x - 8 - 16, y - 12
            for _ in range(rot):
                u, v = -v, u
            cv.set(16 + u, 16 + v, c)
    return cv


# ---- the beaver ----------------------------------------------------------------------------------------------------
def beaver(pose="idle", frame=0, scale=1.0, pal=None):
    """The hero, facing right (the trunk), feet on the cell's bottom row. 32x32 (scale 1) or 16x16 (the family,
    scale 0.5). Poses: idle, windup, bite, recover, hop, bonk, sleep, cheer."""
    C = pal or B
    out, fur, belly, tail, teeth = C[1], C[2:6], (C[6], C[7]), (C[8], C[9]), (C[10], C[11])
    white, ink, nose, pink = C[12], C[13], C[14], C[15]
    S = int(32 * scale)
    cv = Canvas(S, S)
    s = scale

    def E(cx, cy, rx, ry, r4=None, flat=None):
        hs.ellipse(cv, cx * s, cy * s, max(0.6, rx * s), max(0.6, ry * s), r4, flat)

    hx, hy = {"windup": (-1, -1), "bite": (3, 1), "recover": (1, 0), "hop": (0, -1), "bonk": (0, 1),
              "sleep": (-1, 5), "cheer": (0, -1)}.get(pose, (0, 0))
    by = {"sleep": 3, "hop": -1}.get(pose, 0)
    if pose == "idle" and frame == 1:
        hy, by = 0, 0
    # the tail: a flat paddle behind, cross-hatched
    ty = 27 + by + (-2 if pose in ("hop", "cheer") and frame == 1 else -1 if pose == "idle" and frame else 0)
    for yy in range(int((ty - 5) * s), int((ty + 4) * s) + 1):
        for xx in range(0, int(13 * s)):
            # a flat paddle, tilted up toward its tip (the left)
            u, v = xx + 0.5 - 6.0 * s, yy + 0.5 - (ty - 0.5) * s
            v += u * 0.28
            dx, dy = u / (6.2 * s), v / (3.1 * s)
            if dx * dx + dy * dy <= 1.0:
                hatch = ((xx + yy) % 3 == 0 or (xx - yy) % 3 == 0) and dx * dx + dy * dy < 0.6
                cv.set(xx, yy, tail[1] if hatch and s >= 1 else tail[0])
    # the body
    E(14, 22 + by, 7.6, 8.6 if pose != "sleep" else 7.0, fur)
    # the hind leg and foot
    E(12, 28 + min(by, 1), 4.2, 3.0, None, fur[2])
    if pose == "hop":
        E(13, 27, 3.4, 2.2, None, fur[3])
    else:
        for xx in range(int(9 * s), int(16 * s) + 1):
            cv.set(xx, S - 1, fur[3])
    # the belly
    E(17.5, 23 + by, 3.8, 6.0 if pose != "sleep" else 4.8, None, belly[0])
    E(18.5, 25 + by, 2.4, 3.6 if pose != "sleep" else 2.8, None, belly[1])
    # the arms
    if pose == "cheer":
        for k, (ex, ey) in enumerate([(25, 6 - 2 * frame), (9, 7 - 2 * frame)]):
            hs.capsule(cv, (19 - 7 * k) * s, (17 + by) * s, ex * s, ey * s, 1.9 * s, None, fur[1 + 2 * k])
            hs.ellipse(cv, ex * s, ey * s, 1.9 * s, 1.9 * s, None, fur[3 - k])
    elif pose == "sleep":
        hs.capsule(cv, 18 * s, 22 * s, 22 * s, 27 * s, 1.5 * s, None, fur[2])
    else:
        ax = {"bite": 25, "windup": 22, "recover": 24, "hop": 22, "bonk": 21}.get(pose, 23)
        ay = {"bite": 19, "hop": 16}.get(pose, 20)
        hs.capsule(cv, 18 * s, (18 + by) * s, ax * s, ay * s, 1.6 * s, None, fur[2])
        cv.set(int(ax * s + 1), int(ay * s), fur[3])
    # the head
    hcx, hcy = 18 + hx, 12 + hy + by
    E(hcx, hcy, 6.6, 5.6, fur)
    E(hcx - 3.5, hcy - 4.5, 1.9, 1.9, None, fur[2])                 # the ear
    E(hcx - 3.5, hcy - 4.3, 0.9, 0.9, None, fur[3])
    E(hcx + 5, hcy + 2, 3.6, 3.0, None, belly[0])                   # the muzzle
    E(hcx + 3.6, hcy + 3.2, 2.0, 1.4, None, belly[1])               # the cheek
    if s >= 1:
        cv.set(hcx + 6, hcy - 1, nose), cv.set(hcx + 7, hcy - 1, nose), cv.set(hcx + 7, hcy, nose)
        cv.set(hcx + 8, hcy, out)
    else:
        cv.set(int((hcx + 7) * s), int((hcy - 1) * s), nose)
    # the teeth: big, orange, two incisors
    if s >= 1:
        tx, tyy = hcx + 4, hcy + 4
        open_mouth = pose in ("cheer", "bonk", "bite")
        if open_mouth:
            for yy in range(tyy, tyy + 3):
                cv.set(tx - 1, yy, pink)
        for yy in range(tyy, tyy + 4):
            for xx in range(tx, tx + 4):
                c = teeth[0] if xx < tx + 2 else teeth[1]
                if xx == tx + 2:
                    c = teeth[1]
                cv.set(xx, yy, c)
        cv.set(tx, tyy, white)
        if pose == "bite":
            cv.set(tx + 1, tyy + 4, teeth[1]), cv.set(tx + 2, tyy + 4, teeth[1])
    else:
        tx, tyy = int((hcx + 4.5) * s), int((hcy + 4) * s)
        cv.set(tx, tyy, teeth[0]), cv.set(tx + 1, tyy, teeth[1]), cv.set(tx, tyy + 1, teeth[0])
    # the eye
    ex, ey = hcx + 1, hcy - 3
    if s < 1:
        cv.set(int(ex * s), int(ey * s) + 1, ink)
    elif pose == "bonk":
        for (dx, dy) in [(0, 0), (2, 0), (1, 1), (0, 2), (2, 2)]:
            cv.set(ex + dx, ey + dy, ink)
    elif pose == "sleep":
        cv.set(ex, ey + 2, ink), cv.set(ex + 1, ey + 2, ink), cv.set(ex + 2, ey + 2, ink)
    elif pose == "cheer":
        cv.set(ex, ey + 1, ink), cv.set(ex + 1, ey, ink), cv.set(ex + 2, ey + 1, ink)
    else:
        blink = pose == "idle" and frame == 1
        if blink:
            cv.set(ex, ey + 2, ink), cv.set(ex + 1, ey + 2, ink)
        else:
            hs.eye(cv, ex - 1, ey - 1, look=(1, 0), white=white, pupil=ink, big=True)
    if pose == "bonk":                                   # a bump on the head
        E(hcx - 1, hcy - 6, 1.8, 1.4, None, fur[0])
    cv.outline(out)
    if pose == "bite":
        cv = hs.squash(cv, 1.12, 0.9)
        _reoutline(cv, out)
    elif pose == "hop":
        cv = hs.squash(cv, 0.9, 1.08)
        _reoutline(cv, out)
    elif pose == "windup":
        cv = hs.squash(cv, 0.96, 1.04)
        _reoutline(cv, out)
    return cv


def _reoutline(cv, out):
    """after a squash: the outline pixels became interior ones; add the missing outside ones"""
    cv.outline(out)


def family(pose, frame=0):
    return beaver(pose, frame, 0.5)


# ---- the woodpecker ------------------------------------------------------------------------------------------------
def woodpecker(frame, side="L"):
    """16x16, clinging to the trunk: on the left side of the trunk it faces right. frame 1 = the peck."""
    cv = Canvas(16, 16)
    out, black, white, grey, red, redd, beak, beakd = W[1:9]
    hx = 9 if frame == 0 else 11
    # the tail, stiff against the bark
    for y in range(10, 15):
        cv.set(8, y, black), cv.set(9, y, grey if y > 12 else black)
    hs.ellipse(cv, 8, 9, 2.8, 4.2, None, black)
    for (x, y) in [(7, 8), (7, 10), (6, 9), (8, 11)]:
        cv.set(x, y, white)                                 # the barred wing
    hs.ellipse(cv, 10, 10, 1.4, 2.6, None, white)          # the chest
    hs.ellipse(cv, hx, 5, 2.4, 2.2, None, black)           # the head
    cv.set(hx - 1, 3, red), cv.set(hx, 3, red), cv.set(hx - 1, 4, redd), cv.set(hx + 1, 3, red)
    cv.set(hx + 1, 5, white), cv.set(hx, 6, white)
    cv.set(hx + 1, 4, W[1])                                # the eye
    for i in range(3):
        cv.set(hx + 2 + i, 5, beak if i < 2 else beakd)
    cv.set(hx + 2, 6, beakd)
    cv.set(10, 13, grey), cv.set(11, 13, grey)              # the feet on the bark
    cv.outline(out)
    return cv if side == "L" else hs.flip_h(cv)


# ---- sky and weather (OBJ palette 4 and 7) ---------------------------------------------------------------------------
def sun():
    cv = Canvas(32, 32)
    for y in range(32):
        for x in range(32):
            d = math.hypot(x + 0.5 - 16, y + 0.5 - 16)
            if d < 9.5:
                cv.set(x, y, X[6] if lit(x - 16, y - 16) > 2 else X[7] if d < 8.2 else X[8])
            elif d < 14 and int(math.degrees(math.atan2(y - 16, x - 16)) + 360) % 45 < 12 and d > 11:
                cv.set(x, y, X[7])
    return cv


def moon():
    cv = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            if math.hypot(x + 0.5 - 8, y + 0.5 - 8) < 6.5 and math.hypot(x + 0.5 - 11, y + 0.5 - 6) > 5.2:
                cv.set(x, y, X[9] if (x + y) < 13 else X[10])
    return cv


def star(frame):
    cv = Canvas(8, 8)
    cv.set(3, 3, X[12])
    if frame == 0:
        for (x, y) in [(2, 3), (4, 3), (3, 2), (3, 4)]:
            cv.set(x, y, X[12])
    return cv


def snowflake(frame):
    cv = Canvas(8, 8)
    pts = [(3, 3), (2, 3), (4, 3), (3, 2), (3, 4)] if frame == 0 else [(3, 3), (2, 2), (4, 4), (2, 4), (4, 2)]
    for (x, y) in pts:
        cv.set(x, y, X[2])
    return cv


def leaf(frame):
    cv = Canvas(8, 8)
    pts = [(2, 4), (3, 3), (4, 3), (5, 2), (3, 4), (4, 4)] if frame == 0 else [(3, 2), (3, 3), (4, 4), (4, 5), (3, 4)]
    for i, (x, y) in enumerate(pts):
        cv.set(x, y, P[2] if i < 2 else P[3])
    cv.set(*pts[-1], P[5])
    cv.outline(P[4])
    return cv


def firefly(frame):
    cv = Canvas(8, 8)
    cv.set(3, 3, X[11])
    if frame == 0:
        cv.set(4, 3, X[11]), cv.set(3, 4, X[6])
    return cv


def zzz(frame):
    cv = Canvas(8, 8)
    n = 4 if frame == 0 else 3
    for i in range(n):
        cv.set(1 + i, 1, X[13]), cv.set(1 + i, n, X[13]), cv.set(n - i, 1 + i, X[13])
    cv.outline(X[1])
    return cv


def dizzy(frame):
    cv = Canvas(8, 8)
    pts = [(3, 1), (3, 2), (1, 3), (2, 3), (3, 3), (4, 3), (5, 3), (2, 4), (4, 4), (1, 5), (5, 5)]
    for (x, y) in pts:
        cv.set(x, y, X[14] if frame == 0 else X[12])
    cv.outline(X[1])
    return cv


def splash(frame):
    """32x16: a crown of water and droplets, 4 frames"""
    cv = Canvas(32, 16)
    h = [5, 9, 7, 3][frame]
    spread = [5, 9, 12, 14][frame]
    for i in range(-spread, spread + 1):
        x = 16 + i
        top = 15 - int(h * (1 - (i / (spread + 1.0)) ** 2))
        for y in range(top, 16):
            cv.set(x, y, X[3] if y < top + 2 else X[4])
    rng = random.Random(frame)
    for _ in range(3 + frame * 2):
        x, y = 16 + rng.randint(-spread - 2, spread + 2), 14 - h - rng.randint(0, 3)
        if 0 <= y:
            cv.set(x, y, X[2])
    for i in range(-spread, spread + 1, 3):
        cv.set(16 + i, 15, X[2])
    cv.outline(X[5])
    return cv


def ripple(frame):
    """16x8: rings on the water where a log lands"""
    cv = Canvas(16, 8)
    r = [3, 5, 7][frame]
    for x in range(16):
        for y in range(8):
            d = math.hypot((x + 0.5 - 8) / r, (y + 0.5 - 4) / (r * 0.4))
            if abs(d - 1) < 0.18:
                cv.set(x, y, X[3] if frame < 2 else X[4])
    return cv


# ---- the scenery (BG3 and BG4 panoramas, 512 px wide; screen x = panorama x - 96 at rest) ------------------------------
PANO_W = 512
PANO_X = 96


def near_bank():
    """BG3 rows 24..29 (y 192..240): the near bank with the stump (its cut top shows its rings while the trunk
    drops). Palette 3."""
    # a 128-px strip (VRAM: it repeats), then the stump
    SW = 128
    strip = Canvas(SW, 48)
    rng = random.Random(3)
    edge = [4 + int(round(1.5 * math.sin(2 * math.pi * x / 64.0) + 0.8 * math.sin(2 * math.pi * x * 5 / SW)))
            for x in range(SW)]
    grass = G[2:6]
    for x in range(SW):
        for y in range(48):
            e = edge[x]
            if y < e:
                continue
            if y < e + 2:
                strip.set(x, y, grass[0] if y == e else grass[1])
            elif y < e + 9 + (x * 7 % 3):
                strip.set(x, y, grass[1] if (x + y) % 5 else grass[2])
            elif y < e + 11:
                strip.set(x, y, grass[3])
            else:
                c = G[6] if y < 30 else G[7] if y < 40 else G[8]
                if (x * 13 + y * 7) % 23 == 0:
                    c = G[7] if c == G[6] else G[8]
                strip.set(x, y, c)
    for _ in range(16):                              # tufts
        x = rng.randrange(0, SW)
        strip.set(x, edge[x] - 1, grass[1]), strip.set(x, edge[x] - 2, grass[0])
    for _ in range(4):                               # stones
        x, y = rng.randrange(4, SW - 8), rng.randrange(26, 44)
        hs.ellipse(strip, x, y, 3, 2, None, G[10])
        strip.set(x - 1, y - 1, G[9]), strip.set(x, y - 1, G[9])
    cv = Canvas(PANO_W, 48)
    for k in range(PANO_W // SW):
        cv.paste(strip, k * SW, 0)
    # the stump, under the trunk (screen x 136..184 = panorama 232..280), its top at y 200 (row 8 here)
    sx0, sx1, top = PANO_X + 136, PANO_X + 184, 8
    for x in range(sx0 - 10, sx1 + 10):
        for y in range(top, 24):
            w = (sx1 - sx0) / 2.0 + max(0, (y - top - 4)) * 1.4
            cx = (sx0 + sx1) / 2.0
            if abs(x + 0.5 - cx) <= w:
                f = (x + 0.5 - (cx - w)) / (2 * w)
                cv.set(x, y, G[11] if f < 0.3 else G[12] if f < 0.75 else G[13])
    # roots
    for (x0, dx) in ((sx0 - 2, -1), (sx1 + 1, 1)):
        for i in range(10):
            cv.set(x0 + dx * i, 18 + i // 3, G[13]), cv.set(x0 + dx * i, 19 + i // 3, G[1])
    # the cut top: an ellipse of rings, hidden by the trunk except while it drops
    cx, cy = (sx0 + sx1) / 2.0, top + 1.0
    for y in range(top - 4, top + 4):
        for x in range(sx0 - 1, sx1 + 1):
            d = math.hypot((x + 0.5 - cx) / 24.5, (y + 0.5 - cy) / 3.6)
            if d <= 1.0:
                ring = int(d * 7)
                cv.set(x, y, G[15] if ring % 2 else G[14])
    # grass tufts in front of the stump's foot
    for x in (sx0 - 6, sx0 + 3, sx0 + 17, sx1 - 9, sx1 + 4):
        for (dx, dy) in ((0, 0), (1, 0), (0, -1), (1, 1), (-1, 1), (2, 1)):
            cv.set(x + dx, 21 + dy, grass[1] if dy < 1 else grass[2])
    cv.outline(G[1])
    return cv


def dam_base():
    """the dam canvas (240x32, BG3 unique tiles, palette 4): empty at the start; C draws each log into it"""
    return Canvas(240, 32)


def dam_logs():
    """the small logs of the dam (8x3), drawn by C into the dam canvas: wood, mud-plastered, grown over (moss);
    and the crown of a completed section (40x2)"""
    out = {}
    for k, (l, m, d, e, r) in enumerate([(D[2], D[3], D[4], D[5], D[6]), (D[7], D[8], D[9], D[8], D[9]),
                                         (D[10], D[11], D[9], D[10], D[11])]):
        full = [[l] * 7 + [e], [m] * 7 + [r], [d] * 7 + [e]]
        full[0][0] = m
        full[2][0] = D[1]
        out[k] = full
    crown = [[D[10] if (x * 3) % 5 else D[13] for x in range(40)], [D[11]] * 40]
    return out, crown


def banks():
    """BG3 above the near bank: the far banks at both ends of the dam, framing trees (palette 5)."""
    cv = Canvas(PANO_W, 192)
    foliage = K[4:8]
    rng = random.Random(11)
    # the banks: earth slopes from the forest foot down to the river, left and right of the dam
    dam_l, dam_r = PANO_X + 40, PANO_X + 280
    for x in range(PANO_W):
        for y in range(100, 192):
            if x < dam_l:
                top = 116 + max(0, (x - (dam_l - 60))) * 0.5
            elif x >= dam_r:
                top = 116 + max(0, ((dam_r + 60) - x)) * 0.5
            else:
                continue
            if y < top:
                continue
            if y < top + 3:
                c = K[11] if y < top + 2 else K[12]
            else:
                c = K[8] if (y - top) < 18 else K[9] if (y - top) < 40 else K[10]
                if (x * 11 + y * 5) % 29 == 0:
                    c = K[13]
            cv.set(x, y, c)
    for _ in range(10):
        side = rng.random() < 0.5
        x = rng.randrange(dam_l - 50, dam_l - 4) if side else rng.randrange(dam_r + 4, dam_r + 50)
        y = rng.randrange(150, 186)
        hs.ellipse(cv, x, y, 3.5, 2.5, [K[13], K[13], K[14], K[14]])

    def tree(cx, base, height, pine):
        # the trunk (a broadleaf's is thicker and forks under its crown)
        tw = 2 if pine else 3
        for y in range(base - (height // 2 if pine else height * 3 // 5), base):
            for x in range(cx - tw, cx + tw):
                cv.set(x, y, K[2] if x < cx - (0 if pine else 1) else K[3])
        if not pine:
            for i in range(10):
                cv.set(cx - 3 - i // 2, base - height * 3 // 5 - i, K[3]), cv.set(cx + 2 + i // 2, base - height * 3 // 5 - i, K[3])
        if pine:
            for i in range(5):
                ty = base - height + i * height // 7
                w = 5 + i * 3
                for y in range(ty, ty + height // 5):
                    ww = w * (y - ty + 3) / (height / 5.0 + 3)
                    for x in range(int(cx - ww), int(cx + ww) + 1):
                        nd = lit((x - cx) / (ww + 1), (y - ty) / (height / 5.0) - 0.5)
                        cv.set(x, y, shade4(nd, foliage))
        else:
            for (dx, dy, rx, ry) in [(0, -height + 16, 15, 13), (-11, -height + 28, 12, 10), (11, -height + 26, 13, 11),
                                     (-4, -height + 40, 13, 10), (7, -height + 46, 11, 9)]:
                hs.ellipse(cv, cx + dx, base + dy, rx, ry, foliage)
        # leaf dots
        for _ in range(height * 2):
            x, y = cx + rng.randint(-16, 16), base - rng.randint(height // 3, height)
            c = cv.get(x, y)
            if c in foliage:
                i = foliage.index(c)
                cv.set(x, y, foliage[max(0, i - 1)])

    tree(PANO_X + 14, 150, 118, True)
    tree(PANO_X - 20, 140, 96, True)
    tree(PANO_X + 302, 150, 104, False)
    tree(PANO_X + 336, 146, 90, True)
    cv.outline(K[1])
    for y in range(128, 160):                       # the dam canvas (C draws it) is its own
        for x in range(dam_l, dam_r):
            cv.set(x, y, None)
    for y in range(cv.h):                           # never shown (the lean is 6 px at most): no tiles
        for x in list(range(0, PANO_X - 8)) + list(range(PANO_X + 328, PANO_W)):
            cv.set(x, y, None)
    return cv


def far(period=256):
    """BG4: far mountains with snow caps, the far forest (the pond covers its foot) and, lower, the river's
    ripples (a separate band, scrolled to flow). Palette 6; the clouds, palette 7, are separate tiles. Drawn
    over `period` px and repeated (VRAM)."""
    P = period
    cv = Canvas(P, 192)
    peaks = [(30, 58, 64), (112, 48, 84), (196, 64, 62)]
    allp = peaks + [(p[0] + P, p[1], p[2]) for p in peaks] + [(p[0] - P, p[1], p[2]) for p in peaks]
    for x in range(P):
        h = 128
        for (px, py, w) in allp:
            h = min(h, py + abs(x - px) * 60.0 / w)
        for y in range(int(h), 124):
            snow = y < h + 7 and h < 72
            left = any(abs(x - p[0]) < p[2] and x < p[0] for p in allp)
            c = (F[4] if left else F[5]) if snow else (F[1] if left and y < h + 20 else F[2] if y < 104 else F[3])
            cv.set(x, y, c)
    # the far forest: a row of small conifers
    rng = random.Random(5)
    x = 0
    while x < P - 4:
        hgt = rng.randint(12, 22)
        base = 128
        for y in range(base - hgt, base):
            w = (y - (base - hgt)) * 0.36
            for xx in range(int(x - w), int(x + w) + 1):
                c = F[6] if xx < x else F[7]
                if y > base - 5:
                    c = F[8]
                cv.set(xx % P, y, c)
        x += rng.randint(5, 9)
    for y in range(116, 128):
        for x in range(P):
            if cv.get(x, y) is None:
                cv.set(x, y, F[8])
    # ripples on the river (y 160..192): short dashes
    for _ in range(45):
        x, y = rng.randrange(0, P), rng.randrange(162, 191)
        n = rng.randint(2, 5)
        for i in range(n):
            cv.set((x + i) % P, y, F[9] if i < n - 1 else F[10])
    out = Canvas(PANO_W, 192)
    for k in range(PANO_W // P):
        out.paste(cv, k * P, 0)
    return out


def clouds(period=256):
    cv = Canvas(period, 64)
    rng = random.Random(9)
    for (cx, cy, n) in [(40, 22, 4), (170, 36, 3)]:
        for i in range(n):
            bx, by = cx + i * 10 - n * 5, cy + rng.randint(-3, 2)
            rx, ry = rng.randint(8, 12), rng.randint(5, 7)
            for y in range(by - ry, by + ry + 1):
                for x in range(bx - rx, bx + rx + 1):
                    dx, dy = (x + 0.5 - bx) / rx, (y + 0.5 - by) / ry
                    if dx * dx + dy * dy <= 1 and y <= cy + 5:
                        nd = lit(dx, dy)
                        cv.set(x % period, y, PAL_CLOUDS[1] if nd > 0.5 else PAL_CLOUDS[2] if nd > -0.1 else
                               PAL_CLOUDS[3] if y < cy + 4 else PAL_CLOUDS[4])
    out = Canvas(PANO_W, 64)
    for k in range(PANO_W // period):
        out.paste(cv, k * period, 0)
    return out

# ---- the medals: acorns in the four house tiers (bronze, silver, gold, pearl), on the kit's sprite palette ----------------
KIT = [None, hs.UI["out"], hs.UI["white"], hs.UI["shade"], hs.UI["bd"], hs.UI["bl"], hs.UI["sd"], hs.UI["sl"],
       hs.UI["gd"], hs.UI["gl"], hs.UI["pd"], hs.UI["pl"], hs.UI["pearl"], hs.SPARK["spark"], hs.SPARK["white"]]


def acorn(tier):
    """a 24x24 acorn medal: the nut in the tier's light shade with a glint, the scaly cap in its dark shade"""
    dark, light = [(hs.UI["bd"], hs.UI["bl"]), (hs.UI["sd"], hs.UI["sl"]), (hs.UI["gd"], hs.UI["gl"]),
                   (hs.UI["pd"], hs.UI["pl"])][tier]
    nut = hs.UI["pearl"] if tier == 3 else light
    cv = Canvas(24, 24)
    for y in range(24):
        for x in range(24):
            dx, dy = (x + 0.5 - 12) / 6.6, (y + 0.5 - 14.5) / 7.2
            if dx * dx + dy * dy <= 1.0 and y >= 9:
                cv.set(x, y, nut if lit(dx, dy) > -0.35 else light if tier == 3 else dark)
    cv.set(12, 22, dark), cv.set(11, 22, dark)
    for y in range(3, 12):                          # the cap: a scaly dome
        for x in range(24):
            dx, dy = (x + 0.5 - 12) / 8.6, (y + 0.5 - 10.5) / 6.5
            if dx * dx + dy * dy <= 1.0:
                scale = (x + (y % 2) * 2) % 4 == 0 or y == 11
                cv.set(x, y, light if scale and y < 11 else dark)
    for y in range(0, 4):                           # the stem
        cv.set(12 + (1 if y == 0 else 0), y, dark)
    cv.set(9, 13, hs.UI["white"]), cv.set(9, 14, hs.UI["white"]), cv.set(10, 13, hs.UI["white"])
    cv.outline(hs.UI["out"])
    return cv


# ---- everything, as build_assets.py wants it -----------------------------------------------------------------------
def sprites():
    """(name, [frames]) in OBJ order, each with its palette number"""
    beav = [beaver("idle", 0), beaver("idle", 1), beaver("windup"), beaver("bite"), beaver("recover"), beaver("hop"),
            beaver("bonk"), beaver("sleep"), beaver("cheer", 0), beaver("cheer", 1)]
    return [
        ("beaver", 0, beav),
        ("log", 2, log_frames()),
        ("chip", 2, [chip(k) for k in range(3)]),
        ("floater", 2, [floater(False), floater(True)]),
        ("branch_piece", 2, [branch_piece(r) for r in range(2)]),     # 2, 3: 0, 1 turned over
        ("family", 0, [family("idle", 0), family("idle", 1), family("cheer", 0), family("cheer", 1)]),
        ("woodpecker", 6, [woodpecker(0), woodpecker(1)]),                  # on the right: flipped
        ("sun", 4, [sun()]),
        ("moon", 4, [moon()]),
        ("star", 4, [star(0), star(1)]),
        ("snow", 4, [snowflake(0), snowflake(1)]),
        ("firefly", 4, [firefly(0), firefly(1)]),
        ("zzz", 4, [zzz(0), zzz(1)]),
        ("dizzy", 4, [dizzy(0), dizzy(1)]),
        ("splash", 4, [splash(f) for f in range(4)]),
        ("ripple", 4, [ripple(f) for f in range(3)]),
        ("leaf", 7, [leaf(0), leaf(1)]),
        ("acorn", 3, [acorn(k) for k in range(4)]),
    ]


def trunk_parts():
    return dict(segments=[segment(k) for k in range(4)], branch_l=branch("L"), branch_r=branch("R"),
                stolen_l=branch("L", True), stolen_r=branch("R", True))


# ---- review sheets -------------------------------------------------------------------------------------------------
def sheet(cvs, cols=8, pad=2):
    w = max(c.w for c in cvs) + pad
    h = max(c.h for c in cvs) + pad
    out = Canvas(cols * w, ((len(cvs) + cols - 1) // cols) * h)
    for i, c in enumerate(cvs):
        out.paste(c, (i % cols) * w, (i // cols) * h)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(ROOT, "build", "beaverrush-art"))
    ap.add_argument("--scale", type=int, default=3)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    from PIL import Image
    tp = trunk_parts()
    trunk = Canvas(48 + 80, 24 * 6)
    for i, s in enumerate(tp["segments"]):
        trunk.paste(s, 40, i * 24)
    trunk.paste(tp["branch_l"], 0, 0), trunk.paste(tp["branch_r"], 88, 24)
    trunk.paste(tp["stolen_l"], 0, 72), trunk.paste(tp["stolen_r"], 88, 96)
    out = [("trunk", trunk), ("near-bank", near_bank()), ("banks", banks()), ("far", far()), ("clouds", clouds())]
    for name, pal, frames in sprites():
        out.append(("spr-" + name, sheet(frames)))
    for name, cv in out:
        im = cv.image()
        im.resize((im.width * a.scale, im.height * a.scale), Image.NEAREST).save(os.path.join(a.out, name + ".png"))
    print("wrote %d sheets to %s" % (len(out), a.out))


if __name__ == "__main__":
    main()

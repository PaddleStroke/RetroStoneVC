#!/usr/bin/env python3
"""Unicorn Hop: the code-drawn art, in the 8BCraft house style (docs/art-direction.md).

Code-drawn art only, no image generation: every sprite and panorama is drawn here with
games/common/tools/house_style.py (1-px outlines, 3-4-shade ramps lit from the top-left,
squash and stretch). The sheet layout (SPRITES) is shared with build_assets.py.

Writes, in the art folder (default games/unicornhop/art):
  sprites.png (256x128): the unicorn (white body, a pastel rainbow mane and tail, a golden horn and hooves; players
      2-4 recolour the body: lilac, pink, mint), its icon, the stars, the moon, the sparkles (hero palette, OBJ 0);
      the cloud pieces, puffs, rain drops, the thunder cloud and its bolt (cloud palette, OBJ 2; the pink and the
      grey rain clouds are the same tiles recoloured, OBJ 6 and 7)
  tiles/near.png (512x256)    BG2: soft cloud banks, no outline (recoloured with the sky by the game)
  tiles/far.png (256x256)     BG3: thin wisps and the stars (lit by the game at night)
  tiles/scene.png (512x128)   BG4: the start, far below: hills and the castle
  tiles/rainbow.png (320x176) BG4 later: the rainbow arc of the celebrations (translucent in the game)

    python3 games/unicornhop/tools/make_art.py [--out DIR] [--preview]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/unicornhop/LICENSE.
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
SHEET_W, SHEET_H = 256, 128
# name, x, y, w, h, frames, palette group (hero: OBJ 0, players 2-4 = OBJ 1, 4, 5 recoloured; clouds: OBJ 2, 6, 7)
SPRITES = [
    ("uni_idle", 0, 0, 32, 32, 2, "hero"),
    ("uni_squash", 64, 0, 32, 32, 1, "hero"),
    ("uni_stretch", 96, 0, 32, 32, 1, "hero"),
    ("uni_fall", 128, 0, 32, 32, 1, "hero"),
    ("uni_hurt", 160, 0, 32, 32, 2, "hero"),       # the tumble: upright and turned a quarter (flips do the rest)
    ("uni_icon", 0, 32, 16, 16, 1, "hero"),        # the title's player slots, the results
    ("star", 16, 32, 16, 16, 3, "hero"),           # a spinning star: face on, turning, edge on
    ("moon", 64, 32, 32, 32, 1, "hero"),
    ("sparkle", 96, 32, 8, 8, 6, "hero"),          # pink, mint, sky: big then small
    ("tear", 144, 32, 8, 8, 2, "hero"),            # the sad sparkle of a fall
    ("cloud_l", 0, 64, 16, 24, 1, "clouds"),
    ("cloud_m", 16, 64, 16, 24, 3, "clouds"),
    ("cloud_r", 64, 64, 16, 24, 1, "clouds"),
    ("puff", 80, 64, 16, 16, 3, "clouds"),
    ("drop", 128, 64, 8, 8, 1, "clouds"),
    ("thunder", 144, 64, 32, 24, 2, "clouds"),
    ("bolt", 208, 64, 16, 32, 1, "clouds"),
]
PANORAMAS = {"near": ("tiles/near.png", 512, 256), "far": ("tiles/far.png", 256, 256),
             "scene": ("tiles/scene.png", 512, 128), "rainbow": ("tiles/rainbow.png", 320, 176)}

# ---- colours ----------------------------------------------------------------------------------------------------------
# the unicorn's body, light -> dark (white with lavender shading); players 2-4 swap this ramp (build_assets.py)
BODY = [(255, 255, 255), (236, 232, 250), (206, 200, 236), (160, 150, 206)]
BODY_P = {1: [(244, 226, 255), (220, 194, 252), (188, 156, 236), (140, 106, 200)],     # P2 lilac
          2: [(255, 234, 246), (255, 202, 230), (244, 158, 204), (200, 102, 160)],     # P3 pink
          3: [(228, 255, 244), (182, 242, 214), (126, 212, 180), (74, 162, 134)]}      # P4 mint
OUT = (54, 36, 90)                  # the unicorn's outline: violet-navy
MANE = [(255, 146, 194), (255, 210, 104), (118, 224, 168), (106, 186, 255), (176, 128, 240)]
HORN = [(255, 238, 150), (234, 180, 56)]
INK = hs.HOUSE["ink"][0]
EYE = (246, 248, 252)              # the eye white: not a body colour (in RGB555 too), so the swaps keep it white
# the clouds: white with lilac shading and a violet outline; the pink and grey ones are swaps (build_assets.py)
CW = [(255, 255, 255), (238, 236, 252), (214, 210, 244), (178, 170, 226)]
CO = (88, 76, 150)
CLOUD_SWAPS = {"pink": ([(255, 242, 250), (255, 216, 236), (250, 182, 218), (226, 138, 188)], (156, 66, 124)),
               "grey": ([(226, 230, 240), (196, 202, 218), (160, 168, 190), (124, 130, 154)], (60, 62, 92))}
DROP = [(176, 218, 255), (100, 160, 236)]
TH = [(150, 144, 186), (114, 106, 150), (82, 74, 118)]
TH_OUT = (36, 28, 60)
BOLT = [(255, 250, 200), (255, 222, 80)]


def ring_dist(x, y, cx, cy):
    return math.hypot(x + 0.5 - cx, y + 0.5 - cy)


def disc(cv, cx, cy, r, col):
    for y in range(int(cy - r) - 1, int(cy + r) + 2):
        for x in range(int(cx - r) - 1, int(cx + r) + 2):
            if ring_dist(x, y, cx, cy) <= r:
                cv.set(x, y, col)


def strand(cv, pts, r, col):
    """A hair strand: discs along a polyline."""
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        n = int(max(abs(bx - ax), abs(by - ay)) * 2) + 1
        for i in range(n + 1):
            t = i / float(n)
            rr = r[0] + (r[1] - r[0]) * t if isinstance(r, tuple) else r
            disc(cv, ax + (bx - ax) * t, ay + (by - ay) * t, rr, col)


# ---- the unicorn: drawn facing right at rest, then squashed / stretched -------------------------------------------
def unicorn(pose="idle", frame=0):
    cv = Canvas(32, 32)
    flut = frame if pose == "idle" else 0
    up = pose in ("fall",)                      # the mane flies up while coming down
    # the tail: five rainbow strands from the rump, curling down
    for i, col in enumerate(reversed(MANE)):
        k = 4 - i
        if up:
            pts = [(8.5, 17 + k * 0.5), (5, 13 + k * 0.9), (2.5 + k * 0.3, 10 + k * 1.2)]
        else:
            pts = [(8.5, 16.5 + k * 0.6), (5, 18 + k * 0.9), (3 + k * 0.4 - flut * 0.5, 22 + k * 1.1),
                   (4.5 + k * 0.2, 25 + k * 0.8 + flut * 0.5)]
        strand(cv, pts, (1.6, 1.0), col)
    # the legs: the far pair a shade darker; longer when falling (reaching for the cloud), tucked when stretched
    leg = {"stretch": 3, "fall": 7, "hurt": 5}.get(pose, 6)
    for lx, far in ((10, 1), (14, 0), (19, 1), (23, 0)):
        c = BODY[2] if far else BODY[1]
        top = 22
        for y in range(top, top + leg):
            cv.set(lx, y, c), cv.set(lx + 1, y, c)
        cv.set(lx, top + leg, HORN[1]), cv.set(lx + 1, top + leg, HORN[1])
        if pose != "stretch":
            cv.set(lx, top + leg - 1, HORN[0] if not far else HORN[1])
    # the body
    hs.ellipse(cv, 16.0, 19.5, 8.6, 5.6, BODY)
    # the neck and the head
    hs.capsule(cv, 21.5, 18.0, 24.0, 11.5, 3.3, BODY)
    hs.ellipse(cv, 25.5, 9.5, 4.6, 4.1, BODY)
    hs.ellipse(cv, 28.4, 11.6, 2.8, 2.3, BODY)
    # the ear
    for x, y in ((22, 4), (23, 4), (22, 5), (23, 5), (23, 3), (24, 5)):
        cv.set(x, y, BODY[1])
    cv.set(23, 4, MANE[0])
    # the mane: five strands down the back of the neck, the forelock over the brow
    for i in range(len(MANE) - 1, -1, -1):          # back to front: each band shows its edge
        col = MANE[i]
        if up:
            pts = [(22.5 - i * 0.5, 4.5 + i * 0.4), (19.5 - i * 1.0, 3.5 + i * 1.6), (16 - i * 0.9, 5 + i * 2.0)]
        else:
            pts = [(22.5 - i * 0.5, 4.5 + i * 0.4), (20 - i * 0.9, 8 + i * 0.7),
                   (18.5 - i * 0.9, 12.5 + i * 0.6 + flut * 0.4), (17.5 - i * 1.0 - flut * 0.3, 16 + i * 0.4)]
        strand(cv, pts, (1.3, 0.9), col)
    cv.set(24, 5, MANE[0]), cv.set(25, 5, MANE[4]), cv.set(24, 6, MANE[1])
    # the horn: golden, striped, pointing up and forward
    horn = [(26, 5), (27, 5), (26, 4), (27, 4), (27, 3), (28, 3), (28, 2), (28, 1), (29, 0)]
    for i, (x, y) in enumerate(horn):
        cv.set(x, y, HORN[0] if (x + y) % 3 else HORN[1])
    # the face: the eye (X eyes when hurt), a blush, a smile
    if pose == "hurt":
        for d in range(3):
            cv.set(25 + d, 7 + d, INK), cv.set(27 - d, 7 + d, INK)
    else:
        hs.eye(cv, 25, 8, look=(1, 0), white=EYE)
        cv.set(25, 7, OUT)                       # a lash
    cv.set(27, 12, MANE[0])
    cv.set(30, 12, OUT)
    if pose == "squash":
        cv = hs.squash(cv, 1.2)
    elif pose == "stretch":
        cv = hs.squash(cv, 0.86)
    cv.outline(OUT)
    return cv


def rot90(cv):
    """A quarter turn clockwise (the tumble)."""
    out = Canvas(cv.h, cv.w)
    for y in range(cv.h):
        for x in range(cv.w):
            out.p[x][cv.h - 1 - y] = cv.p[y][x]
    return out


def unicorn_icon():
    """The 16x16 face of the title's player slots and the results (the same colours: the swaps apply)."""
    cv = Canvas(16, 16)
    for i, col in enumerate(MANE):
        strand(cv, [(6.5 - i * 0.6, 3.5 + i * 0.5), (4.5 - i * 0.5, 8 + i * 0.6), (4 - i * 0.4, 12 + i * 0.3)],
               (1.4, 1.0), col)
    hs.ellipse(cv, 8.5, 9.0, 4.8, 4.4, BODY)
    hs.ellipse(cv, 11.8, 11.0, 2.8, 2.3, BODY)
    for x, y in ((6, 4), (7, 4), (6, 3)):
        cv.set(x, y, BODY[1])
    for x, y in ((9, 4), (10, 4), (10, 3), (11, 2), (11, 1), (12, 0)):
        cv.set(x, y, HORN[0] if (x + y) % 3 else HORN[1])
    hs.eye(cv, 9, 7, look=(1, 0), white=EYE)
    cv.set(11, 11, MANE[0])
    cv.outline(OUT)
    return cv


def star(frame):
    """A smiling gold star, turning: face on, then 78% and 55% wide."""
    sx = [1.0, 0.78, 0.55][frame]
    cv = Canvas(16, 16)
    pts = []
    for i in range(10):
        a = -math.pi / 2 + i * math.pi / 5
        r = 7.2 if i % 2 == 0 else 3.2
        pts.append((8 + r * math.cos(a) * sx, 8.5 + r * math.sin(a)))
    for y in range(16):
        for x in range(16):
            px, py = x + 0.5, y + 0.5
            inside = False
            for (ax, ay), (bx, by) in zip(pts, pts[1:] + pts[:1]):
                if (ay > py) != (by > py) and px < (bx - ax) * (py - ay) / (by - ay) + ax:
                    inside = not inside
            if inside:
                nd = ((px - 8) * -0.62 + (py - 8.5) * -0.78) / 6.0
                cv.set(x, y, HORN[0] if nd > -0.1 else HORN[1])
    if frame == 0:
        cv.set(6, 6, EYE), cv.set(7, 6, EYE)
        cv.set(6, 8, INK), cv.set(9, 8, INK), cv.set(7, 10, INK), cv.set(8, 10, INK)
    elif frame == 1:
        cv.set(7, 6, EYE)
        cv.set(7, 8, INK), cv.set(9, 8, INK)
    cv.outline(OUT)
    return cv


def moon():
    """A sleepy crescent moon (the night)."""
    cv = Canvas(32, 32)
    for y in range(32):
        for x in range(32):
            d1 = ring_dist(x, y, 15.5, 16.5)
            d2 = ring_dist(x, y, 21.0, 12.5)
            if d1 <= 12.5 and d2 > 10.5:
                nd = ((x + 0.5 - 15.5) * -0.62 + (y + 0.5 - 16.5) * -0.78) / 12.5
                cv.set(x, y, EYE if nd > 0.35 else BODY[1] if nd > -0.25 else BODY[2])
    for x, y in ((10, 17), (11, 18), (12, 18), (13, 17)):        # a closed, sleepy eye
        cv.set(x, y, OUT)
    cv.set(12, 21, MANE[0]), cv.set(13, 21, MANE[0])
    cv.outline(OUT)
    return cv


def sparkle(frame):
    col = [MANE[0], MANE[2], MANE[3]][frame // 2]
    small = frame % 2
    cv = Canvas(8, 8)
    arm = 2 if small else 3
    for d in range(1, arm + 1):
        for x, y in ((3 + d, 3), (3 - d, 3), (3, 3 + d), (3, 3 - d)):
            cv.set(x, y, col)
    cv.set(3, 3, EYE)
    if not small:
        cv.set(4, 4, col), cv.set(2, 2, col), cv.set(4, 2, col), cv.set(2, 4, col)
    return cv


def tear(frame):
    cv = Canvas(8, 8)
    r = 2.4 if frame == 0 else 1.6
    disc(cv, 3.5, 5.0 - frame, r, MANE[3])
    for y in range(1 + frame, 4):
        cv.set(3, y, MANE[3])
    cv.set(3 if frame else 2, 4, EYE)
    cv.outline(OUT)
    return cv


def cloud_strip():
    """The cloud pieces drawn as one strip (L, M0, M1, M2, R), outlined, then cut: every middle piece has the same
    flat body at its edges, so any order of pieces joins without a seam. The bumps stay 2 px inside each piece."""
    W = 80
    cv = Canvas(W, 24)
    fill = set()
    for y in range(7, 20):
        for x in range(8, W - 8):
            fill.add((x, y))
    bumps = [(9.5, 13.0, 7.0), (W - 9.5, 13.0, 7.0), (10.0, 8.0, 4.5), (W - 10.0, 8.0, 4.5),     # the caps
             (24.0, 7.5, 5.8),                                       # M0: one round bump
             (36.5, 8.0, 4.2), (43.5, 7.0, 4.0),                     # M1: two small ones
             (56.5, 7.0, 4.8), (52.0, 9.0, 3.0)]                     # M2: one bump, a little one beside it
    for cx, cy, r in bumps:
        for y in range(24):
            for x in range(W):
                if ring_dist(x, y, cx, cy) <= r:
                    fill.add((x, y))
    for x0 in range(0, W, 16):                                       # a soft scallop under each piece
        for x in range(x0 + 4, x0 + 12):
            if 8 <= x < W - 8:
                fill.add((x, 20))
    top = {}
    for x, y in fill:
        top[x] = min(top.get(x, 99), y)
    for x, y in fill:
        d = y - top[x]
        if d < 2 or (y < 10 and (x + y) % 7 == 0 and d < 4):
            c = CW[0]
        elif y < 14:
            c = CW[1]
        elif y < 18:
            c = CW[2]
        else:
            c = CW[3]
        cv.set(x, y, c)
    for cx, cy, r in bumps[2:]:                                      # each bump: lit top-left, a little shadow right
        for y in range(24):
            for x in range(W):
                dd = ring_dist(x, y, cx, cy)
                if dd <= r and y < 11:
                    nd = ((x + 0.5 - cx) * -0.62 + (y + 0.5 - cy) * -0.78) / r
                    cv.set(x, y, CW[0] if nd > -0.2 else CW[1])
    cv.outline(CO)
    return cv


def puff(frame):
    """A cloud puff: grows and fades (the house checker dither, never inside a body)."""
    cv = Canvas(16, 16)
    r = [3.5, 5.5, 7.0][frame]
    for y in range(16):
        for x in range(16):
            d = min(ring_dist(x, y, 6, 9), ring_dist(x, y, 10, 8))
            if d < r and (frame < 2 or hs.checker(x, y, 0)):
                cv.set(x, y, CW[0] if d < r - 2 else CW[1])
    if frame < 2:
        cv.outline(CO)
    return cv


def drop():
    cv = Canvas(8, 8)
    disc(cv, 3.5, 4.8, 1.8, DROP[1])
    cv.set(3, 2, DROP[1]), cv.set(3, 3, DROP[1])
    cv.set(3, 4, DROP[0])
    return cv


def thunder(frame):
    """A small grumpy thunder cloud; frame 1 crackles (sparks and a bolt under it)."""
    cv = Canvas(32, 24)
    for cx, cy, r in ((9.0, 10.5, 5.8), (16.5, 7.5, 6.8), (24.0, 10.0, 5.6)):
        for y in range(24):
            for x in range(32):
                if ring_dist(x, y, cx, cy) <= r:
                    nd = ((x + 0.5 - cx) * -0.62 + (y + 0.5 - cy) * -0.78) / r
                    cv.set(x, y, TH[0] if nd > 0.3 else TH[1] if nd > -0.4 else TH[2])
    for y in range(9, 16):
        for x in range(5, 28):
            if cv.get(x, y) is None or y > 12:
                cv.set(x, y, TH[2] if y > 13 else TH[1])
    # the grumpy face: frowning brows, two eyes, a small frown
    for x, y in ((11, 8), (12, 9), (13, 9), (20, 9), (21, 9), (22, 8)):
        cv.set(x, y, TH_OUT)
    for ex in (12, 20):
        cv.set(ex, 10, EYE), cv.set(ex + 1, 10, EYE), cv.set(ex, 11, EYE), cv.set(ex + 1, 11, INK)
    for x, y in ((15, 13), (16, 12), (17, 12), (18, 13)):
        cv.set(x, y, TH_OUT)
    if frame == 1:
        for x, y in ((14, 17), (15, 18), (14, 19), (13, 20), (14, 21), (15, 22)):
            cv.set(x, y, BOLT[1])
        cv.set(15, 18, BOLT[0]), cv.set(14, 20, BOLT[0])
        for x, y in ((3, 6), (29, 7), (26, 18), (6, 18)):
            cv.set(x, y, BOLT[0])
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                cv.set(x + dx, y + dy, BOLT[1])
    cv.outline(TH_OUT)
    return cv


def bolt():
    """The zap: a zigzag bolt from the thunder cloud down to the unicorn."""
    cv = Canvas(16, 32)
    path = [(9, 0), (5, 9), (10, 11), (4, 21), (9, 22), (6, 31)]
    for (ax, ay), (bx, by) in zip(path, path[1:]):
        n = max(abs(bx - ax), abs(by - ay)) * 2
        for i in range(n + 1):
            t = i / float(n)
            x, y = ax + (bx - ax) * t, ay + (by - ay) * t
            disc(cv, x, y, 1.3, BOLT[1])
            cv.set(int(x), int(y), BOLT[0])
    cv.outline(TH_OUT)
    return cv


_STRIP = None


def sprite_frame(name, i):
    global _STRIP
    if name == "uni_icon":
        return unicorn_icon()
    if name == "uni_hurt":
        u = unicorn("hurt")
        return rot90(u) if i else u
    if name.startswith("uni_"):
        return unicorn(name[4:], i)
    if name == "star":
        return star(i)
    if name == "moon":
        return moon()
    if name == "sparkle":
        return sparkle(i)
    if name == "tear":
        return tear(i)
    if name.startswith("cloud_"):
        if _STRIP is None:
            _STRIP = cloud_strip()
        x0 = {"cloud_l": 0, "cloud_m": 16 + 16 * i, "cloud_r": 64}[name]
        cv = Canvas(16, 24)
        for y in range(24):
            for x in range(16):
                cv.p[y][x] = _STRIP.p[y][x0 + x]
        return cv
    if name == "puff":
        return puff(i)
    if name == "drop":
        return drop()
    if name == "thunder":
        return thunder(i)
    if name == "bolt":
        return bolt()
    raise KeyError(name)


# ---- panoramas (BG tiles): low contrast, no outline (the background never competes with the sprites) ---------------
# The game recolours the near and far layers with the sky (draw.c): these key colours are their morning colours.
NEAR = [(246, 246, 255), (230, 232, 250), (212, 216, 242)]            # light, mid, shadow
FAR = [(236, 240, 252), (222, 230, 248)]                              # the wisps
STARS = [(255, 255, 160), (160, 255, 255)]                            # two twinkle groups (keys: lit per line)
SCENE = {"hill_back": [(200, 210, 242), (186, 196, 236)],
         "hill": [(196, 230, 196), (176, 216, 182), (158, 202, 170)],
         "wall": [(246, 240, 252), (222, 214, 240), (198, 188, 226)],
         "roof": [(248, 176, 210), (224, 138, 186)],
         "window": [(156, 136, 206)], "flag": [(255, 206, 96)], "tree": [(150, 200, 160)]}
RAINBOW = [(255, 118, 140), (255, 172, 96), (255, 232, 104), (128, 222, 140), (104, 172, 252), (172, 128, 234)]


def blob(cv, cx, cy, w, h, ramp, rng):
    """A soft cloud bank: a row of overlapping discs, lit from the top, no outline."""
    discs = []
    n = max(3, int(w / 9))
    for i in range(n):
        t = (i + 0.5) / n
        x = cx - w / 2 + t * w
        r = h * (0.45 + 0.35 * math.sin(math.pi * t)) * rng.uniform(0.85, 1.1)
        discs.append((x, cy - r * 0.35, r))
    base = cy + h * 0.25
    for y in range(int(cy - h) - 2, int(base) + 2):
        for x in range(int(cx - w / 2 - h), int(cx + w / 2 + h)):
            inside = any(ring_dist(x, y, dx, dy, ) <= r for dx, dy, r in discs) and y <= base
            if not inside:
                continue
            top = min(dy - r for dx, dy, r in discs if abs(x + 0.5 - dx) < r) if any(
                abs(x + 0.5 - dx) < r for dx, dy, r in discs) else y
            d = y - top
            col = ramp[0] if d < h * 0.45 else ramp[1] if y < base - 2 else ramp[2]
            cv.set(x % cv.w, y % cv.h, col)


def near():
    W, H = PANORAMAS["near"][1:]
    cv = Canvas(W, H)
    rng = random.Random(11)
    banks = [(60, 40, 110, 18), (300, 70, 150, 22), (190, 150, 90, 16), (430, 190, 120, 20), (90, 220, 80, 14),
             (380, 110, 76, 13), (250, 228, 84, 14)]
    for cx, cy, w, h in banks:
        blob(cv, cx, cy, w, h, NEAR, rng)
    return cv


def far():
    W, H = PANORAMAS["far"][1:]
    cv = Canvas(W, H)
    rng = random.Random(5)
    for k in range(9):                                     # thin wisps: long, flat, two tones
        y0, x0, ln = rng.randint(0, H - 1), rng.randint(0, W - 1), rng.randint(40, 110)
        for i in range(ln):
            t = i / float(ln)
            th = 1 + int(2.2 * math.sin(math.pi * t))
            yy = y0 + int(2 * math.sin(i / 17.0 + k))
            for d in range(th):
                cv.set((x0 + i) % W, (yy + d) % H, FAR[0] if d == 0 else FAR[1])
    for k in range(70):                                    # the stars (lit at night)
        x, y = rng.randint(1, W - 2), rng.randint(1, H - 2)
        if cv.get(x, y) is not None:
            continue
        col = STARS[k % 2]
        cv.set(x, y, col)
        if k % 9 == 0:
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                cv.set(x + dx, y + dy, col)
    return cv


def scene():
    """The start, far below: lilac far hills, green near hills, a castle with pink roofs and a flag (left of the
    start cloud), round trees. 512 wide: a wide screen shows more hills."""
    W, H = PANORAMAS["scene"][1:]
    cv = Canvas(W, H)
    hb, hl = SCENE["hill_back"], SCENE["hill"]
    for x in range(W):
        h1 = 70 + int(10 * math.sin(x * 2 * math.pi / 256 + 1) + 5 * math.sin(x * 2 * math.pi / 64))
        h2 = 92 + int(9 * math.sin(x * 2 * math.pi / 512 * 3 + 2) + 4 * math.sin(x * 2 * math.pi / 128))
        for y in range(h1, H):
            cv.set(x, y, hb[0] if y - h1 < 2 else hb[1])
        for y in range(h2, H):
            cv.set(x, y, hl[0] if y - h2 < 2 else hl[1] if y < h2 + 14 else hl[2])
    rng = random.Random(4)
    for _ in range(14):                                     # round trees on the near hills
        tx = rng.randint(0, W - 1)
        if 0 < tx < 84:
            continue
        ty = 92 + int(9 * math.sin(tx * 2 * math.pi / 512 * 3 + 2) + 4 * math.sin(tx * 2 * math.pi / 128))
        disc(cv, tx, ty - 3, 4.2, SCENE["tree"][0])
        cv.set(tx, ty + 1, hl[2])
    # the castle: a keep and two towers with cone roofs, on the hill at x 36..100
    wall, roof = SCENE["wall"], SCENE["roof"]
    base = 98

    def tower(x0, w, top, cone):
        for y in range(top, base + 6):
            for x in range(x0, x0 + w):
                cv.set(x, y, wall[0] if x < x0 + 2 else wall[2] if x >= x0 + w - 2 else wall[1])
        for i in range(cone):
            half = (w / 2.0 + 1) * (i + 1) / cone
            cx = x0 + w / 2.0
            for x in range(int(cx - half), int(math.ceil(cx + half))):
                cv.set(x, top - cone + i, roof[0] if x < cx else roof[1])
        cv.set(int(x0 + w / 2), top - cone - 1, wall[2])
        for i in range(3):
            cv.set(int(x0 + w / 2) + 1 + i, top - cone - 2 + (i // 2), SCENE["flag"][0])
        for wy in range(top + 4, base, 9):
            cv.set(x0 + w // 2, wy, SCENE["window"][0]), cv.set(x0 + w // 2, wy + 1, SCENE["window"][0])

    for y in range(78, base + 6):                           # the keep, with battlements (x 24..64)
        for x in range(24, 64):
            cv.set(x, y, wall[1] if y > 80 else wall[0])
    for x in range(24, 64, 4):
        cv.rect(x, 75, x + 1, 77, wall[0])
    cv.rect(40, 88, 47, base + 5, SCENE["window"][0])       # the gate
    for x in range(41, 47):
        cv.set(x, 87, SCENE["window"][0])
    tower(14, 12, 66, 12)
    tower(60, 12, 70, 11)
    tower(37, 14, 52, 14)
    return cv


def rainbow():
    W, H = PANORAMAS["rainbow"][1:]
    cv = Canvas(W, H)
    cx, cy, r0 = W / 2.0, 172.0, 152.0
    band = 4
    for y in range(H):
        for x in range(W):
            d = ring_dist(x, y, cx, cy)
            k = int((r0 - d) // band)
            if 0 <= k < len(RAINBOW) and y < cy - 4:
                cv.set(x, y, RAINBOW[k])
    return cv


def build_sheet():
    im = Image.new("RGB", (SHEET_W, SHEET_H), hs.MAGENTA)
    for name, x, y, w, h, frames, _g in SPRITES:
        for i in range(frames):
            im.paste(sprite_frame(name, i).image(), (x + i * w, y))
    return im


def write_all(out, preview=False):
    os.makedirs(os.path.join(out, "tiles"), exist_ok=True)
    files = {"sprites.png": build_sheet()}
    for key, fn in (("near", near), ("far", far), ("scene", scene), ("rainbow", rainbow)):
        files[PANORAMAS[key][0]] = fn().image()
    for f, im in files.items():
        im.save(os.path.join(out, f))
        if preview:
            os.makedirs(os.path.join(out, "preview"), exist_ok=True)
            im.resize((im.width * 4, im.height * 4), Image.NEAREST).save(
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

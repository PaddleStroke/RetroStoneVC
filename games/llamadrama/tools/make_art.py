#!/usr/bin/env python3
"""Llama Drama: the code-drawn art, in the 8BCraft house style (docs/art-direction.md).

Code-drawn art only, no image generation: every sprite and panorama is drawn here with
games/common/tools/house_style.py (1-px outlines, 3-4-shade ramps lit from the top-left, the house eye).
The sheet layout (SPRITES) is shared with tools/build_assets.py; the head's geometry (HEAD_*) with src/game.h.

Writes, in the art folder (default games/llamadrama/art):
  sprites.png (256x256): the llama (head at 7 aim angles x 3 faces, the knocked-out head, the neck pieces, the body,
                         the icon), the hornets (small, dart, big; flying and bonked), the props (the spit, splats,
                         bonk stars, the aim dots, small digits) and the sky (the sun, the moon, two clouds)
  tiles/mountains.png (512x184): the Andes: a hazy far range, snowy peaks, the green puna slopes, stars in the sky
  tiles/meadow.png (512x56): the meadow the llamas stand on (grass, tufts, little flowers, pebbles)

    python3 games/llamadrama/tools/make_art.py [--out DIR] [--preview]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/llamadrama/LICENSE.
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
SHEET_W, SHEET_H = 256, 256
# the aim angles of the head frames (degrees, snout up > 0): game.h AIM_MIN .. AIM_MAX in HEAD_FRAMES steps
HEAD_ANGLES = [-35, -20, -5, 10, 25, 40, 55]
NECK_LEANS = [0, 14, 28]          # the neck pieces: degrees from upright, the top toward the hornets
# name, x, y, w, h, frames, palette group (llama: OBJ 0, players 2-4 = OBJ 1, 4, 5 recoloured; hornets: OBJ 2;
# props: OBJ 6; sky: OBJ 7, recoloured by the time of day)
SPRITES = [
    ("head", 0, 0, 32, 32, 7, "llama"),            # the faces, by aim angle ...
    ("head_puff", 0, 32, 32, 32, 7, "llama"),      # ... spitting: cheeks round, lips pursed ...
    ("head_scared", 0, 64, 32, 32, 7, "llama"),    # ... a hornet close: big eyes, a sweat drop
    ("head_hit", 224, 0, 32, 32, 1, "llama"),      # stung: X eyes, drooping
    ("neck", 224, 32, 16, 24, 2, "llama"),         # NECK_LEANS[0..1] (and [2] below)
    ("neck2", 224, 56, 16, 24, 1, "llama"),
    ("body", 0, 96, 64, 48, 1, "llama"),
    ("icon", 64, 96, 16, 16, 1, "llama"),          # the title's player slots, the results
    ("hornet", 0, 144, 16, 16, 2, "hornets"),      # small, wings flapping
    ("dart", 32, 144, 16, 16, 2, "hornets"),       # the fast one
    ("big", 64, 144, 32, 32, 2, "hornets"),        # the big one (3 hits)
    ("hornet_bonk", 128, 144, 16, 16, 1, "hornets"),
    ("dart_bonk", 144, 144, 16, 16, 1, "hornets"),
    ("big_bonk", 160, 144, 32, 32, 1, "hornets"),
    ("gob", 0, 176, 8, 8, 2, "props"),             # the spit
    ("splat", 16, 176, 8, 8, 3, "props"),
    ("star", 40, 176, 8, 8, 2, "props"),           # bonk stars
    ("dot", 56, 176, 8, 8, 1, "props"),            # the aim cue
    ("warn", 64, 176, 8, 8, 1, "props"),           # a dart about to dash
    ("digit", 72, 176, 8, 8, 10, "props"),         # small digits (points)
    ("sun", 0, 192, 32, 32, 1, "sky"),
    ("moon", 32, 192, 16, 16, 1, "sky"),
    ("cloud", 48, 192, 32, 16, 1, "sky"),
    ("cloud2", 80, 192, 48, 16, 1, "sky"),
]
PANORAMAS = {"mountains": ("tiles/mountains.png", 512, 184), "meadow": ("tiles/meadow.png", 512, 56)}
MOUNTAINS_Y, MEADOW_Y = 0, 184     # where they sit on the screen

# ---- the llama's colours (P1: a cream-white llama with red tassels and blanket) ----------------------------------------
FUR = [(255, 250, 238), (242, 228, 204), (214, 192, 160), (170, 142, 116)]
ACC = [(255, 156, 150), (230, 72, 96), (160, 34, 74)]
# players 2-4 (build_assets.py remaps FUR and ACC): a brown llama in blue, a grey one in green, a caramel one in purple
FUR_P = {1: [(232, 186, 136), (204, 150, 98), (164, 110, 66), (116, 74, 44)],
         2: [(214, 214, 224), (174, 174, 190), (132, 132, 150), (94, 94, 114)],
         3: [(255, 226, 160), (240, 194, 110), (206, 150, 72), (150, 102, 46)]}
ACC_P = {1: [(160, 210, 255), (64, 132, 232), (36, 68, 160)],
         2: [(176, 236, 126), (82, 178, 70), (34, 116, 52)],
         3: [(226, 176, 255), (160, 98, 230), (100, 48, 158)]}
PINK = (246, 184, 182)              # inner ears, the nose (never recoloured)
OUT = (62, 32, 48)                  # the llama's outline: its darkest shade, towards violet
WHITE = hs.HOUSE["white"][0]
INK = hs.HOUSE["ink"][0]
SWEAT = (150, 214, 255)
LIGHT = hs.LIGHT_DIR

# the head's geometry, in head units (u toward the snout, v up; the origin is the sprite's centre, 16, 16), shared
# with src/game.h: the mouth (HEAD_MOUTH_U, _V) and where the neck joins (HEAD_JOINT_U, _V)
HEAD_MOUTH = (13.0, -3.0)
HEAD_JOINT = (-5.0, -5.0)
# the neck's angle (degrees from the ground) at an aim: game.h NECK_ANG0 (205/1024 of a turn) + aim * NECK_K / 256
NECK_ANG0_DEG = 205 * 360.0 / 1024
NECK_K_PER_DEG = 80 / 256.0
NECK_W = 6.4                        # the neck's half-width (neck())
NECK_STUB = 7.0                     # the neck's top drawn behind the head, px from the joint


def neck_shade(du):
    """The neck's cylinder across (du -1 left .. 1 right; the light from the left)."""
    return FUR[0] if du < -0.55 else FUR[1] if du < 0.05 else FUR[2] if du < 0.62 else FUR[3]


def shade(nx, ny, r4):
    """One of 4 shades from a screen-space normal (nx, ny) by the house light (top-left)."""
    return hs.shade_nd(nx * LIGHT[0] + ny * LIGHT[1], r4)


# ---- the head: drawn in head units, rotated to the aim angle ------------------------------------------------------------
def head(angle, face="normal"):
    """The llama's head (32x32) aiming `angle` degrees (snout up > 0). Faces: normal, puff (spitting: the cheeks round,
    the lips pursed), scared (a hornet close: big round eyes, a sweat drop, the mouth an O), hit (X eyes)."""
    cv = Canvas(32, 32)
    a = math.radians(angle)
    c, s = math.cos(a), math.sin(a)

    def to_screen(u, v):
        return 16 + u * c - v * s, 16 - (u * s + v * c)

    def local(px, py):
        dx, dy = px + 0.5 - 16, -(py + 0.5 - 16)
        return dx * c + dy * s, -dx * s + dy * c

    def snorm(du, dv):                  # a local normal -> screen space
        return du * c - dv * s, -(du * s + dv * c)

    # the neck's top, behind the head: a stub of the neck (its cylinder shading, NECK_W wide) from the joint toward the
    # shoulder at this aim's neck angle (game.h NECK_ANG0 + aim * NECK_K / 256), so that the head sits on the neck
    # without a collar of outline across it at any aim
    n = math.radians(NECK_ANG0_DEG + angle * NECK_K_PER_DEG)
    jx, jy = to_screen(*HEAD_JOINT)
    ax, ay = -math.cos(n), math.sin(n)                   # along the neck, from the joint down to the shoulder
    wx, wy = math.sin(n), math.cos(n)                    # across it (to the right, as in neck())

    def stub(px, py):
        """(along, across) of pixel (px, py) from the joint."""
        rx, ry = px + 0.5 - jx, py + 0.5 - jy
        return rx * ax + ry * ay, rx * wx + ry * wy

    puff = face == "puff"
    # shapes, back to front: (kind, params, ramp)
    shapes = [
        ("ear", (-4.6, 5.0, -6.4, 12.4, 1.9), [FUR[2], FUR[2], FUR[3], FUR[3]]),      # the far ear
        ("ell", (-4.4, -5.0, 4.0, 4.6), FUR),                                           # where the neck joins
        ("ell", (-2.0, 1.6, 6.6, 6.0), FUR),                                            # the skull
        ("cap", (0.0, -1.2, 9.6, -2.0, 5.2 if puff else 4.5), FUR),                     # the long muzzle
        ("ear", (-1.6, 5.6, -1.2, 13.0, 2.0), FUR),                                    # the near ear
        ("cir", (-4.2, 6.4, 2.4), [FUR[0], FUR[0], FUR[1], FUR[2]]),                   # the wool fringe
        ("cir", (-1.8, 7.4, 2.2), [FUR[0], FUR[0], FUR[1], FUR[2]]),
        ("cir", (0.6, 6.6, 1.8), [FUR[0], FUR[0], FUR[1], FUR[2]]),
    ]
    for py in range(32):
        for px in range(32):
            u, v = local(px, py)
            col = None
            al, ac = stub(px, py)
            if 0.0 <= al <= NECK_STUB and abs(ac) <= NECK_W:
                col = neck_shade(ac / NECK_W)
            for kind, p, r4 in shapes:
                if kind == "ell":
                    du, dv = (u - p[0]) / p[2], (v - p[1]) / p[3]
                    if du * du + dv * dv <= 1.0:
                        col = shade(*snorm(du, dv), r4)
                elif kind == "cir":
                    du, dv = (u - p[0]) / p[2], (v - p[1]) / p[2]
                    if du * du + dv * dv <= 1.0:
                        col = shade(*snorm(du, dv), r4)
                else:
                    d, t = hs.seg_dist(u, v, p[0], p[1], p[2], p[3])
                    if d <= p[4]:
                        qx, qy = p[0] + (p[2] - p[0]) * t, p[1] + (p[3] - p[1]) * t
                        col = shade(*snorm((u - qx) / p[4], (v - qy) / p[4]), r4)
                        if kind == "ear" and r4 is FUR and abs(u - qx) < 0.8 and 0.2 < t < 0.85:
                            col = PINK                                                # the inner ear
            if col is not None:
                cv.set(px, py, col)

    def dot(u, v, colr):
        x, y = to_screen(u, v)
        cv.set(int(math.floor(x)), int(math.floor(y)), colr)

    # the tassel on the near ear (the player's colour), the nostril, the blush, the mouth
    for du in (-0.9, 0.0, 0.9):
        for dv in (-0.9, 0.0, 0.9):
            dot(-1.4 + du, 10.6 + dv, ACC[1] if du + dv > -0.5 else ACC[0])   # a pompom on the ear
    dot(-1.4, 9.4, ACC[2])
    dot(12.6, 0.6, OUT)
    if face == "puff":
        for du in (-1, 0, 1):
            for dv in (-1, 0):
                dot(6.0 + du, -2.4 + dv, ACC[0])                                         # cheeks round and pink
        for du, dv in ((13.6, -1.6), (14.4, -2.4), (13.6, -3.2), (12.8, -2.4)):
            dot(du, dv, OUT)                                                             # pursed lips: "ptoo"
    elif face == "scared":
        for du, dv in ((11.2, -3.2), (12.0, -2.6), (12.0, -3.8), (11.2, -4.4), (10.4, -3.8), (10.4, -2.6)):
            dot(du, dv, OUT)                                                             # a little O
        dot(11.2, -3.4, INK)
    elif face == "hit":
        for du in (9.0, 10.0, 11.0, 12.0):
            dot(du, -3.6 + 0.3 * (du - 9), OUT)
        dot(11.6, -4.6, ACC[0])                                                          # the tongue
        dot(12.2, -4.8, ACC[0])
    else:
        for du in (9.2, 10.2, 11.2, 12.2):
            dot(du, -3.4 + (0.5 if du in (9.2, 12.2) else 0.0), OUT)                     # a smile
        dot(5.4, -2.0, ACC[0])                                                           # a blush
        dot(6.2, -2.0, ACC[0])
    cv.outline(OUT)
    for py in range(32):                                  # the stub's lower end stays open: the neck goes on there
        for px in range(32):
            al, ac = stub(px, py)
            if cv.get(px, py) == OUT and al > NECK_STUB - 0.5 and abs(ac) <= NECK_W + 1.5:
                cv.p[py][px] = None

    # the eye: axis-aligned (crisp at 1x), on the skull, looking where the snout points
    ex, ey = to_screen(2.6, 2.4)
    ex, ey = int(round(ex)) - 1, int(round(ey)) - 2
    look_up = angle > 20
    look_down = angle < -10
    if face == "hit":
        for d in range(3):
            cv.set(ex + d, ey + d, INK), cv.set(ex + 2 - d, ey + d, INK)
    elif face == "scared":
        cv.rect(ex - 1, ey - 1, ex + 3, ey + 3, OUT)
        cv.rect(ex, ey, ex + 2, ey + 2, WHITE)
        cv.set(ex + 1, ey + 1, INK)
        cv.set(ex - 1, ey - 3, OUT), cv.set(ex, ey - 3, OUT), cv.set(ex + 1, ey - 4, OUT)   # eyebrows up
        sx, sy = to_screen(-7.0, 4.5)
        sx, sy = int(round(sx)), int(round(sy))
        for k, (dx, dy) in enumerate(((0, 0), (0, 1), (-1, 2), (0, 2), (1, 2), (0, 3))):
            cv.set(sx + dx, sy + dy, SWEAT if k < 5 else WHITE)                         # a sweat drop
    elif puff:
        cv.rect(ex, ey + 1, ex + 2, ey + 1, INK)                                         # eyes squeezed: "ptoo!"
        cv.set(ex - 1, ey, INK), cv.set(ex + 3, ey + 2, INK)
    else:
        # a big dark llama eye with a glint (a white eye would vanish in the cream wool), long lashes
        cv.rect(ex, ey, ex + 2, ey + 3, INK)
        gy = ey + (0 if look_up else 2 if look_down else 1)
        cv.set(ex + 2 if look_up else ex + 1, gy, WHITE)
        cv.set(ex - 1, ey, OUT), cv.set(ex - 1, ey - 1, OUT), cv.set(ex + 1, ey - 1, OUT)   # lashes
    return cv


# ---- the neck: an upright oval of wool, its top leaning toward the hornets; stacked along the neck -------------------
def neck(lean):
    cv = Canvas(16, 24)
    a = math.radians(lean)
    c, s = math.cos(a), math.sin(a)
    for py in range(24):
        for px in range(16):
            dx, dy = px + 0.5 - 8, py + 0.5 - 12
            # rotate into the neck's frame (w across, l along, the top leaning right)
            w, l = dx * c + dy * s, -dx * s + dy * c
            du, dv = w / NECK_W, l / 11.6
            r = du * du + dv * dv
            if r <= 1.0:
                # a cylinder across the neck (light left), wool curls
                col = neck_shade(du)
                if du > 0.2 and (int(l + 12) % 7 == 3) and abs(du) < 0.7:
                    col = FUR[1]
                cv.set(px, py, col)
    # the outline only across (the pieces overlap along the neck: no rings inside it)
    add = []
    for y in range(24):
        for x in range(16):
            if cv.get(x, y) is None and (cv.get(x - 1, y) not in (None, OUT) or cv.get(x + 1, y) not in (None, OUT)):
                add.append((x, y))
    for x, y in add:
        cv.set(x, y, OUT)
    return cv


# ---- the body: 64x48, the shoulder (where the neck starts) at (50, 10), the hooves at the bottom ---------------------
def body():
    cv = Canvas(64, 48)
    legs = [(14, FUR, 1), (22, [FUR[2], FUR[2], FUR[3], FUR[3]], 0), (41, FUR, 1), (49, [FUR[2], FUR[2], FUR[3], FUR[3]], 0)]
    for x, r4, near in sorted(legs, key=lambda e: e[2]):           # far legs first
        hs.capsule(cv, x, 26, x - (1 if near else 0), 42, 3.0, r4)
        cv.rect(x - 3, 43, x + 2, 45, OUT)                           # hooves
        cv.rect(x - 2, 43, x + 1, 43, (96, 66, 70))
    hs.ellipse(cv, 30.0, 18.0, 24.0, 12.0, FUR)                     # the torso
    hs.ellipse(cv, 48.0, 15.0, 9.0, 11.0, FUR)                      # the chest
    hs.ellipse(cv, 50.0, 8.0, 7.0, 8.0, FUR)                        # the neck's base
    hs.ellipse(cv, 7.0, 12.0, 4.5, 4.5, [FUR[0], FUR[0], FUR[1], FUR[2]])   # the tail tuft
    rng = random.Random(5)
    for _ in range(26):                                              # wool curls
        x, y = rng.randint(10, 52), rng.randint(14, 27)
        if cv.get(x, y) in (FUR[1], FUR[2]) and cv.get(x + 1, y) is not None:
            cv.set(x, y, FUR[0] if cv.get(x, y) == FUR[1] else FUR[1])
    # the blanket on the back: the player's colour, Andean zigzags, a fringe of tassels
    for y in range(4, 21):
        for x in range(16, 42):
            if cv.get(x, y) is None:
                continue
            col = ACC[1]
            if y in (9, 15):
                col = ACC[0]
            if 11 <= y <= 13 and (x + (y - 11)) % 4 == 0 or 11 <= y <= 13 and (x - (y - 11)) % 4 == 0:
                col = WHITE if y == 12 else ACC[0]
            if y >= 19:
                col = ACC[2] if x % 2 == 0 else ACC[1]
            if x in (16, 41):
                col = ACC[2]
            cv.set(x, y, col)
    for x in range(17, 41, 3):                                        # tassels
        cv.set(x, 21, ACC[0]), cv.set(x, 22, ACC[1])
    cv.outline(OUT)
    return cv


def icon():
    """The 16x16 icon of the title's player slots and the results: the llama's face (the players' colours apply)."""
    cv = Canvas(16, 16)
    hs.capsule(cv, 4.6, 15.5, 5.0, 9.0, 2.4, FUR)                                 # the top of the neck
    hs.capsule(cv, 4.4, 4.6, 3.6, 0.9, 1.0, [FUR[2], FUR[2], FUR[3], FUR[3]])    # the far ear
    hs.ellipse(cv, 6.0, 6.8, 3.6, 3.0, FUR)                                       # the skull
    hs.capsule(cv, 7.0, 7.6, 12.2, 8.2, 2.2, FUR)                                 # the long muzzle
    hs.capsule(cv, 6.4, 4.4, 6.6, 0.8, 1.1, FUR)                                  # the near ear
    cv.set(6, 2, PINK)
    cv.set(5, 1, ACC[1]), cv.set(6, 1, ACC[1]), cv.set(5, 0, ACC[0])            # the pompom
    cv.set(8, 6, INK), cv.set(8, 7, INK)                                          # the eye
    cv.set(13, 7, OUT)                                                            # the nostril
    cv.set(10, 9, OUT), cv.set(11, 9, OUT), cv.set(12, 9, OUT)                   # the smile
    cv.outline(OUT)
    return cv


# ---- the hornets: they fly to the LEFT (toward the llamas); cute, grumpy, never scary -------------------------------------
YEL = [(255, 238, 120), (250, 204, 46), (214, 146, 22)]
ORA = [(255, 168, 84), (226, 96, 40)]
STRIPE = (52, 34, 40)
H_OUT = (40, 22, 26)
WING = [(232, 246, 255), (160, 204, 240)]


def stripes(cv, x0, x1, y0, y1, xs):
    for y in range(y0, y1):
        for x in range(x0, x1):
            c = cv.get(x, y)
            if c is not None and c not in WING and any(abs(x - k) < 1.0 for k in xs):
                cv.set(x, y, STRIPE)


def hornet(frame, kind="small", bonk=False):
    if kind == "big":
        cv = Canvas(32, 32)
        if not bonk:
            hs.ellipse(cv, 17.0, 8.0 if frame == 0 else 10.0, 8.0, 4.0 if frame == 0 else 2.6, [WING[0]] * 2 + [WING[1]] * 2)
            hs.ellipse(cv, 22.0, 9.0 if frame == 0 else 11.0, 6.0, 3.0 if frame == 0 else 2.0, [WING[0]] * 2 + [WING[1]] * 2)
        hs.ellipse(cv, 20.0, 18.0, 9.5, 7.5, YEL + [YEL[2]])            # the abdomen
        stripes(cv, 14, 31, 10, 27, [17.5, 22.5, 26.5])
        for k in range(3):                                               # the stinger
            cv.set(29 + k, 18 + (k // 2), STRIPE)
        hs.ellipse(cv, 11.5, 16.5, 5.0, 5.0, [ORA[0], ORA[0], ORA[1], ORA[1]])   # a fuzzy thorax
        hs.ellipse(cv, 6.0, 15.0, 5.2, 5.0, YEL + [YEL[2]])              # the head
        for lx in (9, 12, 15):                                           # legs
            cv.set(lx, 22, STRIPE), cv.set(lx - 1, 23, STRIPE)
        cv.set(4, 9, STRIPE), cv.set(3, 8, STRIPE), cv.set(2, 7, STRIPE)        # antennae
        cv.set(7, 9, STRIPE), cv.set(7, 8, STRIPE), cv.set(6, 7, STRIPE)
        cv.set(1, 18, STRIPE), cv.set(2, 19, STRIPE)                    # mandibles
        cv.outline(H_OUT)
        if bonk:
            for ex in (2, 6):
                for d in range(3):
                    cv.set(ex + d, 12 + d, INK), cv.set(ex + 2 - d, 12 + d, INK)
        else:
            for ex in (2, 6):
                cv.rect(ex, 12, ex + 2, 15, WHITE)
                cv.set(ex, 14, INK), cv.set(ex, 13, INK)
            cv.rect(1, 10, 3, 10, STRIPE), cv.set(4, 11, STRIPE)          # grumpy brows
            cv.rect(6, 11, 8, 11, STRIPE), cv.set(5, 10, STRIPE)
        return cv
    cv = Canvas(16, 16)
    dart = kind == "dart"
    if not bonk:
        if dart:
            hs.ellipse(cv, 9.5, 4.5 if frame == 0 else 5.5, 4.5, 2.0 if frame == 0 else 1.4, [WING[0]] * 2 + [WING[1]] * 2)
        else:
            hs.ellipse(cv, 8.0, 3.5 if frame == 0 else 5.0, 3.5, 2.6 if frame == 0 else 1.6, [WING[0]] * 2 + [WING[1]] * 2)
            hs.ellipse(cv, 11.0, 4.0 if frame == 0 else 5.5, 2.8, 2.0 if frame == 0 else 1.2, [WING[0]] * 2 + [WING[1]] * 2)
    if dart:
        hs.ellipse(cv, 10.0, 8.5, 5.4, 2.6, [ORA[0], ORA[0], ORA[1], ORA[1]])
        stripes(cv, 6, 16, 5, 12, [9.5, 12.5])
        cv.set(15, 8, STRIPE), cv.set(15, 9, STRIPE)
        hs.ellipse(cv, 3.8, 8.0, 3.0, 2.8, YEL + [YEL[2]])
        cv.set(2, 4, STRIPE), cv.set(3, 5, STRIPE)
    else:
        hs.ellipse(cv, 10.0, 9.0, 4.6, 3.8, YEL + [YEL[2]])
        stripes(cv, 6, 16, 4, 14, [9.0, 11.8])
        cv.set(14, 9, STRIPE), cv.set(15, 10, STRIPE)
        hs.ellipse(cv, 4.6, 8.4, 3.4, 3.3, YEL + [YEL[2]])
        cv.set(4, 4, STRIPE), cv.set(3, 3, STRIPE)
        cv.set(7, 13, STRIPE), cv.set(10, 13, STRIPE)
    cv.outline(H_OUT)
    if bonk:
        for d in range(3):
            cv.set(2 + d, 6 + d, INK), cv.set(4 - d, 6 + d, INK)
    else:
        cv.rect(2, 6, 3, 8, WHITE)
        cv.set(2, 7, INK), cv.set(2, 8, INK)
        cv.set(1, 5, STRIPE), cv.set(2, 5, STRIPE), cv.set(3, 5, STRIPE) if dart else cv.set(4, 6, STRIPE)
    return cv


# ---- props ------------------------------------------------------------------------------------------------------------------
MINT = [(232, 255, 238), (170, 232, 196), (104, 184, 146)]
MINT_OUT = (36, 92, 76)
STAR = [(255, 240, 130), (244, 180, 40)]
P_OUT = (22, 18, 40)
RED = (232, 72, 72)


def gob(frame):
    cv = Canvas(8, 8)
    if frame == 0:
        hs.ellipse(cv, 4.0, 4.0, 2.8, 2.8, [MINT[0], MINT[0], MINT[1], MINT[2]])
    else:
        hs.ellipse(cv, 4.4, 4.0, 3.2, 2.4, [MINT[0], MINT[0], MINT[1], MINT[2]])
    cv.set(3, 3, WHITE)
    cv.outline(MINT_OUT)
    return cv


def splat(frame):
    cv = Canvas(8, 8)
    r = [1.5, 2.6, 3.4][frame]
    for k in range(6):
        a = k * math.pi / 3 + 0.4
        x, y = 4 + r * math.cos(a), 4 + r * math.sin(a)
        if frame < 2 or hs.checker(int(x), int(y)):
            cv.set(int(x), int(y), MINT[1] if frame else MINT[0])
    if frame == 0:
        cv.rect(3, 3, 4, 4, MINT[0])
    return cv


def star(frame):
    cv = Canvas(8, 8)
    pts = [(3, 0), (4, 0), (3, 1), (4, 1), (0, 3), (1, 3), (2, 3), (5, 3), (6, 3), (7, 3), (1, 4), (6, 4),
           (2, 5), (5, 5), (1, 6), (6, 6), (1, 7), (6, 7)]
    if frame:
        pts = [(x, 7 - y) for x, y in pts]
    for x, y in pts:
        cv.set(x, y, STAR[1])
    for y in range(2, 6):
        for x in range(2, 6):
            if (x, y) not in ((2, 5), (5, 5), (2, 2), (5, 2)) or frame:
                cv.set(x, y, STAR[0])
    cv.set(3, 3, WHITE)
    return cv


def dot():
    cv = Canvas(8, 8)
    cv.rect(3, 3, 4, 4, MINT[0])
    cv.set(4, 4, MINT[1])
    return cv


def warn():
    cv = Canvas(8, 8)
    cv.rect(3, 0, 4, 4, RED)
    cv.rect(3, 6, 4, 7, RED)
    cv.outline(WHITE)
    return cv


DIGITS = ["111 101 101 101 111", "010 110 010 010 111", "111 001 111 100 111", "111 001 011 001 111",
          "101 101 111 001 001", "111 100 111 001 111", "111 100 111 101 111", "111 001 010 010 010",
          "111 101 111 101 111", "111 101 111 001 111"]


def small_digit(n):
    cv = Canvas(8, 8)
    for y, row in enumerate(DIGITS[n].split()):
        for x, ch in enumerate(row):
            if ch == "1":
                cv.set(2 + x, 1 + y, WHITE if y < 4 else STAR[0])
    cv.outline(P_OUT, diag=True)
    return cv


# ---- the sky (OBJ 7: recoloured by the time of day) ------------------------------------------------------------------------
SUN = [(255, 252, 220), (255, 232, 140), (252, 198, 84), (255, 238, 176)]
MOON = [(250, 248, 230), (208, 208, 200)]
CLOUD = [(255, 255, 255), (234, 242, 252), (200, 216, 236)]


def sun():
    cv = Canvas(32, 32)
    for y in range(32):
        for x in range(32):
            d = math.hypot(x + 0.5 - 16, y + 0.5 - 16)
            if d < 10.5:
                col = SUN[0] if d < 6 else SUN[1] if d < 9 else SUN[2]
                if (x + 0.5 - 16) + (y + 0.5 - 16) < -9:
                    col = SUN[0]
                cv.set(x, y, col)
            elif d < 14.5 and hs.checker(x, y) and d < 12.5 + 2 * (((x * 7 + y * 3) % 5) / 5.0):
                cv.set(x, y, SUN[3])                                     # the glow fading (house dither: a fade)
    return cv


def moon():
    cv = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            d = math.hypot(x + 0.5 - 8, y + 0.5 - 8)
            d2 = math.hypot(x + 0.5 - 11, y + 0.5 - 6.5)
            if d < 6.5 and d2 > 5.2:
                cv.set(x, y, MOON[0] if d < 5 else MOON[1])
    return cv


def cloud(w):
    cv = Canvas(w, 16)
    puffs = [(10, 10, 6), (18, 7, 7), (25, 10, 5)] if w == 32 else [(9, 11, 5), (17, 8, 7), (27, 6, 7), (36, 9, 6),
                                                                      (42, 11, 4)]
    for y in range(16):
        for x in range(w):
            for cx, cy, r in puffs:
                if math.hypot(x + 0.5 - cx, y + 0.5 - cy) < r and y < 15:
                    cv.set(x, y, CLOUD[2] if y >= 12 else CLOUD[1] if y >= 9 else CLOUD[0])
    return cv


def sprite_frame(name, i):
    if name == "head":
        return head(HEAD_ANGLES[i])
    if name == "head_puff":
        return head(HEAD_ANGLES[i], "puff")
    if name == "head_scared":
        return head(HEAD_ANGLES[i], "scared")
    if name == "head_hit":
        return head(-35, "hit")
    if name == "neck":
        return neck(NECK_LEANS[i])
    if name == "neck2":
        return neck(NECK_LEANS[2])
    if name == "body":
        return body()
    if name == "icon":
        return icon()
    if name in ("hornet", "dart", "big"):
        return hornet(i, {"hornet": "small"}.get(name, name))
    if name.endswith("_bonk"):
        return hornet(0, {"hornet": "small"}.get(name[:-5], name[:-5]), bonk=True)
    if name == "gob":
        return gob(i)
    if name == "splat":
        return splat(i)
    if name == "star":
        return star(i)
    if name == "dot":
        return dot()
    if name == "warn":
        return warn()
    if name == "digit":
        return small_digit(i)
    if name == "sun":
        return sun()
    if name == "moon":
        return moon()
    if name == "cloud":
        return cloud(32)
    if name == "cloud2":
        return cloud(48)
    raise KeyError(name)


# ---- panoramas (BG tiles): seamless over 512 px (the maps repeat) ---------------------------------------------------------
HAZE = [(176, 200, 226), (156, 182, 214)]
ROCK = [(150, 164, 196), (118, 132, 170), (92, 104, 144)]
SNOW = [(252, 252, 255), (212, 226, 246)]
PUNA = [(196, 210, 120), (160, 184, 92), (122, 152, 76), (92, 122, 66)]
STARC = (255, 255, 254)             # the stars: the raster callback recolours this entry per line (by night only)


def per(x, period, ph=0.0):
    return math.sin((x / float(period) + ph) * 2 * math.pi)


def puna_top(x):
    return 140 + int(8 * per(x, 256, 0.7) + 5 * per(x, 128, 0.2) + 2 * per(x, 32, 0.5))


def puna(x, y):
    """The puna's colour at screen (x, y): green slopes with terraces (the meadow's top rows continue it)."""
    d = y - puna_top(x)
    col = PUNA[0] if d < 2 else PUNA[1] if d < 10 else PUNA[2]
    if d >= 10 and (y + int(4 * per(x, 128))) % 9 == 0:
        col = PUNA[3]                                                    # terraces
    if d >= 30:
        col = PUNA[2] if (y + int(4 * per(x, 128))) % 9 else PUNA[3]
    return col


def mountains():
    W, H = PANORAMAS["mountains"][1:]
    cv = Canvas(W, H)
    rng = random.Random(11)
    # the far range: soft, hazy (no outline, low contrast)
    for x in range(W):
        top = 104 + int(10 * per(x, 256, 0.1) + 6 * per(x, 128, 0.4) + 3 * per(x, 64))
        for y in range(top, H):
            cv.set(x, y, HAZE[0] if y - top < 3 else HAZE[1])
    # the snowy peaks: sharp Andes, placed every 128 px (+ jitter that also repeats)
    peaks = [(30, 40, 46), (96, 62, 40), (150, 30, 58), (210, 56, 44), (282, 36, 52), (350, 64, 38), (410, 34, 56),
             (470, 58, 42)]
    ridge = [H] * W
    for x in range(W):
        best = H
        for px_, py_, half in peaks:
            for k in (-1, 0, 1):
                d = abs(x - (px_ + k * W))
                h = py_ + d * 92.0 / half + 2 * per(x, 32, 0.3)
                best = min(best, h)
        ridge[x] = int(best)
    for x in range(W):
        top = ridge[x]
        for y in range(max(top, 0), H):
            depth = y - top
            # light from the left: the left face lit, the right face in shade
            left = ridge[(x - 2) % W] > ridge[(x + 2) % W]
            col = ROCK[0] if left else ROCK[1]
            if depth > 30 + 6 * per(x, 64):
                col = ROCK[1] if left else ROCK[2]
            snow_line = 22 + 6 * per(x, 64, 0.2) + 4 * per(x, 32)
            if y < 92 and depth < snow_line:
                col = SNOW[0] if left else SNOW[1]
            cv.set(x, y, col)
    # the near puna: green slopes with terraces
    for x in range(W):
        for y in range(puna_top(x), H):
            cv.set(x, y, puna(x, y))
    # stars in the sky (seen at night: their colour is set per line)
    for _ in range(46):
        x, y = rng.randrange(W), rng.randrange(4, 96)
        if cv.get(x, y) is None and all(cv.get(x + dx, y + dy) is None for dx in (-3, 0, 3) for dy in (-3, 0, 3)):
            cv.set(x, y, STARC)
            if rng.random() < 0.3 and y < 60:
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    if cv.get(x + dx, y + dy) is None:
                        cv.set(x + dx, y + dy, STARC)
    return cv


GRASS = [(178, 214, 92), (138, 188, 70), (98, 156, 56), (66, 118, 46)]
FLOWERS = [(255, 236, 110), (250, 150, 186), (250, 250, 250)]
SOIL = (126, 98, 66)


def meadow():
    W, H = PANORAMAS["meadow"][1:]
    cv = Canvas(W, H)
    rng = random.Random(3)
    for x in range(W):
        top = 1 + int(round(0.8 * per(x, 64) + 0.6 * per(x, 32, 0.3)))
        for y in range(top, H):
            d = y - top
            col = GRASS[0] if d == 0 else GRASS[1] if d < 6 else GRASS[2]
            if d >= 6 and (x * 3 + y * 7) % 16 == 0:     # (patterns of 16 and 32 px: few tiles)
                col = GRASS[1]
            if d >= 20 and (x * 5 + y * 11) % 32 == 0:
                col = GRASS[3]
            cv.set(x, y, col)
        cv.set(x, top - 1, GRASS[3])
        for y in range(top - 1):                          # above the grass's edge: the puna behind (no sky seam)
            cv.set(x, y, puna(x, MEADOW_Y + y))
    # tufts and little flowers (a pattern that repeats every 128 px: fewer tiles)
    for _ in range(18):
        x, y = rng.randrange(128), rng.randrange(10, H - 6)
        for k in range(4):
            xx = x + k * 128
            for dy in range(3):
                cv.set(xx, y - dy, GRASS[0] if dy else GRASS[3])
                cv.set(xx + 2, y - dy + 1, GRASS[0] if dy else GRASS[3])
            cv.set(xx + 1, y, GRASS[3])
    for _ in range(14):
        x, y = rng.randrange(128), rng.randrange(8, H - 4)
        f = FLOWERS[rng.randrange(3)]
        for k in range(4):
            xx = x + k * 128
            for dx, dy in ((0, -1), (-1, 0), (1, 0), (0, 1)):
                cv.set(xx + dx, y + dy, f)
            cv.set(xx, y, FLOWERS[0] if f != FLOWERS[0] else SOIL)
    for _ in range(8):
        x, y = rng.randrange(128), rng.randrange(14, H - 3)
        for k in range(4):
            cv.rect(x + k * 128, y, x + k * 128 + 2, y + 1, SOIL)
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
    files[PANORAMAS["mountains"][0]] = mountains().image()
    files[PANORAMAS["meadow"][0]] = meadow().image()
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

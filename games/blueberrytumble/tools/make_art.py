#!/usr/bin/env python3
"""Blueberry Tumble: the code-drawn art, in the 8BCraft house style (docs/art-direction.md).

Code-drawn only, no image generation: every sprite, tile and panorama is drawn here with
games/common/tools/house_style.py (1-px outlines outside the shapes, 3-4-shade ramps lit from the top-left,
squash and stretch, checker dither only for fades). The berry is drawn once per angle (SNES games pre-render
their rotations): its star calyx and leaf stem turn with the roll, the light stays top-left.

The playfield tiles use ONE set of shapes drawn with 15 colour ROLES; each biome is a palette of those roles
(PF_ROLES / PF_BIOMES), so the four biomes share their tiles (the SNES way) and only the surface, the thorns and
the decor have per-biome shapes.

Writes, in the art folder (default games/blueberrytumble/art):
  sprites.png             the sprite sheet (SPRITES: name, x, y, w, h, frames, palette group)
  tiles/playfield.png     the playfield metatiles, one row per biome (PF_TILES)
  tiles/mid.png           the mid-ground panoramas, one band per biome (128 x 48 each)
  tiles/far.png           the far mountains (256 x 64)
  palettes.json           the playfield, mid-ground and far palettes (build_assets.py reads them)

    python3 games/blueberrytumble/tools/make_art.py [--out DIR] [--preview]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/blueberrytumble/LICENSE.
"""
import argparse
import json
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
MAG = hs.MAGENTA

# ---- the berry's colours (the hero: the most saturated thing on screen) -------------------------------------------
# a Blueberry Tumble accent ramp (light -> dark); house_style.ACCENTS could take it as "blueberry"
BERRY = [(150, 168, 246), (88, 104, 226), (54, 60, 170), (34, 32, 112)]
BLOOM = (196, 206, 246)          # the dusty bloom highlight, top-left
BERRY_OUT = (22, 16, 58)         # its outline: the darkest shade pushed towards navy-violet
CALYX = [(200, 188, 240), (74, 56, 140), (26, 18, 70)]     # the pale crown, its shadow, the dark centre
STEM = hs.ramp("leaf")
SNOW = [(252, 253, 255), (218, 230, 246), (170, 190, 222), (122, 140, 186)]
SNOW_OUT = (40, 50, 96)
JUICE = [(206, 96, 186), (150, 50, 150), (96, 28, 110)]

SHEET_W, SHEET_H = 256, 128
# name, x, y, w, h, frames, palette group (berry: OBJ 0, player 2 = OBJ 1 recoloured; props: OBJ 2; propsb: OBJ 4)
SPRITES = [
    ("berry", 0, 0, 16, 16, 16, "berry"),          # 16 angles of the roll
    ("berry_squash", 0, 16, 16, 16, 4, "berry"),   # landing, 4 angles
    ("berry_stretch", 64, 16, 16, 16, 4, "berry"),  # take-off, 4 angles
    ("berry_hit", 128, 16, 16, 16, 1, "berry"),    # X eyes
    ("snowberry", 0, 32, 24, 24, 4, "snow"),       # the snowball, 4 angles (OBJ 5, player 2: OBJ 6)
    ("juice", 96, 32, 8, 8, 3, "berry"),           # juice drops (the splat)
    ("splat", 128, 32, 16, 16, 3, "berry"),        # the splash on the ground
    ("snowbit", 176, 32, 8, 8, 2, "snow"),         # snow flying off / smashed crumbs
    ("dew", 0, 64, 16, 16, 2, "props"),            # the dew drop (a jump orb), shining
    ("dew_ring", 32, 64, 16, 16, 2, "props"),      # used: a ring of water
    ("mushroom", 64, 64, 16, 16, 2, "props"),      # the jump pad, squashed when used
    ("golden", 96, 64, 16, 16, 2, "gold"),         # the golden blueberry (OBJ 7)
    ("spark", 128, 64, 8, 8, 2, "gold"),
    ("cone", 0, 96, 16, 16, 4, "propsb"),          # the pine cone, 4 angles
    ("leaf", 64, 96, 32, 16, 3, "propsb"),         # the maple leaf glider: nose up, level, down
    ("gust", 160, 96, 8, 8, 2, "propsb"),          # little leaves in the gust
]
SHEETS = {"sprites": "sprites.png", "playfield": "tiles/playfield.png", "mid": "tiles/mid.png", "far": "tiles/far.png"}


# ---- the berry ----------------------------------------------------------------------------------------------------
def berry_body(cv, cx, cy, r, angle, eyes="open", calyx=True):
    """The body (fixed lighting), the calyx and stem at `angle` (radians, 0 = on top, turning clockwise as it rolls
    right), the eyes looking ahead (they do not turn: a cartoon face)."""
    hs.ellipse(cv, cx, cy, r, r, BERRY)
    # the bloom: a dusty highlight top-left (two pixels, never a white rim)
    bx, by = int(cx - r * 0.5), int(cy - r * 0.55)
    cv.set(bx, by, BLOOM)
    cv.set(bx + 1, by, BERRY[0])
    cv.set(bx, by + 1, BERRY[0])
    if calyx:
        d = r * 0.58
        sx, sy = cx + d * math.sin(angle), cy - d * math.cos(angle)
        # the stem: a tiny leaf pointing out from the calyx
        lx, ly = cx + (r + 0.7) * math.sin(angle + 0.3), cy - (r + 0.7) * math.cos(angle + 0.3)
        mx, my = (sx * 0.4 + lx * 0.6), (sy * 0.4 + ly * 0.6)
        cv.set(int(round(mx - 0.5)), int(round(my - 0.5)), STEM[2])
        cv.set(int(round(lx - 0.5)), int(round(ly - 0.5)), STEM[1])
        # the star calyx: five pale points (a shaded inner half) around a dark centre
        for k in range(5):
            a = angle + k * 2 * math.pi / 5
            for rr, col in ((1.0, CALYX[1]), (2.1, CALYX[0])):
                px, py = sx + rr * math.sin(a), sy - rr * math.cos(a)
                cv.set(int(round(px - 0.5)), int(round(py - 0.5)), col)
        cv.set(int(round(sx - 0.5)), int(round(sy - 0.5)), CALYX[2])
    if eyes == "open":
        hs.eye(cv, int(cx + 0.5), int(cy - 2), look=(1, 0))
        hs.eye(cv, int(cx + 3.5), int(cy - 2), look=(1, 0))
    elif eyes == "x":
        ink = hs.HOUSE["ink"][0]
        for ex in (int(cx - 1), int(cx + 3)):
            for d in range(3):
                cv.set(ex + d, int(cy - 2) + d, ink)
                cv.set(ex + 2 - d, int(cy - 2) + d, ink)


def berry(angle_i, pose="roll", n=16):
    cv = Canvas(16, 16)
    a = angle_i * 2 * math.pi / n
    berry_body(cv, 7.5, 8.5, 6.5, a, eyes="x" if pose == "hit" else "open")
    if pose == "squash":
        cv = hs.squash(cv, 1.22)
    elif pose == "stretch":
        cv = hs.squash(cv, 0.84)
    cv.outline(BERRY_OUT)
    return cv


def snowberry(angle_i):
    """A snowball with the berry peeking through; the specks turn with the roll."""
    cv = Canvas(24, 24)
    cx, cy, r = 11.5, 12.5, 10.5
    hs.ellipse(cv, cx, cy, r, r, SNOW)
    turn = angle_i * 2 * math.pi / 20          # 4 frames: a fifth of a turn (the specks repeat 5 times around)
    for k in range(5):
        a = turn + k * 2 * math.pi / 5
        for j, rr in enumerate((0.62, 0.3)):
            x, y = cx + rr * r * math.sin(a + j), cy - rr * r * math.cos(a + j)
            if j == 0 and k in (0, 2):        # berry blue showing through
                hs.ellipse(cv, x, y, 1.7, 1.5, None, flat=BERRY[2])
                cv.set(int(x - 0.5), int(y - 1.4), BERRY[1])
            else:                             # snow clumps
                cv.set(int(x), int(y), SNOW[2])
                cv.set(int(x) + 1, int(y), SNOW[2])
    cv.set(int(cx - 5), int(cy - 6), (255, 255, 255))
    hs.eye(cv, int(cx + 1), int(cy - 4), look=(1, 0), big=True)
    hs.eye(cv, int(cx + 5), int(cy - 4), look=(1, 0), big=True)
    cv.outline(SNOW_OUT)
    return cv


def juice(i):
    cv = Canvas(8, 8)
    r = [2.6, 2.0, 1.4][i]
    hs.ellipse(cv, 3.5, 4.0, r, r * 1.1, [JUICE[0], JUICE[0], JUICE[1], JUICE[2]])
    if i < 2:
        cv.set(3, 2, JUICE[0])
    return cv


def splat(i):
    """The berry squashed flat on the ground, juice splashed around (dithered away in the last frame)."""
    cv = Canvas(16, 16)
    w = [5.5, 7.0, 7.5][i]
    h = [3.0, 2.2, 1.8][i]
    hs.ellipse(cv, 7.5, 15.0 - h, w, h, BERRY)
    for dx, dy in [(-6, -4), (6, -5), (-3, -7), (4, -8)][: 2 + i]:
        if i < 2 or hs.checker(dx, dy):
            cv.set(8 + dx, 12 + dy, JUICE[0])
            cv.set(8 + dx, 13 + dy, JUICE[1])
    cv.outline(BERRY_OUT)
    return cv


def snowbit(i):
    cv = Canvas(8, 8)
    pts = [(2, 3), (4, 2), (5, 4), (3, 5)] if i == 0 else [(1, 2), (5, 1), (6, 5), (2, 6)]
    for x, y in pts:
        cv.set(x, y, SNOW[0])
        cv.set(x + 1, y, SNOW[2])
    return cv


# ---- props (OBJ 2): dew drop, mushroom, golden blueberry, sparkle -------------------------------------------------
DEW = [(236, 252, 255), (156, 222, 250), (84, 170, 230), (40, 104, 176)]
DEW_OUT = (22, 52, 104)
CAP = [(255, 150, 136), (232, 70, 64), (170, 34, 44), (104, 18, 34)]
STALK = [(255, 246, 220), (232, 214, 176), (188, 160, 120)]
GOLD = [(255, 242, 160)] + hs.ramp("gold") + [(120, 78, 12)]     # the house gold, lit and shaded


def dew(i):
    cv = Canvas(16, 16)
    # a drop: a round bottom and a pointed top
    for y in range(16):
        for x in range(16):
            dx, dy = x + 0.5 - 8, y + 0.5 - 9.5
            inside = dx * dx + dy * dy <= 5.0 ** 2 or (-8.5 < dy < 0 and abs(dx) <= (dy + 9.5) * 0.55)
            if inside:
                cv.set(x, y, hs.shade_nd((dx * hs.LIGHT_DIR[0] + dy * hs.LIGHT_DIR[1]) / 5.5, DEW))
    cv.set(6, 8, (255, 255, 255))
    cv.set(6, 9, (255, 255, 255))
    if i == 1:
        cv.set(7, 7, (255, 255, 255))
        cv.set(10, 11, DEW[0])
    cv.outline(DEW_OUT)
    return cv


def dew_ring(i):
    cv = Canvas(16, 16)
    r = [4.5, 6.5][i]
    for y in range(16):
        for x in range(16):
            d = math.hypot(x + 0.5 - 8, y + 0.5 - 8)
            if abs(d - r) < 0.8 and (i == 0 or hs.checker(x, y)):
                cv.set(x, y, DEW[1] if y < 8 else DEW[2])
    return cv


def mushroom(i):
    """The jump pad: a red cap with white dots on a cream stalk; frame 1 squashed (just used)."""
    cv = Canvas(16, 16)
    sq = i == 1
    top = 9 if sq else 5
    for x in range(6, 10):
        for y in range(top + 3, 16):
            cv.set(x, y, hs.shade3(dict(h=STALK[0], l=STALK[0], m=STALK[1], d=STALK[2]), x, y, 4, 6))
    hs.ellipse(cv, 7.5, top + 3.0, 7.0 if not sq else 7.5, 3.6 if not sq else 2.6, CAP)
    for x in range(1, 15):
        if cv.get(x, top + 5) is not None:
            cv.set(x, top + 5, CAP[3])
    for dx, dy in ((-3, 0), (1, -1), (4, 1)):
        cv.set(8 + dx, top + 2 + dy, STALK[0])
        cv.set(9 + dx, top + 2 + dy, STALK[0])
    cv.outline((64, 12, 26))
    return cv


def golden(i):
    cv = Canvas(16, 16)
    hs.ellipse(cv, 7.5, 8.5, 5.5, 5.5, GOLD)
    for k in range(5):
        a = k * 2 * math.pi / 5
        cv.set(int(7.5 + 1.4 * math.sin(a)), int(5.5 - 1.4 * math.cos(a)), GOLD[3])
    cv.set(5, 6, (255, 252, 220))
    if i == 1:
        cv.set(11, 11, (255, 252, 220))
        cv.set(4, 7, (255, 252, 220))
    cv.outline((70, 44, 8))
    return cv


def spark(i):
    cv = Canvas(8, 8)
    c = hs.HOUSE["spark"][0]
    if i == 0:
        for d in range(-2, 3):
            cv.set(3 + d, 3, c)
            cv.set(3, 3 + d, c)
    else:
        for d in (-1, 1):
            cv.set(3 + d, 3 + d, c)
            cv.set(3 + d, 3 - d, c)
    cv.set(3, 3, (255, 255, 255))
    return cv


# ---- props B (OBJ 4): pine cone, maple leaf, gust leaves ------------------------------------------------------------
CONE = [(206, 150, 92), (158, 100, 56), (110, 64, 34), (70, 38, 20)]
MAPLE = [(255, 206, 112), (246, 146, 48), (212, 82, 30), (140, 42, 22)]


def cone(i):
    """A pine cone rolling (4 angles of a quarter turn: its scales make it look round)."""
    cv = Canvas(16, 16)
    a = i * math.pi / 8
    for y in range(16):
        for x in range(16):
            dx, dy = x + 0.5 - 8, y + 0.5 - 9
            u = dx * math.cos(a) + dy * math.sin(a)
            v = -dx * math.sin(a) + dy * math.cos(a)
            if (u / 6.0) ** 2 + (v / 5.0) ** 2 <= 1.0:
                nd = (dx * hs.LIGHT_DIR[0] + dy * hs.LIGHT_DIR[1]) / 6.0
                col = hs.shade_nd(nd, CONE)
                if (int(math.floor(u + 8)) + int(math.floor(v * 1.4 + 8))) % 3 == 0:
                    col = CONE[min(3, CONE.index(col) + 1)]
                cv.set(x, y, col)
    cv.outline((44, 24, 12))
    return cv


def maple(i):
    """The maple leaf the berry rides: 32 x 16, frame 0 nose up, 1 level, 2 nose down."""
    cv = Canvas(32, 16)
    tilt = [-0.22, 0.0, 0.22][i]
    # a maple leaf seen from a little above, lying flat: its outline in polar form around the centre (the long middle
    # lobe forward, two pairs of side lobes, the stem at the back), squashed to 45% vertically
    cx, cy = 15.5, 9.5
    for y in range(16):
        for x in range(32):
            dx = x + 0.5 - cx
            dy = (y + 0.5 - cy - dx * tilt) / 0.45
            r, a = math.hypot(dx, dy), math.atan2(dy, dx)
            lobes = 0.62 + 0.38 * abs(math.cos(2.5 * a)) ** 0.6
            if r <= 13.5 * lobes * (1.0 if abs(a) < 2.4 else 0.6):
                cv.set(x, y, hs.shade_nd((-dy * 0.8 - dx * 0.3) / 14.0, MAPLE))
    for k in range(-12, 12):            # the midrib and two veins
        cv.set(int(cx + k), int(round(cy + k * tilt)), MAPLE[3] if k < 0 else MAPLE[2])
    for k in range(1, 6):
        cv.set(int(cx + 2 + k), int(round(cy - k * 0.5 + (2 + k) * tilt)), MAPLE[2])
        cv.set(int(cx + 2 + k), int(round(cy + k * 0.5 + (2 + k) * tilt)), MAPLE[2])
    for k in range(3):                   # the stem
        cv.set(int(cx - 13 - k), int(round(cy + (-13 - k) * tilt)) + 1, MAPLE[3])
    cv.outline((96, 30, 16))
    return cv


def gust(i):
    cv = Canvas(8, 8)
    pts = [(2, 4), (3, 3), (4, 3), (5, 4), (4, 5)] if i == 0 else [(2, 3), (3, 4), (4, 4), (5, 3), (3, 5)]
    for k, (x, y) in enumerate(pts):
        cv.set(x, y, MAPLE[1] if k % 2 else MAPLE[2])
    return cv


def sprite_frame(name, i):
    if name == "berry":
        return berry(i, "roll")
    if name == "berry_squash":
        return berry(i * 4, "squash")
    if name == "berry_stretch":
        return berry(i * 4, "stretch")
    if name == "berry_hit":
        return berry(0, "hit")
    table = {"snowberry": snowberry, "juice": juice, "splat": splat, "snowbit": snowbit, "dew": dew,
             "dew_ring": dew_ring, "mushroom": mushroom, "golden": golden, "spark": spark, "cone": cone,
             "leaf": maple, "gust": gust}
    return table[name](i)


def build_sheet():
    im = Image.new("RGB", (SHEET_W, SHEET_H), MAG)
    for name, x, y, w, h, frames, _g in SPRITES:
        for i in range(frames):
            im.paste(sprite_frame(name, i).image(), (x + i * w, y))
    return im


# ---- the playfield: shared shapes, per-biome palettes of 15 roles ----------------------------------------------------
PF_ROLES = ["out", "dirt_d", "dirt_m", "dirt_l", "surf_d", "surf_m", "surf_l", "rock_d", "rock_m", "rock_l",
            "wood_d", "wood_m", "accent", "ice", "plant"]
PF_BIOMES = [
    # summit: grey-blue rock, a snow crust, frost thorns
    dict(out=(28, 30, 60), dirt_d=(86, 94, 128), dirt_m=(120, 130, 164), dirt_l=(162, 174, 202), surf_d=(172, 194, 224),
         surf_m=(214, 230, 246), surf_l=(250, 252, 255), rock_d=(80, 84, 110), rock_m=(124, 130, 158),
         rock_l=(176, 184, 206), wood_d=(92, 60, 44), wood_m=(146, 102, 66), accent=(206, 64, 92), ice=(136, 198, 238),
         plant=(46, 86, 82)),
    # pine forest: dark soil, needles and moss, bramble thorns
    dict(out=(26, 20, 22), dirt_d=(62, 40, 30), dirt_m=(96, 64, 42), dirt_l=(134, 94, 60), surf_d=(52, 96, 44),
         surf_m=(84, 136, 56), surf_l=(142, 180, 80), rock_d=(70, 72, 80), rock_m=(110, 112, 118), rock_l=(158, 160, 158),
         wood_d=(82, 48, 30), wood_m=(138, 90, 52), accent=(210, 52, 62), ice=(120, 180, 222), plant=(32, 62, 38)),
    # blueberry meadows: warm earth, bright grass, thistles
    dict(out=(30, 22, 42), dirt_d=(92, 60, 44), dirt_m=(134, 92, 60), dirt_l=(182, 136, 88), surf_d=(76, 148, 60),
         surf_m=(122, 196, 76), surf_l=(188, 228, 112), rock_d=(96, 92, 108), rock_m=(144, 140, 152),
         rock_l=(196, 192, 198), wood_d=(102, 62, 36), wood_m=(168, 114, 64), accent=(160, 76, 190), ice=(126, 196, 238),
         plant=(44, 94, 50)),
    # valley village: a cobbled lane, brick, fences, bramble in the hedges
    dict(out=(36, 20, 30), dirt_d=(102, 70, 52), dirt_m=(148, 106, 72), dirt_l=(194, 150, 102), surf_d=(118, 114, 110),
         surf_m=(168, 162, 150), surf_l=(214, 206, 188), rock_d=(122, 64, 54), rock_m=(174, 100, 76),
         rock_l=(214, 148, 112), wood_d=(96, 56, 32), wood_m=(164, 106, 56), accent=(230, 76, 66), ice=(130, 196, 232),
         plant=(56, 108, 48)),
]
BIOME_NAMES = ["SNOWY SUMMIT", "PINE FOREST", "BLUEBERRY MEADOWS", "VALLEY VILLAGE"]
# the metatiles (16 x 16) in sheet order
PF_TILES = ["surf0", "surf1", "surf2", "dirt0", "dirt1", "deep", "edge_l", "edge_r", "wall_l", "wall_r", "void_top",
            "void", "ice", "snow", "water", "rock_top", "rock", "log_l", "log_m", "log_r", "log_1", "log_v", "thorn",
            "hang", "pebble", "sign", "deco0", "deco1", "deco2"]


def speck(x, y, seed=0):
    """A dark speck of soil here? (a hash, not a pattern: no stripes once the playfield is sheared)"""
    h = ((x * 73856093) ^ (y * 19349663) ^ (seed * 83492791)) & 0xffff
    return h % 7 == 0


def P(b):
    return PF_BIOMES[b]


def outline_inside(cv, col):
    """Darken the rim pixels of a drawing that fills its cell (tiles cannot have an outline outside)."""
    rim = []
    for y in range(cv.h):
        for x in range(cv.w):
            if cv.get(x, y) is not None and any(cv.get(x + dx, y + dy) is None for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                rim.append((x, y))
    for x, y in rim:
        cv.set(x, y, col)


def mt_surface(b, variant):
    """Ground with its surface crust: snow on the summit, needles in the forest, grass in the meadow, cobbles in the
    village. variant 0 plain, 1 and 2 with a little texture."""
    p = P(b)
    cv = Canvas(16, 16)
    rng = random.Random(100 + b * 10 + variant)
    for y in range(16):
        for x in range(16):
            cv.set(x, y, p["dirt_d"] if speck(x, y, variant) else p["dirt_m"])
    crust = 5 if b != 3 else 6
    for x in range(16):
        wob = int(round(0.8 * math.sin((x + variant * 5) * 0.8)))
        for y in range(0, crust + wob):
            cv.set(x, y, p["surf_l"] if y == 0 else p["surf_m"] if y < crust - 2 + wob else p["surf_d"])
        cv.set(x, crust + wob, p["dirt_d"])
    if b == 3:      # cobbles: the stones' joints
        for x in range(0, 16, 5):
            for y in range(1, crust):
                cv.set((x + variant * 2) % 16, y, p["surf_d"])
    if variant >= 1:
        for _ in range(3 + variant):
            x, y = rng.randint(1, 14), rng.randint(crust + 2, 14)
            cv.set(x, y, p["dirt_l"])
            cv.set(x + 1, y, p["dirt_d"])
    return cv


def mt_dirt(b, variant):
    p = P(b)
    cv = Canvas(16, 16)
    rng = random.Random(200 + variant)
    for y in range(16):
        for x in range(16):
            cv.set(x, y, p["dirt_d"] if speck(x, y, 7 + variant) else p["dirt_m"])
    for _ in range(4):
        x, y = rng.randint(1, 13), rng.randint(1, 13)
        cv.rect(x, y, x + 1, y, p["rock_m"])
        cv.set(x, y + 1, p["rock_d"])
        cv.set(x + 1, y + 1, p["rock_d"])
    return cv


def mt_deep(b):
    p = P(b)
    cv = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            cv.set(x, y, p["dirt_d"] if (x + y * 2) % 9 else p["out"])
    return cv


def mt_edge(b, side):
    """The ground's last cell before a crevasse (side 'l': the gap is on its right)."""
    cv = mt_surface(b, 0)
    p = P(b)
    for y in range(16):
        for k in range(3):
            cv.set(15 - k if side == "l" else k, y, p["dirt_d"] if k else p["out"])
    for k in range(4):
        cv.set(15 - k if side == "l" else k, 0, p["surf_l"])
    return cv


def mt_wall(b, side):
    cv = mt_dirt(b, 0)
    p = P(b)
    for y in range(16):
        for k in range(3):
            cv.set(15 - k if side == "l" else k, y, p["dirt_d"] if k else p["out"])
    return cv


def mt_void(b, top):
    """Inside a crevasse: dark depths, a little rock showing near the top."""
    p = P(b)
    cv = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            cv.set(x, y, p["dirt_d"] if top and y < 6 and hs.dither_ok(x, y, 8 - y) else p["out"])
    return cv


def mt_ice(b):
    p = P(b)
    cv = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            cv.set(x, y, p["dirt_d"] if speck(x, y, 3) else p["dirt_m"])
    for x in range(16):
        for y in range(0, 5):
            cv.set(x, y, p["surf_l"] if y == 0 else p["ice"] if y < 4 else p["rock_l"])
    for x in (2, 3, 9, 10, 13):
        cv.set(x, 1 if x != 13 else 2, p["surf_l"])
    return cv


def mt_snow(b):
    p = P(b)
    cv = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            cv.set(x, y, p["dirt_d"] if speck(x, y, 4) else p["dirt_m"])
    for x in range(16):
        h = 8 + int(round(1.2 * math.sin(x * 0.7)))
        for y in range(0, h):
            cv.set(x, y, p["surf_l"] if y < 2 else p["surf_m"] if y < h - 2 else p["surf_d"])
    return cv


def mt_water(b):
    p = P(b)
    cv = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            cv.set(x, y, p["dirt_d"] if y > 6 else p["ice"])
    for x in range(16):
        if (x // 3) % 2 == 0:
            cv.set(x, 0, p["surf_l"])
        cv.set(x, 5, p["rock_l"] if x % 4 else p["ice"])
        cv.set(x, 6, p["out"])
    return cv


def mt_rock(b, top):
    """A chunky boulder block: rounded corners, lit from the top-left; the top one wears the biome's crust."""
    p = P(b)
    cv = Canvas(16, 16)
    R = [p["rock_l"], p["rock_m"], p["rock_m"], p["rock_d"]]
    for y in range(16):
        for x in range(16):
            if min(x, 15 - x) + min(y, 15 - y) < 2:
                continue
            nd = ((x - 7.5) * hs.LIGHT_DIR[0] + (y - 7.5) * hs.LIGHT_DIR[1]) / 8.0
            cv.set(x, y, hs.shade_nd(nd, R))
    for (x, y) in ((5, 9), (6, 10), (6, 11), (10, 5), (11, 6)):
        cv.set(x, y, p["rock_d"])
    if top:
        for x in range(2, 14):
            cv.set(x, 1 if 3 < x < 12 else 2, p["surf_l"])
            cv.set(x, 2 if 3 < x < 12 else 3, p["surf_m"])
    outline_inside(cv, p["out"])
    for x in range(16):
        cv.set(x, 0, p["out"] if cv.get(x, 0) is not None else None)
    return cv


def mt_log(b, part):
    """A fallen log lying on its side: bark stripes; its ends show the rings."""
    p = P(b)
    cv = Canvas(16, 16)
    y0, y1 = 3, 14
    for x in range(16):
        for y in range(y0, y1 + 1):
            f = (y - y0) / float(y1 - y0)
            c = p["dirt_l"] if f < 0.15 else p["wood_m"] if f < 0.8 else p["wood_d"]
            if (x + (y // 3)) % 5 == 0:
                c = p["wood_d"]
            cv.set(x, y, c)
        cv.set(x, y0 - 1, p["out"])
        cv.set(x, y1 + 1, p["out"])
    for side in ("l", "r"):
        if part == side or part == "1":
            for x in (range(0, 4) if side == "l" else range(12, 16)):
                for y in range(y0 - 1, y1 + 2):
                    cv.set(x, y, None)
            cxe = 3.5 if side == "l" else 11.5
            for y in range(y0 - 1, y1 + 2):
                for x in range(16):
                    d = math.hypot((x + 0.5 - cxe) / 3.2, (y + 0.5 - 9.0) / 6.4)
                    if d <= 1.0:
                        cv.set(x, y, p["dirt_l"] if d < 0.35 else p["wood_m"] if d < 0.7 else p["wood_d"])
            outline_inside(cv, p["out"])
    return cv


def mt_log_v(b):
    """A log post standing up (the leaf-weave fences)."""
    p = P(b)
    cv = Canvas(16, 16)
    for y in range(16):
        for x in range(3, 13):
            c = hs.shade3(dict(h=p["dirt_l"], l=p["wood_m"], m=p["wood_m"], d=p["wood_d"]), x, y, 10, 3)
            if (y + x // 3) % 6 == 0:
                c = p["wood_d"]
            cv.set(x, y, c)
        cv.set(2, y, p["out"])
        cv.set(13, y, p["out"])
    return cv


def mt_thorn(b):
    """The thorn bush: a dark clump bristling with thorns and a few berries (frost thorns on the summit, thistles in
    the meadow). Its hitbox (6 x 8) is smaller than the drawing (the genre's leniency)."""
    p = P(b)
    cv = Canvas(16, 16)
    hs.ellipse(cv, 7.5, 12.5, 6.2, 3.6, [p["surf_d"], p["plant"], p["plant"], p["out"]])
    spike = p["ice"] if b == 0 else p["surf_l"] if b == 2 else p["rock_l"]
    tips = [(1, 8), (3, 4), (6, 1), (9, 1), (12, 4), (14, 8), (7, 5), (4, 7), (11, 7)]
    for (tx, ty) in tips:
        bx, by = 7.5 + (tx - 7.5) * 0.45, 11.0
        n = int(max(abs(tx - bx), abs(ty - by))) + 1
        for k in range(n + 1):
            x = bx + (tx - bx) * k / float(n)
            y = by + (ty - by) * k / float(n)
            cv.set(int(round(x)), int(round(y)), spike if k >= n - 1 else p["plant"])
    for (x, y) in ((5, 11), (10, 12), (8, 10)):
        cv.set(x, y, p["accent"])
        cv.set(x + 1, y, p["accent"])
    if b == 2:      # thistle flowers on top
        cv.set(6, 0, p["accent"]), cv.set(9, 0, p["accent"]), cv.set(7, 1, p["accent"])
    cv.outline(p["out"])
    return cv


def mt_hang(b):
    """Hanging thorns (icicles on the summit) under a ceiling block."""
    p = P(b)
    cv = Canvas(16, 16)
    body = [p["surf_l"], p["ice"], p["rock_l"]] if b == 0 else [p["surf_l"], p["plant"], p["plant"]]
    for x in range(16):
        cv.set(x, 0, body[2])
        cv.set(x, 1, body[1])
    for tx, ln in ((2, 7), (5, 11), (8, 8), (11, 12), (14, 6)):
        for y in range(2, 2 + ln):
            w = max(0, (2 + ln - y) // 4)
            for x in range(tx - w, tx + w + 1):
                cv.set(x, y, body[0] if x < tx else body[1])
    cv.outline(p["out"])
    return cv


def mt_pebble(b):
    p = P(b)
    cv = Canvas(16, 16)
    R = [p["rock_l"], p["rock_m"], p["rock_d"], p["out"]]
    for cx, cy, r in ((4.5, 13.0, 3.0), (10.5, 13.5, 3.4), (7.5, 10.5, 2.8)):
        hs.ellipse(cv, cx, cy, r, r * 0.8, R)
    cv.outline(p["out"])
    return cv


def mt_sign(b):
    """The biome gate's signpost (the biome's name is written by the UI as it passes)."""
    p = P(b)
    cv = Canvas(16, 16)
    for y in range(6, 16):
        cv.set(7, y, p["wood_m"]), cv.set(8, y, p["wood_d"])
    for y in range(1, 7):
        for x in range(1, 15):
            cv.set(x, y, p["dirt_l"] if y == 1 else p["wood_m"])
    cv.set(3, 3, p["wood_d"]), cv.set(12, 3, p["wood_d"])
    for x in range(4, 12):
        cv.set(x, 4, p["out"] if x % 2 else p["wood_d"])
    cv.outline(p["out"])
    return cv


def mt_deco(b, k):
    """Low decor on the ground (never taller than 5 px: it never reads as an obstacle)."""
    p = P(b)
    cv = Canvas(16, 16)
    if k == 0:      # a grass tuft (a frosty one on the summit)
        for x, h in ((5, 3), (6, 5), (7, 4), (9, 5), (10, 3)):
            for y in range(16 - h, 16):
                cv.set(x, y, p["surf_d"] if b != 0 else p["rock_l"])
            cv.set(x, 16 - h, p["surf_l"])
    elif k == 1:    # flowers
        for x in (4, 9, 12):
            cv.set(x, 15, p["plant"]), cv.set(x, 14, p["plant"])
            cv.set(x, 13, p["accent"]), cv.set(x - 1, 13, p["surf_l"]), cv.set(x + 1, 13, p["surf_l"])
            cv.set(x, 12, p["surf_l"])
    else:           # a small stone
        hs.ellipse(cv, 8.5, 14.0, 2.6, 1.8, [p["rock_l"], p["rock_m"], p["rock_d"], p["out"]])
    return cv


def pf_tile(b, name):
    if name.startswith("surf"):
        return mt_surface(b, int(name[4]))
    if name.startswith("dirt"):
        return mt_dirt(b, int(name[4]))
    if name.startswith("deco"):
        return mt_deco(b, int(name[4]))
    table = {"deep": lambda: mt_deep(b), "edge_l": lambda: mt_edge(b, "l"), "edge_r": lambda: mt_edge(b, "r"),
             "wall_l": lambda: mt_wall(b, "l"), "wall_r": lambda: mt_wall(b, "r"), "void_top": lambda: mt_void(b, True),
             "void": lambda: mt_void(b, False), "ice": lambda: mt_ice(b), "snow": lambda: mt_snow(b),
             "water": lambda: mt_water(b), "rock_top": lambda: mt_rock(b, True), "rock": lambda: mt_rock(b, False),
             "log_l": lambda: mt_log(b, "l"), "log_m": lambda: mt_log(b, "m"), "log_r": lambda: mt_log(b, "r"),
             "log_1": lambda: mt_log(b, "1"), "log_v": lambda: mt_log_v(b), "thorn": lambda: mt_thorn(b),
             "hang": lambda: mt_hang(b), "pebble": lambda: mt_pebble(b), "sign": lambda: mt_sign(b)}
    return table[name]()


def build_playfield():
    im = Image.new("RGB", (16 * len(PF_TILES), 16 * len(PF_BIOMES)), MAG)
    for b in range(len(PF_BIOMES)):
        for i, n in enumerate(PF_TILES):
            im.paste(pf_tile(b, n).image(), (i * 16, b * 16))
    return im


# ---- mid-ground (BG3, parallax 1/2): one 128 x 48 band per biome, no outlines, low contrast ----------------------------
MID_W, MID_H = 128, 48
MID_PALS = [
    [(186, 204, 230), (160, 180, 214), (136, 158, 198), (226, 236, 248), (56, 90, 104), (78, 118, 128)],     # summit
    [(64, 110, 84), (46, 88, 70), (34, 68, 58), (90, 132, 98), (84, 64, 48), (26, 52, 46)],                  # forest
    [(96, 164, 88), (72, 136, 72), (58, 112, 64), (70, 84, 170), (46, 54, 130), (238, 238, 250),
     (18, 12, 30), (150, 196, 110), (116, 132, 220)],                                                          # meadow
    [(176, 96, 82), (138, 70, 64), (206, 186, 160), (170, 150, 130), (250, 214, 120), (104, 86, 90),
     (126, 146, 100), (96, 114, 80)],                                                                           # village
]


def mid_band(b, family=True):
    cv = Canvas(MID_W, MID_H)
    pal = MID_PALS[b]
    rng = random.Random(500 + b)
    if b == 0:      # snowy hills below the summit, a few frosted firs and rocks
        for x in range(MID_W):
            h = 24 + int(6 * math.sin(x * 2 * math.pi / 128) + 3 * math.sin(x * 2 * math.pi / 32 + 1))
            for y in range(MID_H - h, MID_H):
                d = y - (MID_H - h)
                cv.set(x, y, pal[3] if d < 2 else pal[0] if d < 9 else pal[1] if d < 16 else pal[2])
        for tx, th in ((14, 16), (22, 11), (58, 18), (66, 12), (100, 15), (108, 10)):
            base = MID_H - 24 - int(6 * math.sin(tx * 2 * math.pi / 128) + 3 * math.sin(tx * 2 * math.pi / 32 + 1)) + 3
            for y in range(base - th, base):
                w = (y - (base - th)) // 2
                for x in range(tx - w, tx + w + 1):
                    cv.set(x, y, pal[5] if x <= tx else pal[4])
                if (y - (base - th)) % 4 == 1:
                    cv.set(tx - w, y, pal[3])
            cv.set(tx, base - th, pal[3])
        for rx in (40, 86):
            for y in range(MID_H - 20, MID_H - 15):
                for x in range(rx - (y - (MID_H - 20)) - 1, rx + (y - (MID_H - 20)) + 2):
                    cv.set(x, y, pal[4])
            cv.set(rx, MID_H - 21, pal[3])
    elif b == 1:    # a pine wood: two rows of conifers
        for x in range(MID_W):
            for y in range(40, MID_H):
                cv.set(x, y, pal[5])
        for row, (base, col) in enumerate(((44, (pal[2], pal[1])), (46, (pal[1], pal[0])))):
            for tx in range(4 + row * 8, MID_W + 16, 16):
                h = 26 + (tx * 7 + row * 5) % 14
                top = base - h
                for y in range(top, base):
                    w = (y - top) // 2 + 1
                    for x in range(tx - w, tx + w + 1):
                        cv.set(x % MID_W, y, col[0] if x < tx else col[1])
                for y in range(base - 3, MID_H):
                    cv.set(tx % MID_W, y, pal[4])
    elif b == 2:    # blueberry bushes, and the berry's family sitting on them
        for x in range(MID_W):
            h = 20 + int(5 * math.sin(x * 2 * math.pi / 64) + 3 * math.sin(x * 2 * math.pi / 32))
            for y in range(MID_H - h, MID_H):
                d = y - (MID_H - h)
                cv.set(x, y, pal[7] if d < 2 else pal[0] if (x * 3 + y * 5) % 7 else pal[1])
        for _ in range(40):
            x, y = rng.randint(0, MID_W - 2), rng.randint(MID_H - 16, MID_H - 3)
            if cv.get(x, y) is not None:
                cv.set(x, y, pal[3]), cv.set(x + 1, y, pal[4])
        fx = 84 if family else 999     # the family: four berries of different sizes with little faces (the cameo)
        for i, r in enumerate((4.5, 4.0, 2.8, 2.4)):
            cx = fx + [0, 10, 18, 24][i]
            cy = MID_H - 24 + (4.5 - r)
            for y in range(int(cy - r) - 1, int(cy + r) + 2):
                for x in range(int(cx - r) - 1, int(cx + r) + 2):
                    if (x + 0.5 - cx) ** 2 + (y + 0.5 - cy) ** 2 <= r * r:
                        cv.set(x, y, pal[8] if x < cx - r * 0.3 and y < cy - r * 0.3 else pal[3])
            cv.set(int(cx), int(cy - 1), pal[5]), cv.set(int(cx + 2), int(cy - 1), pal[5])
            cv.set(int(cx) + 1, int(cy + 1), pal[6])
            cv.set(int(cx), int(cy - r), pal[4])
    else:           # the village: roofs, chimneys, lit windows, a steeple
        for x in range(MID_W):
            for y in range(38, MID_H):
                cv.set(x, y, pal[6] if y < 41 else pal[7])
        x, k = 0, 0
        while x < MID_W:
            w = 18 + (k * 7) % 10
            h = 14 + (k * 5) % 8
            base = 40
            for yy in range(base - h, base):
                for xx in range(x + 2, min(MID_W, x + w - 2)):
                    cv.set(xx, yy, pal[2] if xx < x + w // 2 else pal[3])
            for j in range(w // 2 + 2):
                for xx in range(x + j, min(MID_W, x + w - j)):
                    cv.set(xx, base - h - j, pal[0] if xx < x + w // 2 else pal[1])
            cv.set(x + w // 2 + 3, base - h - 4, pal[5]), cv.set(x + w // 2 + 3, base - h - 5, pal[5])
            for wx in range(x + 5, x + w - 5, 5):
                for dy in (4, 5):
                    cv.set(wx, base - h + dy, pal[4]), cv.set(wx + 1, base - h + dy, pal[4])
            x += w
            k += 1
        for y in range(4, 40):      # the steeple
            w = 1 if y < 14 else 3
            for xx in range(60 - w, 61 + w):
                cv.set(xx, y, pal[5] if y < 14 else pal[2])
    return cv


def build_mid():
    """Five bands: the four biomes, then the meadows again without the berry family (shown on most repeats)."""
    im = Image.new("RGB", (MID_W, MID_H * 5), MAG)
    for b in range(4):
        im.paste(mid_band(b).image(), (0, b * MID_H))
    im.paste(mid_band(2, family=False).image(), (0, 4 * MID_H))
    return im


# ---- far mountains (BG4, parallax 1/4): one shape, recoloured per biome and time of day at run time ------------------
FAR_W, FAR_H = 256, 64
FAR_PAL = [(150, 170, 206), (122, 142, 184), (98, 118, 162), (236, 244, 252), (190, 206, 230)]


def build_far():
    cv = Canvas(FAR_W, FAR_H)
    for x in range(FAR_W):
        h1 = 44 + int(12 * math.sin(x * 2 * math.pi / 256 + 0.5) + 6 * math.sin(x * 2 * math.pi / 64))
        h2 = 30 + int(8 * math.sin(x * 2 * math.pi / 128 + 2) + 4 * math.sin(x * 2 * math.pi / 32))
        for y in range(FAR_H - h1, FAR_H):
            d = y - (FAR_H - h1)
            cv.set(x, y, FAR_PAL[3] if d < 3 else FAR_PAL[4] if d < 5 else FAR_PAL[0])
        for y in range(FAR_H - h2, FAR_H):
            cv.set(x, y, FAR_PAL[1] if y - (FAR_H - h2) < 2 else FAR_PAL[2])
    return cv.image()


# ---- write everything ------------------------------------------------------------------------------------------------------
def write_all(out, preview=False):
    os.makedirs(os.path.join(out, "tiles"), exist_ok=True)
    files = {SHEETS["sprites"]: build_sheet(), SHEETS["playfield"]: build_playfield(), SHEETS["mid"]: build_mid(),
             SHEETS["far"]: build_far()}
    for f, im in files.items():
        im.save(os.path.join(out, f))
        if preview:
            os.makedirs(os.path.join(out, "preview"), exist_ok=True)
            im.resize((im.width * 4, im.height * 4), Image.NEAREST).save(os.path.join(out, "preview", os.path.basename(f)))
    pals = {"pf_roles": PF_ROLES, "pf": [[list(P(b)[r]) for r in PF_ROLES] for b in range(4)],
            "mid": [[list(c) for c in m] for m in MID_PALS], "far": [list(c) for c in FAR_PAL],
            "biome_names": BIOME_NAMES, "pf_tiles": PF_TILES}
    with open(os.path.join(out, "palettes.json"), "w") as f:
        json.dump(pals, f, indent=1)
    return sorted(files) + ["palettes.json"]


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", default=os.path.join(GAME, "art"))
    ap.add_argument("--preview", action="store_true", help="also write 4x previews in <out>/preview")
    a = ap.parse_args(argv)
    for f in write_all(a.out, a.preview):
        print("wrote %s/%s" % (a.out, f))


if __name__ == "__main__":
    main()

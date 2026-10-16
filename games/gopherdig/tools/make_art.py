#!/usr/bin/env python3
"""Gopher Dig: the code-drawn art, in the 8BCraft house style (docs/art-direction.md).

Code-drawn art only, no image generation: every sprite and tile is drawn here with games/common/tools/house_style.py
(outlines, 3-4-shade ramps lit from the top-left, squash and stretch). The layouts (SPRITES and the cell lists) are
shared with build_assets.py.

Writes, in the art folder (default games/gopherdig/art):
  sprites.png         the gopher in its yellow hard hat with the lamp (OBJ 0; players 2-4 recolour the hat), its
                      icon; the drill bits, air bubbles, dust, sparkles, stars (OBJ 2); the blocks' crumbs (OBJ 6)
  tiles/blocks.png    the four colours of block as quarter tiles (8x8): each quarter by whether it sticks to its
                      side neighbour, its top/bottom neighbour and the diagonal one (BG 1 red + yellow, BG 2 green +
                      blue): stuck blocks draw as one round jelly
  tiles/well.png      the stratum tiles, drawn in palette indices (stratum 0's colours here; BG 3 takes each
                      stratum's palette, line by line): hard rocks and their cracks, the dug-out back wall, the earth
                      around the well, the grass, the well's stone edges, pebbles, roots, burrows, side tunnels
  tiles/decor.png     the buried things of each stratum (BG 6, its own palette): worms and carrots, clay pots and
                      coins, crystals, lava and coals, fossils, gems and a treasure chest
  tiles/surface.png   the garden on top: grass tufts, flowers, a mushroom, clouds (BG 7)
  preview/strata.png  (--preview) the six strata side by side

    python3 games/gopherdig/tools/make_art.py [--out DIR] [--preview]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/gopherdig/LICENSE.
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
WHITE = (250, 250, 250)
INK = hs.HOUSE["ink"][0]

# ---- the sprite sheet --------------------------------------------------------------------------------------------------
SHEET_W, SHEET_H = 256, 96
# name, x, y, w, h, frames, palette group (hero: OBJ 0, players 2-4 = OBJ 1, 4, 5 recoloured; props: OBJ 2;
# crumbs: OBJ 6)
SPRITES = [
    ("idle", 0, 0, 24, 24, 2, "hero"),
    ("walk", 48, 0, 24, 24, 2, "hero"),
    ("drill", 96, 0, 24, 24, 2, "hero"),        # drilling sideways (facing right)
    ("dig", 144, 0, 24, 24, 2, "hero"),         # drilling down
    ("fall", 192, 0, 24, 24, 1, "hero"),
    ("climb", 216, 0, 24, 24, 1, "hero"),
    ("cheer", 0, 24, 24, 24, 2, "hero"),        # a checkpoint
    ("flat", 48, 24, 24, 24, 1, "hero"),        # crushed
    ("faint", 72, 24, 24, 24, 1, "hero"),       # out of air
    ("icon", 96, 24, 16, 16, 1, "hero"),        # the title's player slots, the results
    ("bit_side", 112, 24, 8, 8, 2, "props"),    # the drill bit, spinning (sideways: facing right)
    ("bit_down", 128, 24, 8, 8, 2, "props"),
    ("dust", 144, 24, 8, 8, 3, "props"),
    ("spark", 168, 24, 8, 8, 2, "props"),       # drill sparks on a rock
    ("star", 184, 24, 8, 8, 2, "props"),        # stars around a crushed gopher
    ("puff", 200, 24, 8, 8, 2, "props"),        # a breath bubble (low on air)
    ("air", 216, 24, 16, 16, 2, "props"),       # the air bubble in the soil (a block of its own)
    ("crumb", 0, 48, 8, 8, 10, "crumbs"),       # blocks breaking: 4 colours x 2 frames, then rock x 2
    ("pop", 80, 48, 16, 16, 2, "crumbs"),       # the flash of a popped block
]

# ---- colours ---------------------------------------------------------------------------------------------------------------
FUR = [(222, 168, 112), (196, 136, 82), (160, 104, 60), (118, 72, 42)]
CREAM = [(252, 234, 214), (232, 200, 170)]
NOSE = (240, 120, 140)
BLUSH = (250, 168, 168)
HAT = [(255, 236, 120), (250, 200, 50), (214, 150, 24)]
LAMP = (180, 236, 255)
OUT = (52, 26, 36)              # the gopher's outline: its darkest fur pushed towards violet

METAL = [(236, 240, 246), (178, 186, 200), (116, 122, 140), (64, 66, 82)]
SPARK = [(255, 244, 170), (255, 206, 80)]
BUBBLE = [(214, 248, 255), (140, 214, 244), (70, 150, 206)]
DUST = [(226, 200, 160), (176, 146, 108)]
STAR = (255, 222, 90)
PROP_OUT = (30, 24, 44)

# the blocks: light, mid-light, mid, dark (house ramps, bright: the playfield is the most colourful thing in the well)
BLOCK_RAMPS = [
    [(255, 184, 184), (250, 112, 112), (222, 58, 76), (156, 30, 62)],      # red (strawberry)
    [(255, 248, 176), (254, 222, 82), (234, 170, 32), (170, 104, 22)],     # yellow (lemon)
    [(206, 246, 146), (130, 212, 84), (64, 160, 64), (30, 100, 56)],       # green (lime)
    [(184, 224, 255), (104, 168, 244), (60, 104, 214), (38, 56, 146)],     # blue (berry)
]
BLOCK_OUT = (34, 18, 46)
SHINE = (255, 255, 255)

# the strata: one palette each, index-aligned (the same tiles drawn in each); 1..15:
#  1-4 earth (light .. deepest), 5-7 the dug-out back wall, 8-11 the hard rock (light .. dark/crack),
#  12-13 accents (grass; clay bands; crystal glints; lava glow; bone; gold), 14-15 the well's edge stones
STRATA_NAMES = ["garden soil", "red clay", "crystal cave", "warm rocks", "fossil beds", "gem core"]
STRATA_PALS = [
    [(184, 128, 78), (152, 100, 60), (118, 74, 44), (88, 52, 32),          # garden soil
     (102, 66, 44), (80, 50, 34), (58, 36, 26),
     (214, 208, 196), (172, 164, 152), (128, 120, 110), (76, 66, 60),
     (150, 210, 84), (78, 152, 54),
     (150, 120, 94), (84, 60, 44)],
    [(222, 134, 98), (190, 102, 74), (150, 72, 56), (112, 50, 44),         # red clay
     (116, 58, 48), (90, 44, 38), (64, 30, 28),
     (238, 200, 168), (204, 160, 128), (160, 118, 94), (98, 64, 54),
     (250, 190, 140), (176, 84, 60),
     (172, 112, 90), (96, 52, 44)],
    [(118, 118, 178), (90, 88, 148), (66, 62, 118), (46, 42, 90),          # crystal cave
     (54, 52, 98), (42, 40, 78), (30, 28, 58),
     (204, 214, 240), (152, 162, 208), (108, 114, 166), (64, 66, 112),
     (150, 250, 246), (80, 196, 220),
     (132, 128, 186), (52, 48, 98)],
    [(126, 80, 70), (100, 60, 54), (76, 44, 42), (56, 30, 32),             # warm rocks
     (74, 36, 32), (58, 28, 26), (40, 20, 20),
     (214, 170, 140), (172, 128, 104), (128, 90, 76), (78, 50, 46),
     (255, 186, 70), (236, 96, 44),
     (140, 96, 84), (64, 34, 34)],
    [(216, 196, 154), (186, 164, 122), (152, 130, 94), (120, 100, 70),     # fossil beds
     (128, 108, 82), (102, 84, 64), (76, 60, 46),
     (240, 234, 216), (204, 194, 170), (158, 146, 122), (104, 92, 76),
     (252, 248, 230), (206, 194, 162),
     (176, 156, 120), (104, 86, 62)],
    [(96, 66, 118), (74, 50, 96), (56, 36, 76), (40, 26, 56),              # gem core
     (52, 34, 70), (40, 26, 56), (28, 16, 40),
     (194, 166, 222), (150, 120, 184), (108, 82, 140), (66, 48, 92),
     (255, 222, 96), (222, 160, 40),
     (112, 82, 140), (48, 30, 66)],
]
E_HI, E_MID, E_DARK, E_DEEP, B_LIGHT, B_MID, B_DARK, R_HI, R_LIGHT, R_MID, R_DARK, ACC_A, ACC_B, ED_LIGHT, ED_DARK = range(1, 16)

# the buried things: each stratum's own palette (BG 6): 1-4 its earth (as above), 5-15 the things' colours
DECOR_PALS = [
    [(250, 176, 186), (226, 122, 142), (170, 76, 100),         # garden: the worm
     (255, 170, 66), (222, 112, 34),                           # the carrot
     (130, 196, 74), (74, 138, 50),                            # its leaves
     (186, 132, 82), (126, 82, 48),                            # the acorn
     (246, 238, 214), (44, 24, 30)],                           # the bone, ink
    [(226, 130, 86), (176, 86, 56), (118, 52, 40),             # clay: the pot
     (255, 226, 96), (214, 158, 36), (150, 100, 30),           # the coin, the key
     (244, 230, 200), (196, 168, 130), (132, 100, 72),         # the snail shell
     (250, 250, 250), (40, 22, 30)],
    [(180, 252, 248), (90, 214, 230), (44, 140, 180),          # crystal: cyan crystals
     (255, 186, 236), (220, 110, 196), (150, 60, 140),         # pink crystals
     (200, 255, 190), (110, 220, 140),                         # the glowing mushroom
     (236, 220, 255), (250, 250, 250), (30, 22, 50)],
    [(255, 236, 140), (255, 168, 60), (226, 82, 40), (150, 40, 34),     # warm rocks: lava, coal glow
     (60, 50, 60), (30, 26, 36),                               # obsidian
     (255, 140, 120), (220, 70, 90),                           # the fire opal
     (120, 86, 80), (250, 240, 220), (24, 14, 20)],
    [(250, 246, 230), (220, 210, 184), (172, 158, 128), (124, 110, 86),  # fossils: bone, stone
     (200, 170, 120), (150, 118, 80),                          # the ammonite
     (150, 190, 120), (96, 140, 84),                           # the leaf
     (120, 160, 190), (66, 46, 40), (36, 24, 22)],
    [(200, 236, 255), (100, 170, 255), (50, 90, 210),          # gem core: the diamond
     (255, 140, 160), (220, 40, 80), (140, 20, 60),            # the ruby
     (255, 232, 110), (230, 170, 40), (160, 100, 30),          # gold
     (156, 96, 54), (30, 18, 30)],                             # the chest's wood, ink
]
SURFACE_PAL = [(170, 226, 96), (110, 182, 64), (62, 132, 50), (36, 86, 40),      # grass
               (255, 150, 182), (230, 86, 130), (255, 236, 110), (250, 250, 250),  # flowers
               (236, 70, 66), (250, 236, 210), (196, 180, 150),                   # the mushroom
               (255, 255, 255), (226, 238, 250), (186, 210, 236)]                  # clouds

# the cell lists: (name, width) in tiles/well.png, tiles/surface.png (16 px tall each)
WELL_CELLS = [("hard%d" % i, 16) for i in range(5)] + [
    ("back0", 16), ("back1", 16), ("backsh0", 16), ("backsh1", 16),
    ("earth0", 16), ("earth1", 16), ("earth2", 16), ("earth3", 16), ("grass0", 16), ("grass1", 16),
    ("pebbles", 16), ("roots", 16), ("burrow", 16), ("crack", 16),
    ("tunnel", 16), ("tunnel_end_l", 16), ("tunnel_end_r", 16),
    ("edge_l0", 8), ("edge_l1", 8), ("edge_r0", 8), ("edge_r1", 8), ("edge_open_l", 8), ("edge_open_r", 8)]
DECOR_ITEMS = 4
SURFACE_CELLS = [("tuft", 16), ("flowers0", 16), ("flowers1", 16), ("mushroom", 16), ("cloud_l", 16), ("cloud_r", 16)]


# ---- helpers ------------------------------------------------------------------------------------------------------------------
def dist(x, y, cx, cy):
    return math.hypot(x + 0.5 - cx, y + 0.5 - cy)


def disc(cv, cx, cy, r, col):
    for y in range(int(cy - r) - 1, int(cy + r) + 2):
        for x in range(int(cx - r) - 1, int(cx + r) + 2):
            if dist(x, y, cx, cy) <= r:
                cv.set(x, y, col)


def line(cv, x0, y0, x1, y1, col):
    n = max(abs(x1 - x0), abs(y1 - y0), 1)
    for i in range(n + 1):
        cv.set(int(round(x0 + (x1 - x0) * i / n)), int(round(y0 + (y1 - y0) * i / n)), col)


def x_eyes(cv, x, y):
    for d in range(3):
        cv.set(x + d, y + d, INK)
        cv.set(x + 2 - d, y + d, INK)


# ---- the gopher (24x24: its cell is x 4..19, y 8..23, the feet on y 23) --------------------------------------------------------
def hard_hat(cv, cx, top, brim_y, w):
    """The yellow hard hat: a dome lit from the top-left, a darker brim, the lamp at the front (right)."""
    for y in range(24):
        for x in range(24):
            dx, dy = (x + 0.5 - cx) / w, (y + 0.5 - brim_y) / (brim_y - top)
            if dx * dx + dy * dy <= 1.0 and y + 0.5 <= brim_y:
                nd = dx * hs.LIGHT_DIR[0] + dy * hs.LIGHT_DIR[1]
                cv.set(x, y, HAT[0] if nd > 0.45 else HAT[1] if nd > -0.35 else HAT[2])
    by = int(brim_y)
    for x in range(int(cx - w - 0.5), int(cx + w + 1.5)):
        cv.set(x, by, HAT[2])
    cv.set(int(cx - w * 0.45), int(top + 1), WHITE)                  # the shine
    lx, ly = int(cx + w * 0.62), int(top + 1.4)
    cv.rect(lx, ly, lx + 1, ly + 1, LAMP)
    cv.set(lx + 1, ly, WHITE)


def gopher(pose="idle", frame=0):
    """The gopher facing right, 24x24 (its cell: x 4..19, y 8..23; the feet on y 23)."""
    cv = Canvas(24, 24)
    bob = 1 if (pose == "idle" and frame == 1) or (pose == "walk" and frame == 1) else 0
    lean = 1 if pose == "drill" else 0
    hx, hy = 13.4 + lean, 12.8 + bob                 # the head's centre
    if pose == "dig":
        hx, hy = 12.8, 13.8
    # feet (behind)
    if pose == "walk":
        feet = [(8.0 + 3 * frame, 22.5), (15.0 - 3 * frame, 22.5)]
    elif pose == "fall":
        feet = [(8.5, 21.6), (15.0, 22.2)]
    else:
        feet = [(8.5, 22.5), (14.5, 22.5)]
    for fx, fy in feet:
        hs.ellipse(cv, fx, fy, 2.5, 1.5, [FUR[2], FUR[2], FUR[3], FUR[3]])
    # the tail
    hs.ellipse(cv, 4.6, 19.5 + bob, 1.7, 1.3, FUR)
    # the body, the head: one round gopher
    hs.ellipse(cv, 10.8 + lean * 0.5, 18.0 + bob * 0.5, 6.3, 5.4, FUR)
    hs.ellipse(cv, hx, hy, 5.4, 4.8, FUR)
    # the belly
    hs.ellipse(cv, 11.8 + lean, 19.4 + bob * 0.5, 3.6, 3.2, [CREAM[0], CREAM[0], CREAM[1], CREAM[1]])
    # the snout, poking out in front, the nose at its tip, the buck teeth under it
    sx, sy = hx + 4.6, hy + 1.6
    if pose == "dig":
        sx, sy = hx + 3.0, hy + 2.6
    hs.ellipse(cv, sx, sy, 2.8, 2.2, [CREAM[0], CREAM[0], CREAM[1], CREAM[1]])
    nx, ny = int(sx + 2.0), int(sy - 1.6)
    cv.set(nx, ny, NOSE), cv.set(nx + 1, ny, NOSE), cv.set(nx, ny + 1, NOSE)
    if pose != "flat":
        tx, ty = int(sx - 0.4), int(sy + 2.2)
        cv.set(tx, ty, WHITE), cv.set(tx + 1, ty, WHITE)
    cv.set(int(hx - 0.5), int(hy + 2.4), BLUSH)
    # the eye, under the brim
    ex, ey = int(hx + 0.6), int(hy - 1.8)
    if pose in ("flat", "faint"):
        x_eyes(cv, ex, ey)
    elif pose == "cheer":
        cv.set(ex, ey + 2, INK), cv.set(ex + 1, ey + 1, INK), cv.set(ex + 2, ey + 2, INK)    # a happy ^
    else:
        hs.eye(cv, ex, ey, look=(0, 1) if pose == "dig" else (1, 0), big=True)
    # the paws
    if pose in ("fall", "cheer", "climb"):
        up = 4 if (pose != "cheer" or frame == 0) else 2
        hs.ellipse(cv, 6.4, 13.0 - up, 1.6, 1.8, FUR)
        hs.ellipse(cv, 18.6 + (1 if pose == "climb" else 0), 12.0 - up, 1.6, 1.8, FUR)
    elif pose == "drill":
        cv.rect(19, 17, 23, 18, METAL[2])
        cv.rect(19, 17, 23, 17, METAL[1])
        hs.ellipse(cv, 18.0, 17.8, 1.9, 1.6, FUR)          # both paws on the drill's handle
    elif pose == "dig":
        cv.rect(11, 21, 13, 23, METAL[2])
        cv.rect(11, 21, 11, 23, METAL[1])
        hs.ellipse(cv, 10.0, 20.4, 1.7, 1.5, FUR)
        hs.ellipse(cv, 14.4, 20.4, 1.7, 1.5, FUR)
    else:
        hs.ellipse(cv, 15.8 + lean, 18.6 + bob * 0.5, 1.6, 1.5, FUR)
    # the hard hat
    hard_hat(cv, hx - 0.4, hy - 7.4, hy - 2.2, 6.0)
    if pose == "flat":
        cv = hs.squash(cv, 1.6)
    elif pose == "fall":
        cv = hs.squash(cv, 0.88)
    elif pose == "climb":
        cv = hs.squash(cv, 0.9)
    elif pose == "cheer" and frame == 1:
        cv = hs.squash(cv, 1.12)
    elif pose == "dig" and frame == 1:
        cv = hs.squash(cv, 1.08)
    cv.outline(OUT)
    return cv


def gopher_icon():
    """The 16x16 icon (the title's player slots, the results): the head in its hard hat."""
    cv = Canvas(16, 16)
    hs.ellipse(cv, 8.0, 9.5, 5.8, 5.2, FUR)
    hs.ellipse(cv, 10.6, 11.4, 2.6, 2.0, [CREAM[0], CREAM[0], CREAM[1], CREAM[1]])
    cv.set(13, 10, NOSE)
    cv.set(10, 13, WHITE), cv.set(11, 13, WHITE)
    hs.eye(cv, 8, 7, look=(1, 0))
    for y in range(16):
        for x in range(16):
            dx, dy = (x + 0.5 - 8.0) / 6.2, (y + 0.5 - 6.2) / 4.4
            if dx * dx + dy * dy <= 1.0 and y + 0.5 <= 6.2:
                nd = dx * hs.LIGHT_DIR[0] + dy * hs.LIGHT_DIR[1]
                cv.set(x, y, HAT[0] if nd > 0.5 else HAT[1] if nd > -0.3 else HAT[2])
    for x in range(2, 15):
        cv.set(x, 6, HAT[2])
    cv.rect(12, 3, 13, 4, LAMP)
    cv.set(12, 3, WHITE)
    cv.set(5, 2, WHITE)
    cv.outline(OUT)
    return cv


# ---- props -------------------------------------------------------------------------------------------------------------------
def drill_bit(down, frame):
    """The drill's cone, 8x8, pointing right (or down); two spin frames (the thread moves)."""
    cv = Canvas(8, 8)
    for i in range(8):
        h = 3.6 * (1 - i / 8.0)
        for j in range(8):
            d = abs(j + 0.5 - 4.0)
            if d <= h:
                c = METAL[0] if j < 3 else METAL[1] if j < 5 else METAL[2]
                if (i + j + frame * 2) % 4 == 0:
                    c = METAL[2] if c != METAL[2] else METAL[3]
                if down:
                    cv.set(j, i, c)
                else:
                    cv.set(i, j, c)
    cv.outline(PROP_OUT)
    return cv


def dust(frame):
    cv = Canvas(8, 8)
    r = [1.8, 2.8, 3.5][frame]
    for y in range(8):
        for x in range(8):
            d = dist(x, y, 4, 4)
            if d < r and (frame == 0 or hs.checker(x, y, frame)):
                cv.set(x, y, DUST[0] if d < r - 1.1 else DUST[1])
    return cv


def spark(frame):
    cv = Canvas(8, 8)
    pts = [(1, 2), (3, 0), (6, 1), (7, 4), (5, 6)] if frame == 0 else [(0, 4), (2, 6), (4, 2), (6, 5), (7, 1)]
    for i, (x, y) in enumerate(pts):
        cv.set(x, y, SPARK[0] if i % 2 else SPARK[1])
    cv.set(3, 3, WHITE)
    cv.set(4, 4, SPARK[0])
    return cv


def star(frame):
    cv = Canvas(8, 8)
    pts = [(3, 0), (4, 0), (3, 1), (4, 1), (0, 3), (1, 3), (2, 3), (5, 3), (6, 3), (7, 3), (0, 4), (1, 4), (2, 4),
           (5, 4), (6, 4), (7, 4), (3, 2), (4, 2), (3, 5), (4, 5), (2, 6), (5, 6), (1, 7), (6, 7), (3, 3), (4, 3),
           (3, 4), (4, 4)]
    for x, y in pts:
        cv.set(x, y, STAR if frame == 0 or (x + y) % 3 else SPARK[0])
    cv.set(3, 3, WHITE)
    cv.outline(PROP_OUT)
    return cv


def puff(frame):
    cv = Canvas(8, 8)
    r = 2.2 if frame == 0 else 3.0
    for y in range(8):
        for x in range(8):
            d = dist(x, y, 4, 4)
            if r - 1.0 <= d <= r:
                cv.set(x, y, BUBBLE[1])
    cv.set(3, 2 if frame == 0 else 2, WHITE)
    return cv


def air_bubble(frame):
    """The air bubble in the soil: a big shiny bubble with a little wobble (two frames)."""
    cv = Canvas(16, 16)
    rx, ry = (6.8, 6.4) if frame == 0 else (6.4, 6.8)
    for y in range(16):
        for x in range(16):
            dx, dy = (x + 0.5 - 8) / rx, (y + 0.5 - 8) / ry
            d = math.sqrt(dx * dx + dy * dy)
            if d <= 1.0:
                if d > 0.78:
                    cv.set(x, y, BUBBLE[2] if (dx + dy) > 0.3 else BUBBLE[1])
                elif d > 0.55:
                    cv.set(x, y, BUBBLE[1] if (dx + dy) > 0.2 else BUBBLE[0])
                else:
                    cv.set(x, y, BUBBLE[0])
    cv.rect(4, 4, 5, 5, WHITE)
    cv.set(6, 3, WHITE)
    cv.set(10, 11, WHITE)
    # a tiny "O2"-free sparkle: a cross of light
    cv.set(11, 4, WHITE), cv.set(10, 4, BUBBLE[0]), cv.set(12, 4, BUBBLE[0]), cv.set(11, 3, BUBBLE[0]), cv.set(11, 5, BUBBLE[0])
    cv.outline(PROP_OUT)
    return cv


CRUMB_RAMPS = [BLOCK_RAMPS[0], BLOCK_RAMPS[1], BLOCK_RAMPS[2], BLOCK_RAMPS[3],
               [(214, 208, 196), (172, 164, 152), (128, 120, 110), (76, 66, 60)]]


def crumb(i):
    """A block's crumbs flying apart: colour i // 2 (the last: rock), frame i % 2."""
    r4 = CRUMB_RAMPS[i // 2]
    cv = Canvas(8, 8)
    if i % 2 == 0:
        cv.rect(1, 1, 3, 3, r4[1]), cv.set(1, 1, r4[0]), cv.rect(5, 4, 6, 5, r4[2]), cv.set(2, 6, r4[1])
    else:
        cv.rect(2, 2, 3, 3, r4[1]), cv.set(2, 2, r4[0]), cv.set(6, 5, r4[2]), cv.set(5, 1, r4[1])
    cv.outline(BLOCK_OUT)
    return cv


def pop(frame):
    """The flash of a block popping: a ring of light, then sparkles."""
    cv = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            d = dist(x, y, 8, 8)
            if frame == 0 and 3.0 <= d <= 5.5:
                cv.set(x, y, SHINE if d < 4.5 else BLOCK_RAMPS[1][0])
    if frame == 1:                                  # then four little sparkles flying out
        for cx, cy in ((3, 3), (12, 3), (3, 12), (12, 12)):
            cv.set(cx, cy, SHINE)
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                cv.set(cx + dx, cy + dy, BLOCK_RAMPS[1][0])
    return cv


def sprite_frame(name, i):
    if name == "icon":
        return gopher_icon()
    if name in ("idle", "walk", "drill", "dig", "fall", "climb", "cheer", "flat", "faint"):
        return gopher(name, i)
    return {"bit_side": lambda: drill_bit(False, i), "bit_down": lambda: drill_bit(True, i), "dust": lambda: dust(i),
            "spark": lambda: spark(i), "star": lambda: star(i), "puff": lambda: puff(i), "air": lambda: air_bubble(i),
            "crumb": lambda: crumb(i), "pop": lambda: pop(i)}[name]()


# ---- the blocks: quarter tiles ------------------------------------------------------------------------------------------------
def block(r4, L, R, U, D, UL, UR, DL, DR):
    """A 16x16 colour block; L..DR: it sticks to that neighbour (the same colour). A stuck side has no rim, so stuck
    blocks read as one jelly; a free corner is round; a stuck corner whose diagonal is free gets a little notch."""
    cv = Canvas(16, 16)
    hi, light, mid, dark = r4
    corners = [(0, 0, L, U, UL), (15, 0, R, U, UR), (0, 15, L, D, DL), (15, 15, R, D, DR)]
    for y in range(16):
        for x in range(16):
            inside, rim = True, False
            for cx, cy, h, v, d in corners:
                near_x = x if cx == 0 else 15 - x
                near_y = y if cy == 0 else 15 - y
                if not h and not v and near_x < 3 and near_y < 3:
                    dd = math.hypot(3 - near_x - 0.5, 3 - near_y - 0.5)
                    if dd > 3.2:
                        inside = False
                    elif dd > 2.2:
                        rim = True
            if not inside:
                continue
            if (x == 0 and not L) or (x == 15 and not R) or (y == 0 and not U) or (y == 15 and not D):
                rim = True
            c = mid
            if not rim:
                if (y <= 2 and not U) or (x <= 1 and not L):
                    c = light
                if (y >= 13 and not D) or (x >= 14 and not R):
                    c = dark
                if y == 1 and not U and 3 <= x <= 12:
                    c = hi
            cv.set(x, y, BLOCK_OUT if rim else c)
    # inner corners: stuck both ways, but not to the diagonal
    for cx, cy, h, v, d in corners:
        if h and v and not d:
            sx, sy = (1 if cx == 0 else -1), (1 if cy == 0 else -1)
            cv.set(cx, cy, BLOCK_OUT)
            cv.set(cx + sx, cy, dark)
            cv.set(cx, cy + sy, dark)
    # the cute shine (top-left of every block)
    cv.set(3, 3, SHINE), cv.set(4, 3, SHINE), cv.set(3, 4, SHINE), cv.set(6, 3, hi)
    return cv


def block_quarters():
    """tiles/blocks.png: for each colour a row of 32 tiles: quarter q (TL, TR, BL, BR) x state s (side stuck + 2 x
    top/bottom stuck + 4 x diagonal stuck)."""
    im = Image.new("RGB", (256, 32), hs.MAGENTA)
    for k, r4 in enumerate(BLOCK_RAMPS):
        for q in range(4):
            for s in range(8):
                h, v, d = s & 1, (s >> 1) & 1, (s >> 2) & 1
                fl = dict(L=0, R=0, U=0, D=0, UL=0, UR=0, DL=0, DR=0)
                side = "L" if q in (0, 2) else "R"
                ud = "U" if q in (0, 1) else "D"
                fl[side], fl[ud], fl[ud + side] = h, v, d and h and v
                cv = block(r4, **fl)
                ox, oy = (0 if side == "L" else 8), (0 if ud == "U" else 8)
                sub = Canvas(8, 8)
                for y in range(8):
                    for x in range(8):
                        sub.set(x, y, cv.get(ox + x, oy + y))
                im.paste(sub.image(), ((q * 8 + s) * 8, k * 8))
    return im


# ---- the stratum tiles (drawn in stratum 0's colours, converted to indices) --------------------------------------------------
P0 = STRATA_PALS[0]


def C(i):
    """Stratum palette index (1..15) -> stratum 0's colour (the tiles are drawn in it, then read back as indices)."""
    return P0[i - 1]


def speckle(cv, seed, x0, w, base, dots):
    rng = random.Random(seed)
    for y in range(16):
        for x in range(x0, x0 + w):
            cv.set(x, y, C(base))
    for col, n in dots:
        for _ in range(n):
            x, y = rng.randrange(x0, x0 + w), rng.randrange(16)
            cv.set(x, y, C(col))
            if rng.random() < 0.4:
                cv.set(min(x0 + w - 1, x + 1), y, C(col))


def earth(v):
    cv = Canvas(16, 16)
    speckle(cv, 10 + v, 0, 16, E_MID, [(E_HI, 7), (E_DARK, 8), (E_DEEP, 3)])
    return cv


def back(v, shadow):
    cv = Canvas(16, 16)
    speckle(cv, 30 + v, 0, 16, B_MID, [(B_LIGHT, 4), (B_DARK, 6)])
    if shadow:
        for y in range(3):
            for x in range(16):
                if y < 2 or (x + v) % 3:
                    cv.set(x, y, C(B_DARK))
    return cv


def hard(hits):
    """A hard rock: a round boulder with facets; more cracks with each drill."""
    cv = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            dx, dy = (x + 0.5 - 8) / 7.9, (y + 0.5 - 8) / 7.9
            d = max(abs(dx), abs(dy)) * 0.55 + math.hypot(dx, dy) * 0.5
            if d > 1.0:
                continue
            if d > 0.9:
                cv.set(x, y, C(R_DARK))
                continue
            nd = dx * hs.LIGHT_DIR[0] + dy * hs.LIGHT_DIR[1]
            cv.set(x, y, C(R_HI) if nd > 0.55 else C(R_LIGHT) if nd > 0.0 else C(R_MID))
    for x, y in ((5, 4), (6, 4), (4, 5), (10, 10), (11, 9)):
        cv.set(x, y, C(R_HI))
    cracks = [[(8, 2), (7, 5), (9, 7)], [(9, 7), (12, 8), (13, 11)], [(9, 7), (6, 10), (4, 12)],
              [(7, 5), (4, 7)]]
    for k in range(min(hits, 4)):
        pts = cracks[k]
        for a, b in zip(pts, pts[1:]):
            line(cv, a[0], a[1], b[0], b[1], C(R_DARK))
    return cv


def grass(v):
    cv = earth(2 + v)
    rng = random.Random(70 + v)
    for x in range(16):
        top = 3 + (1 if (x + v) % 5 == 0 else 0)
        for y in range(top + 1):
            cv.set(x, y, C(ACC_A) if y < top else C(ACC_B))
        if rng.random() < 0.35:
            cv.set(x, top + 1, C(ACC_B))
    return cv


def pebbles():
    cv = earth(1)
    for cx, cy, r in ((5, 6, 2.2), (10, 10, 2.6), (12, 4, 1.4)):
        for y in range(16):
            for x in range(16):
                d = dist(x, y, cx, cy)
                if d <= r:
                    nd = ((x + 0.5 - cx) * hs.LIGHT_DIR[0] + (y + 0.5 - cy) * hs.LIGHT_DIR[1]) / r
                    cv.set(x, y, C(R_DARK) if d > r - 0.7 else C(R_HI) if nd > 0.4 else C(R_LIGHT) if nd > -0.3 else C(R_MID))
    return cv


def roots():
    cv = earth(3)
    for sx, wob in ((4, 1.0), (11, -1.3)):
        x = float(sx)
        for y in range(16):
            x += wob * 0.25 * math.sin(y / 2.5)
            cv.set(int(x), y, C(E_DEEP))
            if y % 5 == 2:
                cv.set(int(x) + (1 if wob > 0 else -1), y + 1, C(E_DEEP))
            if y < 15:
                cv.set(int(x) + 1, y, C(E_DARK))
    return cv


def burrow():
    cv = earth(0)
    for y in range(16):
        for x in range(16):
            dx, dy = (x + 0.5 - 8) / 6.0, (y + 0.5 - 9) / 4.2
            d = math.hypot(dx, dy)
            if d <= 1.0:
                cv.set(x, y, C(B_DARK) if dy < -0.2 or d > 0.85 else C(B_MID))
            elif d <= 1.2 and dy < 0:
                cv.set(x, y, C(E_DEEP))
    return cv


def crack():
    cv = earth(2)
    for a, b in (((2, 3), (7, 7)), ((7, 7), (6, 12)), ((7, 7), (13, 9)), ((13, 9), (14, 14))):
        line(cv, a[0], a[1], b[0], b[1], C(E_DEEP))
    return cv


def tunnel(end=None):
    """A side tunnel through the earth (rows 3..12 dug out); end 'l' or 'r': its round far end on that side."""
    cv = earth(1)
    for y in range(16):
        for x in range(16):
            if end == "l":
                inside = 3 <= y <= 12 and math.hypot(max(0, 8 - x - 0.5) / 7.0, (y + 0.5 - 8) / 5.0) <= 1.0
            elif end == "r":
                inside = 3 <= y <= 12 and math.hypot(max(0, x + 0.5 - 8) / 7.0, (y + 0.5 - 8) / 5.0) <= 1.0
            else:
                inside = 3 <= y <= 12
            if inside:
                cv.set(x, y, C(B_DARK) if y <= 4 else C(B_MID) if (x * 7 + y * 3) % 11 else C(B_LIGHT))
    for x in range(16):
        for y in (2, 13):
            if cv.get(x, y + (1 if y == 2 else -1)) in (C(B_DARK), C(B_MID), C(B_LIGHT)):
                cv.set(x, y, C(E_DEEP))
    return cv


def edge(side, v, opened=False):
    """The well's stone lining, one tile wide: side 'l' (left of the well) or 'r'; opened: a tunnel goes through."""
    cv = Canvas(8, 16)
    rng = random.Random(90 + v + (5 if side == "r" else 0))
    for y in range(16):
        for x in range(8):
            cv.set(x, y, C(E_DARK))
    y = -((v * 5) % 6)
    while y < 16:
        h = 5 + rng.randrange(3)
        for yy in range(max(0, y), min(16, y + h - 1)):
            for xx in range(1, 7):
                light = (yy - y) < 2 or xx < 3
                cv.set(xx, yy, C(ED_LIGHT) if light else C(ED_DARK))
        y += h
    # the dark rim against the well
    for yy in range(16):
        cv.set(7 if side == "l" else 0, yy, C(E_DEEP))
    if opened:
        for yy in range(3, 13):
            for xx in range(8):
                cv.set(xx, yy, C(B_DARK) if yy <= 4 else C(B_MID))
        for xx in range(8):
            cv.set(xx, 2, C(E_DEEP)), cv.set(xx, 13, C(E_DEEP))
    return cv


def well_cell(name):
    if name.startswith("hard"):
        return hard(int(name[4:]))
    if name.startswith("backsh"):
        return back(int(name[6:]), True)
    if name.startswith("back"):
        return back(int(name[4:]), False)
    if name.startswith("earth"):
        return earth(int(name[5:]))
    if name.startswith("grass"):
        return grass(int(name[5:]))
    if name.startswith("edge_open"):
        return edge(name[-1], 0, True)
    if name.startswith("edge"):
        return edge(name[5], int(name[6]))
    if name == "tunnel":
        return tunnel()
    if name.startswith("tunnel_end"):
        return tunnel(name[-1])
    return {"pebbles": pebbles, "roots": roots, "burrow": burrow, "crack": crack}[name]()


# ---- the buried things (decor.png: one row per stratum, 4 things; their background is the stratum's earth) -----------------
def decor_canvas(s, v):
    """The earth under a thing, in the decor palette's first 4 colours (= the stratum's earth)."""
    cv = Canvas(16, 16)
    pal = STRATA_PALS[s]
    rng = random.Random(400 + v)
    for y in range(16):
        for x in range(16):
            cv.set(x, y, pal[1])
    for col, n in ((0, 6), (2, 7), (3, 3)):
        for _ in range(n):
            cv.set(rng.randrange(16), rng.randrange(16), pal[col])
    return cv


def decor_item(s, i):
    cv = Canvas(16, 16)
    P = DECOR_PALS[s]
    E = STRATA_PALS[s]

    def sh(cx, cy, rx, ry, r3, out=None):
        """a shaded ellipse in 3 colours + its outline"""
        r4 = [r3[0], r3[0], r3[1], r3[2]]
        hs.ellipse(cv, cx, cy, rx, ry, r4)

    if s == 0:
        if i == 0:      # a worm wriggling
            for k in range(9):
                x = 3 + k * 1.2
                y = 9 + 2.2 * math.sin(k * 0.9)
                disc(cv, x, y, 1.6, P[1] if k % 3 else P[2])
            disc(cv, 13.5, 9 + 2.2 * math.sin(8 * 0.9), 2.0, P[0])
            cv.set(14, int(8 + 2.2 * math.sin(7.2)), P[10])
        elif i == 1:    # a carrot
            for y in range(5, 15):
                w = (15 - y) * 0.32
                for x in range(16):
                    if abs(x + 0.5 - 8) <= w:
                        cv.set(x, y, P[3] if x + 0.5 < 8 else P[4])
            for y in (7, 10, 12):
                cv.set(int(8 - (15 - y) * 0.25), y, P[4])
            for dx in (-2, 0, 2):
                line(cv, 8, 5, 8 + dx, 1, P[5] if dx else P[6])
        elif i == 2:    # an acorn stash
            for k, (cx, cy) in enumerate(((5, 10), (10, 11), (8, 6))):
                sh(cx, cy + 1, 2.4, 2.6, [P[7], P[7], P[8]])
                for x in range(int(cx - 2.6), int(cx + 3)):
                    cv.set(x, int(cy - 1), P[8])
                    cv.set(x, int(cy - 2), P[8])
                cv.set(int(cx), int(cy - 3), P[8])
        else:           # a bone
            line(cv, 4, 11, 12, 5, P[9])
            line(cv, 4, 12, 12, 6, P[9])
            for cx, cy in ((3, 11), (4, 13), (12, 4), (13, 6)):
                disc(cv, cx, cy, 1.4, P[9])
    elif s == 1:
        if i == 0:      # a little clay pot
            for y in range(4, 15):
                w = 5.2 * math.sin((y - 3) / 12.0 * math.pi) + 1.0
                for x in range(16):
                    if abs(x + 0.5 - 8) <= w:
                        cv.set(x, y, P[0] if x + 0.5 < 7 else P[1] if x + 0.5 < 10 else P[2])
            for x in range(5, 11):
                cv.set(x, 3, P[2]), cv.set(x, 8, P[2])
        elif i == 1:    # a gold coin
            sh(8, 8, 5.0, 5.0, [P[3], P[4], P[5]])
            for y in range(5, 11):
                cv.set(8, y, P[5])
            cv.set(6, 5, P[9])
        elif i == 2:    # an old key
            disc(cv, 5, 8, 3.0, P[4])
            disc(cv, 5, 8, 1.2, E[1])
            line(cv, 8, 8, 14, 8, P[4])
            line(cv, 8, 7, 14, 7, P[3])
            cv.set(12, 9, P[5]), cv.set(14, 9, P[5]), cv.set(12, 10, P[5])
        else:           # a snail shell
            sh(8, 9, 5.5, 5.0, [P[6], P[7], P[8]])
            for k in range(26):
                a = k * 0.45
                r = 0.2 * k
                cv.set(int(8 + r * math.cos(a)), int(9 + r * math.sin(a)), P[8])
    elif s == 2:
        if i in (0, 1):  # crystal clusters (cyan, pink)
            r3 = [P[0], P[1], P[2]] if i == 0 else [P[3], P[4], P[5]]
            for bx, h, w in ((5, 9, 2), (8, 12, 3), (11, 8, 2)):
                for y in range(15 - h, 15):
                    tip = (y - (15 - h)) < w
                    half = w * (y - (15 - h)) / float(w) if tip else w
                    for x in range(16):
                        if abs(x + 0.5 - bx) <= half:
                            cv.set(x, y, r3[0] if x + 0.5 < bx else r3[1] if x + 0.5 < bx + w * 0.6 else r3[2])
            cv.set(7, 5, P[9])
        elif i == 2:    # a glowing mushroom
            for y in range(9, 15):
                cv.set(7, y, P[8]), cv.set(8, y, P[8])
            sh(8, 8, 5.5, 3.6, [P[6], P[6], P[7]])
            for x in range(3, 14):
                if cv.get(x, 9) in (P[6], P[7]):
                    cv.set(x, 10, P[7])
            cv.set(6, 7, P[9]), cv.set(10, 6, P[9])
        else:           # a geode
            sh(8, 8, 6.0, 5.4, [E[0], E[1], E[2]])
            sh(8, 8.5, 3.6, 3.0, [P[8], P[4], P[5]])
            cv.set(7, 7, P[9]), cv.set(9, 9, P[3])
    elif s == 3:
        if i == 0:      # a lava blob
            sh(8, 9, 6.0, 4.6, [P[0], P[1], P[2]])
            for x in range(3, 14):
                if (x * 5) % 7 < 3:
                    cv.set(x, 12, P[3])
            cv.set(6, 7, P[0]), cv.set(7, 6, P[0])
        elif i == 1:    # a glowing coal
            sh(8, 8, 5.0, 4.6, [P[4], P[5], P[5]])
            for a, b in (((5, 7), (8, 9)), ((8, 9), (11, 7)), ((8, 9), (8, 12))):
                line(cv, a[0], a[1], b[0], b[1], P[1])
            cv.set(8, 9, P[0])
        elif i == 2:    # a fire opal
            for y in range(16):
                for x in range(16):
                    d = abs(x + 0.5 - 8) + abs(y + 0.5 - 8)
                    if d <= 5.5:
                        cv.set(x, y, P[6] if (x + 0.5 < 8 and y + 0.5 < 8) else P[7] if d > 3 else P[0])
            cv.set(6, 6, P[9])
        else:           # an obsidian shard
            for y in range(3, 14):
                w = (y - 3) * 0.4
                for x in range(16):
                    if abs(x + 0.5 - 8 - (y - 8) * 0.2) <= w:
                        cv.set(x, y, P[4] if x + 0.5 < 8 else P[5])
            line(cv, 7, 6, 6, 11, P[8])
    elif s == 4:
        if i == 0:      # an ammonite
            sh(8, 8, 6.0, 6.0, [P[4], P[4], P[5]])
            for k in range(36):
                a = k * 0.42
                r = 0.16 * k
                cv.set(int(8 + r * math.cos(a)), int(8 + r * math.sin(a)), P[5])
            cv.set(5, 4, P[0])
        elif i == 1:    # a fish skeleton
            line(cv, 3, 8, 12, 8, P[0])
            for x in range(5, 12, 2):
                line(cv, x, 5, x, 11, P[1])
            disc(cv, 12.5, 8, 2.2, P[0])
            cv.set(13, 7, P[10])
            line(cv, 3, 8, 1, 5, P[1]), line(cv, 3, 8, 1, 11, P[1])
        elif i == 2:    # a leaf fossil
            for y in range(16):
                for x in range(16):
                    dx, dy = (x + 0.5 - 8) / 3.6, (y + 0.5 - 8) / 6.4
                    if dx * dx + dy * dy <= 1:
                        cv.set(x, y, P[6] if x + 0.5 < 8 else P[7])
            line(cv, 8, 2, 8, 14, P[2])
            for y in (5, 8, 11):
                line(cv, 8, y, 5, y - 2, P[2]), line(cv, 8, y, 11, y - 2, P[2])
        else:           # a little dino skull
            sh(7, 8, 5.4, 4.0, [P[0], P[1], P[2]])
            sh(12, 9.5, 3.2, 2.2, [P[0], P[1], P[2]])
            disc(cv, 6, 7, 1.5, P[10])
            for x in (10, 12, 14):
                cv.set(x, 12, P[0])
    else:
        if i == 0:      # a big blue gem
            for y in range(3, 14):
                for x in range(16):
                    top = y < 6
                    w = 3 + (y - 3) if top else 6 - (y - 6) * 0.75
                    if abs(x + 0.5 - 8) <= w:
                        cv.set(x, y, P[0] if (top and x < 8) else P[1] if top else P[2] if x >= 8 else P[1])
            line(cv, 3, 6, 13, 6, P[2])
            cv.set(6, 4, P[0])
        elif i == 1:    # a ruby
            sh(8, 8, 4.6, 4.6, [P[3], P[4], P[5]])
            cv.set(6, 6, P[6])
        elif i == 2:    # gold nuggets
            for cx, cy, r in ((5, 10, 2.6), (10, 9, 3.2), (8, 5, 2.0)):
                sh(cx, cy, r, r * 0.85, [P[6], P[7], P[8]])
            cv.set(9, 7, P[6])
        else:           # a treasure chest
            cv.rect(2, 7, 13, 13, P[9])
            for y in range(3, 7):
                for x in range(2, 14):
                    if math.hypot((x + 0.5 - 8) / 6.0, (y + 0.5 - 7) / 4.0) <= 1.0:
                        cv.set(x, y, P[9])
            for x in range(2, 14):
                cv.set(x, 7, P[7]), cv.set(x, 13, P[8])
            for y in range(3, 14):
                cv.set(5, y, P[7]) if cv.get(5, y) == P[9] else None
                cv.set(10, y, P[7]) if cv.get(10, y) == P[9] else None
            cv.rect(7, 8, 8, 9, P[6])
    cv.outline(P[10])
    bg = decor_canvas(s, s * 4 + i)
    bg.paste(cv, 0, 0)
    return bg


# ---- the surface (above the ground: grass, flowers, clouds over the sky) -------------------------------------------------
def surface_cell(name):
    G = SURFACE_PAL
    cv = Canvas(16, 16)
    if name == "tuft" or name.startswith("flowers"):
        rng = random.Random(len(name) * 7 + (1 if name == "flowers1" else 0))
        for k in range(9):
            x = 1 + k * 1.7 + rng.random()
            h = 3 + rng.randrange(5)
            for y in range(16 - h, 16):
                cv.set(int(x), y, G[1] if (y - (16 - h)) < 2 else G[2])
        if name.startswith("flowers"):
            cols = [(G[4], G[5]), (G[6], G[7])] if name == "flowers0" else [(G[7], G[6]), (G[4], G[6])]
            for (cx, cy), (petal, heart) in zip(((4, 7), (11, 5)), cols):
                line(cv, cx, cy + 2, cx, 15, G[2])
                for dx, dy in ((0, -1), (-1, 0), (1, 0), (0, 1)):
                    cv.set(cx + dx, cy + dy, petal)
                cv.set(cx, cy, heart)
        cv.outline(G[3])
    elif name == "mushroom":
        cv.rect(7, 10, 8, 15, G[9])
        cv.set(7, 10, G[10])
        hs.ellipse(cv, 7.8, 9.0, 5.0, 3.4, [G[8], G[8], G[8], (180, 40, 50)])
        for x in range(3, 13):
            if cv.get(x, 10) is None and cv.get(x, 9) is not None:
                pass
        cv.set(6, 7, G[9]), cv.set(9, 8, G[9]), cv.set(5, 9, G[9])
        cv.outline(G[3])
    else:   # a cloud, 32x16 in two cells
        big = Canvas(32, 16)
        for cx, cy, r in ((8, 10, 5.0), (15, 7, 6.5), (23, 9, 5.5), (12, 11, 4.5), (20, 11, 4.5)):
            for y in range(16):
                for x in range(32):
                    if dist(x, y, cx, cy) <= r:
                        big.set(x, y, G[11] if y < 8 else G[12] if y < 12 else G[13])
        off = 0 if name == "cloud_l" else 16
        for y in range(16):
            for x in range(16):
                cv.set(x, y, big.get(off + x, y))
    return cv


# ---- sheets ------------------------------------------------------------------------------------------------------------------
def build_sheet():
    im = Image.new("RGB", (SHEET_W, SHEET_H), hs.MAGENTA)
    for name, x, y, w, h, frames, _g in SPRITES:
        for i in range(frames):
            im.paste(sprite_frame(name, i).image(), (x + i * w, y))
    return im


def strip(cells, fn):
    w = sum(c[1] for c in cells)
    im = Image.new("RGB", (w, 16), hs.MAGENTA)
    x = 0
    for name, cw in cells:
        im.paste(fn(name).image(), (x, 0))
        x += cw
    return im


def decor_sheet():
    im = Image.new("RGB", (16 * DECOR_ITEMS, 16 * len(STRATA_PALS)), hs.MAGENTA)
    for s in range(len(STRATA_PALS)):
        for i in range(DECOR_ITEMS):
            im.paste(decor_item(s, i).image(), (i * 16, s * 16))
    return im


def strata_preview(well):
    """Each stratum's look: its earth, back wall, rocks and things (a preview for the docs, not used by the game)."""
    out = Image.new("RGB", (6 * 112, 64), (0, 0, 0))
    px = well.load()
    idx = {c: i + 1 for i, c in enumerate(P0)}
    names = ["earth0", "earth1", "back0", "backsh0", "hard0", "hard3", "pebbles", "grass0", "tunnel", "edge_l0"]
    xs = {}
    x = 0
    for n, w in WELL_CELLS:
        xs[n] = (x, w)
        x += w
    for s, pal in enumerate(STRATA_PALS):
        for k, n in enumerate(names):
            x0, w = xs[n]
            for y in range(16):
                for xx in range(w):
                    c = px[x0 + xx, y]
                    if c in idx:
                        out.putpixel((s * 112 + (k % 7) * 16 + xx, (k // 7) * 16 + y), pal[idx[c] - 1])
        dec = decor_sheet()
        for i in range(DECOR_ITEMS):
            out.paste(dec.crop((i * 16, s * 16, i * 16 + 16, s * 16 + 16)), (s * 112 + i * 16, 40))
    return out


def write_all(out, preview=False):
    os.makedirs(os.path.join(out, "tiles"), exist_ok=True)
    files = {"sprites.png": build_sheet(), "tiles/blocks.png": block_quarters(),
             "tiles/well.png": strip(WELL_CELLS, well_cell), "tiles/decor.png": decor_sheet(),
             "tiles/surface.png": strip(SURFACE_CELLS, surface_cell)}
    for f, im in files.items():
        im.save(os.path.join(out, f))
    if preview:
        os.makedirs(os.path.join(out, "preview"), exist_ok=True)
        prev = dict(files)
        prev["strata.png"] = strata_preview(files["tiles/well.png"])
        for f, im in prev.items():
            im.resize((im.width * 4, im.height * 4), Image.NEAREST).save(os.path.join(out, "preview", os.path.basename(f)))
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

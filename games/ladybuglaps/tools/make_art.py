#!/usr/bin/env python3
"""Ladybug Laps: the code-drawn art, in the 8BCraft house style (docs/art-direction.md).

Code-drawn art only, no image generation: every sprite and every track is drawn here with
games/common/tools/house_style.py (1-px outlines in the darkest shade of the material, 3-4-shade ramps lit from the
top-left, big eyes). build_assets.py cuts the sheets into tiles, palettes and maps.

The tracks (DESIGN.md "Art"): each is drawn whole from src/tracks.c (tools/trackdata.py reads it): the ground, the
band of the pieces with its edges, the start line, the zones (jam, puddles...), the objects with their shadows (light
from the top-left), the table's rim or the garden's frame. The textures repeat every 64 px and are mirrored inside each
64-px cell, so that a piece and its flipped twins (a corner and the three others) are the same tiles; repeated objects
sit at the same place modulo 64 px and share theirs: a track fits in about 500 tiles.

Writes, in the art folder (default games/ladybuglaps/art):
  ladybugs.png   the ladybugs: 9 headings (0 to 90 degrees; the game flips them for the other 23), the same with the
                 wings open (a hop), the falling frames, the HUD icon; in player 1's palette (red)
  props.png      shadow, dust, splash, sparks, the warning, the player tags, the start lights, the flag, touch controls
  leaves.png     the canopy over the track (sprites): two leaves and the carrot fronds
  tracks/<name>.png      the world of each track (transparent off the table)
  tracks/<name>-far.png  64 x 64: what is seen below the table or beyond the garden (BG4, repeated)

    python3 games/ladybuglaps/tools/make_art.py [--out DIR] [--preview] [--only NAME]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/ladybuglaps/LICENSE.
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
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "games", "common", "tools"))
import house_style as hs  # noqa: E402
import trackdata as td  # noqa: E402

Canvas = hs.Canvas
CELL = td.CELL
HW = td.HALF_W


def mirror(v, p=64):
    """Mirror-tiling: a coordinate folded into 0..p/2-1, symmetric inside each p-px cell."""
    v %= p
    return v if v < p // 2 else p - 1 - v


def h2(a, b, s=0):
    """A small deterministic hash 0..255."""
    v = (a * 73856093) ^ (b * 19349663) ^ (s * 83492791)
    v = (v ^ (v >> 13)) * 1274126177
    return (v ^ (v >> 16)) & 255


# ---- the themes: every colour a track uses ---------------------------------------------------------------------------
THEMES = {
    "TH_TEA": dict(
        g=[(222, 180, 124), (206, 162, 106), (234, 198, 146), (158, 112, 68)],        # light wood: base, grain, streak, seam
        t=[(250, 242, 222), (238, 226, 200)], e=[(98, 138, 208), (58, 88, 160)],     # a linen table runner, blue stitching
        rim=[(158, 112, 68), (96, 58, 30)], shadow=(158, 112, 68),
        jam=[(244, 120, 132), (204, 34, 58), (152, 20, 44), (112, 12, 34)],
        water=[(214, 238, 252), (150, 200, 236), (100, 150, 220), (60, 100, 170)],
        far=[(92, 140, 76), (78, 124, 66), (104, 154, 86), (66, 108, 58)]),          # the lawn far below
    "TH_DESK": dict(
        g=[(178, 208, 186), (162, 194, 172), (194, 222, 200), (120, 152, 132)],       # a pale green painted desk
        t=[(250, 250, 244), (232, 234, 238)], e=[(236, 96, 84), (176, 60, 60)],      # a paper strip, red crayon edges
        rim=[(120, 152, 132), (92, 118, 102)], shadow=(120, 152, 132),
        jam=[(252, 252, 248), (222, 230, 240), (176, 190, 214), (96, 110, 150)],     # glue
        water=[(214, 238, 252), (150, 200, 236), (100, 150, 220), (60, 100, 170)],
        far=[(120, 104, 150), (106, 90, 136), (134, 118, 164), (92, 78, 120)]),       # the carpet below
    "TH_PICNIC": dict(
        g=[(124, 182, 86), (108, 166, 74), (148, 200, 100), (86, 142, 62)],           # the lawn
        t=[(230, 210, 156), (214, 190, 132)], e=[(178, 150, 100), (140, 114, 74)],   # a sandy path
        frame=[(64, 124, 62), (42, 92, 48), (92, 152, 78)], shadow=(86, 142, 62),     # the hedge
        blanket=[(236, 92, 92), (250, 238, 236), (244, 166, 164)],
        jam=[(255, 214, 90), (246, 150, 40), (206, 96, 24), (140, 60, 18)],         # spilt juice
        far=[(86, 142, 62), (74, 128, 54), (96, 152, 70), (64, 112, 48)]),
    "TH_VEG": dict(
        g=[(124, 88, 60), (108, 74, 50), (142, 104, 72), (86, 58, 40)],               # the soil
        t=[(216, 172, 112), (190, 144, 88)], e=[(238, 200, 142), (152, 106, 64)],    # a path of wood chips
        frame=[(176, 124, 72), (132, 90, 54), (206, 156, 98)], shadow=(86, 58, 40),   # the raised bed's planks
        jam=[(132, 96, 68), (98, 66, 46), (80, 54, 38), (62, 42, 30)],                 # mud
        water=[(214, 238, 252), (150, 200, 236), (100, 150, 220), (60, 100, 170)],
        far=[(108, 74, 50), (96, 64, 44), (120, 84, 58), (80, 54, 38)]),
    "TH_PATIO": dict(
        g=[(178, 178, 188), (162, 162, 174), (198, 198, 208), (122, 124, 138)],       # wet flagstones
        t=[(228, 216, 188), (212, 198, 168)], e=[(152, 142, 122), (112, 106, 94)],   # a sandstone path
        frame=[(194, 104, 84), (150, 72, 62), (222, 144, 114)], shadow=(122, 124, 138),  # a low brick wall
        water=[(214, 238, 252), (150, 200, 236), (100, 150, 220), (60, 100, 170)],
        far=[(150, 150, 162), (136, 136, 148), (164, 164, 176), (120, 120, 134)]),
}
INK = (30, 22, 36)
WHITE = (250, 250, 250)


# ---- the grounds (mirror-tiled: symmetric in each 64-px cell) ---------------------------------------------------------
def ground_tea(T, x, y):
    mx, my = mirror(x), mirror(y)
    g = T["g"]
    if my in (7, 23):                               # the seams between the planks
        return g[3]
    plank = 0 if my < 7 or my > 23 else 1
    if plank == 1 and mx == 12:                     # a plank's end
        return g[3]
    k = h2(plank, my, 3)
    if k < 40 and (mx + k) % 9 < 4:                 # the grain: a few broken lines
        return g[1]
    if k > 225 and (mx + k) % 13 < 3:
        return g[2]
    return g[0]


def ground_desk(T, x, y):
    mx, my = mirror(x), mirror(y)
    g = T["g"]
    if my == 31:                                    # long boards, a soft grain
        return g[3]
    k = h2(my // 2, 0, 7)
    if k < 50 and (mx * 3 + k) % 17 < 5:
        return g[1]
    if k > 230:
        return g[2]
    return g[0]


def ground_grass(T, x, y):
    mx, my = mirror(x), mirror(y)
    g = T["g"]
    k = h2(mx, my, 11)
    if k < 22:
        return g[3]
    if k < 60:
        return g[1]
    if k > 236:
        return g[2]
    return g[0]


def ground_soil(T, x, y):
    mx, my = mirror(x), mirror(y)
    g = T["g"]
    k = h2(mx, my, 17)
    if my % 8 == 3 and k < 160:                     # furrows
        return g[3]
    if k < 30:
        return g[1]
    if k > 238:
        return g[2]
    return g[0]


def ground_patio(T, x, y):
    mx, my = mirror(x), mirror(y)
    g = T["g"]
    if mx in (0, 31) or my in (0, 31):              # the joints between the slabs (32-px slabs)
        return g[3]
    if mx == 1 or my == 1:
        return g[2]
    k = h2(mx, my, 23)
    if k < 26:
        return g[1]
    return g[0]


GROUNDS = {"TH_TEA": ground_tea, "TH_DESK": ground_desk, "TH_PICNIC": ground_grass, "TH_VEG": ground_soil,
           "TH_PATIO": ground_patio}


def band_colour(T, theme, off, x, y):
    """The band at offset `off` from the centre line (|off| <= HW)."""
    a = abs(off)
    t, e = T["t"], T["e"]
    if a > HW - 1.5:
        return e[1]
    if a > HW - 3.5:
        return e[0]
    mx, my = mirror(x), mirror(y)
    if theme == "TH_TEA":                           # the linen's weave
        return t[1] if (mx % 4 == 1 and my % 4 == 1) else t[0]
    if theme == "TH_DESK":                          # paper, a faint dot grid
        return t[1] if (mx % 8 == 3 and my % 8 == 3) else t[0]
    if theme == "TH_PICNIC":                        # sand grains
        return t[1] if h2(mx, my, 31) < 34 else t[0]
    if theme == "TH_VEG":                           # wood chips: little dashes
        k = h2(mx // 3, my, 37)
        return t[1] if k < 60 else e[0] if k > 240 else t[0]
    return t[1] if h2(mx, my, 41) < 20 else t[0]   # sandstone


# ---- drawing helpers on a world canvas (rows of RGB / None) ---------------------------------------------------------------
class World:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.p = [[None] * w for _ in range(h)]

    def set(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h and self.p[y][x] is not None:
            self.p[y][x] = c

    def get(self, x, y):
        return self.p[y][x] if 0 <= x < self.w and 0 <= y < self.h else None


def paste(wd, cv, x0, y0):
    for y in range(cv.h):
        for x in range(cv.w):
            c = cv.p[y][x]
            if c is not None:
                wd.set(x0 + x, y0 + y, c)


def shadow(wd, cv, x0, y0, col, dx=3, dy=3):
    """The object's shadow (the light from the top-left): its silhouette moved down-right."""
    for y in range(cv.h):
        for x in range(cv.w):
            if cv.p[y][x] is not None:
                wd.set(x0 + x + dx, y0 + y + dy, col)


def disc(cv, cx, cy, r, col):
    for y in range(int(cy - r) - 1, int(cy + r) + 2):
        for x in range(int(cx - r) - 1, int(cx + r) + 2):
            if (x + 0.5 - cx) ** 2 + (y + 0.5 - cy) ** 2 <= r * r:
                cv.set(x, y, col)


def ring(cv, cx, cy, r0, r1, col):
    for y in range(int(cy - r1) - 1, int(cy + r1) + 2):
        for x in range(int(cx - r1) - 1, int(cx + r1) + 2):
            d = math.hypot(x + 0.5 - cx, y + 0.5 - cy)
            if r0 <= d <= r1:
                cv.set(x, y, col)


def shaded_disc(cv, cx, cy, r, r4):
    hs.ellipse(cv, cx, cy, r, r, r4)


def rrect(cv, x0, y0, x1, y1, r4, rad=2):
    """A box with rounded corners, shaded by the house light (a slab seen from above: light top-left edges)."""
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            cx = min(max(x, x0 + rad), x1 - rad)
            cy = min(max(y, y0 + rad), y1 - rad)
            if (x - cx) ** 2 + (y - cy) ** 2 > rad * rad + 0.5:
                continue
            edge_l, edge_t = x - x0, y - y0
            edge_r, edge_b = x1 - x, y1 - y
            if edge_t < 2 or edge_l < 2:
                c = r4[0]
            elif edge_b < 2 or edge_r < 2:
                c = r4[2]
            else:
                c = r4[1]
            cv.set(x, y, c)


# ---- the objects (top-down, light from the top-left; canvases centred on the object's position) ---------------------------
# ---- the objects' colours: a few shared families, so that a track's tiles fit its 5 BG palettes -------------------------
PORC = [(252, 252, 248), (222, 230, 240), (176, 190, 214), (96, 110, 150)]       # porcelain, sugar, silver, paper
PATTERN = [(84, 124, 200), (214, 92, 112), (92, 168, 104)]                      # the cups' patterns
BROWN = [(226, 168, 96), (196, 132, 66), (150, 96, 44), (96, 58, 30)]            # biscuits, tea, wicker, twigs, shells
RED = [(244, 120, 132), (204, 34, 58), (152, 20, 44), (112, 12, 34)]             # jam, apples, red crayons
ORANGE = [(255, 214, 90), (246, 150, 40), (206, 96, 24), (140, 60, 18)]          # juice, pumpkins, pencils, carrots
GREEN = [(200, 236, 120), (140, 200, 70), (80, 150, 60), (40, 96, 40)]          # leaves, lettuces, the can
PINK = [(255, 196, 214), (240, 140, 170)]                                        # icing, erasers, flowers
BLUEW = [(214, 238, 252), (150, 200, 236), (100, 150, 220), (60, 100, 170)]     # water, glass, blue crayons
TERRA = [(232, 150, 110), (204, 112, 76), (160, 76, 52), (110, 50, 36)]          # flower pots
TEA_C = [BROWN[1], BROWN[2], BROWN[0]]
SILVER = PORC
COOKIE = BROWN
ICING = PINK
SUGAR = PORC
CRAYON = [RED[1:], BLUEW[1:], GREEN[1:], ORANGE[:3]]
WRAP = [PORC[0], PORC[1]]
ERASER = [[PINK[0], PINK[1], RED[2]], [BLUEW[1], BLUEW[2], BLUEW[3]]]
PENCIL = ORANGE[:3]
WOOD_TIP = [BROWN[0], INK]
GLASS = BLUEW
PAINTS = [[BLUEW[1], BLUEW[2]], [RED[0], RED[1]]]
APPLE = [RED, GREEN]
STEM = [BROWN[3], GREEN[1]]
BREAD = [(250, 228, 176), BROWN[0], BROWN[1], BROWN[3]]
FILL = [GREEN[1], RED[0]]
JUICE = [ORANGE[0], ORANGE[1]]
STRAW = [PORC[0], RED[1]]
WICKER = BROWN
CLOTH = [RED[1], PORC[0]]
TWIG = BROWN[1:]
LETTUCE = GREEN
PUMPKIN = ORANGE
STONE = [PORC[1], PORC[2], PORC[3], (70, 80, 110)]
CARROT = [ORANGE[1], ORANGE[2], GREEN[1]]
POT = TERRA
FLOWERS = [[PINK[0], ORANGE[0]], [ORANGE[0], ORANGE[1]], [PINK[1], PORC[0]]]
LEAFG = GREEN[:3]
SHELL = BROWN
SNAILB = [PORC[1], PORC[2], PORC[3], PORC[3]]
CAN = GREEN


def outline(cv, col):
    cv.outline(col)


def obj_cup(var):
    """A teacup on its saucer, from above: the saucer's rim, the cup's rim, the tea, the handle to the east."""
    cv = Canvas(64, 64)
    c = 30.0
    shaded_disc(cv, c, c, 26.5, PORC)
    ring(cv, c, c, 23.0, 24.2, PATTERN[var])                # the saucer's pattern line
    shaded_disc(cv, c, c, 16.5, PORC)
    ring(cv, c, c, 14.0, 16.5, PORC[0])                     # the cup's rim
    ring(cv, c, c, 15.0, 15.8, PATTERN[var])
    disc(cv, c, c, 13.2, TEA_C[0])                          # the tea
    ring(cv, c, c, 11.6, 13.2, TEA_C[1])
    cv.set(int(c - 5), int(c - 5), TEA_C[2]), cv.set(int(c - 4), int(c - 5), TEA_C[2]), cv.set(int(c - 5), int(c - 4), TEA_C[2])
    for y in range(-4, 5):                                  # the handle
        for x in range(16, 23):
            d = math.hypot(x - 16.5, y * 1.3)
            if 3.0 <= d <= 5.6:
                cv.set(int(c + x), int(c + y), PORC[1] if y < 0 else PORC[2])
    outline(cv, PORC[3])
    return cv


def obj_teapot(var):
    cv = Canvas(96, 96)
    c = 48.0
    for x in range(-52, -32):                               # the spout (west)
        for y in range(-6, 7):
            k = (-32 - x) / 20.0
            if abs(y) <= 6 - k * 3.5:
                cv.set(int(c + x + 6), int(c + y), PORC[1] if y < 0 else PORC[2])
    for y in range(-10, 11):                                # the handle (east)
        for x in range(30, 46):
            d = math.hypot(x - 32, y * 1.2)
            if 9.0 <= d <= 13.5:
                cv.set(int(c + x), int(c + y), PORC[1] if y < 0 else PORC[2])
    shaded_disc(cv, c, c, 36.5, PORC)
    ring(cv, c, c, 30.0, 31.5, PATTERN[0])                  # a blue band round it
    for k in range(12):                                     # little flowers on the band
        a = k * math.pi / 6
        disc(cv, c + math.cos(a) * 33.5, c + math.sin(a) * 33.5, 1.6, PATTERN[1 if k % 2 else 0])
    shaded_disc(cv, c, c, 16.5, PORC)                       # the lid
    ring(cv, c, c, 15.0, 16.5, PATTERN[0])
    shaded_disc(cv, c, c, 5.5, [PATTERN[0], PATTERN[0], PORC[3], PORC[3]])   # the knob
    cv.set(int(c - 2), int(c - 2), PORC[0])
    outline(cv, PORC[3])
    return cv


def obj_sugar(var):
    cv = Canvas(24, 24)
    rrect(cv, 4, 4, 19, 19, [SUGAR[0], SUGAR[1], SUGAR[2]], rad=2)
    rnd = random.Random(5)
    for _ in range(14):                                     # sparkly grains
        cv.set(rnd.randint(6, 17), rnd.randint(6, 17), SUGAR[2] if rnd.random() < 0.5 else SUGAR[0])
    outline(cv, SUGAR[3])
    return cv


def obj_spoon(var):
    cv = Canvas(72, 24)
    c = 12
    for x in range(6, 48):                                  # the handle
        w = 2.2 if x > 14 else 1.6
        for y in range(-3, 4):
            if abs(y) <= w:
                cv.set(x, c + y, SILVER[0] if y < 0 else SILVER[1] if y < 1 else SILVER[2])
    hs.ellipse(cv, 56, c, 10.5, 7.5, SILVER)                # the bowl
    hs.ellipse(cv, 57, c + 0.5, 7.0, 4.6, None, flat=SILVER[2])
    hs.ellipse(cv, 55, c - 1, 4.0, 2.0, None, flat=SILVER[1])
    outline(cv, SILVER[3])
    return cv


def obj_cookie(var):
    cv = Canvas(40, 40)
    c = 19.5
    shaded_disc(cv, c, c, 15.0, COOKIE)
    if var == 1:                                            # pink icing
        disc(cv, c, c, 11.5, ICING[1])
        disc(cv, c - 1, c - 1, 9.5, ICING[0])
    rnd = random.Random(9 + var)
    for _ in range(7 if var == 0 else 6):
        a, r = rnd.uniform(0, 6.28), rnd.uniform(3, 11)
        x, y = int(c + math.cos(a) * r), int(c + math.sin(a) * r)
        col = COOKIE[3] if var == 0 else [PINK[1], PORC[0], ORANGE[0]][_ % 3]
        cv.set(x, y, col), cv.set(x + 1, y, col)
    outline(cv, COOKIE[3])
    return cv


def obj_crayon(var, vertical):
    cv = Canvas(80, 24)
    c = 12
    r3 = CRAYON[var]
    for x in range(4, 70):
        for y in range(-6, 7):
            if x < 14:                                      # the sharpened tip (west)
                if abs(y) > (x - 4) * 0.6:
                    continue
                col = r3[1] if x > 8 else r3[2]
            else:
                col = r3[0] if y < -2 else r3[1] if y < 3 else r3[2]
                if 30 <= x < 58:                            # the paper wrapper with two stripes
                    col = WRAP[0] if y < 3 else WRAP[1]
                    if x in (33, 34, 54, 55):
                        col = r3[2]
            cv.set(x, c + y, col)
    outline(cv, r3[2] if var != 3 else ORANGE[3])
    if vertical:
        out = Canvas(24, 80)
        for y in range(80):
            for x in range(24):
                out.p[y][x] = cv.p[x][y]
        return out
    return cv


def obj_eraser(var):
    cv = Canvas(48, 32)
    rrect(cv, 3, 3, 43, 27, ERASER[var], rad=3)
    for x in range(14, 33):                                 # a sleeve
        for y in range(4, 27):
            cv.set(x, y, WRAP[0] if y < 15 else WRAP[1])
    for x in (16, 30):
        for y in range(6, 25):
            cv.set(x, y, ERASER[var][2])
    outline(cv, ERASER[var][2])
    return cv


def obj_pencil(var):
    cv = Canvas(16, 72)
    c = 8
    for y in range(4, 66):
        for x in range(-4, 5):
            if y < 14:
                if abs(x) > (y - 4) * 0.45:
                    continue
                col = WOOD_TIP[1] if y < 7 else WOOD_TIP[0]
            elif y > 60:
                col = PINK[1]                               # the eraser end
            else:
                col = PENCIL[0] if x < -1 else PENCIL[1] if x < 2 else PENCIL[2]
            cv.set(c + x, y, col)
    outline(cv, PENCIL[2])
    return cv


def obj_paint(var):
    cv = Canvas(56, 56)
    c = 27.5
    shaded_disc(cv, c, c, 22.0, GLASS)
    ring(cv, c, c, 19.0, 22.0, GLASS[0])
    disc(cv, c, c, 17.5, PAINTS[var][1])
    disc(cv, c - 1.5, c - 1.5, 13.0, PAINTS[var][0])
    for k in range(3):
        cv.set(int(c - 7 + k), int(c - 9), PORC[0])
    for y in range(-26, -6):                                # a brush leaning out of it
        for x in range(-2, 2):
            cv.set(int(c + 8 + x - y * 0.35), int(c + y), BROWN[1] if y > -20 else INK)
    outline(cv, GLASS[3])
    return cv


def obj_apple(var):
    cv = Canvas(40, 40)
    c = 19.5
    shaded_disc(cv, c, c, 17.0, APPLE[var])
    for k in range(5):                                      # the stem and a leaf (colours the apple's tiles hold)
        cv.set(int(c + 1), int(c - 2 + k), GREEN[3])
    hs.ellipse(cv, c + 6, c - 4, 4.5, 2.4, None, flat=STEM[1])
    glint = PORC[0] if var == 0 else APPLE[var][0]
    cv.set(int(c - 7), int(c - 8), glint), cv.set(int(c - 6), int(c - 8), glint)
    outline(cv, APPLE[var][3])
    return cv


def obj_sandwich(var):
    cv = Canvas(56, 48)
    rrect(cv, 3, 4, 52, 43, [BREAD[1], BREAD[1], BREAD[2]], rad=5)  # the crust
    rrect(cv, 6, 7, 49, 40, [BREAD[0], BREAD[0], BREAD[1]], rad=4)  # the bread
    for x in range(4, 52):                                   # the lettuce and the tomato peeking out
        if (x // 3) % 2 == 0:
            cv.set(x, 44, FILL[0])
        if x % 7 == 3:
            cv.set(x, 45, FILL[1])
    for x in range(10, 46, 6):
        cv.set(x, 20 + (x % 5), BREAD[1])
    outline(cv, BREAD[3])
    return cv


def obj_juice(var):
    cv = Canvas(32, 32)
    c = 15.5
    shaded_disc(cv, c, c, 13.5, PORC)
    disc(cv, c, c, 10.5, JUICE[1])
    disc(cv, c - 1, c - 1, 8.0, JUICE[0])
    for k in range(10):                                     # the straw, red and white
        cv.set(int(c + 1 + k * 0.8), int(c - 1 - k), STRAW[(k // 2) % 2])
        cv.set(int(c + 2 + k * 0.8), int(c - 1 - k), STRAW[(k // 2) % 2])
    outline(cv, PORC[3])
    return cv


def obj_basket(var):
    cv = Canvas(96, 72)
    rrect(cv, 6, 8, 89, 63, [WICKER[0], WICKER[1], WICKER[2]], rad=6)
    for y in range(10, 62):                                 # the weave
        for x in range(8, 88):
            if (x + (y // 4) * 2) % 8 == 0 or y % 4 == 0:
                cv.set(x, y, WICKER[2])
    for x in range(12, 84):                                 # a cloth folded over the top
        for y in range(10, 30):
            if y < 12 + int(4 * math.sin(x / 7.0)) + 8:
                cv.set(x, y, CLOTH[((x // 6) + (y // 6)) % 2])
    for x in range(20, 76):                                 # the handle across
        y = 36 + int(14 * ((x - 48) / 28.0) ** 2) - 14
        for k in range(3):
            cv.set(x, y + k, WICKER[0] if k == 0 else WICKER[1])
    outline(cv, WICKER[3])
    return cv


def obj_twig(var):
    cv = Canvas(20, 72)
    for y in range(4, 68):
        x = 10 + int(2.2 * math.sin(y / 9.0))
        for k in range(-2, 3):
            cv.set(x + k, y, TWIG[0] if k < 0 else TWIG[1] if k < 2 else TWIG[2])
        if y in (20, 45):                                   # little side shoots
            for k in range(5):
                cv.set(x + 2 + k, y - k, TWIG[1])
    outline(cv, TWIG[2])
    return cv


def obj_lettuce(var):
    cv = Canvas(48, 48)
    c = 23.5
    for k in range(7):                                      # leaves round, then the heart
        a = k * 2 * math.pi / 7
        hs.ellipse(cv, c + math.cos(a) * 9, c + math.sin(a) * 9, 10.0, 10.0, LETTUCE)
    shaded_disc(cv, c, c, 9.5, LETTUCE)
    for k in range(7):
        a = k * 2 * math.pi / 7 + 0.3
        for r in range(3, 17):
            cv.set(int(c + math.cos(a) * r), int(c + math.sin(a) * r), LETTUCE[0] if r < 9 else LETTUCE[1])
    outline(cv, LETTUCE[3])
    return cv


def obj_pumpkin(var):
    cv = Canvas(56, 56)
    c = 27.5
    shaded_disc(cv, c, c, 22.0, PUMPKIN)
    for k in range(8):                                      # the ribs
        a = k * math.pi / 4
        for r in range(6, 22):
            cv.set(int(c + math.cos(a) * r), int(c + math.sin(a) * r), PUMPKIN[2])
    disc(cv, c, c, 4.0, GREEN[2])                          # the stem
    disc(cv, c - 1, c - 1, 2.0, GREEN[1])
    if var == 1:
        hs.ellipse(cv, c + 10, c - 12, 6, 3, None, flat=GREEN[1])
    outline(cv, PUMPKIN[3])
    return cv


def obj_stone(var):
    cv = Canvas(24, 24)
    hs.ellipse(cv, 11.5, 11.5, 9.5, 8.0, STONE)
    outline(cv, STONE[3])
    return cv


def obj_carrot(var):
    cv = Canvas(16, 16)
    disc(cv, 7.5, 7.5, 5.5, CARROT[0])
    disc(cv, 8.5, 8.5, 3.0, CARROT[1])
    outline(cv, ORANGE[3])
    return cv


def obj_pot(var):
    cv = Canvas(56, 56)
    c = 27.5
    shaded_disc(cv, c, c, 22.0, POT)
    ring(cv, c, c, 18.0, 20.0, POT[2])
    disc(cv, c, c, 17.0, BROWN[3])                         # the soil
    rnd = random.Random(30 + var)
    for k in range(9):                                      # leaves and flowers
        a = k * 2 * math.pi / 9 + rnd.uniform(-0.2, 0.2)
        hs.ellipse(cv, c + math.cos(a) * 9, c + math.sin(a) * 9, 5, 5, None, flat=LEAFG[1])
        hs.ellipse(cv, c + math.cos(a) * 9 - 1, c + math.sin(a) * 9 - 1, 3, 3, None, flat=LEAFG[0])
    for k in range(5):
        a = k * 2 * math.pi / 5 + 0.6
        x, y = c + math.cos(a) * 8, c + math.sin(a) * 8
        for dx, dy in ((0, -2), (2, 0), (0, 2), (-2, 0)):
            disc(cv, x + dx, y + dy, 1.8, FLOWERS[var][0])
        disc(cv, x, y, 1.3, FLOWERS[var][1])
    outline(cv, POT[3])
    return cv


def obj_snail(var):
    cv = Canvas(40, 32)
    hs.ellipse(cv, 22, 17, 15, 6, SNAILB)                   # the body
    for k in range(2):                                      # the eye stalks
        for s in range(5):
            cv.set(35 + s, 12 + k * 8 - (s if k == 0 else -s) // 2, SNAILB[3])
        cv.set(39, 10 + k * 12, INK)
    shaded_disc(cv, 17, 15, 11.0, SHELL)
    for t in range(60):                                     # the spiral
        a = t * 0.3
        r = 9.5 - t * 0.14
        if r < 1:
            break
        cv.set(int(17 + math.cos(a) * r), int(15 + math.sin(a) * r), SHELL[3])
    outline(cv, SHELL[3])
    return cv


def obj_can(var):
    cv = Canvas(72, 48)
    rrect(cv, 8, 6, 51, 41, [CAN[0], CAN[1], CAN[2]], rad=8)
    for x in range(51, 70):                                 # the spout and its rose
        for y in range(-3, 4):
            cv.set(x, 14 + y - (x - 51) // 3, CAN[1] if y < 0 else CAN[2])
    hs.ellipse(cv, 66, 8, 4.5, 4.5, CAN)
    for x in range(14, 46):                                 # the handle over the top
        cv.set(x, 23, CAN[3])
        cv.set(x, 24, CAN[0])
    disc(cv, 22, 30, 5, BLUEW[3])                     # the water inside the opening
    outline(cv, CAN[3])
    return cv


OBJ_DRAW = {
    "OB_CUP": obj_cup, "OB_SUGAR": obj_sugar, "OB_TEAPOT": obj_teapot, "OB_SPOON": obj_spoon, "OB_COOKIE": obj_cookie,
    "OB_CRAYON_H": lambda v: obj_crayon(v, False), "OB_CRAYON_V": lambda v: obj_crayon(v, True),
    "OB_ERASER": obj_eraser, "OB_PENCIL": obj_pencil, "OB_PAINT": obj_paint, "OB_APPLE": obj_apple,
    "OB_SANDWICH": obj_sandwich, "OB_JUICE": obj_juice, "OB_BASKET": obj_basket, "OB_TWIG": obj_twig,
    "OB_LETTUCE": obj_lettuce, "OB_PUMPKIN": obj_pumpkin, "OB_STONE": obj_stone, "OB_CARROT": obj_carrot,
    "OB_POT": obj_pot, "OB_SNAIL": obj_snail, "OB_CAN": obj_can,
}
FLAT = {"OB_SPOON", "OB_PENCIL", "OB_TWIG", "OB_CARROT"}         # lying on the ground: a thin shadow


# ---- the scenery: flat things lying about off the band (no rules: napkins, crumbs, drawings, daisies, moss...) ---------
# Each is a "stamp" a whole number of tiles wide and high, put down at the same place in its 64-px cell where there is
# room (the ground alone round it), maybe flipped about the cell's middle: the ground is mirrored in each cell, so all
# its copies are the same tiles (flipped): a stamp costs its own tiles once. Their colours are ones the track's
# palettes hold with its ground (build_assets.py packs them: no tile may lose a colour).
def blob(cv, pts, c):
    for x, y in pts:
        cv.set(x, y, c)


def poly_in(px, py, pts):
    """Even-odd rule: is (px, py) inside the polygon pts?"""
    inside = False
    for i in range(len(pts)):
        ax, ay = pts[i]
        bx, by = pts[(i + 1) % len(pts)]
        if (ay > py) != (by > py) and px < ax + (py - ay) * (bx - ax) / (by - ay):
            inside = not inside
    return inside


# -- Tea Time: a table laid for tea
def sc_napkin(var):
    """A blue gingham napkin folded in four, lying flat, its top corner turned back."""
    cv = Canvas(32, 32)
    for y in range(4, 28):
        for x in range(4, 28):
            if x - y > 13:                                  # the corner turned back
                continue
            a, b = ((x - 4) // 4) % 2, ((y - 4) // 4) % 2
            cv.set(x, y, PATTERN[0] if (a and b) else BLUEW[1] if (a or b) else PORC[0])
    for y in range(4, 15):                                  # the corner folded over: its back, plain
        for x in range(17, 28):
            if 2 < x - y <= 13:
                cv.set(x, y, PORC[1] if x - y == 13 else PORC[0])
    cv.outline(PORC[3])
    return cv


def sc_doily(var):
    """A lace doily: a scalloped ring of white lace with a ring of holes, the table showing through."""
    cv = Canvas(32, 32)
    c = 16.0
    for y in range(32):
        for x in range(32):
            dx, dy = x + 0.5 - c, y + 0.5 - c
            d, a = math.hypot(dx, dy), math.atan2(dy, dx)
            if d > 13.2 + 1.4 * math.cos(a * 10):
                continue
            if (abs(d - 9.6) < 1.1 and math.cos(a * 10) > 0.25) or (abs(d - 4.4) < 0.8 and math.cos(a * 6) > 0.3):
                continue                                    # the holes of the lace
            cv.set(x, y, PORC[1] if (dx + dy > 8 and d > 10.5) else PORC[0])
    cv.outline(PORC[2])
    return cv


def sc_teabag(var):
    """A used tea bag, its string and its paper tag."""
    cv = Canvas(24, 16)
    for y in range(3, 13):
        for x in range(2, 11):
            if x in (2, 10) and y in (3, 12):
                continue
            cv.set(x, y, BROWN[1] if x + y < 14 else BROWN[2])
    blob(cv, ((4, 5), (5, 5), (4, 6)), BROWN[0])
    cv.outline(BROWN[3])
    for k in range(9):                                      # the string
        cv.set(11 + k, 7 + (1 if 3 < k < 7 else 0), PORC[2])
    for y in range(4, 11):                                  # the tag
        for x in range(19, 23):
            cv.set(x, y, PORC[0] if y < 9 else PORC[1])
    blob(cv, ((20, 6), (21, 6)), RED[1])
    return cv


def sc_crumbs(var):
    """A corner broken off a biscuit (its straight broken edges, golden) and its crumbs."""
    cv = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            d = math.hypot(x + 0.5 - 3.0, y + 0.5 - 3.0)
            if x >= 3 and y >= 3 and d <= 9.0:              # a quarter of a round biscuit
                cv.set(x, y, COOKIE[0] if (x < 5 or y < 5) else COOKIE[1])
    for x in range(3, 12):                                  # the broken edges are rough
        if x % 3 == 1:
            cv.set(x, 3, None)
            cv.set(3, x, None)
    blob(cv, ((7, 7), (9, 6), (6, 9)), COOKIE[2])
    cv.outline(COOKIE[2])
    for x, y in ((1, 1), (13, 3), (2, 13), (14, 12)):
        cv.set(x, y, COOKIE[1])
    return cv

def sc_stain(var):
    """A tea ring left by a cup, broken at the top-left."""
    cv = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            d = math.hypot(x + 0.5 - 8, y + 0.5 - 8)
            a = math.atan2(y + 0.5 - 8, x + 0.5 - 8)
            if 5.4 <= d <= 6.7 and not (-2.7 < a < -1.8):
                cv.set(x, y, BROWN[1] if -0.4 < a < 2.4 else BROWN[0])
    return cv


def sc_petals(cols):
    """Three petals, fallen."""
    cv = Canvas(16, 8)
    for (cx, cy) in ((3.0, 3.0), (7.5, 5.0), (12.5, 2.5)):
        hs.ellipse(cv, cx, cy, 2.2, 1.6, None, flat=cols[0])
        cv.set(int(cx), int(cy), cols[1])
        cv.set(int(cx) + 1, int(cy), cols[1])
    return cv


# -- Crayon Desk: a child's desk
def sc_drawing(var):
    """A child's crayon drawing on a sheet of paper: the sun, a house, the sky."""
    cv = Canvas(48, 40)
    paper, edge = (255, 255, 247), (239, 239, 239)
    for y in range(4, 36):
        for x in range(4, 44):
            cv.set(x, y, edge if (x > 41 or y > 33) else paper)
    hs.ellipse(cv, 12.0, 12.0, 4.0, 4.0, None, flat=ORANGE[0])  # the sun and its rays
    for k in range(8):
        a = k * math.pi / 4
        for r in (6, 7):
            cv.set(int(12 + math.cos(a) * r), int(12 + math.sin(a) * r), ORANGE[1])
    for y in range(22, 32):                                 # the house
        for x in range(24, 37):
            cv.set(x, y, (236, 96, 84) if (x in (24, 36) or y in (22, 31)) else paper)
    for k in range(9):                                      # its roof
        for x in range(23 + k, 38 - k):
            if k == 0 or x in (23 + k, 37 - k):
                cv.set(x, 21 - k, ORANGE[2])
    for y in range(26, 32):                                 # the door
        blob(cv, ((29, y), (30, y)), ORANGE[3])
    for x in range(6, 42):                                  # the grass line, in blue (the crayon at hand)
        cv.set(x, 32 - (x // 3) % 2, BLUEW[3])
    cv.outline(THEMES["TH_DESK"]["g"][3])
    return cv


def sc_sticky(var):
    """A yellow sticky note with a few lines on it, its bottom edge curling up."""
    cv = Canvas(24, 24)
    for y in range(3, 21):
        for x in range(3, 21):
            cv.set(x, y, ORANGE[0] if y < 18 else BROWN[0])
    for y in (8, 11, 14):
        for x in range(6, 18 - (4 if y == 14 else 0)):
            if (x + y) % 5:
                cv.set(x, y, ORANGE[1])
    cv.outline(ORANGE[2])
    return cv


def sc_star(var):
    """A gold star sticker."""
    cv = Canvas(16, 16)
    pts = [(8 + (6.6 if k % 2 == 0 else 2.9) * math.cos(-math.pi / 2 + k * math.pi / 5),
            8.5 + (6.6 if k % 2 == 0 else 2.9) * math.sin(-math.pi / 2 + k * math.pi / 5)) for k in range(10)]
    for y in range(16):
        for x in range(16):
            if poly_in(x + 0.5, y + 0.5, pts):
                cv.set(x, y, ORANGE[0] if x + y < 15 else ORANGE[1])
    cv.outline(ORANGE[2])
    return cv


def sc_heart(var):
    """A pink heart sticker."""
    cv = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            px, py = x + 0.5, y + 0.5
            if (math.hypot(px - 5.6, py - 6.2) <= 3.1 or math.hypot(px - 10.4, py - 6.2) <= 3.1
                    or poly_in(px, py, ((2.6, 7.2), (13.4, 7.2), (8.0, 13.4)))):
                cv.set(x, y, PINK[1])
    blob(cv, ((4, 5), (5, 4), (4, 6)), PINK[0])
    cv.outline(RED[2])
    return cv

def sc_shavings(cols):
    """Crayon shavings: a frilly ring, the crayon's colour (light, mid, outline) round the pale wood."""
    cv = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            dx, dy = x + 0.5 - 8, y + 0.5 - 8
            d, a = math.hypot(dx, dy), math.atan2(dy, dx)
            rim = 5.4 + 0.9 * math.sin(a * 7)
            if 1.6 < d <= rim and a < 2.2:                  # a ring, opened at the bottom left
                cv.set(x, y, cols[3] if d < rim - 1.6 else cols[0] if (dx + dy) < 0 else cols[1])
    cv.outline(cols[2])
    return cv


def sc_scribble(col):
    """A crayon scribble: loops along a line."""
    cv = Canvas(24, 16)
    for k in range(160):
        t = k / 159.0
        cv.set(int(3.5 + t * 16 + 2.6 * math.cos(t * 6.283 * 4)),
               int(8.0 + 4.4 * math.sin(t * 6.283 * 4) - 1.5 * math.sin(t * 3.1)), col)
    return cv


def sc_paper(cols):
    """A scrap of lined paper, a corner torn off: paper, the lines, the outline."""
    cv = Canvas(16, 16)
    for y in range(3, 14):
        for x in range(2, 14):
            if x + y > 23 or (y == 3 and x > 10):
                continue
            cv.set(x, y, cols[1] if (y % 3 == 0 or x == 4) else cols[0])
    cv.outline(cols[2])
    return cv


def sc_splat(cols):
    """A splash of paint and its drops."""
    cv = Canvas(16, 16)
    hs.ellipse(cv, 6.5, 6.5, 4.2, 3.6, None, flat=cols[1])
    for a in range(0, 360, 60):
        r = math.radians(a + 15)
        hs.ellipse(cv, 6.5 + math.cos(r) * 4.2, 6.5 + math.sin(r) * 3.6, 1.4, 1.4, None, flat=cols[1])
    blob(cv, ((4, 4), (5, 4), (4, 5)), cols[0])
    blob(cv, ((13, 12), (12, 13), (13, 13), (2, 13)), cols[1])
    return cv


def sc_bits(cols):
    """Rolled-up bits of eraser."""
    cv = Canvas(8, 8)
    for x, y in ((1, 2), (4, 4), (2, 6), (5, 1)):
        cv.set(x, y, cols[0])
        cv.set(x + 1, y, cols[1])
    return cv


# -- Picnic: the lawn
def daisy(cv, x, y, big=True):
    r = 2 if big else 1
    for dx, dy in ((0, -r), (r, 0), (0, r), (-r, 0)):
        cv.set(x + dx, y + dy, PORC[0])
    if big:
        for dx, dy in ((-1, -1), (1, -1), (-1, 1), (1, 1)):
            cv.set(x + dx, y + dy, PORC[0])
    cv.set(x, y, ORANGE[0])


def sc_daisies(var):
    """Daisies in the grass (var 1: a patch of them, with their leaves)."""
    if var == 0:
        cv = Canvas(16, 16)
        daisy(cv, 4, 4), daisy(cv, 11, 10), daisy(cv, 12, 3, False)
        return cv
    cv = Canvas(24, 24)
    for x, y in ((5, 8), (9, 15), (16, 6), (18, 17), (12, 10)):
        blob(cv, ((x - 1, y + 3), (x, y + 3), (x + 1, y + 4)), GREEN[2])
    for x, y, big in ((5, 6, 1), (10, 13, 1), (16, 4, 1), (19, 15, 1), (13, 9, 0), (4, 16, 0)):
        daisy(cv, x, y, big)
    return cv


def sc_dandelions(var):
    cv = Canvas(16, 16)
    for x, y, r in ((5, 9, 2.6), (11, 4, 2.1)):
        blob(cv, ((x - 2, y + 3), (x - 1, y + 3), (x + 2, y + 3)), GREEN[2])
        hs.ellipse(cv, x + 0.5, y + 0.5, r, r, None, flat=ORANGE[0])
        blob(cv, ((x, y), (x + 1, y + 1)), ORANGE[1])
    return cv


def sc_clover(var):
    """Clover: trefoils of darker leaves, one white flower."""
    cv = Canvas(16, 16)
    for x, y in ((4, 4), (11, 6), (6, 11)):
        for dx, dy in ((0, -1.6), (-1.5, 0.9), (1.5, 0.9)):
            hs.ellipse(cv, x + 0.5 + dx, y + 0.5 + dy, 1.5, 1.5, None, flat=GREEN[2])
        cv.set(x, y, GREEN[1])
    blob(cv, ((12, 12), (13, 12), (12, 13), (13, 13), (12, 11)), PORC[0])
    return cv


def sc_tuft(var):
    """A tuft of longer grass: darker blades."""
    cv = Canvas(8, 8)
    for bx, h, lean in ((1, 4, 1), (3, 6, 0), (4, 5, 1), (6, 4, -1)):
        for k in range(h):
            cv.set(bx + (lean * k) // 3, 7 - k, GREEN[2] if k < h - 1 else GREEN[1])
    return cv


def sc_ants(var):
    """A line of ants marching to the picnic."""
    cv = Canvas(32, 8)
    for k in range(6):
        x, y = 2 + k * 5, 4 + (1 if k % 2 else 0) - (1 if k == 3 else 0)
        blob(cv, ((x, y), (x + 1, y), (x + 3, y)), BROWN[3])
        blob(cv, ((x + 1, y - 1), (x + 2, y + 1)), BROWN[3])
    return cv


def sc_leaf(var):
    """A fallen leaf (var 0 green, 1 autumn)."""
    cv = Canvas(16, 16)
    ramp = [ORANGE[0], ORANGE[1], ORANGE[2]] if var else [GREEN[0], GREEN[1], GREEN[2]]
    ax, ay, bx, by = 3.0, 12.5, 13.0, 3.5
    L = math.hypot(bx - ax, by - ay)
    ux, uy = (bx - ax) / L, (by - ay) / L
    for y in range(16):
        for x in range(16):
            px, py = x + 0.5 - ax, y + 0.5 - ay
            t, s = px * ux + py * uy, -px * uy + py * ux
            if 0 <= t <= L and abs(s) <= 4.2 * math.sin(math.pi * t / L) ** 0.8:
                cv.set(x, y, ramp[2] if abs(s) < 0.6 else ramp[0] if s < 0 else ramp[1])
    blob(cv, ((2, 13), (1, 14)), ramp[2])
    cv.outline(ORANGE[3] if var else GREEN[3])
    return cv


# -- Veggie Patch: the soil
def sprout(cv, x, y):
    blob(cv, ((x + 1, y + 2), (x + 1, y + 3)), GREEN[2])
    hs.ellipse(cv, x + 0.0, y + 1.5, 1.6, 1.2, None, flat=GREEN[1])
    hs.ellipse(cv, x + 3.0, y + 1.0, 1.6, 1.2, None, flat=GREEN[1])
    blob(cv, ((x - 1, y + 1), (x + 2, y)), GREEN[0])


def sc_sprouts(n):
    """Seedlings: n of them in a little row."""
    cv = Canvas(8 * n, 8)
    for k in range(n):
        sprout(cv, 2 + k * 8, 2 + (k % 2))
    cv.outline(GREEN[3])
    return cv


def sc_rosette(var):
    """A young plant: a rosette of five leaves."""
    cv = Canvas(16, 16)
    for k in range(5):
        a = k * 2 * math.pi / 5 - math.pi / 2
        hs.ellipse(cv, 8 + math.cos(a) * 3.4, 8 + math.sin(a) * 3.4, 2.4, 2.4, LETTUCE)
    hs.ellipse(cv, 8.0, 8.0, 1.6, 1.6, None, flat=GREEN[0])
    cv.outline(GREEN[3])
    return cv


def sc_pebbles(var):
    cv = Canvas(8, 8)
    hs.ellipse(cv, 2.5, 2.5, 1.8, 1.5, STONE)
    hs.ellipse(cv, 5.5, 5.5, 1.5, 1.2, STONE)
    cv.outline(STONE[3])
    return cv


def sc_worm(var):
    """An earthworm wriggling along."""
    cv = Canvas(16, 8)
    for k in range(13):
        x, y = 2 + k, 4 + int(round(1.4 * math.sin(k * 0.75)))
        cv.set(x, y, PINK[1] if k % 3 else RED[0])
        cv.set(x, y - 1, PINK[0] if k < 12 else PINK[1])
    cv.outline(RED[2])
    return cv


def sc_chips(var):
    """Wood chips strayed off the path."""
    cv = Canvas(8, 8)
    for x, y in ((1, 1), (4, 3), (2, 5), (5, 6)):
        cv.set(x, y, BROWN[0])
        cv.set(x + 1, y, BROWN[1])
    return cv


def sc_prints(col):
    """A bird's footprints across the soft soil."""
    cv = Canvas(24, 24)
    for k, (x, y) in enumerate(((4, 18), (10, 12), (14, 16), (20, 8))):
        for dx, dy in ((0, 0), (0, 1), (0, 2), (-1, -1), (-2, -2), (1, -1), (2, -2), (0, -1), (0, -2)):
            cv.set(x + dx, y + dy, col)
    return cv


# -- Rainy Patio: the wet flagstones
def sc_wetslab(arg):
    """A slab darker with the rain (the ground of a slab, one shade down, a pale reflection across it). arg: (T,
    ground, x and y of the slab in the cell)."""
    T, ground, ox, oy = arg
    g = T["g"]
    down = {g[0]: g[1], g[1]: g[1], g[2]: g[0], g[3]: g[3]}
    cv = Canvas(32, 32)
    for y in range(32):
        for x in range(32):
            c = ground(T, ox + x, oy + y)
            mx, my = mirror(ox + x), mirror(oy + y)
            if mx in (0, 31) or my in (0, 31):
                cv.set(x, y, c)                             # the joints stay
            elif 7 < x + y * 0.5 < 10 and 4 < y < 26:
                cv.set(x, y, g[2])                          # the sky in it
            else:
                cv.set(x, y, down[c])
    return cv


def sc_ripples(cols):
    """Rain falling in a film of water: two rings spreading (their lit half, their shadowed half)."""
    cv = Canvas(16, 16)
    for cx, cy, rx, ry in ((8, 8, 5.6, 4.0), (8, 8, 2.4, 1.6)):
        for y in range(16):
            for x in range(16):
                d = math.hypot((x + 0.5 - cx) / rx, (y + 0.5 - cy) / ry)
                if abs(d - 1.0) < 0.5 / min(rx, ry) + 0.05:
                    cv.set(x, y, cols[0] if y + 0.5 < cy else cols[1])
    return cv


def sc_moss(var):
    """Moss in the joint between two slabs (the stamp sits on a joint: its x 7 and 8, or its y 7 and 8: var 1)."""
    cv = Canvas(16, 16)
    rnd = random.Random(90 + var)
    for a in range(2, 14):
        for b in (7, 8):
            if rnd.random() < 0.8:
                cv.set(b, a, GREEN[2]) if var == 0 else cv.set(a, b, GREEN[2])
        if rnd.random() < 0.35:
            b = 6 if rnd.random() < 0.5 else 9
            cv.set(b, a, GREEN[1]) if var == 0 else cv.set(a, b, GREEN[1])
    return cv


def sc_wet(cols):
    """A little puddle left by the rain, with its shine (water, shine, outline)."""
    cv = Canvas(24, 16)
    hs.ellipse(cv, 12.0, 8.0, 8.5, 4.0, None, flat=cols[0])
    hs.ellipse(cv, 17.0, 10.0, 3.5, 2.5, None, flat=cols[0])
    blob(cv, ((7, 6), (8, 6), (9, 6), (10, 6), (8, 7)), cols[1])
    cv.outline(cols[2])
    return cv


def sc_meadow(var):
    """A patch of wild flowers: daisies, dandelions and clover together."""
    cv = Canvas(32, 32)
    rnd = random.Random(44)
    for k in range(9):
        x, y = rnd.randint(4, 27), rnd.randint(5, 27)
        blob(cv, ((x - 1, y + 3), (x, y + 3), (x + 1, y + 4), (x - 2, y + 2)), GREEN[2])
    for x, y in ((6, 7), (14, 4), (24, 9), (9, 17), (20, 19), (27, 25), (12, 26), (4, 24)):
        daisy(cv, x, y, (x + y) % 3 != 0)
    for x, y in ((18, 11), (5, 14), (22, 27)):
        hs.ellipse(cv, x + 0.5, y + 0.5, 1.8, 1.8, None, flat=ORANGE[0])
        cv.set(x, y, ORANGE[1])
    return cv


def sc_packet(var):
    """A packet of carrot seeds, dropped: a paper envelope, a carrot drawn on it."""
    cv = Canvas(24, 32)
    for y in range(3, 29):
        for x in range(3, 21):
            cv.set(x, y, (250, 228, 176) if y > 7 else ORANGE[0])
    for y in range(11, 25):                                 # the carrot on the label
        w = (25 - y) * 0.28
        for x in range(12 - int(w), 12 + int(w) + 1):
            cv.set(x, y, ORANGE[1] if x < 12 else ORANGE[2])
    for k in range(4):
        blob(cv, ((11 - k // 2, 10 - k), (13 + k // 2, 10 - k), (12, 10 - k)), GREEN[1])
    for x in range(5, 19, 2):
        cv.set(x, 5, ORANGE[2])
    cv.outline(BROWN[3])
    return cv


def sc_flowers(var):
    """Flower heads fallen from the pots."""
    cv = Canvas(16, 16)
    for x, y, cols in ((4, 5, (PINK[0], ORANGE[0])), (11, 10, (ORANGE[0], ORANGE[1]))):
        for dx, dy in ((0, -2), (2, 0), (0, 2), (-2, 0)):
            hs.ellipse(cv, x + 0.5 + dx * 0.8, y + 0.5 + dy * 0.8, 1.5, 1.5, None, flat=cols[0])
        cv.set(x, y, cols[1])
    return cv


# per theme: (stamp, its argument, x and y in the cell (multiples of 8), how many, the orientations: 0 as drawn,
# 1 flipped left-right, 2 upside down, 3 both)
ANY4, UPRIGHT, AS_DRAWN = (0, 1, 2, 3), (0, 1), (0,)


def scenery(theme, T, ground):
    if theme == "TH_TEA":
        return [(sc_napkin, 0, 16, 16, 6, AS_DRAWN), (sc_doily, 0, 24, 24, 4, AS_DRAWN), (sc_teabag, 0, 24, 40, 4, AS_DRAWN),
                (sc_crumbs, 0, 8, 40, 18, ANY4), (sc_stain, 0, 40, 8, 14, ANY4),
                (sc_petals, (PINK[0], PINK[1]), 24, 8, 24, ANY4)]
    if theme == "TH_DESK":
        return [(sc_drawing, 0, 8, 16, 3, AS_DRAWN), (sc_sticky, 0, 32, 8, 5, AS_DRAWN), (sc_star, 0, 24, 24, 8, AS_DRAWN),
                (sc_heart, 0, 40, 40, 6, AS_DRAWN),
                (sc_shavings, (RED[1], RED[2], RED[3], PORC[1]), 40, 8, 10, ANY4),
                (sc_shavings, (BLUEW[1], BLUEW[2], BLUEW[3], BLUEW[0]), 8, 40, 10, ANY4),
                (sc_scribble, GREEN[2], 32, 40, 5, ANY4), (sc_scribble, ORANGE[1], 8, 8, 5, ANY4),
                (sc_paper, ((255, 255, 247), (236, 96, 84), T["g"][3]), 48, 24, 5, ANY4),
                (sc_splat, (BLUEW[1], BLUEW[2]), 16, 32, 8, ANY4), (sc_splat, (PINK[0], PINK[1]), 40, 48, 8, ANY4),
                (sc_bits, (PINK[0], PINK[1]), 24, 48, 14, ANY4)]
    if theme == "TH_PICNIC":
        return [(sc_meadow, 0, 0, 0, 10, ANY4), (sc_ants, 0, 16, 24, 4, ANY4), (sc_daisies, 1, 32, 32, 18, ANY4),
                (sc_daisies, 0, 8, 8, 20, ANY4), (sc_dandelions, 0, 40, 8, 16, ANY4), (sc_clover, 0, 8, 40, 20, ANY4),
                (sc_leaf, 1, 40, 0, 14, ANY4), (sc_leaf, 0, 24, 48, 12, ANY4),
                (sc_tuft, 0, 32, 16, 40, UPRIGHT), (sc_tuft, 0, 16, 40, 40, UPRIGHT), (sc_tuft, 0, 48, 0, 30, UPRIGHT)]
    if theme == "TH_VEG":
        return [(sc_packet, 0, 16, 8, 2, AS_DRAWN), (sc_prints, T["g"][3], 32, 32, 4, ANY4), (sc_sprouts, 3, 8, 8, 10, UPRIGHT), (sc_sprouts, 3, 16, 48, 10, UPRIGHT),
                (sc_rosette, 0, 40, 40, 14, ANY4), (sc_sprouts, 1, 24, 40, 12, UPRIGHT), (sc_sprouts, 1, 48, 16, 12, UPRIGHT), (sc_worm, 0, 32, 8, 6, ANY4),
                (sc_pebbles, 0, 24, 24, 16, ANY4), (sc_chips, 0, 48, 24, 20, ANY4), (sc_leaf, 1, 8, 40, 10, ANY4)]
    if theme == "TH_PATIO":
        return [(sc_wetslab, (T, ground, 0, 0), 0, 0, 16, ANY4), (sc_wet, (BLUEW[1], T["g"][2], BLUEW[3]), 8, 40, 12, UPRIGHT),
                (sc_moss, 0, 24, 40, 18, ANY4), (sc_moss, 1, 40, 24, 18, ANY4), (sc_flowers, 0, 40, 8, 10, ANY4),
                (sc_leaf, 0, 8, 8, 12, ANY4), (sc_leaf, 1, 16, 24, 8, ANY4), (sc_petals, (TERRA[0], TERRA[1]), 8, 24, 16, ANY4)]
    return []

def flipped(cv, hf, vf):
    out = Canvas(cv.w, cv.h)
    for y in range(cv.h):
        for x in range(cv.w):
            out.p[y][x] = cv.p[cv.h - 1 - y if vf else y][cv.w - 1 - x if hf else x]
    return out


def scatter(wd, t, T, ground, keep_out):
    """Put the theme's stamps down where the ground alone is round them (4 px of it on every side): not on or near the
    band, the zones, the blanket, the objects and their shadows (keep_out: boxes), the table's rim or the garden's
    frame; spread out (a stamp's copies apart). Returns how many."""
    x0, y0, x1, y1 = t.rect if t.table else (0, 0, t.w, t.h)
    edge = 7 if t.table else 16
    sx, sy, _d = td.start_line(t)                           # the start's view stays calm: the title's texts, the lights
    placed = list(keep_out) + [(sx - 300, sy - 130, sx + 300, sy + 150)]
    n = 0
    for si, (fn, arg, ox, oy, count, orients) in enumerate(scenery(t.theme, T, ground)):
        base = fn(arg)
        flips = {o: flipped(base, o & 1, o >> 1) for o in orients}
        cands = [(cx, cy, o) for cy in range(t.gh) for cx in range(t.gw) for o in orients]
        random.Random(1000 + 37 * si + 7 * len(t.name) + t.gw).shuffle(cands)
        apart = 0.6 * math.sqrt((x1 - x0) * (y1 - y0) / float(count))
        mine = []
        for cx, cy, o in cands:
            if len(mine) >= count:
                break
            hf, vf = o & 1, o >> 1
            x = cx * CELL + (CELL - ox - base.w if hf else ox)
            y = cy * CELL + (CELL - oy - base.h if vf else oy)
            bx0, by0, bx1, by1 = x - 4, y - 4, x + base.w + 4, y + base.h + 4
            if bx0 < x0 + edge or by0 < y0 + edge or bx1 > x1 - edge or by1 > y1 - edge:
                continue
            if any(math.hypot(x - mx, y - my) < apart for mx, my in mine):
                continue
            if any(bx0 < q[2] + 4 and q[0] - 4 < bx1 and by0 < q[3] + 4 and q[1] - 4 < by1 for q in placed):
                continue
            if any(wd.get(xx, yy) is None or wd.get(xx, yy) != ground(T, xx, yy)
                   for yy in range(by0, by1) for xx in range(bx0, bx1)):
                continue
            paste(wd, flips[o], x, y)
            placed.append((x, y, x + base.w, y + base.h))
            mine.append((x, y))
            n += 1
    return n


# ---- a track's world ----------------------------------------------------------------------------------------------------------
def zone_colour(T, z, x, y):
    surf, shape, zx, zy, rx, ry = z
    pal = T.get("jam") if surf == "SF_JAM" else T.get("water")
    dx, dy = (x + 0.5 - zx) / rx, (y + 0.5 - zy) / ry
    if shape == "ZS_RECT":
        d = max(abs(dx), abs(dy))
    else:
        d = math.hypot(dx, dy)
    wob = 0.06 * math.sin(math.atan2(dy, dx) * 5 + zx)      # a blob, not a perfect ellipse
    if d > 1.0 + wob:
        return None
    edge = 1.0 + wob - d
    if edge * min(rx, ry) < 1.2:
        return pal[3]
    hx, hy = dx + 0.35, dy + 0.45                           # the shine, top-left
    if hx * hx + hy * hy < 0.05 or (abs(dy + 0.3) < 0.08 and -0.5 < dx < 0.1):
        return pal[0]
    return pal[1] if (d < 0.75 or surf == "SF_WATER") else pal[2]


def render_track(t, kinds):
    T = THEMES[t.theme]
    ground = GROUNDS[t.theme]
    wd = World(t.w, t.h)
    x0, y0, x1, y1 = t.rect
    for y in range(t.h):
        row = wd.p[y]
        for x in range(t.w):
            if t.table and not (x0 <= x < x1 and y0 <= y < y1):
                continue                                    # off the table: BG4 shows through
            row[x] = ground(T, x, y)
    # the picnic blanket (under everything else)
    if t.theme == "TH_PICNIC":
        B = T["blanket"]
        for y in range(272, 688):
            for x in range(400, 944):
                a, b = ((x - 400) // 12) % 2, ((y - 272) // 12) % 2
                col = B[0] if (a and b) else B[1] if not (a or b) else B[2]
                if x in (400, 943) or y in (272, 687):
                    col = B[0]
                wd.p[y][x] = col
    # the band
    for y in range(t.h):
        for x in range(t.w):
            if wd.p[y][x] is None:
                continue
            b = td.band(t, x, y)
            if b is not None and abs(b[0]) <= HW:
                wd.p[y][x] = band_colour(T, t.theme, b[0], x, y)
    # the start line: a chequered strip across the band, 6 px, at the start cell's entry edge
    sx, sy, d = td.start_line(t)
    for k in range(-HW + 3, HW - 2):
        for j in range(6):
            col = T["e"][1] if ((k + 30) // 3 + j // 3) % 2 else T["t"][0]
            if d % 2 == 0:
                wd.set(sx + (j if d == 0 else -1 - j), sy + k, col)
            else:
                wd.set(sx + k, sy + (j if d == 1 else -1 - j), col)
    # the zones
    for z in t.zones:
        surf, shape, zx, zy, rx, ry = z
        for y in range(zy - ry - 2, zy + ry + 3):
            for x in range(zx - rx - 2, zx + rx + 3):
                if wd.get(x, y) is None:
                    continue
                c = zone_colour(T, z, x, y)
                if c is not None:
                    wd.set(x, y, c)
    # the objects: the shadows first, then the objects
    drawn = []
    cache = {}
    for kind, var, ox, oy in t.objs:
        key = (kind, var)
        if key not in cache:
            cache[key] = OBJ_DRAW[kind](var)
        cv = cache[key]
        drawn.append((kind, cv, ox - cv.w // 2, oy - cv.h // 2))
    # the scenery, clear of the objects and their shadows
    scatter(wd, t, T, ground, [(x, y, x + cv.w + 3, y + cv.h + 3) for kind, cv, x, y in drawn])
    for kind, cv, x, y in drawn:
        sh = 1 if kind in FLAT else 3
        shadow(wd, cv, x, y, T["shadow"], sh, sh)
    for kind, cv, x, y in drawn:
        paste(wd, cv, x, y)
    # the table's rim, or the garden's frame
    if t.table:
        R = T["rim"]
        for y in range(y0, y1):
            for x in range(x0, x1):
                e = min(x - x0, y - y0, x1 - 1 - x, y1 - 1 - y)
                if e < 3:
                    wd.p[y][x] = R[1] if e < 2 else R[0]
    else:
        F = T["frame"]
        wi = 12
        for y in range(t.h):
            for x in range(t.w):
                e = min(x, y, t.w - 1 - x, t.h - 1 - y)
                if e >= wi:
                    continue
                if t.theme == "TH_PICNIC":                  # a hedge: leafy bumps
                    k = h2(mirror(x, 16), mirror(y, 16), 51)
                    col = F[2] if k < 50 else F[1] if (k > 200 or e == wi - 1) else F[0]
                elif t.theme == "TH_VEG":                   # planks
                    col = F[1] if e in (0, wi - 1) or (e == 6) else F[2] if e == 1 else F[0]
                else:                                       # bricks
                    along = x if (y < wi or y >= t.h - wi) else y
                    mortar = e in (0, 6) or (along + (8 if e > 6 else 0)) % 16 == 0
                    col = (210, 200, 190) if mortar else F[2] if e in (1, 7) else F[0]
                wd.p[y][x] = col
    return wd


def render_far(t):
    T = THEMES[t.theme]
    f = T["far"]
    cv = Canvas(64, 64)
    for y in range(64):
        for x in range(64):
            mx, my = mirror(x), mirror(y)
            if t.theme == "TH_DESK":                        # a carpet's diamonds
                d = (mx + my) % 16
                col = f[3] if d == 0 else f[2] if d == 8 else f[0] if h2(mx, my, 61) > 40 else f[1]
            elif t.theme == "TH_PATIO":
                col = f[3] if mx in (0, 31) or my in (0, 31) else f[0] if h2(mx, my, 63) > 30 else f[1]
            else:
                k = h2(mx, my, 67)
                col = f[3] if k < 30 else f[1] if k < 80 else f[2] if k > 240 else f[0]
            cv.set(x, y, col)
    return cv


# ---- the ladybugs (OBJ 0, 1, 4, 5: one palette each; drawn in player 1's, the red) --------------------------------------
# palette indices: 1 outline, 2 white, 3 ink, 4 head shine, 5-8 the shell (highlight, light, mid, dark), 9-10 the
# wings (light, veins), 11 legs
RAMPS = {
    "red":    ((70, 14, 36), [(255, 172, 150), (242, 82, 70), (202, 40, 46), (142, 22, 40)]),
    "yellow": ((96, 54, 18), [(255, 248, 176), (252, 214, 64), (226, 164, 28), (164, 104, 20)]),
    "blue":   ((22, 30, 84), [(180, 216, 255), (86, 152, 242), (48, 100, 206), (30, 62, 150)]),
    "green":  ((18, 62, 38), [(200, 248, 154), (112, 208, 86), (60, 160, 64), (34, 110, 52)]),
}
COLOURS = ["red", "yellow", "blue", "green"]
HEAD_SHINE = (88, 84, 112)
WING = [(226, 238, 248), (150, 170, 200)]
LEGS = (60, 44, 52)


def bug_palette(colour):
    out, r4 = RAMPS[colour]
    return [out, WHITE, INK, HEAD_SHINE] + list(r4) + WING + [LEGS]


SHEET_BUG = bug_palette("red")
SPOTS = [(0.7, 2.5), (-3.0, 2.5)]                     # u (forward), |v|: two spots on each wing case


def ladybug(deg, hop=False, scale=1.0, size=16):
    """A ladybug seen from above, heading `deg` degrees (0 east, 90 south). Sampled in its own frame (u forward, v
    right), shaded by the house light in screen space. The front reads at every heading: a black head and pronotum
    with the two white patches of a seven-spot ladybird; the spots are crisp 2 x 2 dots."""
    out, r4 = RAMPS["red"]
    cv = Canvas(size, size)
    c = size / 2.0
    a = math.radians(deg)
    ca, sa = math.cos(a), math.sin(a)
    k = 1.0 / scale
    ANG = 0.32                                              # the wing cases opened on a hop

    def local(x, y):
        dx, dy = (x + 0.5 - c) * k, (y + 0.5 - c) * k
        return dx * ca + dy * sa, -dx * sa + dy * ca        # u forward, v to the right

    def screen_n(nu, nv):
        return nu * ca - nv * sa, nu * sa + nv * ca

    for y in range(size):
        for x in range(size):
            u, v = local(x, y)
            col = None
            if hop:                                          # the wings spread behind and to the sides
                for s in (-1, 1):
                    wu, wv = u + 2.0, v - s * 4.6
                    ru = wu * math.cos(0.55) + s * wv * math.sin(0.55)
                    rv = -s * wu * math.sin(0.55) + wv * math.cos(0.55)
                    if (ru / 6.4) ** 2 + (rv / 2.5) ** 2 <= 1.0:
                        col = WING[1] if (abs(rv) < 0.5 or (ru < -3 and abs(rv) < 1.2)) else WING[0]
            # the shell: two elytra (opened on a hop)
            for s in ((-1, 1) if hop else (0,)):
                if hop:
                    eu, ev = u + 0.6, v - s * 2.2
                    ru = eu * math.cos(ANG) + s * ev * math.sin(ANG)
                    rv = -s * eu * math.sin(ANG) + ev * math.cos(ANG)
                    inside = (ru / 5.4) ** 2 + (rv / 2.7) ** 2 <= 1.0 and s * rv > -2.7
                    nu, nv = ru / 5.4, (rv + s * 1.2) / 3.6
                else:
                    eu, ev = u + 1.1, v
                    inside = (eu / 5.3) ** 2 + (ev / 4.9) ** 2 <= 1.0
                    nu, nv = eu / 5.3, ev / 4.9
                if not inside:
                    continue
                sx, sy = screen_n(nu, nv)
                nd = sx * hs.LIGHT_DIR[0] + sy * hs.LIGHT_DIR[1] - 0.25 * (nu * nu + nv * nv) + 0.15
                col = hs.shade_nd(nd, r4)
                if not hop and -0.48 < v <= 0.52 and u < 2.4:   # the line between the elytra (one pixel: v = 0.5)
                    col = INK
            # the pronotum and the head in front (black), the white patches on its sides
            if ((u - 4.2) / 2.5) ** 2 + (v / 3.4) ** 2 <= 1.0 and u > 2.5 + 0.13 * v * v:
                col = INK
                if u > 5.0 and v < 0 and screen_n(0.0, -1.0)[0] * hs.LIGHT_DIR[0] + \
                        screen_n(0.0, -1.0)[1] * hs.LIGHT_DIR[1] > 0.2 and abs(v) < 1.0:
                    col = HEAD_SHINE
            if col is not None:
                cv.set(x, y, col)
    # the white patches on the pronotum and the spots: 2 x 2 dots on the pixel grid, the spots either side of the line (v = 0.5)
    def corner(pu, pv):
        return (int(math.floor(c + (pu * ca - pv * sa) * scale + 0.5)),
                int(math.floor(c + (pu * sa + pv * ca) * scale + 0.5)))

    for pv in (-2.0, 2.0):                                 # (the patches either side of the head's middle)
        gx, gy = corner(3.9, pv)
        for py in (gy - 1, gy):
            for px in (gx - 1, gx):
                if cv.get(px, py) == INK:
                    cv.set(px, py, WHITE)
    for su, sv in SPOTS:
        for s in (-1, 1):
            if hop:                                         # on the opened wing case
                ru, rv = su + 0.6, s * sv - s * 2.2
                eu = ru * math.cos(ANG) - s * rv * math.sin(ANG)
                ev = s * ru * math.sin(ANG) + rv * math.cos(ANG)
                pu, pv = eu - 0.6, ev + s * 2.2
            else:
                pu, pv = su, 0.5 + s * sv
            gx, gy = corner(pu, pv)
            for py in (gy - 1, gy):
                for px in (gx - 1, gx):
                    if cv.get(px, py) in r4:
                        cv.set(px, py, INK)
    # the glint: top-left of the shell
    best = None
    for y in range(size):
        for x in range(size):
            if cv.p[y][x] in r4[:2]:
                sc = x + y
                if best is None or sc < best[0]:
                    best = (sc, x, y)
    if best and cv.get(best[1] + 1, best[2] + 1) in r4:
        cv.set(best[1] + 1, best[2] + 1, WHITE)
    cv.outline(out)
    return cv

def mini_bug(size, heading_deg):
    """A tiny ladybug (the HUD icons, falling away)."""
    out, r4 = RAMPS["red"]
    cv = Canvas(size, size)
    c = size / 2.0
    a = math.radians(heading_deg)
    r = size / 2.0 - 1.2
    for y in range(size):
        for x in range(size):
            dx, dy = x + 0.5 - c, y + 0.5 - c
            u, v = dx * math.cos(a) + dy * math.sin(a), -dx * math.sin(a) + dy * math.cos(a)
            if (u / r) ** 2 + (v / (r * 0.9)) ** 2 <= 1.0:
                col = r4[1] if (dx + dy) < -r * 0.4 else r4[2]
                if u > r * 0.45:
                    col = INK
                elif size >= 8 and abs(v) > r * 0.25 and abs(v) < r * 0.75 and abs(u + r * 0.15) < r * 0.3:
                    col = INK
                cv.set(x, y, col)
    cv.outline(out)
    return cv


BUG_ANGLES = [i * 11.25 for i in range(9)]


def ladybugs():
    """name -> canvas, the sheet of the ladybugs (in the red palette)."""
    out = {}
    for i, d in enumerate(BUG_ANGLES):
        out["bug_%d" % i] = ladybug(d)
        out["hop_%d" % i] = ladybug(d, hop=True, scale=1.18, size=24)
    out["fall_0"] = mini_bug(8, 0)
    out["fall_1"] = mini_bug(8, 45)
    out["icon"] = mini_bug(8, -90)
    return out


# ---- the props (OBJ 2) --------------------------------------------------------------------------------------------------------
PROPS_PAL = [(22, 18, 40), WHITE, (255, 246, 220), INK, (36, 32, 64), (232, 216, 184), (190, 168, 130),
             (214, 238, 252), (130, 190, 236), (234, 74, 66), (150, 30, 40), (98, 208, 98), (252, 214, 72),
             (150, 156, 176), (84, 122, 202)]


def pp(i):
    return PROPS_PAL[i - 1]


MINI_FONT = {"P": ["111", "101", "111", "100", "100"], "1": ["010", "110", "010", "010", "111"],
             "2": ["111", "001", "111", "100", "111"], "3": ["111", "001", "111", "001", "111"],
             "4": ["101", "101", "111", "001", "001"], "!": ["1", "1", "1", "0", "1"],
             "+": ["000", "010", "111", "010", "000"]}


def mini_text(cv, x0, y0, s, col):
    for ch in s:
        g = MINI_FONT[ch]
        for y, row in enumerate(g):
            for x, b in enumerate(row):
                if b == "1":
                    cv.set(x0 + x, y0 + y, col)
        x0 += len(g[0]) + 1


def prop_shadow():
    cv = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            if ((x + 0.5 - 8) / 6.4) ** 2 + ((y + 0.5 - 8) / 5.6) ** 2 <= 1.0 and hs.checker(x, y):
                cv.set(x, y, pp(5))
    return cv


def prop_puff(frame, cols):
    cv = Canvas(8, 8)
    r = 1.6 + frame * 0.9
    for y in range(8):
        for x in range(8):
            d = math.hypot(x + 0.5 - 4, y + 0.5 - 4)
            if d <= r and (frame < 2 or hs.checker(x, y)):
                cv.set(x, y, cols[0] if (x + y) < 7 else cols[1])
    return cv


def prop_splash(frame):
    cv = Canvas(8, 8)
    pts = [[(3, 3), (4, 3), (3, 4), (4, 4)], [(1, 2), (6, 2), (2, 5), (5, 5), (3, 1)], [(0, 1), (7, 1), (1, 6), (6, 6), (3, 0)]]
    for x, y in pts[frame]:
        cv.set(x, y, pp(8) if (x + y) % 2 else pp(9))
    return cv


def prop_spark(frame):
    cv = Canvas(8, 8)
    r = 3 if frame == 0 else 2
    for k in range(-r, r + 1):
        cv.set(4 + k, 4, pp(13))
        cv.set(4, 4 + k, pp(13))
    cv.set(4, 4, WHITE)
    if frame == 0:
        for d in (-1, 1):
            cv.set(4 + d * 2, 4 + d * 2, pp(13)), cv.set(4 + d * 2, 4 - d * 2, pp(13))
    return cv


def prop_warn():
    cv = Canvas(8, 8)
    for y in range(1, 7):
        for x in range(1, 7):
            if not ((x in (1, 6)) and (y in (1, 6))):
                cv.set(x, y, pp(10))
    mini_text(cv, 3, 1, "!", WHITE)
    cv.set(3, 6, pp(10))
    cv.outline(pp(1))
    return cv


def prop_tag(n):
    cv = Canvas(16, 8)
    for y in range(1, 7):
        for x in range(1, 13):
            if not ((x in (1, 12)) and (y in (1, 6))):
                cv.set(x, y, pp(3))
    for x in range(5, 9):                                   # a little point down to the ladybug
        cv.set(x, 7, pp(3)) if x in (6, 7) else None
    mini_text(cv, 3, 1, "P%d" % n, pp(4))
    cv.outline(pp(1))
    return cv


def prop_lamp(state):
    cv = Canvas(16, 16)
    hs.ellipse(cv, 8, 8, 7.0, 7.0, None, flat=(48, 44, 70))
    col = [pp(14), pp(10), pp(12)][state]
    hs.ellipse(cv, 8, 8, 5.0, 5.0, None, flat=col)
    if state:
        cv.set(6, 6, WHITE), cv.set(7, 6, WHITE), cv.set(6, 7, WHITE)
    cv.outline(pp(1))
    return cv


def prop_flag(frame):
    cv = Canvas(16, 16)
    for y in range(1, 16):
        cv.set(2, y, pp(14))
    for y in range(2, 10):
        for x in range(3, 15):
            yy = y + (int(1.5 * math.sin((x + frame * 3) / 3.0)))
            cv.set(x, yy, INK if ((x - 3) // 3 + (y - 2) // 3) % 2 else WHITE)
    cv.outline(pp(1))
    return cv


def disc_quarter(R, Q, pressed):
    """A quarter of the round touch button (the apps' drawn buttons: a see-through mid-blue disc, the house checker;
    pressed: solid), a cream ring, a navy outline."""
    cv = Canvas(Q, Q)
    for y in range(Q):
        for x in range(Q):
            d = math.hypot(x + 0.5 - Q, y + 0.5 - Q)
            if d <= R - 2:
                if pressed:
                    cv.set(x, y, pp(15))
                elif hs.checker(x, y):
                    cv.set(x, y, pp(15))
            elif d <= R:
                cv.set(x, y, pp(3))
            elif d <= R + 1:
                cv.set(x, y, pp(1))
    return cv


def stick_quarter(R=22):
    Q = 24
    cv = Canvas(Q, Q)
    for y in range(Q):
        for x in range(Q):
            d = math.hypot(x + 0.5 - Q, y + 0.5 - Q)
            if R - 2 < d <= R:
                cv.set(x, y, pp(3))
            elif d <= R - 2 and hs.checker(x, y) and (x + y) % 4 == 0:
                cv.set(x, y, pp(15))
            elif R < d <= R + 1:
                cv.set(x, y, pp(1))
    return cv


def knob():
    cv = Canvas(16, 16)
    hs.ellipse(cv, 8.0, 8.0, 6.5, 6.5, [WHITE, pp(3), pp(14), pp(15)])
    cv.outline(pp(1))
    return cv


def icon_brake():
    """BRAKE: a red disc with two white bars (the brake lights)."""
    cv = Canvas(16, 16)
    hs.ellipse(cv, 8, 8, 6.5, 6.5, None, flat=pp(10))
    for x in range(4, 12):
        cv.set(x, 6, WHITE), cv.set(x, 9, WHITE)
    cv.outline(pp(1))
    return cv


def prop_plus():
    """+1 rising over the leader when it takes a point."""
    cv = Canvas(16, 8)
    mini_text(cv, 4, 1, "+1", pp(13))
    cv.outline(pp(1))
    return cv


def props():
    out = {"shadow": prop_shadow(), "warn": prop_warn(), "stick": stick_quarter(), "knob": knob(), "plus": prop_plus(),
           "icon_brake": icon_brake(), "btn": disc_quarter(22, 24, False), "btn_on": disc_quarter(22, 24, True)}
    for f in range(3):
        out["dust_%d" % f] = prop_puff(f, [pp(6), pp(7)])
        out["splash_%d" % f] = prop_splash(f)
        out["lamp_%d" % f] = prop_lamp(f)
    for f in range(2):
        out["spark_%d" % f] = prop_spark(f)
        out["flag_%d" % f] = prop_flag(f)
    for n in range(1, 5):
        out["tag_%d" % n] = prop_tag(n)
    return out


# ---- the canopy (OBJ 6): leaves over the track, see-through in the middle (the house checker) ------------------------------
LEAVES_PAL = [(40, 82, 36), (172, 222, 112), (122, 186, 78), (82, 142, 58), (204, 238, 152), (252, 216, 112),
              (238, 172, 72), (192, 112, 42), (122, 66, 26), (152, 216, 98), (88, 152, 62)]


def lp(i):
    return LEAVES_PAL[i - 1]


def leaf(autumn):
    cv = Canvas(32, 32)
    ramp = [lp(6), lp(7), lp(8)] if autumn else [lp(2), lp(3), lp(4)]
    vein = lp(6) if autumn else lp(5)
    edge = lp(9) if autumn else lp(1)
    # a leaf along the diagonal, from (4, 27) (the stem) to (28, 4)
    ax, ay, bx, by = 6.0, 26.0, 28.0, 4.0
    L = math.hypot(bx - ax, by - ay)
    ux, uy = (bx - ax) / L, (by - ay) / L
    for y in range(32):
        for x in range(32):
            px, py = x + 0.5 - ax, y + 0.5 - ay
            t = px * ux + py * uy
            s = -px * uy + py * ux
            if t < 0 or t > L:
                continue
            w = 9.5 * math.sin(math.pi * t / L) ** 0.8
            if abs(s) > w:
                continue
            inner = abs(s) < w - 2.2 and 2 < t < L - 3
            if abs(s) < 0.6 or (inner and abs(abs(s) - (t % 7) * 0.9) < 0.5 and t > 4):
                col = vein
            elif inner:
                if not hs.checker(x, y):                    # see-through: the ladybug shows under it
                    continue
                col = ramp[1] if s < 0 else ramp[2]
            else:
                col = ramp[0] if s < 0 else ramp[1]
            cv.set(x, y, col)
    for k in range(5):                                      # the stem
        cv.set(int(ax - k * ux * 0.8), int(ay - k * uy * 0.8) + 1, edge)
    cv.outline(edge)
    return cv


def frond():
    cv = Canvas(32, 32)
    rnd = random.Random(77)
    for k in range(5):                                      # feathery carrot leaves from the root at the centre
        a = -math.pi / 2 + (k - 2) * 0.55 + rnd.uniform(-0.1, 0.1)
        for t in range(2, 15):
            x, y = 16 + math.cos(a) * t, 18 + math.sin(a) * t
            cv.set(int(x), int(y), lp(11))
            if t % 3 == 0 and t > 3:
                for s in (-1, 1):
                    for j in range(1, 4):
                        cv.set(int(x + math.cos(a + s * 1.1) * j), int(y + math.sin(a + s * 1.1) * j), lp(10))
    cv.outline(lp(1))
    return cv


def leaves():
    return {"leaf_0": leaf(False), "leaf_1": leaf(True), "leaf_2": frond()}


# ---- sheets ------------------------------------------------------------------------------------------------------------
def pack(sprites, width=256):
    """Shelf-pack named canvases into a sheet: {name: (x, y, w, h)}, height."""
    pos, x, y, row = {}, 0, 0, 0
    for name in sorted(sprites, key=lambda n: (-sprites[n].h, n)):
        cv = sprites[name]
        if x + cv.w > width:
            x, y, row = 0, y + row, 0
        pos[name] = (x, y, cv.w, cv.h)
        x += cv.w
        row = max(row, cv.h)
    return pos, y + row


def sheet(sprites, width=256):
    pos, h = pack(sprites, width)
    im = Image.new("RGB", (width, h), hs.MAGENTA)
    for name, (x, y, w, hh) in pos.items():
        im.paste(sprites[name].image(), (x, y))
    return im, pos


SHEETS = {"ladybugs.png": ladybugs, "props.png": props, "leaves.png": leaves}


def write_sprites(out, preview=False):
    for f, fn in SHEETS.items():
        im, _ = sheet(fn())
        im.save(os.path.join(out, f))
        if preview:
            os.makedirs(os.path.join(out, "preview"), exist_ok=True)
            im.resize((im.width * 4, im.height * 4), Image.NEAREST).save(os.path.join(out, "preview", f))
    return sorted(SHEETS)


def world_image(wd):
    im = Image.new("RGBA", (wd.w, wd.h), (0, 0, 0, 0))
    px = im.load()
    for y in range(wd.h):
        row = wd.p[y]
        for x in range(wd.w):
            if row[x] is not None:
                px[x, y] = row[x] + (255,)
    return im


def write_tracks(out, only=None, preview=False):
    tracks, kinds = td.load()
    os.makedirs(os.path.join(out, "tracks"), exist_ok=True)
    written = []
    for t in tracks:
        if only and t.name != only:
            continue
        wd = render_track(t, kinds)
        im = world_image(wd)
        im.save(os.path.join(out, "tracks", t.name + ".png"))
        far = render_far(t).image()
        far.save(os.path.join(out, "tracks", t.name + "-far.png"))
        written += ["tracks/%s.png" % t.name, "tracks/%s-far.png" % t.name]
        if preview:
            os.makedirs(os.path.join(out, "preview"), exist_ok=True)
            bg = Image.new("RGB", im.size, (60, 60, 60))
            bg.paste(im, (0, 0), im)
            bg.save(os.path.join(out, "preview", t.name + ".png"))
    return written


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", default=os.path.join(GAME, "art"))
    ap.add_argument("--preview", action="store_true", help="also write previews in <out>/preview")
    ap.add_argument("--only", help="one track (tea, desk, picnic, veg, patio), or 'sprites'")
    a = ap.parse_args(argv)
    os.makedirs(a.out, exist_ok=True)
    files = write_sprites(a.out, a.preview) if a.only in (None, "sprites") else []
    if a.only != "sprites":
        files += write_tracks(a.out, a.only, a.preview)
    for f in files:
        print("wrote %s/%s" % (a.out, f))


def write_all(out):
    """Everything (build_assets.py calls it when the art folder is empty)."""
    os.makedirs(out, exist_ok=True)
    return write_sprites(out) + write_tracks(out)


if __name__ == "__main__":
    main()

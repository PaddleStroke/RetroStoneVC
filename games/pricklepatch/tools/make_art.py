#!/usr/bin/env python3
"""Prickle Patch: the code-drawn art, in the 8BCraft house style (docs/art-direction.md).

Code-drawn only, no image generation: every sprite and tile is drawn here with games/common/tools/house_style.py
(1-px outlines outside the shapes, 3-4 flat shades lit from the top-left, squash and stretch). Each sheet uses ONE
fixed 15-colour palette and is written as an indexed PNG (build_assets.py reads the indices as they are):

  soft.png        the swarm: every pest, the three bosses, the small shots, the flying gems, the poofs (BG palette 2:
                  they are drawn into the soft sprite layer by src/soft.c, not by hardware sprites)
  hero_<c>.png    the three gardeners (hedgehog, tortoise, hen): 4 walking frames, 24x24
  tools.png       the weapons' sprites and the level-up icons (OBJ palette 1)
  items.png       the pickups, chests, the passives' icons, the HUD icons (OBJ palette 2)
  fx.png          the see-through effects: the water splash, the prickle ring, the joystick (OBJ palette 4, added)
  ground.png      the garden's 8x8 tiles: grass, flowers, dirt, pebbles, cabbages, the XP gems (BG palette 1)
  palettes.json   the palettes (and the moonlit map's ground palette) and the sheets' layouts
and art/preview/*.png: the sheets at 3x on a grass colour, for review.

    python3 games/pricklepatch/tools/make_art.py [--out DIR]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pricklepatch/LICENSE.
"""
import argparse
import json
import math
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.dirname(HERE)
ROOT = os.path.abspath(os.path.join(GAME, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "games", "common", "tools"))
import house_style as hs  # noqa: E402

Canvas = hs.Canvas

# ---- palettes (index 0 = clear) ---------------------------------------------------------------------------------
# the soft layer (BG palette 2): every pest and shot shares it (they share 8x8 tiles in the soft layer)
SOFT_PAL = [
    (40, 22, 52),     # 1 outline
    (255, 255, 255),  # 2 white
    (255, 236, 180),  # 3 cream
    (250, 200, 50),   # 4 yellow
    (214, 124, 52),   # 5 orange
    (222, 64, 60),    # 6 red
    (122, 40, 46),    # 7 maroon
    (190, 226, 100),  # 8 green light
    (118, 178, 66),   # 9 green mid
    (62, 116, 54),    # 10 green dark
    (160, 150, 246),  # 11 violet light
    (106, 86, 206),   # 12 violet mid
    (62, 44, 134),    # 13 violet dark
    (146, 150, 176),  # 14 grey light
    (70, 70, 98),     # 15 grey dark
]
S = {n: SOFT_PAL[i] for i, n in enumerate(["out", "white", "cream", "yellow", "orange", "red", "maroon", "gl", "gm",
                                            "gd", "vl", "vm", "vd", "greyl", "greyd"])}
# the garden (BG palette 1): grass, dirt, flowers, gems (index 15: the gems' glint, cycled by draw.c)
GROUND_PAL = [
    (30, 56, 48),     # 1 dark (outlines of flowers and gems)
    (70, 128, 62),    # 2 grass dark
    (100, 158, 72),   # 3 grass mid
    (132, 186, 86),   # 4 grass light
    (172, 212, 110),  # 5 grass highlight
    (118, 84, 60),    # 6 dirt dark
    (164, 122, 84),   # 7 dirt light
    (250, 250, 236),  # 8 white
    (250, 206, 60),   # 9 yellow
    (222, 64, 60),    # 10 red
    (160, 150, 246),  # 11 violet light
    (106, 86, 206),   # 12 violet mid
    (122, 40, 46),    # 13 maroon
    (150, 150, 162),  # 14 stone
    (255, 255, 255),  # 15 glint (cycled)
]
# the moonlit garden (the second map): the same tiles, night colours
GROUND_NIGHT = [
    (14, 20, 40), (34, 62, 82), (48, 84, 98), (66, 108, 116), (96, 140, 140), (58, 52, 70), (84, 76, 98),
    (220, 230, 250), (250, 220, 110), (226, 92, 120), (176, 166, 255), (116, 98, 226), (90, 40, 70),
    (104, 112, 138), (255, 255, 255)]
G = {n: GROUND_PAL[i] for i, n in enumerate(["dark", "g0", "g1", "g2", "g3", "dirt0", "dirt1", "white", "yellow",
                                              "red", "vl", "vm", "maroon", "stone", "glint"])}
# OBJ 1: the tools (weapons)
TOOLS_PAL = [
    (40, 22, 40),     # 1 outline
    (255, 255, 255),  # 2 white
    (250, 214, 70),   # 3 yellow
    (196, 132, 30),   # 4 amber
    (40, 30, 40),     # 5 ink (bee stripes)
    (226, 70, 60),    # 6 red
    (140, 36, 50),    # 7 dark red
    (150, 160, 180),  # 8 steel light
    (90, 98, 122),    # 9 steel dark
    (210, 160, 100),  # 10 wood light
    (150, 98, 56),    # 11 wood dark
    (250, 150, 50),   # 12 orange (pumpkin, flame)
    (200, 90, 30),    # 13 orange dark
    (110, 180, 70),   # 14 green
    (210, 236, 255),  # 15 pale blue (wings, water)
]
T = {n: TOOLS_PAL[i] for i, n in enumerate(["out", "white", "yellow", "amber", "ink", "red", "dred", "steel", "steeld",
                                             "wood", "woodd", "orange", "oranged", "green", "pale"])}
# OBJ 2: the items (pickups, chests, passives' icons, HUD icons)
ITEMS_PAL = [
    (40, 22, 40),     # 1 outline
    (255, 255, 255),  # 2 white
    (252, 216, 72),   # 3 gold
    (186, 128, 22),   # 4 gold dark
    (226, 64, 64),    # 5 red
    (140, 30, 50),    # 6 dark red
    (120, 196, 80),   # 7 green
    (56, 120, 56),    # 8 green dark
    (200, 150, 96),   # 9 tan
    (130, 86, 50),    # 10 brown
    (120, 170, 240),  # 11 blue
    (60, 90, 170),    # 12 blue dark
    (180, 184, 200),  # 13 silver
    (250, 236, 200),  # 14 cream
    (246, 150, 170),  # 15 pink
]
I = {n: ITEMS_PAL[i] for i, n in enumerate(["out", "white", "gold", "goldd", "red", "dred", "green", "greend", "tan",
                                             "brown", "blue", "blued", "silver", "cream", "pink"])}
# OBJ 4: the see-through effects (added at half: draw.c rs_math)
FX_PAL = [(30, 60, 120), (60, 110, 180), (100, 160, 230), (150, 210, 255), (220, 244, 255), (70, 140, 60),
          (120, 190, 90), (190, 240, 140), (255, 255, 255), (200, 120, 40), (255, 190, 90), (120, 120, 140),
          (190, 190, 210), (255, 220, 120), (255, 120, 120)]
FX = {n: FX_PAL[i] for i, n in enumerate(["w0", "w1", "w2", "w3", "w4", "t0", "t1", "t2", "white", "f0", "f1", "s0",
                                          "s1", "glow", "pink"])}
# OBJ 0: the gardeners, one palette each (loaded with the character)
HEDGEHOG_PAL = [
    (44, 24, 38),     # 1 outline
    (92, 58, 44),     # 2 spines dark
    (138, 92, 62),    # 3 spines mid
    (184, 132, 88),   # 4 spines light
    (248, 222, 180),  # 5 face cream
    (212, 170, 128),  # 6 face shade
    (26, 14, 26),     # 7 ink
    (255, 255, 255),  # 8 white
    (250, 226, 132),  # 9 straw light
    (206, 160, 72),   # 10 straw dark
    (220, 64, 64),    # 11 band red
    (98, 176, 92),    # 12 apron green
    (52, 112, 66),    # 13 apron dark
    (242, 150, 146),  # 14 pink (nose shine, feet)
    (150, 96, 70),    # 15 feet dark
]
TORTOISE_PAL = [
    (30, 34, 30), (60, 104, 52), (96, 148, 66), (146, 192, 92), (200, 196, 122), (150, 146, 86), (26, 14, 26),
    (255, 255, 255), (200, 120, 64), (140, 78, 44), (240, 196, 88), (110, 160, 220), (60, 96, 160), (242, 150, 146),
    (104, 98, 58)]
HEN_PAL = [
    (48, 28, 40), (250, 250, 244), (222, 214, 200), (176, 160, 150), (230, 60, 56), (150, 30, 44), (26, 14, 26),
    (255, 255, 255), (250, 200, 60), (210, 140, 30), (120, 180, 230), (70, 120, 190), (60, 80, 140), (242, 150, 146),
    (190, 120, 40)]


def ramp4(*names, d=S):
    return [d[n] for n in names]


# ---- the soft layer's art --------------------------------------------------------------------------------------
GREEN = ramp4("cream", "gl", "gm", "gd")
VIOLET = ramp4("white", "vl", "vm", "vd")
REDR = ramp4("orange", "red", "red", "maroon")
YEL = ramp4("cream", "yellow", "yellow", "orange")
GREY = ramp4("greyl", "greyl", "greyd", "greyd")
BROWN = ramp4("cream", "orange", "orange", "maroon")
WING = ramp4("white", "white", "cream", "cream")


def eye(cv, x, y, look=1, big=False):
    """a house eye: white with a pupil on the side it looks at"""
    w, h = (3, 3) if big else (2, 2)
    cv.rect(x, y, x + w - 1, y + h - 1, S["white"])
    px = x + (w - 1 if look > 0 else 0)
    cv.set(px, y + h - 1, S["out"])
    if big:
        cv.set(px, y + 1, S["out"])


def slug(f):
    cv = Canvas(16, 16)
    st = 1 if f else 0
    # the foot: a long low capsule, the head up at the right (it crawls right)
    hs.capsule(cv, 2.5 - st, 12.0, 11.5 + st, 12.0, 2.8, BROWN)
    hs.ellipse(cv, 11.0 + st, 9.0 + st * 0.5, 3.6, 3.4, BROWN)
    for sx, top in ((11 + st, 3), (14 + st, 4)):           # the eye stalks
        for y in range(top + 2, 8):
            cv.set(sx, y, S["maroon"])
        cv.set(sx, top + 1, S["white"]), cv.set(sx + 1, top + 1, S["white"]), cv.set(sx + 1, top + 2, S["out"])
        cv.set(sx, top + 2, S["white"])
    cv.set(13 + st, 9, S["out"]), cv.set(14 + st, 10, S["out"])          # a smile
    for x in range(3, 10, 3):                                           # the slime's shine
        cv.set(x + st, 10, S["cream"])
    cv.outline(S["out"])
    return cv


def aphid(f):
    cv = Canvas(12, 12)
    hs.ellipse(cv, 5.5, 6.5, 4.3, 3.6, GREEN)
    hs.ellipse(cv, 9.0, 5.5, 2.2, 2.0, GREEN)
    for i, x in enumerate((3, 5, 7)):
        cv.set(x, 10, S["gd"])
        cv.set(x + (1 if (i + f) % 2 else 0), 11, S["gd"])
    cv.set(10, 2, S["gd"]), cv.set(11, 1, S["gd"]), cv.set(9, 3, S["gd"])
    cv.set(9, 5, S["out"])
    cv.set(4, 4, S["cream"])
    cv.outline(S["out"])
    return cv


def beetle(f):
    cv = Canvas(16, 16)
    for i, lx in enumerate((4, 7, 10)):                     # legs (behind)
        d = 1 if (i + f) % 2 else -1
        cv.set(lx + d, 13, S["out"]), cv.set(lx, 12, S["out"])
    hs.ellipse(cv, 7.0, 8.5, 6.2, 4.6, VIOLET)
    for x in range(3, 12):                                  # the shell's split
        cv.set(x, 8, S["vd"])
    cv.set(5, 6, S["white"]), cv.set(6, 6, S["vl"])
    hs.ellipse(cv, 13.0, 9.5, 2.6, 2.4, ramp4("greyl", "greyd", "greyd", "out"))
    cv.set(14, 8, S["white"]), cv.set(15, 8, S["out"])
    cv.set(15, 6, S["out"]), cv.set(14, 7, S["out"])        # an antenna
    cv.outline(S["out"])
    return cv


def ant(f):
    cv = Canvas(16, 12)
    for i, lx in enumerate((5, 7, 9)):
        d = 1 if (i + f) % 2 else -1
        cv.set(lx + d, 10, S["out"]), cv.set(lx, 9, S["maroon"])
    hs.ellipse(cv, 3.5, 6.5, 3.2, 2.8, REDR)
    hs.ellipse(cv, 7.5, 6.5, 1.8, 1.6, REDR)
    hs.ellipse(cv, 11.5, 5.5, 2.8, 2.6, REDR)
    eye(cv, 12, 4)
    cv.set(13, 2, S["maroon"]), cv.set(14, 1, S["maroon"]), cv.set(12, 2, S["maroon"])
    cv.set(2, 5, S["orange"])
    cv.outline(S["out"])
    return cv


def wasp(f):
    cv = Canvas(16, 14)
    wy = 2 if f == 0 else 5                                 # wings (up / down)
    hs.ellipse(cv, 6.0, wy + 1.5, 3.4, 2.2, WING)
    hs.ellipse(cv, 7.0, 8.0, 5.6, 3.4, YEL)
    for x in (4, 7):                                        # stripes
        for y in range(5, 12):
            if cv.get(x, y) in (S["yellow"], S["orange"], S["cream"]):
                cv.set(x, y, S["greyd"])
    cv.set(1, 8, S["out"]), cv.set(0, 9, S["out"])          # the sting
    hs.ellipse(cv, 12.5, 7.5, 2.6, 2.6, YEL)
    eye(cv, 12, 6)
    if f:
        hs.ellipse(cv, 7.0, 4.0, 3.0, 1.8, WING)
    cv.outline(S["out"])
    return cv


def thistle(f):
    cv = Canvas(20, 24)
    for y in range(11, 21):                                 # the stem body
        for x in range(8, 12):
            cv.set(x, y, S["gm"] if x < 10 else S["gd"])
    arm = 1 if f else -1
    for i in range(5):                                      # two leaf arms, waving
        cv.set(7 - i, 14 - i // 2 + (arm if i > 2 else 0), S["gm"])
        cv.set(12 + i, 14 - i // 2 - (arm if i > 2 else 0), S["gd"])
        cv.set(7 - i, 15 - i // 2 + (arm if i > 2 else 0), S["gd"])
        cv.set(12 + i, 15 - i // 2 - (arm if i > 2 else 0), S["gd"])
    cv.set(8, 21 + f, S["gd"]), cv.set(7, 22, S["gd"]), cv.set(11, 22 - f, S["gd"]), cv.set(12, 22, S["gd"])
    hs.ellipse(cv, 10.0, 8.5, 4.6, 3.6, GREEN)              # the flower head: a cup, a spiky crown, a grumpy face
    for i in range(7):
        x = 5 + i * 1.6
        for y in range(1 + (i % 2), 6):
            cv.set(int(x), y, S["vm"] if i % 2 else S["vl"])
    eye(cv, 9, 8), eye(cv, 12, 8)
    cv.set(10, 11, S["out"]), cv.set(11, 11, S["out"])
    cv.outline(S["out"])
    return cv


def crow(f):
    cv = Canvas(24, 20)
    if f == 0:                                              # the wing up
        hs.ellipse(cv, 9.0, 4.5, 6.0, 3.0, GREY)
    hs.ellipse(cv, 10.0, 11.0, 7.5, 5.5, GREY)
    hs.ellipse(cv, 17.0, 7.5, 4.0, 3.8, GREY)
    for i in range(4):                                      # the tail
        cv.set(2 - i // 2, 9 + i, S["greyd"]), cv.set(3 - i // 2, 9 + i, S["greyd"])
    for i in range(3):                                      # the beak
        cv.set(21 + i, 8, S["yellow"])
    cv.set(21, 9, S["orange"]), cv.set(22, 9, S["orange"])
    eye(cv, 17, 6, big=True)
    if f == 1:                                              # the wing down
        hs.ellipse(cv, 9.0, 15.5, 6.0, 2.6, GREY)
    cv.set(9, 17 + f, S["orange"]), cv.set(12, 17 + f, S["orange"])
    cv.outline(S["out"])
    return cv


def locust(f):
    cv = Canvas(18, 14)
    hs.ellipse(cv, 8.0, 8.0, 6.5, 2.8, YEL)
    hs.ellipse(cv, 14.0, 6.5, 2.6, 2.6, YEL)
    if f == 0:                                              # the hind leg folded (a big jumper) ...
        for i in range(5):
            cv.set(6 + i, 5 - i // 3, S["maroon"])
        for i in range(4):
            cv.set(8 - i // 2, 6 + i, S["maroon"])
    else:                                                   # ... and kicked out
        for i in range(7):
            cv.set(9 - i, 9 + i // 2, S["maroon"])
    cv.set(4, 11, S["maroon"]), cv.set(11, 11, S["maroon"])
    for x in range(3, 12, 2):
        cv.set(x, 6, S["cream"])
    eye(cv, 14, 5)
    cv.set(16, 2, S["orange"]), cv.set(17, 1, S["orange"]), cv.set(15, 3, S["orange"])
    cv.outline(S["out"])
    return cv


def crown(cv, cx, top, w=9):
    for i in range(w):
        x = cx - w // 2 + i
        h = 3 if i % 4 == 0 else 1
        for y in range(top + 3 - h, top + 5):
            cv.set(x, y, S["yellow"])
        cv.set(x, top + 5, S["orange"])
    cv.set(cx - w // 2 + 1, top + 4, S["red"]), cv.set(cx + w // 2 - 1, top + 4, S["red"])
    cv.set(cx, top + 4, S["vl"])


def queen(f):
    cv = Canvas(44, 36)
    st = 2 if f else 0
    hs.capsule(cv, 6.0 - st, 28.0, 30.0 + st, 28.0, 6.5, BROWN)
    hs.ellipse(cv, 30.0 + st, 21.0 + st * 0.5, 9.5, 9.0, BROWN)
    for sx, top in ((27 + st, 2), (35 + st, 4)):
        for y in range(top + 4, 14):
            cv.set(sx, y, S["maroon"]), cv.set(sx + 1, y, S["orange"])
        cv.rect(sx - 1, top, sx + 2, top + 3, S["white"])
        cv.set(sx + 2, top + 2, S["out"]), cv.set(sx + 2, top + 3, S["out"]), cv.set(sx + 1, top + 3, S["out"])
    crown(cv, 31 + st, 6, 11)
    for x in range(33, 37):
        cv.set(x + st, 24, S["out"])
    cv.set(32 + st, 23, S["out"]), cv.set(37 + st, 23, S["out"])
    cv.set(34 + st, 25, S["red"]), cv.set(35 + st, 25, S["red"])
    for x in range(8, 28, 5):
        cv.set(x + st, 23, S["cream"]), cv.set(x + 1 + st, 23, S["cream"])
    cv.outline(S["out"])
    return cv


def stag(f):
    cv = Canvas(44, 40)
    for i, lx in enumerate((10, 17, 24)):
        d = 2 if (i + f) % 2 else -2
        for k in range(4):
            cv.set(lx + d * k // 3, 31 + k, S["out"])
    hs.ellipse(cv, 18.0, 22.0, 15.0, 10.5, VIOLET)
    for x in range(5, 32):
        cv.set(x, 22, S["vd"])
    cv.set(11, 15, S["white"]), cv.set(12, 15, S["white"]), cv.set(11, 16, S["vl"])
    hs.ellipse(cv, 34.0, 22.0, 6.0, 5.5, ramp4("greyl", "greyd", "greyd", "out"))
    o = 3 if f == 0 else 1                                  # the mandibles (open / closed)
    for i in range(9):
        y0, y1 = 18 - o - i // 2, 26 + o + i // 2
        cv.set(38 + i, y0, S["maroon"]), cv.set(38 + i, y0 + 1, S["red"])
        cv.set(38 + i, y1, S["maroon"]), cv.set(38 + i, y1 - 1, S["red"])
    cv.set(43, 15 - o, S["maroon"]), cv.set(43, 29 + o, S["maroon"])
    eye(cv, 34, 19, big=True)
    cv.outline(S["out"])
    return cv


def crowking(f):
    cv = Canvas(44, 40)
    for y in range(16, 34):                                 # the cape behind
        for x in range(4 + (y - 16) // 3, 22):
            cv.set(x - (y - 16) // 2, y, S["red"] if x % 5 else S["maroon"])
    if f == 0:
        hs.ellipse(cv, 18.0, 11.0, 11.0, 5.0, GREY)
    hs.ellipse(cv, 20.0, 24.0, 13.0, 10.0, GREY)
    hs.ellipse(cv, 32.0, 16.0, 7.5, 7.0, GREY)
    if f == 1:
        hs.ellipse(cv, 17.0, 32.0, 10.0, 4.0, GREY)
    for i in range(5):
        cv.set(38 + i, 16, S["yellow"]), cv.set(38 + i, 17, S["yellow"])
    cv.set(38, 18, S["orange"]), cv.set(39, 18, S["orange"]), cv.set(40, 18, S["orange"])
    eye(cv, 31, 12, big=True)
    cv.set(29, 11, S["out"]), cv.set(30, 11, S["out"]), cv.set(31, 10, S["out"])   # a frown
    crown(cv, 31, 2, 11)
    cv.set(18, 35 + 2 * f, S["orange"]), cv.set(24, 35 + 2 * f, S["orange"])
    cv.outline(S["out"])
    return cv


def seed_shot(k):
    cv = Canvas(6, 6)
    if k == 0:                                              # a seed
        hs.ellipse(cv, 3.0, 3.0, 2.0, 1.6, BROWN)
    else:                                                   # a sunflower seed (evolved)
        hs.ellipse(cv, 3.0, 3.0, 2.4, 1.8, ramp4("cream", "cream", "greyd", "greyd"))
        cv.set(2, 3, S["greyd"]), cv.set(3, 3, S["cream"])
    cv.outline(S["out"])
    return cv


def puff(f):
    cv = Canvas(10, 10)
    r = 3.4 if f == 0 else 3.8
    for i in range(12):
        a = i * math.pi / 6
        cv.set(int(round(5 + math.cos(a) * r)), int(round(5 + math.sin(a) * r)), S["white"])
        cv.set(int(round(5 + math.cos(a) * r * 0.5)), int(round(5 + math.sin(a) * r * 0.5)), S["cream"])
    cv.set(5, 5, S["orange"]), cv.set(4, 5, S["orange"])
    return cv


def gem_fly(t):
    cv = Canvas(8, 8)
    cols = [ramp4("white", "vl", "vm", "vd"), ramp4("cream", "red", "maroon", "maroon"),
            ramp4("white", "yellow", "orange", "orange")][t]
    for y in range(7):
        w = 3 - abs(y - 3)
        for x in range(4 - w, 4 + w + 1):
            cv.set(x, y, cols[1] if x < 4 else cols[2])
    cv.set(3, 2, cols[0])
    cv.outline(S["out"])
    return cv


def poof(f):
    cv = Canvas(16, 16)
    r, n, rr = [3.5, 5.5, 6.5][f], [5, 7, 8][f], [2.6, 2.2, 1.4][f]
    for i in range(n):
        a = i * 2 * math.pi / n + f * 0.3
        hs.ellipse(cv, 8 + math.cos(a) * r, 8 + math.sin(a) * r, rr, rr, ramp4("white", "cream", "cream", "greyl"))
    if f == 0:
        hs.ellipse(cv, 8, 8, 3, 3, ramp4("white", "cream", "cream", "cream"))
    return cv


def spark(f):
    cv = Canvas(8, 8)
    c = S["white"] if f == 0 else S["yellow"]
    r = 3 if f == 0 else 2
    for i in range(-r, r + 1):
        cv.set(4 + i, 4, c), cv.set(4, 4 + i, c)
    cv.set(4, 4, S["white"])
    return cv


def coin_fly():
    cv = Canvas(8, 8)
    hs.ellipse(cv, 4.0, 4.0, 2.8, 3.0, ramp4("cream", "yellow", "yellow", "orange"))
    cv.set(4, 3, S["orange"]), cv.set(4, 4, S["orange"])
    cv.outline(S["out"])
    return cv


# name, drawing, frames: the soft sheet (the C side: SOFT_* ids, build_assets.py)
SOFT = [
    ("slug", slug, 2), ("aphid", aphid, 2), ("beetle", beetle, 2), ("ant", ant, 2), ("wasp", wasp, 2),
    ("thistle", thistle, 2), ("crow", crow, 2), ("locust", locust, 2), ("queen", queen, 2), ("stag", stag, 2),
    ("crowking", crowking, 2), ("seed", seed_shot, 2), ("puff", puff, 2), ("gem", gem_fly, 3), ("poof", poof, 3),
    ("spark", spark, 2), ("coin", lambda f: coin_fly(), 1),
]


# ---- the gardeners -----------------------------------------------------------------------------------------------
def pal_dict(pal, names):
    return {n: pal[i] for i, n in enumerate(names)}


HH = pal_dict(HEDGEHOG_PAL, ["out", "sd", "sm", "sl", "face", "faced", "ink", "white", "straw", "strawd", "band",
                             "apron", "aprond", "pink", "feet"])
STEPS = [(-1, 1), (0, 0), (1, -1), (0, 0)]


def straw_hat(cv, d, cx, top, w=13):
    """a straw hat: a wide brim, a round crown, a red band"""
    for x in range(cx - w // 2, cx + w // 2 + 1):
        cv.set(x, top + 4, d["straw"]), cv.set(x, top + 5, d["strawd"])
    for y in range(top, top + 4):
        for x in range(cx - 3, cx + 4):
            cv.set(x, y, d["straw"] if x < cx + 2 else d["strawd"])
    for x in range(cx - 3, cx + 4):
        cv.set(x, top + 3, d["band"])
    cv.set(cx - 2, top + 1, d["white"])


def hedgehog(f, size=24):
    d = HH
    cv = Canvas(size, size)
    bob = 1 if f in (1, 3) else 0
    o = size - 24
    cx, cy = 10.5 + o / 2, 14.5 + bob + o
    step = STEPS[f]
    for fx in (7 + step[0], 14 + step[1]):                  # feet (behind), stepping
        cv.rect(fx + o // 2, 21 + o, fx + 2 + o // 2, 22 + o, d["feet"])
    hs.ellipse(cv, cx, cy, 8.5, 6.5, [d["sl"], d["sm"], d["sm"], d["sd"]])
    for i in range(11):                                     # the spines along the back
        a = math.pi * (0.55 + i * 0.105)
        bx, by = cx + math.cos(a) * 7.0, cy - math.sin(a) * 5.5
        tx, ty = cx + math.cos(a) * 10.5, cy - math.sin(a) * 8.5
        for k in range(5):
            t = k / 4.0
            cv.set(int(round(bx + (tx - bx) * t)), int(round(by + (ty - by) * t)), d["sd"] if i % 2 else d["sm"])
    ox, oy = o // 2, bob + o
    hs.ellipse(cv, 16.5 + ox, 16.0 + oy, 5.0, 4.2, [d["face"], d["face"], d["face"], d["faced"]])
    hs.ellipse(cv, 9.0 + ox, 18.0 + oy, 4.5, 2.5, [d["face"], d["face"], d["faced"], d["faced"]])
    cv.set(21 + ox, 15 + oy, d["ink"]), cv.set(22 + ox, 15 + oy, d["ink"]), cv.set(21 + ox, 14 + oy, d["pink"])
    cv.rect(16 + ox, 12 + oy, 17 + ox, 14 + oy, d["white"])
    cv.set(17 + ox, 13 + oy, d["ink"]), cv.set(17 + ox, 14 + oy, d["ink"])
    cv.set(19 + ox, 17 + oy, d["ink"])                       # a smile
    cv.set(14 + ox, 17 + oy, d["pink"])                      # a blush
    for x in range(7, 12):                                   # the apron over the belly
        cv.set(x + ox, 17 + oy, d["apron"]), cv.set(x + ox, 18 + oy, d["aprond"])
    straw_hat(cv, d, 11 + ox, 3 + oy)
    cv.outline(d["out"])
    return cv


TT = pal_dict(TORTOISE_PAL, ["out", "shd", "shm", "shl", "skin", "skind", "ink", "white", "cap", "capd", "capl",
                             "blue", "blued", "pink", "rim"])


def tortoise(f, size=24):
    d = TT
    cv = Canvas(size, size)
    o = size - 24
    ox, oy = o // 2, (1 if f in (1, 3) else 0) + o
    step = STEPS[f]
    for fx in (5 + step[0], 14 + step[1]):
        cv.rect(fx + ox, 19 + o, fx + 3 + ox, 22 + o, d["skin"])
        cv.set(fx + ox, 22 + o, d["skind"])
    hs.ellipse(cv, 19.0 + ox, 13.5 + oy, 3.8, 3.4, [d["skin"], d["skin"], d["skin"], d["skind"]])
    cv.rect(16 + ox, 15 + oy, 18 + ox, 18 + oy, d["skin"])
    cv.rect(19 + ox, 11 + oy, 20 + ox, 13 + oy, d["white"])
    cv.set(20 + ox, 12 + oy, d["ink"]), cv.set(20 + ox, 13 + oy, d["ink"])
    cv.set(22 + ox, 15 + oy, d["ink"])
    hs.ellipse(cv, 10.0 + ox, 14.0 + oy, 8.5, 6.8, [d["shl"], d["shm"], d["shm"], d["shd"]])
    for y in range(18, 22):                                  # the shell's rim, flat below
        for x in range(0, 22):
            if cv.get(x + ox, y + oy) in (d["shl"], d["shm"], d["shd"]):
                cv.set(x + ox, y + oy, d["rim"] if y == 18 else None)
    for (px, py) in ((7, 11), (12, 11), (9, 15)):            # the plates
        for k in range(-1, 2):
            cv.set(px + k + ox, py + oy - 1, d["shl"]), cv.set(px + k + ox, py + oy + 1, d["shd"])
    for x in range(16, 23):                                  # a gardener's cap
        cv.set(x + ox, 9 + oy, d["capd"])
    for y in range(7, 9):
        for x in range(17, 22):
            cv.set(x + ox, y + oy, d["cap"])
    cv.set(18 + ox, 7 + oy, d["capl"])
    cv.outline(d["out"])
    return cv


HN = pal_dict(HEN_PAL, ["out", "w", "wm", "wd", "comb", "combd", "ink", "white", "beak", "beakd", "blue", "blued",
                        "navy", "pink", "legs"])


def hen(f, size=24):
    d = HN
    cv = Canvas(size, size)
    o = size - 24
    ox, oy = o // 2, (1 if f in (1, 3) else 0) + o
    step = STEPS[f]
    for lx in (9 + step[0], 13 + step[1]):
        for y in range(19, 22):
            cv.set(lx + ox, y + o, d["legs"])
        for k in (-1, 0, 1):
            cv.set(lx + k + ox, 22 + o, d["beak"])
    for i in range(4):                                       # the tail feathers
        cv.set(2 + i // 2 + ox, 8 + i + oy, d["wd"]), cv.set(3 + i // 2 + ox, 8 + i + oy, d["wm"])
    hs.ellipse(cv, 11.0 + ox, 14.0 + oy, 8.0, 6.0, [d["w"], d["w"], d["wm"], d["wd"]])
    hs.ellipse(cv, 16.5 + ox, 8.0 + oy, 4.0, 4.0, [d["w"], d["w"], d["wm"], d["wd"]])
    for x, h in ((15, 2), (17, 3), (19, 2)):                 # the comb
        for y in range(h):
            cv.set(x + ox, 3 - y + oy, d["comb"]), cv.set(x + 1 + ox, 3 - y + oy, d["comb"])
    cv.rect(20 + ox, 7 + oy, 22 + ox, 8 + oy, d["beak"])
    cv.set(22 + ox, 8 + oy, d["beakd"])
    cv.rect(19 + ox, 9 + oy, 19 + ox, 11 + oy, d["comb"])
    cv.rect(17 + ox, 6 + oy, 18 + ox, 7 + oy, d["white"])
    cv.set(18 + ox, 7 + oy, d["ink"]), cv.set(18 + ox, 6 + oy, d["ink"])
    hs.ellipse(cv, 9.5 + ox, 14.5 + oy, 4.0, 2.8, [d["wm"], d["wm"], d["wd"], d["wd"]])
    for x in range(13, 19):                                  # a blue gardening scarf
        cv.set(x + ox, 11 + oy, d["blue"]), cv.set(x + ox, 12 + oy, d["blued"])
    cv.outline(d["out"])
    return cv


CHARS = [("hedgehog", hedgehog, HEDGEHOG_PAL), ("tortoise", tortoise, TORTOISE_PAL), ("hen", hen, HEN_PAL)]


# ---- the tools (weapons) and their level-up icons ------------------------------------------------------------------
def bee(f, big=False):
    n = 16 if big else 8
    cv = Canvas(n, n)
    c = n / 2.0
    s = 2 if big else 1
    wing = [T["white"], T["white"], T["pale"], T["pale"]]
    if f == 0:
        hs.ellipse(cv, c - 0.5, c - 2.0 * s, 1.8 * s, 1.4 * s, wing)
    hs.ellipse(cv, c, c + 0.5, 2.8 * s, 2.0 * s, [T["white"], T["yellow"], T["yellow"], T["amber"]])
    for x in (int(c - s), int(c + s) - (1 if big else 0)):
        for y in range(n):
            if cv.get(x, y) in (T["yellow"], T["amber"]):
                cv.set(x, y, T["ink"])
    if f == 1:
        hs.ellipse(cv, c - 0.5, c - 1.6 * s, 2.2 * s, 1.0 * s, wing)
    cv.set(int(c + 2 * s), int(c), T["ink"])
    if big:                                                  # the queen's little crown
        for i in range(5):
            cv.set(5 + i, 3, T["yellow"])
        cv.set(5, 2, T["yellow"]), cv.set(7, 2, T["yellow"]), cv.set(9, 2, T["yellow"])
    cv.outline(T["out"])
    return cv


def mower(f):
    cv = Canvas(16, 16)
    for i in range(6):                                       # the handle
        cv.set(2 + i, 3 + i, T["steeld"])
    cv.rect(1, 2, 3, 3, T["dred"])
    for y in range(8, 13):                                   # the red deck, the engine, the wheels
        for x in range(4, 15):
            cv.set(x, y, T["red"] if y < 11 else T["dred"])
    cv.rect(7, 6, 12, 8, T["steel"])
    cv.set(8, 6, T["white"])
    for wx in (5, 13):
        hs.ellipse(cv, wx, 13.0, 2.0, 2.0, [T["steel"], T["steel"], T["steeld"], T["ink"]])
        cv.set(wx - f, 12 + f, T["white"])
    cv.set(15, 12 - f, T["green"]), cv.set(14, 14 + f, T["green"])     # cut grass flying
    cv.outline(T["out"])
    return cv


def rake(f):
    cv = Canvas(16, 16)
    a = f * math.pi / 4
    ca, sa = math.cos(a), math.sin(a)
    for i in range(-7, 5):
        cv.set(int(round(8 + ca * i)), int(round(8 + sa * i)), T["wood"] if i % 3 else T["woodd"])
    hx, hy = 8 + ca * 5, 8 + sa * 5                          # the head: a bar with teeth
    for j in range(-4, 5):
        x, y = hx - sa * j, hy + ca * j
        cv.set(int(round(x)), int(round(y)), T["steel"])
        if j % 2 == 0:
            cv.set(int(round(x + ca * 1.5)), int(round(y + sa * 1.5)), T["steeld"])
    cv.outline(T["out"])
    return cv


def pumpkin():
    cv = Canvas(16, 16)
    for dx in (-3, 3, 0):
        hs.ellipse(cv, 8 + dx, 9.5, 4.0, 5.0, [T["yellow"], T["orange"], T["orange"], T["oranged"]])
    for y in range(6, 15):
        cv.set(6, y, T["oranged"]), cv.set(10, y, T["oranged"])
    cv.rect(7, 2, 8, 4, T["green"])
    cv.set(9, 3, T["green"]), cv.set(10, 2, T["green"])
    cv.set(5, 7, T["yellow"])
    cv.outline(T["out"])
    return cv


def blast(f):
    """a quarter of the pumpkin's burst (draw.c mirrors it into a 32x32 ring), 3 frames"""
    cv = Canvas(16, 16)
    r = [6, 10, 13][f]
    for y in range(16):
        for x in range(16):
            d = math.hypot(15.5 - x, 15.5 - y)
            if f < 2 and d < r - 3:
                cv.set(x, y, T["yellow"] if d < r - 6 else T["orange"])
            elif d < r and not (f == 2 and d < r - 2):
                cv.set(x, y, T["oranged"] if f == 2 else T["orange"])
            if f == 2 and r - 3 <= d < r - 2 and (x + y) % 2 == 0:
                cv.set(x, y, T["yellow"])
    for i in range(3):
        a = 0.3 + i * 0.5
        cv.set(int(15.5 - math.cos(a) * (r + 1)), int(15.5 - math.sin(a) * (r + 1)), T["white"] if f < 2 else T["yellow"])
    return cv


def flame(f):
    cv = Canvas(16, 16)
    sway = [0, 1, -1][f]
    for y in range(16):
        for x in range(16):
            yy = y - 3
            if yy <= 0:
                continue
            t = yy / 12.5
            w = 6.0 * t ** 0.7 if t < 0.72 else 6.0 * 0.72 ** 0.7 * math.sqrt(max(0.0, 1 - ((t - 0.72) / 0.3) ** 2))
            cx = 8 + sway * (1 - t)
            dx = abs(x + 0.5 - cx)
            if dx < w:
                core = dx < w * 0.45 and yy > 5
                cv.set(x, y, T["yellow"] if core else T["orange"] if dx < w * 0.8 else T["red"])
    cv.set(8 + sway, 2, T["orange"])
    cv.outline(T["dred"])
    return cv


def thorn(f):
    cv = Canvas(8, 8)
    pts = [(1, 6), (6, 1)] if f == 0 else [(1, 1), (6, 6)]
    hs.capsule(cv, pts[0][0], pts[0][1], pts[1][0], pts[1][1], 1.2, [T["green"], T["green"], T["green"], T["woodd"]])
    cv.set(pts[1][0], pts[1][1], T["white"])
    cv.outline(T["out"])
    return cv


def icon_seed(evo=False):
    cv = Canvas(16, 16)
    if evo:                                                  # a sunflower
        for i in range(10):
            a = i * math.pi / 5
            hs.ellipse(cv, 8 + math.cos(a) * 4.5, 8 + math.sin(a) * 4.5, 2.2, 2.2, [T["yellow"]] * 3 + [T["amber"]])
        hs.ellipse(cv, 8, 8, 3.2, 3.2, [T["woodd"], T["woodd"], T["ink"], T["ink"]])
    else:
        for (x, y) in ((5, 6), (10, 5), (8, 10)):
            hs.ellipse(cv, x, y, 2.5, 1.8, [T["wood"], T["wood"], T["woodd"], T["woodd"]])
    cv.outline(T["out"])
    return cv


def icon_can():
    cv = Canvas(16, 16)
    hs.ellipse(cv, 7, 10, 5.0, 4.5, [T["pale"], T["steel"], T["steel"], T["steeld"]])
    for i in range(5):
        cv.set(11 + i, 9 - i, T["steeld"]), cv.set(11 + i, 10 - i, T["steel"])
    for x in range(4, 10):
        cv.set(x, 5, T["steeld"])
    cv.set(3, 6, T["steeld"]), cv.set(10, 6, T["steeld"])
    cv.set(15, 3, T["pale"]), cv.set(14, 2, T["pale"])
    cv.outline(T["out"])
    return cv


def icon_rainbow():
    cv = Canvas(16, 16)
    cols = [T["red"], T["orange"], T["yellow"], T["green"], T["pale"]]
    for y in range(13):
        for x in range(16):
            k = int(7.5 - math.hypot(x + 0.5 - 8, y + 0.5 - 13))
            if 0 <= k < 5:
                cv.set(x, y, cols[4 - k])
    cv.outline(T["out"])
    return cv


def icon_thorns(evo=False):
    cv = Canvas(16, 16)
    for i in range(8):
        a = i * math.pi / 4
        for k in range(3, 7 + (1 if evo else 0)):
            cv.set(int(round(8 + math.cos(a) * k)), int(round(8 + math.sin(a) * k)), T["green"] if k < 6 else T["white"])
    hs.ellipse(cv, 8, 8, 2.6, 2.6, [T["yellow"], T["yellow"], T["amber"], T["amber"]] if evo else
               [T["wood"], T["wood"], T["woodd"], T["woodd"]])
    cv.outline(T["out"])
    return cv


def icon_chili(evo=False):
    cv = Canvas(16, 16)
    for i in range(10):
        r = 2.0 - i * 0.12
        hs.ellipse(cv, 3 + i, 4 + int(round(math.sin(i / 3.0) * 2 + i * 0.6)), r, r,
                   [T["orange"], T["red"], T["red"], T["dred"]])
    cv.rect(1, 2, 3, 3, T["green"])
    if evo:
        for (x, y) in ((11, 3), (13, 5), (12, 1)):
            cv.set(x, y, T["yellow"])
    cv.outline(T["out"])
    return cv


def icon_dandelion(evo=False):
    cv = Canvas(16, 16)
    for y in range(9, 16):
        cv.set(8, y, T["green"])
    for i in range(14):
        a = i * 2 * math.pi / 14
        for k in (3, 5):
            cv.set(int(round(8 + math.cos(a) * k)), int(round(6 + math.sin(a) * k)), T["white"] if k == 5 else T["pale"])
    cv.set(8, 6, T["amber"])
    if evo:
        for (x, y) in ((1, 2), (14, 1), (13, 12)):
            cv.set(x, y, T["yellow"])
    cv.outline(T["out"])
    return cv


TOOLS = [
    ("bee", lambda f: bee(f), 8, 8, 2), ("queen_bee", lambda f: bee(f, True), 16, 16, 2),
    ("mower", mower, 16, 16, 2), ("rake", rake, 16, 16, 4), ("pumpkin", lambda f: pumpkin(), 16, 16, 1),
    ("blast", blast, 16, 16, 3), ("flame", flame, 16, 16, 3), ("thorn", thorn, 8, 8, 2),
    ("icon_seed", lambda f: icon_seed(f == 1), 16, 16, 2), ("icon_can", lambda f: icon_can(), 16, 16, 1),
    ("icon_rainbow", lambda f: icon_rainbow(), 16, 16, 1), ("icon_thorns", lambda f: icon_thorns(f == 1), 16, 16, 2),
    ("icon_chili", lambda f: icon_chili(f == 1), 16, 16, 2),
    ("icon_dandelion", lambda f: icon_dandelion(f == 1), 16, 16, 2),
]


# ---- the items: pickups, chests, the passives' icons, the HUD's icons -------------------------------------------
def apple():
    cv = Canvas(16, 16)
    for cx in (6.0, 10.0):
        hs.ellipse(cv, cx, 9.5, 4.5, 4.8, [I["pink"], I["red"], I["red"], I["dred"]])
    cv.set(8, 4, I["brown"]), cv.set(8, 3, I["brown"]), cv.set(9, 2, I["brown"])
    hs.ellipse(cv, 11.0, 3.5, 2.4, 1.2, [I["green"], I["green"], I["greend"], I["greend"]])
    cv.set(5, 7, I["white"])
    cv.outline(I["out"])
    return cv


def magnet():
    cv = Canvas(16, 16)
    for y in range(11):
        for x in range(16):
            d = math.hypot(x + 0.5 - 8, y + 0.5 - 8)
            if 3.0 <= d < 6.5:
                cv.set(x, y, I["red"] if x < 8 else I["dred"])
    for y in range(8, 14):
        for x in list(range(2, 5)) + list(range(11, 14)):
            cv.set(x, y, (I["red"] if x < 8 else I["dred"]) if y < 12 else I["silver"])
    cv.set(4, 4, I["pink"])
    cv.outline(I["out"])
    return cv


def coin(f):
    cv = Canvas(8, 8)
    w = [3.0, 2.0, 0.8, 2.0][f]
    hs.ellipse(cv, 4.0, 4.0, w, 3.2, [I["cream"], I["gold"], I["gold"], I["goldd"]])
    if f != 2:
        cv.set(4, 3, I["goldd"]), cv.set(4, 4, I["goldd"])
    cv.outline(I["out"])
    return cv


def chest(f):
    cv = Canvas(16, 16)
    lid = 0 if f == 0 else -3
    for y in range(8, 15):
        for x in range(2, 14):
            cv.set(x, y, I["tan"] if y < 13 else I["brown"])
    for y in range(4 + lid, 8 + lid):
        for x in range(2, 14):
            cv.set(x, y, I["tan"] if y > 4 + lid else I["cream"])
    if f:
        for y in range(5, 8):
            for x in range(4, 12):
                cv.set(x, y, I["gold"] if (x + y) % 3 else I["white"])
    for x in range(2, 14):
        cv.set(x, 8 + lid, I["goldd"])
    for y in list(range(4 + lid, 8 + lid)) + list(range(8, 15)):
        cv.set(4, y, I["gold"]), cv.set(11, y, I["gold"])
    cv.rect(7, 8, 8, 10, I["gold"])
    cv.set(7, 10, I["goldd"])
    cv.outline(I["out"])
    return cv


def i_compost():
    cv = Canvas(16, 16)
    for y in range(4, 15):
        w = 4 + (y - 4) // 3
        for x in range(8 - w, 8 + w):
            cv.set(x, y, I["tan"] if x < 8 else I["brown"])
    for x in range(5, 11):
        cv.set(x, 4, I["brown"])
    cv.set(7, 2, I["green"]), cv.set(8, 1, I["green"]), cv.set(9, 2, I["greend"]), cv.set(8, 3, I["greend"])
    cv.rect(6, 8, 9, 10, I["greend"])
    cv.outline(I["out"])
    return cv


def i_hat():
    cv = Canvas(16, 16)
    for x in range(1, 15):
        cv.set(x, 10, I["gold"]), cv.set(x, 11, I["goldd"])
    for y in range(4, 10):
        for x in range(4, 12):
            cv.set(x, y, I["gold"] if x < 9 else I["goldd"])
    for x in range(4, 12):
        cv.set(x, 8, I["red"])
    cv.set(5, 5, I["cream"])
    cv.outline(I["out"])
    return cv


def i_boots():
    cv = Canvas(16, 16)
    for bx in (1, 8):
        cv.rect(bx + 1, 3, bx + 4, 10, I["green"])
        cv.rect(bx + 1, 11, bx + 6, 13, I["green"])
        cv.rect(bx + 1, 14, bx + 6, 14, I["greend"])
        cv.set(bx + 2, 4, I["white"])
    cv.outline(I["out"])
    return cv


def i_honey():
    cv = Canvas(16, 16)
    hs.ellipse(cv, 8, 10, 6.0, 5.0, [I["cream"], I["gold"], I["gold"], I["goldd"]])
    cv.rect(4, 3, 11, 5, I["tan"])
    cv.rect(4, 5, 11, 5, I["brown"])
    cv.set(6, 8, I["white"])
    for y in range(6, 9):
        cv.set(10, y, I["gold"])
    cv.outline(I["out"])
    return cv


def i_sundial():
    cv = Canvas(16, 16)
    hs.ellipse(cv, 8, 9, 6.5, 5.5, [I["white"], I["silver"], I["silver"], I["blued"]])
    for a in range(0, 360, 45):
        r = math.radians(a)
        cv.set(int(round(8 + math.cos(r) * 5)), int(round(9 + math.sin(r) * 4)), I["blued"])
    for i in range(6):
        cv.set(8 - i // 2, 9 - i, I["out"])
    cv.outline(I["out"])
    return cv


def i_pouch():
    cv = Canvas(16, 16)
    hs.ellipse(cv, 8, 10, 5.5, 4.8, [I["cream"], I["tan"], I["tan"], I["brown"]])
    cv.rect(6, 3, 10, 5, I["tan"])
    cv.rect(5, 5, 11, 5, I["red"])
    for (x, y) in ((7, 10), (9, 11), (8, 9)):
        cv.set(x, y, I["brown"])
    cv.outline(I["out"])
    return cv


def i_clover():
    cv = Canvas(16, 16)
    for (dx, dy) in ((-3, -3), (3, -3), (-3, 3), (3, 3)):
        hs.ellipse(cv, 8 + dx, 7 + dy, 3.0, 3.0, [I["green"], I["green"], I["green"], I["greend"]])
    for y in range(9, 16):
        cv.set(8 + (y - 9) // 3, y, I["greend"])
    cv.set(5, 3, I["white"])
    cv.outline(I["out"])
    return cv


def i_pest():
    """the HUD's pest counter icon: a little slug face"""
    cv = Canvas(8, 8)
    hs.ellipse(cv, 4.0, 5.0, 3.4, 2.6, [I["green"], I["green"], I["green"], I["greend"]])
    for (x, y) in ((3, 1), (5, 1), (3, 2), (5, 2)):
        cv.set(x, y, I["greend"])
    cv.set(3, 4, I["out"]), cv.set(5, 4, I["out"])
    cv.outline(I["out"])
    return cv


def i_star():
    """the evolution's mark over an icon"""
    cv = Canvas(8, 8)
    for i in range(-3, 4):
        cv.set(4 + i, 4, I["gold"]), cv.set(4, 4 + i, I["gold"])
    for d in (-1, 1):
        cv.set(4 + d, 4 + d, I["gold"]), cv.set(4 + d, 4 - d, I["gold"])
    cv.set(4, 4, I["white"])
    cv.outline(I["out"])
    return cv


def i_lock():
    cv = Canvas(8, 8)
    for y in range(3, 8):
        for x in range(1, 7):
            cv.set(x, y, I["gold"] if y < 6 else I["goldd"])
    for (x, y) in ((2, 1), (2, 2), (5, 1), (5, 2), (3, 0), (4, 0)):
        cv.set(x, y, I["silver"])
    cv.set(3, 4, I["out"]), cv.set(3, 5, I["out"])
    return cv


def hp_bar(k):
    """an 8x8 tile of the health bar under the gardener: k/8 of it full (rows 0-3)"""
    cv = Canvas(8, 8)
    for x in range(8):
        cv.set(x, 0, I["out"]), cv.set(x, 3, I["out"])
        for y in (1, 2):
            cv.set(x, y, (I["green"] if y == 1 else I["greend"]) if x < k else I["dred"])
    return cv


def cursor(f):
    cv = Canvas(8, 8)
    for i in range(4):
        for y in range(4 - i, 4 + i + 1):
            cv.set(i + 1 + f, y, I["gold"] if i < 3 else I["goldd"])
    cv.outline(I["out"])
    return cv


ITEMS = [
    ("apple", lambda f: apple(), 16, 16, 1), ("magnet", lambda f: magnet(), 16, 16, 1), ("coin", coin, 8, 8, 4),
    ("chest", chest, 16, 16, 2), ("i_compost", lambda f: i_compost(), 16, 16, 1),
    ("i_hat", lambda f: i_hat(), 16, 16, 1), ("i_boots", lambda f: i_boots(), 16, 16, 1),
    ("i_honey", lambda f: i_honey(), 16, 16, 1), ("i_sundial", lambda f: i_sundial(), 16, 16, 1),
    ("i_pouch", lambda f: i_pouch(), 16, 16, 1), ("i_clover", lambda f: i_clover(), 16, 16, 1),
    ("i_pest", lambda f: i_pest(), 8, 8, 1), ("i_star", lambda f: i_star(), 8, 8, 1),
    ("i_lock", lambda f: i_lock(), 8, 8, 1), ("hpbar", hp_bar, 8, 8, 9), ("cursor", cursor, 8, 8, 2),
]


# ---- the see-through effects (OBJ 4, added at half) -------------------------------------------------------------------
def splash(f):
    """the watering can's splash, 32x16 (pointing right), 2 frames"""
    cv = Canvas(32, 16)
    for i in range(26):
        arc = 8 - math.sin(i / 25.0 * math.pi) * (5 + f)
        for k in range(-2, 3):
            y = int(round(arc + k + (i / 25.0) * 4))
            if 0 <= y < 16:
                cv.set(3 + i, y, FX["w3"] if k == 0 else FX["w2"] if abs(k) == 1 else FX["w1"])
    for (x, y) in ((28 + f, 12), (25, 14 - f), (29, 9 + f), (20 - f, 13)):
        cv.set(x, y, FX["w4"]), cv.set(x + 1, y, FX["w3"])
    return cv


def ring_dot(f):
    cv = Canvas(8, 8)
    r = 2.5 - f * 0.8
    hs.ellipse(cv, 4, 4, r, r, [FX["t2"], FX["t1"], FX["t1"], FX["t0"]])
    return cv


def stick(f):
    """the floating joystick: its ring (f 0, 32x32) and its knob (f 1)"""
    cv = Canvas(32, 32)
    for y in range(32):
        for x in range(32):
            d = math.hypot(x + 0.5 - 16, y + 0.5 - 16)
            if f == 0 and 13.0 <= d < 15.5:
                cv.set(x, y, FX["s1"] if y < 16 else FX["s0"])
            if f == 1 and d < 7.0:
                cv.set(x, y, FX["s1"] if d < 5 else FX["s0"])
    return cv


def glow(f):
    """a soft glow: a level-up's flash, a chest's light, 16x16"""
    cv = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            d = math.hypot(x + 0.5 - 8, y + 0.5 - 8)
            if d < 7.5 - f * 2:
                cv.set(x, y, FX["glow"] if d < 3 else FX["f1"] if d < 5.5 else FX["f0"])
    return cv


FXS = [("splash", splash, 32, 16, 2), ("ringdot", ring_dot, 8, 8, 2), ("stick", stick, 32, 32, 2),
       ("glow", glow, 16, 16, 2)]


# ---- the garden's tiles (BG palette 1) ----------------------------------------------------------------------------
def hsh(*a):
    v = 2166136261
    for x in a:
        v = ((v ^ (x & 0xffffffff)) * 16777619) & 0xffffffff
    return v ^ (v >> 13)


def grass(v):
    cv = Canvas(8, 8)
    for y in range(8):
        for x in range(8):
            k = hsh(x, y, v) % 100
            cv.set(x, y, G["g2"] if k < 4 else G["g0"] if k < 7 else G["g1"])
    for i in range(2):                                       # a blade or two
        k = hsh(v, i, 7)
        x, y = k % 7, 2 + (k >> 4) % 5
        cv.set(x, y, G["g2"]), cv.set(x + (v % 2), y - 1, G["g3"] if v % 2 else G["g2"])
        cv.set(x, y + 1, G["g0"])
    return cv


def tuft(v):
    cv = grass(v + 10)
    for i, x in enumerate((2, 4, 6)):
        h = 3 + (i + v) % 2
        for y in range(7 - h, 7):
            cv.set(x - (1 if y < 4 and i == 0 else 0), y, G["g3"] if y < 5 else G["g2"])
        cv.set(x, 7, G["g0"])
    return cv


def flower(kind):
    cv = grass(20 + kind)
    petal = [G["white"], G["yellow"], G["red"], G["vl"]][kind]
    centre = [G["yellow"], G["dirt0"], G["dark"], G["white"]][kind]
    cx, cy = 3 + kind % 2, 3 + (kind // 2) % 2
    for dx, dy in ((0, -1), (-1, 0), (1, 0), (0, 1)):
        cv.set(cx + dx, cy + dy, petal)
    cv.set(cx, cy, centre)
    cv.set(cx + 1, cy + 2, G["g0"]), cv.set(cx + 1, cy + 3, G["g0"])
    if kind == 0:                                            # two daisies
        for (x, y) in ((6, 5), (5, 6), (7, 6), (6, 7)):
            cv.set(x, y, G["white"])
        cv.set(6, 6, G["yellow"])
    return cv


def pebble(v):
    cv = grass(30 + v)
    hs.ellipse(cv, 4.0, 4.5, 2.6 - v * 0.6, 1.8, [G["white"], G["stone"], G["stone"], G["dark"]])
    for x in range(2, 7):
        if cv.get(x, 6) == G["g1"]:
            cv.set(x, 6, G["g0"])
    return cv


def dirt(q):
    """a quarter of a round dirt patch (q: 0 TL, 1 TR, 2 BL, 3 BR)"""
    cv = Canvas(8, 8)
    for y in range(8):
        for x in range(8):
            gx, gy = x + (8 if q % 2 else 0), y + (8 if q >= 2 else 0)
            d = math.hypot(gx + 0.5 - 8, (gy + 0.5 - 8) * 1.15)
            if d < 6.5:
                cv.set(x, y, G["dirt0"] if hsh(gx, gy, 3) % 100 < 18 or d > 5.6 else G["dirt1"])
            elif d < 7.3:
                cv.set(x, y, G["g0"])
            else:
                k = hsh(gx, gy, 40) % 100
                cv.set(x, y, G["g2"] if k < 8 else G["g0"] if k < 13 else G["g1"])
    return cv


def cabbage(q):
    """a quarter of a 16x16 cabbage (the patch's vegetables)"""
    big = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            k = hsh(x, y, 40) % 100
            big.set(x, y, G["g2"] if k < 8 else G["g0"] if k < 13 else G["g1"])
    for y in range(13, 16):
        for x in range(3, 13):
            big.set(x, y, G["dirt0"] if (x + y) % 3 else G["dirt1"])
    leaf = [G["g3"], G["g2"], G["g2"], G["g0"]]
    for (cx, cy, r) in ((5, 9, 4.2), (11, 9, 4.2), (8, 7, 4.6)):
        hs.ellipse(big, cx, cy, r, r * 0.85, leaf)
    hs.ellipse(big, 8, 8, 2.6, 2.2, [G["g3"], G["g3"], G["g2"], G["g1"]])
    big.set(7, 7, G["white"])
    cv = Canvas(8, 8)
    for y in range(8):
        for x in range(8):
            cv.set(x, y, big.get(x + (8 if q % 2 else 0), y + (8 if q >= 2 else 0)))
    return cv


def gem(tier):
    cv = grass(50 + tier)
    cols = [(G["vl"], G["vm"]), (G["red"], G["maroon"]), (G["yellow"], G["dirt1"])][tier]
    h = [3, 3, 4][tier]
    for y in range(-h, h + 1):
        w = min(h - abs(y), [2, 3, 3][tier])
        for x in range(-w, w + 1):
            cv.set(4 + x, 4 + y, cols[0] if x < 0 or (x == 0 and y <= 0) else cols[1])
    body = (cols[0], cols[1])
    edge = []
    for py in range(8):
        for px in range(8):
            if cv.get(px, py) not in body and any(cv.get(px + dx, py + dy) in body
                                                  for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                edge.append((px, py))
    for px, py in edge:
        cv.set(px, py, G["dark"])
    cv.set(3, 3, G["glint"])
    if tier == 2:
        cv.set(4, 2, G["glint"])
    return cv


GROUND = ([("grass%d" % v, lambda v=v: grass(v)) for v in range(4)] +
          [("tuft%d" % v, lambda v=v: tuft(v)) for v in range(2)] +
          [("flower%d" % k, lambda k=k: flower(k)) for k in range(4)] +
          [("pebble%d" % v, lambda v=v: pebble(v)) for v in range(2)] +
          [("dirt%d" % q, lambda q=q: dirt(q)) for q in range(4)] +
          [("cabbage%d" % q, lambda q=q: cabbage(q)) for q in range(4)] +
          [("gem%d" % t, lambda t=t: gem(t)) for t in range(3)])


# ---- writing ---------------------------------------------------------------------------------------------------------
def indexed(cv, pal, name):
    """the canvas as indices of pal (1..15; 0 clear); every colour must be one of pal's"""
    out = []
    for y in range(cv.h):
        row = []
        for x in range(cv.w):
            c = cv.p[y][x]
            if c is None:
                row.append(0)
            else:
                assert c in pal, "%s: colour %r not in its palette" % (name, c)
                row.append(pal.index(c) + 1)
        out.append(row)
    return out


def sheet(entries, pal, width=256):
    """lays the frames out left to right in rows; returns (image, layout [(name, x, y, w, h, frames)])"""
    x = y = rowh = 0
    placed, layout = [], []
    for name, fn, w, h, n in entries:
        if x + w * n > width:
            x, y, rowh = 0, y + rowh, 0
        layout.append((name, x, y, w, h, n))
        for f in range(n):
            placed.append((x + f * w, y, fn(f), name))
        x += w * n
        rowh = max(rowh, h)
    im = Image.new("P", (width, y + rowh), 0)
    im.putpalette([255, 0, 255] + [c for p in pal for c in p] + [0] * (3 * (255 - len(pal))))
    px = im.load()
    for (ox, oy, cv, name) in placed:
        idx = indexed(cv, pal, name)
        for yy in range(cv.h):
            for xx in range(cv.w):
                if idx[yy][xx]:
                    px[ox + xx, oy + yy] = idx[yy][xx]
    return im, layout


def preview(im, path, bg=(100, 158, 72), k=3):
    rgb = im.convert("RGB")
    out = Image.new("RGB", rgb.size, bg)
    mask = Image.frombytes("L", im.size, bytes(255 if b else 0 for b in im.tobytes()))
    out.paste(rgb, (0, 0), mask)
    out.resize((rgb.width * k, rgb.height * k), Image.NEAREST).save(path)


def write_all(out):
    os.makedirs(os.path.join(out, "preview"), exist_ok=True)
    layouts = {}
    soft_entries = []
    for name, fn, n in SOFT:
        c0 = fn(0)
        soft_entries.append((name, fn, c0.w, c0.h, n))
    jobs = [("soft", soft_entries, SOFT_PAL, 256), ("tools", TOOLS, TOOLS_PAL, 256), ("items", ITEMS, ITEMS_PAL, 256),
            ("fx", FXS, FX_PAL, 256)]
    for cname, fn, pal in CHARS:
        jobs.append(("hero_" + cname, [("walk", lambda f, fn=fn: fn(f), 24, 24, 4)], pal, 96))
    jobs.append(("ground", [(n, lambda f, fn=fn: fn(), 8, 8, 1) for n, fn in GROUND], GROUND_PAL, 64))
    for name, entries, pal, width in jobs:
        im, layout = sheet(entries, pal, width)
        im.save(os.path.join(out, name + ".png"))
        preview(im, os.path.join(out, "preview", name + ".png"))
        layouts[name] = layout
    pals = {"soft": SOFT_PAL, "ground": GROUND_PAL, "ground_night": GROUND_NIGHT, "tools": TOOLS_PAL,
            "items": ITEMS_PAL, "fx": FX_PAL, "hedgehog": HEDGEHOG_PAL, "tortoise": TORTOISE_PAL, "hen": HEN_PAL}
    with open(os.path.join(out, "palettes.json"), "w") as f:
        json.dump({"palettes": pals, "layouts": layouts}, f, indent=1, sort_keys=True)
    return layouts


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", default=os.path.join(GAME, "art"))
    a = ap.parse_args()
    write_all(a.out)
    print("art written to", a.out)


if __name__ == "__main__":
    main()

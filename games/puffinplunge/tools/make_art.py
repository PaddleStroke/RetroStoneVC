#!/usr/bin/env python3
"""Puffin Plunge: the code-drawn art, in the 8BCraft house style (docs/art-direction.md).

Code-drawn only, no image generation: every sprite and tile is drawn here with games/common/tools/house_style.py
(1-px outlines outside the shapes, 3-4 flat shades lit from the top-left, squash and stretch). Each sheet uses ONE
fixed 15-colour palette and is written as an indexed PNG (build_assets.py reads the indices as they are):

  hero.png        the puffin and its bubble boots (OBJ palette 0): idle, run, rise, fall, fire, hurt, squash, stretch
  critters.png    every critter, drawn with the four symbolic ramps A B C D (OBJ palette 1: each zone gives the ramps its
                  colours, palettes.json "zone1".."zone4": the same urchin is a crystal one, a sea urchin, a thorny burr,
                  an ice spike)
  walrus.png      the Grumpy Walrus, the boss at the bottom of the well (OBJ palette 5)
  items.png       gems, hearts, bubble pips, the shots, the shop's icons, the small digits, puffs and shards (OBJ palette 2)
  fx.png          the see-through effects: the muzzle flash, the beam, the shield (OBJ palette 4, added)
  terrain_zN.png  the zone's 8x8 tiles: the rock (autotiled from quarters), its cap, crystals, doors, the bottom's floor,
                  the side panels' decorations (BG palette 1; palettes.json "terrain_zN"); and the back wall (BG 3)
  palettes.json   the palettes and the sheets' layouts
and art/preview/*.png: the sheets at 3x, for review.

    python3 games/puffinplunge/tools/make_art.py [--out DIR]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/puffinplunge/LICENSE.
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


def hsh(*a):
    v = 2166136261
    for x in a:
        v = ((v ^ (x & 0xffffffff)) * 16777619) & 0xffffffff
    v ^= v >> 13
    v = (v * 0x5bd1e995) & 0xffffffff
    return v ^ (v >> 15)


# ---- palettes (index 0 = clear) -------------------------------------------------------------------------------------------
HERO_PAL = [
    (30, 18, 46),     # 1 outline
    (44, 44, 64),     # 2 black feathers
    (78, 80, 104),    # 3 black feathers lit
    (250, 250, 248),  # 4 white
    (196, 204, 226),  # 5 white shade
    (252, 152, 52),   # 6 beak orange
    (226, 66, 58),    # 7 beak red
    (255, 220, 84),   # 8 beak yellow
    (246, 128, 44),   # 9 feet
    (178, 74, 34),    # 10 feet dark
    (150, 224, 255),  # 11 boot light
    (70, 150, 232),   # 12 boot mid
    (40, 76, 160),    # 13 boot dark
    (244, 150, 166),  # 14 cheek
    (18, 10, 28),     # 15 pupil
]
H = dict(zip(["out", "blk", "blkl", "wht", "whts", "bko", "bkr", "bky", "ft", "ftd", "btl", "btm", "btd", "chk", "ink"],
             HERO_PAL))

# the critters: symbolic slots; each zone's palette gives them colours
SYM_NAMES = ["out", "white", "ink", "a0", "a1", "a2", "b0", "b1", "b2", "c0", "c1", "c2", "d0", "d1", "d2"]
SYM_PAL = [(1, 1, 1 + i) for i in range(15)]           # (placeholder colours, unique: the sheet stores indices)
Y = dict(zip(SYM_NAMES, SYM_PAL))
ZONE_PALS = {
    # out, white, ink, A walker, B flyer, C spiky, D special
    "zone1": [(36, 18, 52), (255, 255, 255), (20, 10, 30),
              (255, 196, 214), (236, 120, 160), (164, 64, 108),        # A: pink snail shells
              (190, 150, 240), (128, 88, 200), (78, 50, 140),          # B: violet bats
              (190, 255, 250), (90, 214, 230), (40, 130, 170),         # C: cyan crystal urchins
              (255, 236, 196), (230, 196, 140), (170, 130, 86)],       # D: snail bodies, bat ears
    "zone2": [(30, 20, 50), (255, 255, 255), (20, 10, 30),
              (255, 176, 120), (238, 98, 64), (160, 44, 46),           # A: crabs
              (255, 236, 120), (246, 190, 50), (190, 120, 30),         # B: fish, puffers
              (232, 150, 236), (170, 76, 190), (100, 36, 120),         # C: sea urchins
              (190, 236, 255), (120, 190, 246), (70, 120, 200)],       # D: jellyfish, fins
    "zone3": [(34, 22, 30), (255, 255, 240), (20, 12, 16),
              (196, 222, 120), (110, 160, 70), (56, 96, 50),           # A: beetles
              (240, 220, 255), (196, 160, 236), (130, 100, 180),       # B: moths
              (230, 196, 140), (176, 128, 80), (110, 72, 44),          # C: thorny burrs
              (255, 140, 120), (228, 60, 64), (150, 30, 50)],          # D: red mushroom caps, spores
    "zone4": [(24, 26, 56), (255, 255, 255), (16, 14, 36),
              (255, 236, 220), (240, 196, 170), (190, 140, 120),       # A: woolly grubs
              (210, 236, 255), (140, 186, 236), (86, 120, 196),        # B: ice bats
              (240, 252, 255), (176, 226, 250), (104, 160, 220),       # C: ice spikes
              (190, 255, 240), (100, 230, 220), (40, 160, 180)],       # D: icicles, frost wisps
}
ITEMS_PAL = [
    (22, 18, 40),     # 1 outline
    (255, 255, 255),  # 2 white
    (170, 246, 255),  # 3 gem light
    (64, 196, 244),   # 4 gem mid
    (36, 112, 204),   # 5 gem dark
    (255, 232, 120),  # 6 gold light
    (244, 182, 40),   # 7 gold
    (172, 110, 22),   # 8 gold dark
    (255, 130, 130),  # 9 red light
    (228, 54, 66),    # 10 red
    (142, 24, 50),    # 11 red dark
    (210, 244, 255),  # 12 bubble light
    (120, 192, 242),  # 13 bubble mid
    (156, 158, 180),  # 14 grey
    (72, 74, 100),    # 15 dark grey
]
I = dict(zip(["out", "white", "gl", "gm", "gd", "ol", "o", "od", "rl", "r", "rd", "bl", "bm", "grey", "dgrey"], ITEMS_PAL))
FX_PAL = [(255, 255, 255), (255, 244, 170), (255, 210, 90), (150, 240, 255), (70, 170, 240), (200, 160, 255),
          (120, 90, 220), (255, 255, 255), (255, 255, 255), (255, 255, 255), (255, 255, 255), (255, 255, 255),
          (255, 255, 255), (255, 255, 255), (255, 255, 255)]
F = dict(zip(["w", "y", "o", "c", "b", "v", "vd"], FX_PAL))
WALRUS_PAL = [
    (36, 20, 30),     # 1 outline
    (255, 255, 255),  # 2 white
    (20, 10, 16),     # 3 ink
    (226, 168, 132),  # 4 skin light
    (190, 126, 96),   # 5 skin
    (146, 88, 70),    # 6 skin dark
    (98, 56, 52),     # 7 skin deepest
    (255, 246, 220),  # 8 tusk
    (214, 196, 160),  # 9 tusk shade
    (244, 140, 150),  # 10 nose pink
    (226, 52, 60),    # 11 scarf red
    (150, 26, 46),    # 12 scarf dark
    (250, 214, 80),   # 13 scarf stripe
    (210, 236, 255),  # 14 snow
    (130, 170, 220),  # 15 snow shade
]
WL = dict(zip(["out", "white", "ink", "s0", "s1", "s2", "s3", "tusk", "tusks", "nose", "sc", "scd", "scy", "snow",
               "snows"], WALRUS_PAL))

# the terrain: per zone. 1 outline, 2-5 the rock (light .. dark), 6-8 its cap, 9-11 crystal blocks, 12-13 door wood,
# 14 a glow, 15 the deepest shade
TERRAIN = {
    1: [(22, 16, 44), (132, 120, 170), (98, 86, 140), (70, 60, 110), (48, 40, 82),
        (210, 250, 255), (120, 220, 240), (60, 150, 200),
        (255, 210, 250), (220, 130, 236), (140, 70, 180),
        (196, 140, 90), (120, 76, 50), (255, 250, 200), (32, 26, 60)],
    2: [(16, 22, 44), (96, 130, 160), (70, 100, 136), (50, 74, 110), (34, 52, 84),
        (255, 170, 170), (240, 100, 120), (170, 50, 90),
        (220, 250, 255), (130, 210, 240), (60, 140, 200),
        (196, 150, 100), (120, 84, 56), (255, 236, 160), (22, 34, 60)],
    3: [(26, 18, 22), (150, 120, 104), (116, 90, 80), (84, 64, 60), (58, 42, 44),
        (190, 230, 110), (110, 176, 70), (60, 116, 54),
        (255, 220, 160), (240, 160, 80), (180, 90, 50),
        (200, 150, 100), (124, 84, 54), (255, 250, 190), (38, 26, 30)],
    4: [(16, 22, 50), (160, 186, 220), (120, 146, 190), (86, 108, 156), (58, 76, 120),
        (255, 255, 255), (220, 240, 255), (160, 196, 236),
        (220, 255, 255), (130, 230, 240), (60, 170, 210),
        (190, 150, 110), (120, 86, 60), (230, 255, 250), (36, 46, 84)],
}
BACK = {   # the back wall (BG palette 3): 1-5 dark tones, 6-8 the zone's glow
    1: [(24, 18, 46), (32, 24, 58), (40, 30, 70), (50, 38, 84), (60, 48, 98), (90, 170, 210), (130, 210, 240),
        (190, 240, 255)],
    2: [(12, 26, 50), (16, 34, 62), (22, 44, 76), (30, 54, 90), (40, 66, 104), (70, 160, 150), (120, 200, 190),
        (200, 250, 240)],
    3: [(26, 18, 26), (34, 24, 34), (44, 32, 42), (56, 40, 50), (68, 50, 60), (160, 100, 60), (230, 170, 90),
        (255, 230, 150)],
    4: [(14, 22, 48), (20, 30, 62), (28, 40, 78), (38, 52, 94), (50, 66, 110), (110, 190, 220), (170, 230, 250),
        (240, 255, 255)],
}
TERRAIN_NAMES = ["out", "r0", "r1", "r2", "r3", "cap0", "cap1", "cap2", "cr0", "cr1", "cr2", "wood", "woodd", "glow", "deep"]


def ramp(d, *names):
    return [d[n] for n in names]


# ---- the puffin -------------------------------------------------------------------------------------------------------
def puffin(pose="idle", f=0):
    """A 24x24 puffin facing right: black back and head, a white face and belly, the big striped beak, orange feet
    in little blue bubble boots. pose: idle, run, rise, fall, fire, hurt."""
    cv = Canvas(24, 24)
    bob = 0
    if pose == "idle":
        bob = 1 if f == 1 else 0
    if pose == "run":
        bob = 1 if f in (1, 3) else 0
    cx, by = 11.5, 21 + bob                       # body centre x, feet line
    # the feet and boots
    if pose == "run":
        steps = [(-2, 1), (0, 0), (2, -1), (0, 0)][f]
        feet = [(8 + steps[0], by - 1), (13 + steps[1], by - 1)]
    elif pose in ("fire",):
        feet = [(9, by), (13, by)]
    elif pose == "fall":
        feet = [(8, by - 2), (14, by - 1)]
    elif pose == "rise":
        feet = [(9, by - 1), (13, by)]
    else:
        feet = [(9, by - 1), (13, by - 1)]
    for fx, fy in feet:
        cv.rect(fx, fy - 2, fx + 2, fy, H["btm"])       # the boot
        cv.set(fx, fy - 2, H["btl"])
        cv.rect(fx, fy + 1, fx + 3, fy + 1, H["btd"])   # its sole
        cv.set(fx + 3, fy, H["ft"])                     # a toe peeking out
    # the body: a black egg, the white belly
    hs.ellipse(cv, cx, by - 9.0, 6.6, 8.0, [H["blkl"], H["blk"], H["blk"], H["blk"]])
    hs.ellipse(cv, cx + 1.4, by - 7.6, 4.4, 6.2, [H["wht"], H["wht"], H["whts"], H["whts"]])
    # the head: black cap, white face disc
    hy = by - 15.5
    if pose == "hurt":
        hy += 1
    hs.ellipse(cv, cx + 1.0, hy, 5.6, 5.0, [H["blkl"], H["blk"], H["blk"], H["blk"]])
    hs.ellipse(cv, cx + 2.4, hy + 0.8, 3.5, 3.4, [H["wht"], H["wht"], H["whts"], H["whts"]])
    # the wing (raised when rising or firing)
    if pose in ("rise", "fire", "hurt"):
        for i in range(5):
            cv.set(int(cx - 6 - i * 0.6), int(by - 10 - i), H["blk"])
            cv.set(int(cx - 5 - i * 0.6), int(by - 10 - i), H["blkl"])
    elif pose == "fall":
        for i in range(4):
            cv.set(int(cx - 6 - i), int(by - 8 - i // 2), H["blk"])
    else:
        hs.ellipse(cv, cx - 3.5, by - 9, 2.2, 4.0, [H["blkl"], H["blk"], H["blk"], H["blk"]])
    # the beak: a big triangle, red tip, yellow base, an orange middle
    bx, bty = int(cx + 5), int(hy) - 1
    for i in range(6):
        h = 3 - i // 2
        for j in range(-h, h + 1):
            col = H["bky"] if i == 0 else H["bko"] if i < 3 else H["bkr"]
            cv.set(bx + i, bty + 1 + j, col)
    cv.set(bx, bty - 1, H["bky"])
    # the eye and cheek
    ex, ey = int(cx + 2), int(hy) - 1
    if pose == "hurt":
        cv.set(ex, ey, H["ink"]), cv.set(ex + 1, ey + 1, H["ink"]), cv.set(ex + 1, ey, H["ink"]), cv.set(ex, ey + 1, H["ink"])
    elif pose == "idle" and f == 2:
        cv.set(ex, ey + 1, H["ink"]), cv.set(ex + 1, ey + 1, H["ink"])       # a blink
    else:
        cv.set(ex, ey, H["ink"]), cv.set(ex, ey + 1, H["ink"]), cv.set(ex + 1, ey, H["wht"])
    cv.set(ex - 1, ey + 3, H["chk"]), cv.set(ex, ey + 3, H["chk"])
    cv.outline(H["out"])
    return cv


def hero_frames():
    out = [("idle", lambda f: puffin("idle", f), 24, 24, 3), ("run", lambda f: puffin("run", f), 24, 24, 4),
           ("rise", lambda f: puffin("rise"), 24, 24, 1), ("fall", lambda f: puffin("fall"), 24, 24, 1),
           ("fire", lambda f: puffin("fire"), 24, 24, 1), ("hurt", lambda f: puffin("hurt"), 24, 24, 1)]

    def sq(f):
        c = puffin("idle")
        base = Canvas(24, 24)
        base.p = [[None if v == H["out"] else v for v in row] for row in c.p]
        s = hs.squash(base, 1.28 if f == 0 else 0.82)
        s.outline(H["out"])
        return s
    out.append(("squash", sq, 24, 24, 2))
    return out


# ---- the critters (16x16, symbolic ramps) ------------------------------------------------------------------------------
A = ramp(Y, "a0", "a0", "a1", "a2")
B = ramp(Y, "b0", "b0", "b1", "b2")
C = ramp(Y, "c0", "c0", "c1", "c2")
D = ramp(Y, "d0", "d0", "d1", "d2")


def eye(cv, x, y, look=1):
    cv.rect(x, y, x + 1, y + 1, Y["white"])
    cv.set(x + (1 if look > 0 else 0), y + 1, Y["ink"])


def snail(f):
    cv = Canvas(16, 16)
    s = f & 1
    hs.capsule(cv, 2.5, 13.0, 12.5 + s, 13.0, 2.2, D)                   # the foot
    hs.ellipse(cv, 12.5 + s, 10.0, 2.6, 3.0, D)                          # the head
    for sx in (12 + s, 14 + s):
        cv.set(sx, 6, Y["d1"]), cv.set(sx, 7, Y["d1"])
        cv.set(sx, 5, Y["ink"])
    hs.ellipse(cv, 7.0, 8.5, 5.0, 5.0, A)                                # the shell
    for a in range(0, 330, 30):                                          # its spiral
        r = 1.0 + a / 110.0
        cv.set(int(7 + r * math.cos(math.radians(a))), int(8.5 + r * math.sin(math.radians(a))), Y["a2"])
    cv.set(5, 5, Y["white"])
    cv.set(14 + s, 10, Y["ink"])
    cv.outline(Y["out"])
    return cv


def bat(f):
    cv = Canvas(16, 16)
    if f == 2:                                                           # asleep, hanging upside down
        hs.ellipse(cv, 8, 8, 3.5, 5, B)
        for x in range(4, 13):
            cv.set(x, 4 + abs(x - 8) // 2, Y["b2"])
        cv.set(7, 10, Y["ink"]), cv.set(9, 10, Y["ink"])
        cv.set(8, 2, Y["b2"]), cv.set(8, 1, Y["b2"])
        cv.outline(Y["out"])
        return cv
    up = f == 0
    for side in (-1, 1):
        for i in range(7):
            x = 8 + side * (3 + i)
            top = (5 - i // 2) if up else (8 + i // 3)
            for y in range(top, top + 3 - i // 4):
                cv.set(x, y, Y["b1"] if y == top else Y["b2"])
    hs.ellipse(cv, 8, 9, 3.6, 3.6, B)
    cv.set(6, 5, Y["d1"]), cv.set(10, 5, Y["d1"]), cv.set(6, 6, Y["b1"]), cv.set(10, 6, Y["b1"])   # ears
    eye(cv, 6, 8), eye(cv, 9, 8)
    cv.set(8, 12, Y["white"])
    cv.outline(Y["out"])
    return cv


def urchin(f):
    cv = Canvas(16, 16)
    rot = f * 0.2
    for k in range(10):
        a = k * math.pi / 5 + rot
        for r in range(4, 8):
            cv.set(int(round(7.5 + math.cos(a) * r)), int(round(8 + math.sin(a) * r)), Y["c1"] if r < 6 else Y["c0"])
    hs.ellipse(cv, 7.5, 8.0, 4.4, 4.0, C)
    eye(cv, 5, 7), eye(cv, 9, 7)
    cv.set(7, 10, Y["c2"]), cv.set(8, 10, Y["c2"])
    cv.outline(Y["out"])
    return cv


def crab(f):
    cv = Canvas(16, 16)
    s = f & 1
    for i, lx in enumerate((3, 5, 10, 12)):                              # legs
        cv.set(lx, 12 + ((i + s) & 1), Y["a2"]), cv.set(lx + (1 if lx > 8 else -1), 13 + ((i + s) & 1), Y["a2"])
    hs.ellipse(cv, 8, 10, 5.5, 3.4, A)
    for side in (-1, 1):                                                 # claws up
        cx = 8 + side * 6
        hs.ellipse(cv, cx, 5 + s, 2.2, 2.2, A)
        cv.set(cx, 3 + s, None), cv.set(cx + side, 3 + s, None)
    for ex in (6, 10):                                                   # eye stalks
        cv.set(ex, 7, Y["a1"])
        cv.rect(ex - 1, 5, ex, 6, Y["white"]), cv.set(ex, 6, Y["ink"])
    cv.set(7, 11, Y["ink"]), cv.set(8, 12, Y["ink"]), cv.set(9, 11, Y["ink"])
    cv.outline(Y["out"])
    return cv


def fish(f):
    cv = Canvas(16, 16)
    s = f & 1
    hs.ellipse(cv, 8.5, 8.5, 5.5, 3.8, B)
    for i in range(4):                                                   # the tail
        for y in range(8 - i - s, 9 + i + s):
            cv.set(2 - i // 2, y, Y["d1"])
    cv.set(9, 4, Y["d1"]), cv.set(8, 4, Y["d1"]), cv.set(10, 5, Y["d1"])  # the fin
    for x in (6, 9):
        for y in range(6, 12):
            if cv.get(x, y) is not None:
                cv.set(x, y, Y["b1"])
    eye(cv, 11, 7)
    cv.set(13, 10, Y["ink"])
    cv.outline(Y["out"])
    return cv


def jelly(f):
    cv = Canvas(16, 16)
    s = f & 1
    hs.ellipse(cv, 8, 6.5, 5.5, 4.5, D)
    for y in range(7, 10):
        for x in range(2, 15):
            if y > 7 and cv.get(x, y) is not None:
                cv.set(x, y, None)
    for k, tx in enumerate((4, 7, 10, 12)):
        for y in range(9, 14 + ((k + s) & 1)):
            cv.set(tx + ((y + k + s) % 3 == 0), y, Y["d0"] if y % 2 else Y["d1"])
    eye(cv, 6, 5), eye(cv, 9, 5)
    cv.set(5, 3, Y["white"])
    cv.outline(Y["out"])
    return cv


def puffer(f):
    cv = Canvas(16, 16)
    if f == 2:                                                           # puffed up: spikes out
        for k in range(12):
            a = k * math.pi / 6
            for r in range(5, 8):
                cv.set(int(round(7.5 + math.cos(a) * r)), int(round(7.5 + math.sin(a) * r)), Y["c1"])
        hs.ellipse(cv, 7.5, 7.5, 5.5, 5.5, B)
        eye(cv, 8, 5)
        cv.set(11, 9, Y["ink"]), cv.set(12, 9, Y["ink"])
    else:
        hs.ellipse(cv, 8, 8.5, 4.5, 3.6, B)
        for y in range(7 - f, 11 + f):
            cv.set(3, y, Y["d1"])
        cv.set(9, 5, Y["d1"])
        eye(cv, 9, 7)
        cv.set(12, 9, Y["ink"])
    cv.outline(Y["out"])
    return cv


def beetle(f):
    cv = Canvas(16, 16)
    s = f & 1
    for i, lx in enumerate((4, 7, 10)):
        cv.set(lx + ((i + s) & 1), 13, Y["ink"]), cv.set(lx, 12, Y["ink"])
    hs.ellipse(cv, 7.5, 9, 5.5, 4.0, A)
    for y in range(5, 13):
        if cv.get(7, y) is not None:
            cv.set(7, y, Y["a2"])
    hs.ellipse(cv, 13, 10, 2.2, 2.2, ramp(Y, "a1", "a1", "a2", "a2"))
    cv.set(14, 7, Y["ink"]), cv.set(15, 6, Y["ink"])                     # a horn
    cv.set(13, 9, Y["white"]), cv.set(14, 9, Y["ink"])
    cv.set(5, 6, Y["white"])
    cv.outline(Y["out"])
    return cv


def moth(f):
    cv = Canvas(16, 16)
    up = f == 0
    for side in (-1, 1):
        hs.ellipse(cv, 8 + side * 4, 6 if up else 9, 3.4, 3.8 if up else 3.0, B)
        cv.set(8 + side * 4, 6 if up else 9, Y["d1"])
    hs.capsule(cv, 8, 6, 8, 12, 1.6, D)
    cv.set(7, 3, Y["ink"]), cv.set(6, 2, Y["ink"]), cv.set(9, 3, Y["ink"]), cv.set(10, 2, Y["ink"])
    cv.set(7, 6, Y["white"]), cv.set(9, 6, Y["white"])
    cv.outline(Y["out"])
    return cv


def puffcap(f):
    cv = Canvas(16, 16)
    sq = 1 if f == 1 else 0
    hs.capsule(cv, 8, 10, 8, 14, 2.4, ramp(Y, "white", "white", "d0", "d0"))      # the stem
    hs.ellipse(cv, 8, 7 + sq, 6.5 + sq, 4.4 - sq, D)                               # the cap
    for y in range(9 + sq, 13):
        for x in range(1, 16):
            if y > 8 + sq and cv.get(x, y) in (Y["d0"], Y["d1"], Y["d2"]) and abs(x - 8) > 2:
                cv.set(x, y, None)
    for sx, sy in ((5, 5), (10, 6), (8, 4)):
        cv.set(sx, sy + sq, Y["white"])
    cv.set(7, 12, Y["ink"]), cv.set(9, 12, Y["ink"])
    cv.outline(Y["out"])
    return cv


def spore(f):
    cv = Canvas(8, 8)
    hs.ellipse(cv, 3.5, 3.5, 2.5 + (f & 1) * 0.4, 2.5, D)
    cv.set(2, 2, Y["white"])
    cv.outline(Y["out"])
    return cv


def grub(f):
    cv = Canvas(16, 16)
    s = f & 1
    for k in range(4):
        x = 3 + k * 3 + (s if k % 2 else 0)
        hs.ellipse(cv, x, 10.5 - (1 if (k + s) % 2 else 0), 2.6, 2.8, A)
    for k in range(4):
        cv.set(3 + k * 3, 13, Y["ink"])
    hs.ellipse(cv, 13.5, 9, 2.6, 2.8, A)
    eye(cv, 13, 8)
    for x in range(2, 13, 2):                                            # the wool's tufts
        cv.set(x, 7 + (x % 4 == 0), Y["white"])
    cv.outline(Y["out"])
    return cv


def icicle(f):
    cv = Canvas(16, 16)
    for y in range(0, 15):
        w = max(0, 4 - y // 4)
        for x in range(8 - w, 8 + w + 1):
            cv.set(x + (f if y > 10 else 0), y, Y["d0"] if x < 8 else Y["d1"] if x == 8 else Y["d2"])
    cv.set(7, 3, Y["white"]), cv.set(7, 4, Y["white"])
    eye(cv, 6, 5), eye(cv, 9, 5)
    cv.outline(Y["out"])
    return cv


def wisp(f):
    cv = Canvas(16, 16)
    for k in range(4):                                                   # the trailing tail
        y = 12 + k // 2
        cv.set(5 + k + (f if k % 2 else 0), y, Y["d1"] if k < 2 else Y["d2"])
    hs.ellipse(cv, 8, 7.5, 5.0, 5.0, D)
    hs.ellipse(cv, 7, 6.5, 2.4, 2.4, ramp(Y, "white", "white", "d0", "d0"))
    cv.set(6, 7, Y["ink"]), cv.set(9, 7, Y["ink"])
    cv.set(7, 9 + f, Y["d2"]), cv.set(8, 9 + f, Y["d2"])
    for k in range(6):                                                   # frost spikes
        a = k * math.pi / 3 + f * 0.5
        cv.set(int(round(8 + math.cos(a) * 6.5)), int(round(7.5 + math.sin(a) * 6.5)), Y["c0"])
    cv.outline(Y["out"])
    return cv


CRITTERS = [("snail", snail, 2), ("bat", bat, 3), ("urchin", urchin, 2), ("crab", crab, 2), ("fish", fish, 2),
            ("jelly", jelly, 2), ("puffer", puffer, 3), ("beetle", beetle, 2), ("moth", moth, 2),
            ("puffcap", puffcap, 2), ("grub", grub, 2), ("icicle", icicle, 2), ("wisp", wisp, 2)]


# ---- the Grumpy Walrus (48x48) ----------------------------------------------------------------------------------------------
def walrus(f):
    """f 0, 1: sliding; 2: rearing up (throwing); 3: hurt (eyes shut)"""
    cv = Canvas(48, 48)
    rear = f == 2
    by = 44
    skin = ramp(WL, "s0", "s1", "s2", "s3")
    if rear:
        hs.ellipse(cv, 24, by - 16, 15, 16, skin)                        # the body upright
        hs.ellipse(cv, 26, by - 30, 11, 9, skin)                         # the head
        hy = by - 30
    else:
        hs.ellipse(cv, 22, by - 11, 19, 11 - (f & 1), skin)
        hs.ellipse(cv, 32, by - 20, 11, 9, skin)
        hy = by - 20
    hx = 32 if not rear else 26
    # flippers
    for side in (-1, 1):
        fx = (12 if not rear else 24 + side * 14)
        hs.ellipse(cv, fx + side * (0 if rear else 6), by - (3 if not rear else 14), 5, 3, skin)
    # the scarf
    for x in range(hx - 10, hx + 9):
        for y in range(hy + 7, hy + 10):
            if cv.get(x, y) is not None:
                cv.set(x, y, WL["scy"] if (x // 3) % 3 == 0 else WL["sc"] if y < hy + 9 else WL["scd"])
    for y in range(hy + 9, hy + 16):
        cv.set(hx - 8, y, WL["sc"]), cv.set(hx - 7, y, WL["scd"])
    # the muzzle, the whiskers, the tusks
    hs.ellipse(cv, hx + 3, hy + 3, 6, 4, ramp(WL, "s0", "s0", "s1", "s1"))
    cv.rect(hx + 2, hy, hx + 4, hy + 1, WL["nose"])
    for wx in range(hx - 1, hx + 9, 3):
        cv.set(wx, hy + 3, WL["s3"]), cv.set(wx + 1, hy + 4, WL["s3"])
    for tx in (hx, hx + 5):
        for y in range(hy + 6, hy + 13):
            cv.set(tx, y, WL["tusk"]), cv.set(tx + 1, y, WL["tusks"])
    # the grumpy brows and eyes
    for ex in (hx - 4, hx + 6):
        if f == 3:
            cv.rect(ex - 1, hy - 4, ex + 1, hy - 4, WL["ink"])
        else:
            cv.rect(ex - 1, hy - 5, ex + 1, hy - 4, WL["white"])
            cv.set(ex, hy - 4, WL["ink"])
        cv.set(ex - 2, hy - 7 + (1 if ex > hx else 0), WL["s3"]), cv.set(ex - 1, hy - 7, WL["s3"])
        cv.set(ex, hy - 6, WL["s3"]), cv.set(ex + 1, hy - 6 + (0 if ex > hx else 1), WL["s3"])
    # a little snow on its back
    for x in range(10, 30, 2):
        if cv.get(x, by - 22 + abs(x - 20) // 3) is not None and not rear:
            cv.set(x, by - 22 + abs(x - 20) // 3, WL["snow"])
    cv.outline(WL["out"])
    return cv


# ---- items (OBJ palette 2) ------------------------------------------------------------------------------------------------------
def gem(big=0, f=0):
    cv = Canvas(8, 8)
    ramp4 = [I["gl"], I["gl"], I["gm"], I["gd"]] if not big else [I["ol"], I["ol"], I["o"], I["od"]]
    for y in range(7):
        w = 3 - abs(y - 3) if y < 4 else 3 - (y - 3)
        for x in range(4 - w - 1, 4 + w):
            cv.set(x, y, ramp4[1] if x < 3 else ramp4[2] if y < 4 else ramp4[3])
    cv.set(2 + f, 2, I["white"])
    cv.outline(I["out"])
    return cv


def heart(full=1):
    cv = Canvas(8, 8)
    shape = ["..#.#..", ".#####.", "#######", "#######", ".#####.", "..###..", "...#..."]
    for y, row in enumerate(shape):
        for x, c in enumerate(row):
            if c == "#":
                cv.set(x, y, (I["rl"] if (x + y) < 4 else I["r"] if y < 5 else I["rd"]) if full else I["dgrey"])
    if full:
        cv.set(1, 2, I["white"])
    cv.outline(I["out"])
    return cv


def pip(full=1):
    cv = Canvas(8, 8)
    hs.ellipse(cv, 3.5, 3.5, 2.6, 2.6, [I["bl"], I["bl"], I["bm"], I["gd"]] if full else [I["dgrey"]] * 4)
    if full:
        cv.set(2, 2, I["white"])
    cv.outline(I["out"])
    return cv


def bubble_shot(k=0):
    """a soap bubble: a white rim round a pale inside, a shine (round: never mistaken for a gem's diamond)"""
    cv = Canvas(8, 8)
    r = 3.1 if k == 0 else 2.2
    for y in range(8):
        for x in range(8):
            d = math.hypot(x + 0.5 - 4, y + 0.5 - 4)
            if d < r:
                cv.set(x, y, I["white"] if d > r - 1.0 else I["bl"])
    cv.set(3, 2, I["white"]), cv.set(2, 3, I["white"])
    cv.outline(I["bm"])
    return cv


def bar(k):
    """the walrus's health: an 8x8 piece of the bar, k eighths full"""
    cv = Canvas(8, 8)
    for x in range(8):
        cv.set(x, 1, I["out"]), cv.set(x, 6, I["out"])
        for y in range(2, 6):
            cv.set(x, y, (I["rl"] if y == 2 else I["r"]) if x < k else I["dgrey"])
    return cv


def casing(f):
    """a tiny spent bubble shell (gold), 4 spins"""
    cv = Canvas(8, 8)
    pts = [[(3, 3), (4, 3), (3, 4), (4, 4), (5, 3)], [(3, 2), (4, 3), (4, 4), (3, 3)],
           [(3, 3), (4, 3), (2, 3), (3, 4)], [(4, 2), (3, 3), (3, 4), (4, 3)]][f]
    for x, y in pts:
        cv.set(x, y, I["o"] if (x + y) % 2 else I["ol"])
    cv.outline(I["od"])
    return cv


def puff(f):
    cv = Canvas(8, 8)
    r = [1.5, 2.6, 3.3][f]
    for y in range(8):
        for x in range(8):
            d = math.hypot(x + 0.5 - 4, y + 0.5 - 4)
            if d < r and not (f == 2 and hs.checker(x, y)):
                cv.set(x, y, I["white"] if d < r - 1 else I["grey"])
    return cv


def spark(f):
    cv = Canvas(8, 8)
    r = 3 - f
    for i in range(-r, r + 1):
        cv.set(4 + i, 4, I["ol"]), cv.set(4, 4 + i, I["ol"])
    cv.set(4, 4, I["white"])
    return cv


def shard(f):
    cv = Canvas(8, 8)
    pts = [[(2, 2), (3, 3), (4, 4), (3, 2)], [(4, 1), (4, 2), (3, 3), (5, 2)], [(2, 4), (3, 4), (4, 3), (3, 5)]][f]
    for x, y in pts:
        cv.set(x, y, I["gl"] if (x + y) % 2 else I["gm"])
    cv.outline(I["out"])
    return cv


def poof(f):
    cv = Canvas(16, 16)
    r = [3.0, 5.5, 7.0][f]
    for y in range(16):
        for x in range(16):
            d = math.hypot(x + 0.5 - 8, y + 0.5 - 8)
            if d < r and (f < 2 or not hs.checker(x, y)) and not (f >= 1 and d < r - 3):
                cv.set(x, y, I["white"] if d < r - 1.5 else I["bl"])
    for k in range(6):
        a = k * math.pi / 3 + f
        cv.set(int(8 + math.cos(a) * (r + 1)), int(8 + math.sin(a) * (r + 1)), I["ol"])
    return cv


def snowball(f):
    cv = Canvas(8, 8)
    hs.ellipse(cv, 3.5, 3.5, 3.0, 3.0, [I["white"], I["white"], I["bl"], I["bm"]])
    cv.set(2 + f, 2, I["grey"])
    cv.outline(I["out"])
    return cv


SMALL_FONT = {
    "0": ["###", "#.#", "#.#", "#.#", "###"], "1": [".#.", "##.", ".#.", ".#.", "###"],
    "2": ["###", "..#", "###", "#..", "###"], "3": ["###", "..#", ".##", "..#", "###"],
    "4": ["#.#", "#.#", "###", "..#", "..#"], "5": ["###", "#..", "###", "..#", "###"],
    "6": ["###", "#..", "###", "#.#", "###"], "7": ["###", "..#", ".#.", ".#.", ".#."],
    "8": ["###", "#.#", "###", "#.#", "###"], "9": ["###", "#.#", "###", "..#", "###"],
    "x": ["...", "#.#", ".#.", "#.#", "..."], "+": ["...", ".#.", "###", ".#.", "..."],
    "-": ["...", "...", "###", "...", "..."],
}


def small_digit(ch, gold=0):
    """a 3x5 digit (and x + -) in an 8x8 tile, cream (gold for a combo), outlined: the HUD's small numbers"""
    cv = Canvas(8, 8)
    for y, row in enumerate(SMALL_FONT[ch]):
        for x, c in enumerate(row):
            if c == "#":
                cv.set(2 + x, 1 + y, (I["ol"] if y < 3 else I["o"]) if gold else (I["white"] if y < 3 else I["bl"]))
    cv.outline(I["out"], diag=True)
    return cv


# the shop's icons, 16x16
def icon(kind):
    cv = Canvas(16, 16)
    if kind in ("heart", "heal"):
        hc = heart(1)
        for y in range(8):
            for x in range(8):
                v = hc.get(x, y)
                if v is not None:
                    for sy in range(2):
                        for sx in range(2):
                            cv.set(1 + x * 2 + sx, 1 + y * 2 + sy, v)
        if kind == "heart":                                              # a plus: one more heart
            cv.rect(11, 1, 13, 7, None)
            cv.rect(10, 3, 14, 5, I["white"]), cv.rect(12, 1, 12, 7, I["white"])
            cv.set(11, 3, I["white"]), cv.set(13, 3, I["white"]), cv.set(11, 5, I["white"]), cv.set(13, 5, I["white"])
            cv.rect(12, 2, 12, 6, I["white"])
        else:                                                            # a bandage across
            for k in range(-3, 4):
                cv.set(8 + k, 8 - k, I["ol"]), cv.set(8 + k, 9 - k, I["ol"])
        cv.outline(I["out"])
        return cv
    if kind == "ammo":
        for k, (x, y) in enumerate(((5, 5), (10, 5), (5, 10), (10, 10))):
            hs.ellipse(cv, x, y, 2.6, 2.6, [I["white"], I["bl"], I["bm"], I["gd"]])
        cv.rect(12, 0, 14, 2, None)
    elif kind == "spread":
        for k, dx in enumerate((-4, 0, 4)):
            for i in range(4):
                cv.set(7 + dx * i // 3, 4 + i * 3, I["bm"])
            hs.ellipse(cv, 7.5 + dx * 1.3, 13, 1.8, 1.8, [I["white"], I["bl"], I["bm"], I["gd"]])
        hs.ellipse(cv, 7.5, 3, 2.5, 2.2, [I["gl"], I["gl"], I["gm"], I["gd"]])
    elif kind == "beam":
        cv.rect(6, 2, 9, 15, I["bl"])
        cv.rect(7, 2, 8, 15, I["white"])
        hs.ellipse(cv, 7.5, 3, 3.5, 2.5, [I["gl"], I["gl"], I["gm"], I["gd"]])
    elif kind == "burst":
        for k in range(3):
            hs.ellipse(cv, 4.5 + k * 3.5, 5 + k * 3.5, 2.2, 2.2, [I["white"], I["bl"], I["bm"], I["gd"]])
        for k in range(3):
            cv.set(13, 2 + k, I["ol"]), cv.set(12 + k, 3, I["ol"])
    elif kind == "magnet":
        for y in range(3, 14):
            for x in range(2, 14):
                d = math.hypot(x + 0.5 - 8, y + 0.5 - 8)
                if 3 < d < 6.5 and y < 9:
                    cv.set(x, y, I["r"] if x < 8 else I["rd"])
        cv.rect(2, 8, 4, 13, I["r"]), cv.rect(11, 8, 13, 13, I["rd"])
        cv.rect(2, 12, 4, 13, I["grey"]), cv.rect(11, 12, 13, 13, I["grey"])
    elif kind == "springs":
        for k in range(4):
            cv.rect(4, 4 + k * 2, 11, 4 + k * 2, I["grey"] if k % 2 else I["white"])
        cv.rect(3, 12, 12, 14, I["bm"]), cv.rect(3, 12, 12, 12, I["bl"])
        cv.rect(5, 1, 10, 3, I["o"])
    elif kind == "feather":
        for k in range(12):
            x, y = 3 + k * 0.8, 13 - k
            for d in range(-2, 3):
                if abs(d) <= 2 - abs(k - 6) // 4:
                    cv.set(int(x + d), int(y), I["white"] if d <= 0 else I["bl"])
        for k in range(12):
            cv.set(int(3 + k * 0.8), 13 - k, I["grey"])
    elif kind == "shell":
        for y in range(16):
            for x in range(16):
                d = math.hypot(x + 0.5 - 8, y + 0.5 - 8)
                if 5.2 < d < 7.0:
                    cv.set(x, y, I["bl"] if x + y < 16 else I["bm"])
        cv.set(5, 4, I["white"]), cv.set(4, 5, I["white"])
    elif kind == "lucky":
        for k, (x, y) in enumerate(((8, 4.5), (4.5, 8), (11.5, 8), (8, 11.5))):
            hs.ellipse(cv, x, y, 2.8, 2.8, [I["rl"], I["rl"], I["r"], I["rd"]] if k == 0 else [I["gl"], I["gl"], I["gm"], I["gd"]])
        cv.set(8, 8, I["ol"])
    elif kind == "door":                                                 # (the HUD: a side room)
        cv.rect(4, 4, 11, 15, I["od"])
        hs.ellipse(cv, 7.5, 5, 4, 3, [I["o"]] * 4)
    cv.outline(I["out"])
    return cv


UP_ICONS = ["heart", "heal", "ammo", "spread", "beam", "burst", "magnet", "springs", "feather", "shell", "lucky"]


def items_frames():
    out = [("gem", lambda f: gem(0, f), 8, 8, 2), ("biggem", lambda f: gem(1, f), 8, 8, 2),
           ("heart", lambda f: heart(1 - f), 8, 8, 2), ("pip", lambda f: pip(1 - f), 8, 8, 2),
           ("shot", lambda f: bubble_shot(f), 8, 8, 2), ("bar", bar, 8, 8, 9), ("casing", casing, 8, 8, 4), ("puff", puff, 8, 8, 3),
           ("spark", spark, 8, 8, 3), ("shard", shard, 8, 8, 3), ("snowball", snowball, 8, 8, 2)]
    for ch in "0123456789x+-":
        out.append(("d_" + {"x": "x", "+": "plus", "-": "minus"}.get(ch, ch), lambda f, ch=ch: small_digit(ch, 0), 8, 8, 1))
    for ch in "0123456789x+":
        out.append(("g_" + {"x": "x", "+": "plus"}.get(ch, ch), lambda f, ch=ch: small_digit(ch, 1), 8, 8, 1))
    out.append(("poof", poof, 16, 16, 3))
    for k in UP_ICONS:
        out.append(("i_" + k, lambda f, k=k: icon(k), 16, 16, 1))
    out.append(("cursor", lambda f: cursor(f), 8, 8, 2))
    return out


def cursor(f):
    cv = Canvas(8, 8)
    for y in range(7):
        for x in range(0, 4 - abs(y - 3) + 1):
            cv.set(x + 1 + f, y, I["ol"] if y < 3 else I["o"])
    cv.outline(I["out"])
    return cv


# ---- the see-through effects (OBJ palette 4, added) -------------------------------------------------------------------------
def flash(f):
    cv = Canvas(16, 8)
    for y in range(8):
        for x in range(16):
            d = math.hypot((x + 0.5 - 8) / (6.5 - f * 2), (y + 0.5 - 1) / (6.5 - f * 2.5))
            if d < 1:
                cv.set(x, y, F["w"] if d < 0.4 else F["y"] if d < 0.7 else F["o"])
    return cv


def beam(f):
    cv = Canvas(8, 16)
    for y in range(16):
        for x in range(8):
            d = abs(x + 0.5 - 4)
            if d < 3.4 - f:
                cv.set(x, y, F["w"] if d < 1.2 else F["c"] if d < 2.4 else F["b"])
    return cv


def shield(f):
    cv = Canvas(24, 24)
    for y in range(24):
        for x in range(24):
            d = math.hypot(x + 0.5 - 12, y + 0.5 - 12)
            if 9.5 - f < d < 11.5 - f:
                cv.set(x, y, F["c"] if (x + y) < 22 else F["b"])
    cv.set(7, 6, F["w"]), cv.set(6, 7, F["w"])
    return cv


def glow(f):
    cv = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            d = math.hypot(x + 0.5 - 8, y + 0.5 - 8)
            if d < 7 - f * 2 and hs.dither_ok(x, y, 16 - int(d * 2)):
                cv.set(x, y, F["y"] if d < 3 else F["o"])
    return cv


FXS = [("flash", flash, 16, 8, 2), ("beam", beam, 8, 16, 2), ("shield", shield, 24, 24, 2), ("glow", glow, 16, 16, 2)]


# ---- terrain (per zone, 8x8 tiles) ----------------------------------------------------------------------------------------------
def terrain_tiles(z):
    T = dict(zip(TERRAIN_NAMES, TERRAIN[z]))
    rock = [T["r0"], T["r1"], T["r2"], T["r3"]]

    def rock_px(x, y, cx, cy):
        h = hsh(x + cx * 16, y + cy * 16, z) % 100
        base = T["r1"] if h < 20 else T["r2"]
        if h > 92:
            base = T["r3"]
        if (x * 3 + y * 5 + cx * 7) % 23 == 0:
            base = T["r0"]
        return base

    def cell(up, dn, lf, rt, ul, ur, dl, dr, var=0):
        cv = Canvas(16, 16)
        for y in range(16):
            for x in range(16):
                cv.set(x, y, rock_px(x, y, var, var * 3))
        # the open sides: an outline, a light rim at the top, a shade at the bottom
        for x in range(16):
            if not up:
                cv.set(x, 0, T["out"]), cv.set(x, 1, T["r0"]), cv.set(x, 2, T["r1"] if x % 3 else T["r0"])
            if not dn:
                cv.set(x, 15, T["out"]), cv.set(x, 14, T["r3"])
        for y in range(16):
            if not lf:
                cv.set(0, y, T["out"]), cv.set(1, y, T["r1"] if y > 2 else cv.get(1, y))
            if not rt:
                cv.set(15, y, T["out"]), cv.set(14, y, T["r3"] if y > 1 else cv.get(14, y))
        if up and lf and not ul:
            cv.set(0, 0, T["out"])
        if up and rt and not ur:
            cv.set(15, 0, T["out"])
        if dn and lf and not dl:
            cv.set(0, 15, T["out"])
        if dn and rt and not dr:
            cv.set(15, 15, T["out"])
        if not up:                                                       # the zone's cap on the top
            for x in range(16):
                k = hsh(x, z, var) % 7
                if z == 1:       # crystal shards
                    hgt = [0, 2, 3, 1, 0, 2, 1][k]
                    for y in range(hgt):
                        cv.set(x, 1 + y, T["cap0"] if x % 2 == 0 else T["cap1"])
                elif z == 2:     # pink coral fuzz
                    cv.set(x, 1, T["cap0"] if k < 4 else T["cap1"])
                    cv.set(x, 2, T["cap1"] if k < 5 else T["cap2"])
                elif z == 3:     # moss
                    cv.set(x, 1, T["cap0"]), cv.set(x, 2, T["cap1"])
                    if k < 3:
                        cv.set(x, 3, T["cap2"])
                else:            # snow
                    cv.set(x, 1, T["cap0"]), cv.set(x, 2, T["cap0"] if k < 5 else T["cap1"])
                    if k < 2:
                        cv.set(x, 3, T["cap1"])
                if x == 0 and not lf:
                    cv.set(x, 1, T["out"]), cv.set(x, 2, T["out"])
                if x == 15 and not rt:
                    cv.set(x, 1, T["out"]), cv.set(x, 2, T["out"])
        return cv

    def q(cv, qx, qy):
        out = Canvas(8, 8)
        for y in range(8):
            for x in range(8):
                out.p[y][x] = cv.get(qx * 8 + x, qy * 8 + y)
        return out

    tiles = []
    # quarters: corner (TL, TR, BL, BR) x type (out, hor, ver, inn, int)
    for corner in range(4):
        qx, qy = corner & 1, corner >> 1
        for typ in range(5):
            v = typ in (2, 3, 4)          # the vertical neighbour is rock
            hh = typ in (1, 3, 4)         # the horizontal one
            d = typ == 4
            nb = dict(up=1, dn=1, lf=1, rt=1, ul=1, ur=1, dl=1, dr=1)
            if qy == 0:
                nb["up"] = v
            else:
                nb["dn"] = v
            if qx == 0:
                nb["lf"] = hh
            else:
                nb["rt"] = hh
            dk = ("u" if qy == 0 else "d") + ("l" if qx == 0 else "r")
            nb[dk] = d
            tiles.append(("q%d%d" % (corner, typ), q(cell(**nb), qx, qy)))
    for k in range(4):                                                   # interior variants
        full = cell(1, 1, 1, 1, 1, 1, 1, 1, var=k + 1)
        tiles.append(("int%d" % k, q(full, k & 1, k >> 1)))
    # crystal blocks (whole and cracked)
    for cracked in (0, 1):
        cv = Canvas(16, 16)
        cr = [T["cr0"], T["cr0"], T["cr1"], T["cr2"]]
        for y in range(16):
            for x in range(16):
                facet = (x + y) // 5 % 3
                cv.set(x, y, cr[1 + facet] if facet else cr[1])
        for k in range(16):
            cv.set(k, 15 - k, T["cr0"]) if k % 5 else None
        cv.set(3, 3, T["glow"]), cv.set(4, 3, T["glow"]), cv.set(3, 4, T["glow"])
        if cracked:
            for k, (x, y) in enumerate(((8, 2), (7, 4), (8, 6), (6, 8), (5, 10), (8, 9), (10, 11), (9, 13))):
                cv.set(x, y, T["out"]), cv.set(x + 1, y, T["out"])
        for x in range(16):
            cv.set(x, 0, T["out"]), cv.set(x, 15, T["out"])
        for y in range(16):
            cv.set(0, y, T["out"]), cv.set(15, y, T["out"])
        for k in range(4):
            tiles.append(("%s%d" % ("crk" if cracked else "crys", k), q(cv, k & 1, k >> 1)))
    # the side rooms' doors, in the left wall (the right one: flipped): an arch, a lamp, a sign (shop: a gem; gift: a
    # bow), and the shut one (boards)
    for kind in ("shop", "gift", "shut"):
        cv = cell(1, 1, 1, 1, 1, 1, 1, 1, var=7)
        for y in range(3, 16):
            for x in range(5, 16):
                inside = (x - 15) ** 2 / 100.0 + (y - 9) ** 2 / 49.0 < 1.0 or (y >= 9 and x >= 6)
                if inside:
                    cv.set(x, y, T["deep"] if kind != "shut" else (T["wood"] if (y % 4) else T["woodd"]))
        for y in range(3, 16):                                           # the frame
            for x in range(4, 16):
                if cv.get(x, y) == T["deep"] or cv.get(x, y) in (T["wood"], T["woodd"]):
                    if cv.get(x - 1, y) not in (T["deep"], T["wood"], T["woodd"]):
                        cv.set(x - 1, y, T["woodd"])
        if kind == "shop":
            for y in range(6, 11):                                       # a gem sign glowing in the dark
                w = 2 - abs(y - 8)
                for x in range(12 - w, 13 + w):
                    cv.set(x, y, T["glow"] if x < 12 else T["cr1"])
        elif kind == "gift":
            cv.rect(10, 9, 14, 13, T["cap1"])
            cv.rect(12, 9, 12, 13, T["glow"]), cv.rect(10, 11, 14, 11, T["glow"])
            cv.set(11, 8, T["glow"]), cv.set(13, 8, T["glow"])
        else:
            for k in range(6, 16):
                cv.set(k, 4 + (k - 6), T["woodd"])
        for k in range(4):
            tiles.append(("door_%s%d" % (kind, k), q(cv, k & 1, k >> 1)))
    # the bottom's floor: a warm glowing cap over the rock
    fl = cell(0, 1, 1, 1, 1, 1, 1, 1, var=9)
    for x in range(16):
        fl.set(x, 1, T["glow"]), fl.set(x, 2, T["cap0"] if x % 2 else T["glow"]), fl.set(x, 3, T["cap1"])
    for k in range(2):
        tiles.append(("floor_top%d" % k, q(fl, k, 0)))
    # the side panels' decorations (in the rock beside the shaft, BG palette 2 = darker): 3 per zone, 16x16
    for d in range(3):
        cv = cell(1, 1, 1, 1, 1, 1, 1, 1, var=11 + d)
        if z == 1:      # crystal clusters
            for k in range(3):
                bx = 4 + k * 4
                hgt = [9, 13, 7][(k + d) % 3]
                for y in range(16 - hgt, 15):
                    for x in range(bx - 1, bx + 2):
                        cv.set(x, y, T["cap0"] if x < bx else T["cap1"] if x == bx else T["cap2"])
                cv.set(bx, 15 - hgt, T["glow"])
        elif z == 2:    # coral fans and shells
            if d == 2:
                hs.ellipse(cv, 8, 9, 5, 4, [T["cr0"], T["cr0"], T["cr1"], T["cr2"]])
                for k in range(-3, 4, 2):
                    cv.set(8 + k, 6, T["out"]), cv.set(8 + k, 7, T["out"])
            else:
                for k in range(7):
                    a = math.pi * (0.15 + k * 0.12)
                    for r in range(2, 8):
                        cv.set(int(8 + math.cos(a) * r), int(14 - math.sin(a) * r * 1.3), T["cap0"] if r > 5 else T["cap1"])
        elif z == 3:    # glowing mushrooms
            for k in range(2):
                bx, top = 5 + k * 6, 6 + ((k + d) % 2) * 3
                cv.rect(bx, top + 2, bx + 1, 14, T["cap0"])
                hs.ellipse(cv, bx + 0.5, top + 1.5, 3.2, 2.0, [T["glow"], T["glow"], T["cr1"], T["cr2"]])
        else:           # frozen fish and ice veins
            if d == 0:
                hs.ellipse(cv, 8, 8, 5, 2.6, [T["cap0"], T["cap0"], T["cap1"], T["cap2"]])
                cv.set(3, 6, T["cap1"]), cv.set(2, 5, T["cap1"]), cv.set(2, 10, T["cap1"]), cv.set(10, 7, T["out"])
            else:
                for k in range(16):
                    cv.set(k, int(8 + 4 * math.sin(k / 3.0 + d)), T["cap0"])
                    cv.set(k, int(9 + 4 * math.sin(k / 3.0 + d)), T["cap1"])
        for k in range(4):
            tiles.append(("deco%d_%d" % (d, k), q(cv, k & 1, k >> 1)))
    return tiles, TERRAIN[z]


def back_tiles(z):
    """the back wall's 8x8 tiles (BG palette 3), 12 a zone: 0-3 the dark rock, 4-7 a lighter patch of it (draw.c lays
    them in big soft patches: depth), 8-9 the zone's motif (crystal glints, seaweed and a bubble, glowing spores, frost
    flakes), 10 a glint, 11 a crack"""
    Bk = BACK[z]
    tiles = []
    for k in range(12):
        cv = Canvas(8, 8)
        light = 4 <= k < 8 or k in (9, 10)
        for y in range(8):
            for x in range(8):
                h = hsh(x, y, k, z) % 100
                if light:
                    c = Bk[2] if h < 55 else Bk[3] if h < 85 else Bk[1]
                else:
                    c = Bk[1] if h < 55 else Bk[0] if h < 80 else Bk[2]
                cv.set(x, y, c)
        if k in (8, 9):                                                  # the zone's motif, dim
            if z == 1:                                                   # a little crystal cluster
                for (x, y, c) in ((3, 2, 6), (3, 3, 5), (3, 4, 5), (4, 3, 5), (4, 4, 4), (2, 4, 4), (5, 5, 4)):
                    cv.set(x - (k - 8) * 2, y + (k - 8), Bk[c] if c < 8 else Bk[7])
            elif z == 2:                                                 # a seaweed strand, a bubble
                if k == 8:
                    for y in range(8):
                        cv.set(3 + (y // 2) % 2, y, Bk[5])
                else:
                    cv.set(4, 2, Bk[6]), cv.set(3, 3, Bk[6]), cv.set(5, 3, Bk[6]), cv.set(4, 4, Bk[6]), cv.set(4, 3, Bk[2])
            elif z == 3:                                                 # glowing spores
                cv.set(2, 2, Bk[6]), cv.set(5, 5, Bk[5]), cv.set(6, 1, Bk[5])
                if k == 9:
                    cv.set(3, 6, Bk[7])
            else:                                                        # frost flakes
                for d in range(-1, 2):
                    cv.set(4 + d, 4, Bk[5]), cv.set(4, 4 + d, Bk[5])
                cv.set(4, 4, Bk[6])
                if k == 9:
                    cv.set(1, 1, Bk[6]), cv.set(6, 6, Bk[5])
        if k == 10:
            cv.set(5, 5, Bk[7]), cv.set(4, 5, Bk[6])
        if k == 11:                                                      # a crack
            for x in range(8):
                cv.set(x, (x // 2 + 3) % 8, Bk[0])
        tiles.append(("back%d" % k, cv))
    return tiles, Bk


# ---- the sheets ---------------------------------------------------------------------------------------------------------------------
def indexed(cv, pal, name):
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


def preview(im, path, bg=(40, 36, 70), k=3, pal=None):
    if pal is not None:
        im = im.copy()
        im.putpalette([255, 0, 255] + [c for p in pal for c in p] + [0] * (3 * (255 - len(pal))))
    rgb = im.convert("RGB")
    out = Image.new("RGB", rgb.size, bg)
    mask = Image.frombytes("L", im.size, bytes(255 if b else 0 for b in im.tobytes()))
    out.paste(rgb, (0, 0), mask)
    out.resize((rgb.width * k, rgb.height * k), Image.NEAREST).save(path)


def write_all(out):
    os.makedirs(os.path.join(out, "preview"), exist_ok=True)
    layouts = {}
    crit = []
    for name, fn, n in CRITTERS:
        c0 = fn(0)
        crit.append((name, fn, c0.w, c0.h, n))
    crit.append(("spore", spore, 8, 8, 2))
    jobs = [("hero", hero_frames(), HERO_PAL, 192, None), ("critters", crit, SYM_PAL, 256, ZONE_PALS["zone1"]),
            ("walrus", [("walrus", walrus, 48, 48, 4)], WALRUS_PAL, 192, None),
            ("items", items_frames(), ITEMS_PAL, 256, None), ("fx", FXS, FX_PAL, 128, None)]
    for name, entries, pal, width, ppal in jobs:
        im, layout = sheet(entries, pal, width)
        im.save(os.path.join(out, name + ".png"))
        preview(im, os.path.join(out, "preview", name + ".png"), pal=ppal)
        if name == "critters":
            for z in range(2, 5):
                preview(im, os.path.join(out, "preview", "critters_z%d.png" % z), pal=ZONE_PALS["zone%d" % z])
        layouts[name] = layout
    pals = {"hero": HERO_PAL, "items": ITEMS_PAL, "fx": FX_PAL, "walrus": WALRUS_PAL}
    pals.update(ZONE_PALS)
    for z in range(1, 5):
        tiles, tpal = terrain_tiles(z)
        im, layout = sheet([(n, lambda f, cv=cv: cv, 8, 8, 1) for n, cv in tiles], tpal, 128)
        im.save(os.path.join(out, "terrain_z%d.png" % z))
        preview(im, os.path.join(out, "preview", "terrain_z%d.png" % z))
        layouts["terrain"] = layout
        pals["terrain_z%d" % z] = tpal
        bt, bpal = back_tiles(z)
        im, layout = sheet([(n, lambda f, cv=cv: cv, 8, 8, 1) for n, cv in bt], bpal, 64)
        im.save(os.path.join(out, "back_z%d.png" % z))
        layouts["back"] = layout
        pals["back_z%d" % z] = bpal + [(0, 0, 0)] * (15 - len(bpal))
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

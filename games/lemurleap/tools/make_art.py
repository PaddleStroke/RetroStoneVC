#!/usr/bin/env python3
"""Lemur Leap: the code-drawn art, in the 8BCraft house style (docs/art-direction.md).

Code-drawn only, no image generation: every sprite and tile is drawn here with games/common/tools/house_style.py
(1-px outlines outside the shapes, 3-4 flat shades lit from the top-left, squash and stretch). Each sheet uses ONE fixed
15-colour palette and is written as an indexed PNG (build_assets.py reads the indices as they are):

  hero.png        the ring-tailed lemur, 24x24: rising, falling, landing (squashed), shooting, curled (a spring's
                  somersault), knocked, flying (OBJ palette 0; the four furs are palettes of the same pixels)
  hats.png        the wardrobe's hats, 16x16 (OBJ palette 1)
  plats.png       the leaves (a leaf, a winged moving leaf, a dry leaf and its halves, a dandelion puff, a mushroom, a
                  cloud), the spring, the garden's ground (OBJ palette 2)
  items.png       coins, power-ups, the seed shot, the record's rope and flag, the propeller and the rocket worn, the HUD
                  icons (OBJ palette 4)
  pests.png       the pests, 24x24: a bumblebee, a storm cloud, a bat, a little saucer (OBJ palette 5)
  fx.png          dust, sparkles, dandelion seeds, leaf bits, the shield's bubble (OBJ palette 6)
  panel.png       the side panels' art: the giant beanstalk's stalk, its leaves and a vine (BG palette 1), 168 x 256,
                  tiled upward; the right panel is the left one mirrored
  clouds.png      the far clouds (BG palette 2), 256 x 256, tiled; stars.png the stars and planets (BG palette 3)
  palettes.json   the palettes (the furs, the zones' panel and cloud colours) and the sheets' layouts
and art/preview/*.png: the sheets at 3x, for review.

    python3 games/lemurleap/tools/make_art.py [--out DIR]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/lemurleap/LICENSE.
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


def pal_dict(pal, names):
    assert len(pal) == len(names) == 15, (len(pal), len(names))
    return {n: pal[i] for i, n in enumerate(names)}


# ---- palettes (index 0 = clear) ---------------------------------------------------------------------------------------
HERO_NAMES = ["out", "fl", "fm", "fd", "face", "faced", "mask", "eye", "eyed", "white", "pink", "scarf", "scarfd",
              "paw", "ink"]
HERO_PAL = [
    (40, 22, 46),     # 1 outline (violet navy)
    (200, 200, 212),  # 2 fur light
    (152, 150, 170),  # 3 fur mid
    (102, 98, 124),   # 4 fur dark
    (250, 246, 238),  # 5 face white, the tail's light rings
    (210, 204, 204),  # 6 face shade
    (46, 38, 56),     # 7 the mask, the tail's dark rings, the nose
    (252, 186, 44),   # 8 eye amber
    (206, 116, 26),   # 9 eye amber dark
    (255, 255, 255),  # 10 glint
    (240, 150, 164),  # 11 pink (ears, mouth)
    (236, 84, 62),    # 12 scarf red
    (156, 40, 46),    # 13 scarf dark
    (72, 66, 88),     # 14 paws
    (22, 12, 28),     # 15 pupils
]
H = pal_dict(HERO_PAL, HERO_NAMES)


def fur(base, fl, fm, fd, paw, face=None, faced=None, mask=None):
    p = list(base)
    p[1], p[2], p[3], p[13] = fl, fm, fd, paw
    if face:
        p[4], p[5] = face, faced
    if mask:
        p[6] = mask
    return p


FURS = {
    "grey": HERO_PAL,
    "golden": fur(HERO_PAL, (252, 214, 120), (226, 160, 66), (170, 104, 40), (120, 70, 34)),
    "snow": fur(HERO_PAL, (246, 250, 255), (200, 216, 236), (150, 168, 200), (110, 124, 160), (255, 255, 255),
                (220, 230, 244), (90, 110, 150)),
    "midnight": fur(HERO_PAL, (110, 116, 168), (72, 74, 124), (44, 44, 84), (30, 28, 56), (196, 206, 236),
                    (150, 160, 200), (20, 18, 36)),
}

HATS_NAMES = ["out", "gl", "gm", "gd", "pink", "yellow", "bl", "bd", "red", "redd", "black", "white", "goldl", "goldd",
              "glass"]
HATS_PAL = [(40, 22, 46), (170, 226, 96), (98, 176, 60), (52, 116, 42), (246, 140, 186), (252, 222, 80),
            (110, 176, 246), (52, 96, 196), (232, 70, 60), (150, 34, 44), (44, 40, 56), (250, 250, 250),
            (252, 216, 72), (186, 128, 22), (196, 232, 255)]
HT = pal_dict(HATS_PAL, HATS_NAMES)

PLAT_NAMES = ["out", "ll", "lm", "ld", "vein", "dl", "dm", "dd", "white", "cshade", "red", "redd", "cream", "metal",
              "soil"]
PLAT_PAL = [(30, 50, 30), (176, 230, 100), (110, 188, 64), (58, 128, 44), (220, 246, 156), (230, 184, 104),
            (182, 124, 60), (112, 68, 36), (252, 252, 252), (190, 210, 238), (230, 66, 58), (150, 34, 42),
            (252, 236, 196), (156, 166, 184), (96, 62, 42)]
P = pal_dict(PLAT_PAL, PLAT_NAMES)

ITEM_NAMES = ["out", "gl", "gm", "gd", "red", "redd", "silver", "steel", "white", "bl", "bm", "acorn", "acornd", "flame",
              "fyellow"]
ITEM_PAL = [(40, 22, 46), (255, 232, 110), (246, 190, 40), (184, 116, 20), (232, 64, 60), (150, 30, 46),
            (210, 218, 230), (120, 130, 150), (255, 255, 255), (150, 214, 255), (64, 140, 230), (196, 130, 70),
            (120, 72, 40), (250, 120, 40), (255, 236, 120)]
I = pal_dict(ITEM_PAL, ITEM_NAMES)

PEST_NAMES = ["out", "yellow", "amber", "ink", "white", "wing", "gl", "gm", "gd", "pl", "pm", "pd", "green", "silver",
              "red"]
PEST_PAL = [(36, 20, 40), (252, 216, 60), (214, 140, 30), (40, 30, 44), (255, 255, 255), (214, 236, 255),
            (196, 200, 214), (130, 136, 158), (78, 82, 104), (196, 150, 230), (132, 84, 186), (78, 46, 120),
            (124, 214, 96), (176, 190, 210), (236, 70, 80)]
E = pal_dict(PEST_PAL, PEST_NAMES)

FX_NAMES = ["out", "white", "cream", "yellow", "gold", "bl", "bm", "gl", "gm", "brown", "greyl", "greym", "pink",
            "orange", "bubble"]
FX_PAL = [(40, 22, 46), (255, 255, 255), (250, 238, 210), (255, 236, 120), (246, 190, 40), (200, 236, 255),
          (110, 180, 240), (176, 230, 100), (98, 176, 60), (170, 110, 58), (216, 220, 232), (150, 156, 176),
          (246, 150, 186), (250, 140, 50), (150, 220, 255)]
F = pal_dict(FX_PAL, FX_NAMES)

# BG 1: the side panels (the zones change the background's three colours: indices 1-3)
STALK_NAMES = ["bg0", "bg1", "bg2", "sh", "sl", "sm", "sd", "sdd", "ll", "lm", "ld", "vine", "vined", "edge", "bud"]
STALK_PAL = [(38, 74, 58), (50, 92, 68), (66, 112, 78), (198, 236, 120), (150, 206, 84), (100, 162, 62),
             (62, 118, 48), (36, 78, 38), (164, 222, 104), (104, 172, 66), (54, 112, 46), (136, 98, 56),
             (86, 58, 36), (20, 34, 26), (246, 140, 170)]
ST = pal_dict(STALK_PAL, STALK_NAMES)
STALK_ZONES = {               # the background (bg0, bg1, bg2) by zone: garden, clouds, sky (sunset), space
    "garden": [(38, 74, 58), (50, 92, 68), (66, 112, 78)],
    "clouds": [(52, 92, 104), (66, 112, 120), (86, 134, 136)],
    "sky": [(92, 52, 84), (118, 66, 96), (146, 84, 104)],
    "space": [(16, 16, 44), (26, 26, 62), (38, 38, 82)],
}
CLOUD_NAMES = ["c0", "c1", "c2"] + ["x%d" % i for i in range(12)]
CLOUD_PAL = [(255, 255, 255), (226, 238, 252), (196, 214, 240)] + [(0, 0, 0)] * 12
CLOUD_ZONES = {
    "garden": [(255, 255, 255), (232, 242, 252), (204, 222, 244)],
    "clouds": [(255, 255, 255), (238, 244, 252), (214, 226, 244)],
    "sky": [(255, 214, 190), (246, 172, 160), (210, 126, 140)],
    "space": [(118, 86, 168), (92, 66, 142), (70, 50, 116)],
}
STAR_NAMES = ["s0", "s1", "s2", "pl", "pm", "pd", "rl", "rd", "ml", "mm", "md"] + ["x%d" % i for i in range(4)]
STAR_PAL = [(255, 255, 255), (200, 210, 255), (150, 160, 220), (250, 190, 120), (220, 130, 80), (150, 80, 70),
            (240, 220, 170), (170, 150, 120), (236, 236, 244), (196, 196, 214), (140, 140, 166)] + [(0, 0, 0)] * 4


# ---- the lemur ------------------------------------------------------------------------------------------------------
FUR4 = [H["fl"], H["fl"], H["fm"], H["fd"]]
FACE4 = [H["face"], H["face"], H["face"], H["faced"]]


def limb(cv, ax, ay, bx, by, r=1.6, far=False):
    hs.capsule(cv, ax, ay, bx, by, r, [H["fm"], H["fm"], H["fd"], H["fd"]] if far else FUR4)


def paw(cv, x, y):
    cv.set(int(round(x)), int(round(y)), H["paw"])
    cv.set(int(round(x)) + 1, int(round(y)), H["paw"])


def tail(cv, pts, ring=2.6):
    """the ringed tail along a polyline (behind the body): light and dark rings, a dark tip"""
    segs = []
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        segs.append((ax, ay, bx, by, math.hypot(bx - ax, by - ay)))
    total = sum(s[4] for s in segs)
    d = 0.0
    for ax, ay, bx, by, ln in segs:
        n = max(1, int(ln * 3))
        for k in range(n + 1):
            t = k / float(n)
            x, y = ax + (bx - ax) * t, ay + (by - ay) * t
            dist = d + ln * t
            r = 1.7 if dist < total - 2 else 1.3
            dark = int(dist / ring) % 2 == 1 or dist > total - 2.5
            c = H["mask"] if dark else H["face"]
            for yy in range(int(y - r) - 1, int(y + r) + 2):
                for xx in range(int(x - r) - 1, int(x + r) + 2):
                    if (xx + 0.5 - x) ** 2 + (yy + 0.5 - y) ** 2 <= r * r:
                        cv.set(xx, yy, c)
        d += ln


def head(cv, hx, hy, eyes="open", mouth="smile"):
    """the head, seen from the front: pointed ears, a white face, two big amber eyes in dark patches, a dark nose"""
    for ex, sgn in ((hx - 4.0, -1), (hx + 4.0, 1)):                  # the ears behind, pink inside
        for k in range(4):
            for w in range(-1, 2):
                cv.set(int(round(ex + sgn * k * 0.5 + w * 0.7)), int(round(hy - 3.2 - k)), H["fm"])
        cv.set(int(round(ex + sgn * 0.5)), int(round(hy - 4.4)), H["pink"])
    hs.ellipse(cv, hx, hy, 5.7, 4.7, FUR4)
    hs.ellipse(cv, hx, hy + 1.3, 4.6, 3.4, FACE4)                     # the white face
    ey = int(round(hy - 0.6))
    ex = [int(round(hx - 3.4)), int(round(hx + 1.6))]
    for x0 in ex:                                                     # the dark patches round the eyes
        cv.rect(x0, ey - 1, x0 + 2, ey + 2, H["mask"])
    if eyes == "x":
        for x0 in ex:
            cv.set(x0, ey - 1, H["white"]), cv.set(x0 + 2, ey - 1, H["white"]), cv.set(x0 + 1, ey, H["white"])
            cv.set(x0, ey + 1, H["white"]), cv.set(x0 + 2, ey + 1, H["white"])
    elif eyes == "shut":
        for x0 in ex:
            cv.set(x0, ey + 1, H["eye"]), cv.set(x0 + 1, ey + 1, H["eye"]), cv.set(x0 + 2, ey + 1, H["eye"])
    else:
        for x0 in ex:
            cv.rect(x0, ey - 1, x0 + 1, ey + 1, H["eye"])
            cv.set(x0, ey + 1, H["eyed"]), cv.set(x0 + 1, ey + 1, H["eyed"])
            py = ey - 1 if eyes == "up" else ey + 1 if eyes == "down" else ey
            cv.set(x0 + 1, py, H["ink"])
            cv.set(x0, ey - 1 if eyes != "up" else ey, H["white"])
    nx = int(round(hx - 0.5))                                         # the nose and the mouth
    cv.set(nx, ey + 2, H["mask"]), cv.set(nx + 1, ey + 2, H["mask"])
    my = ey + 4
    if mouth == "o":
        cv.set(nx, my, H["pink"]), cv.set(nx + 1, my, H["pink"]), cv.set(nx, my - 1, H["mask"]), cv.set(nx + 1, my - 1, H["mask"])
    elif mouth == "smile":
        cv.set(nx - 1, my - 1, H["mask"]), cv.set(nx, my, H["mask"]), cv.set(nx + 1, my, H["mask"]), cv.set(nx + 2, my - 1, H["mask"])


def scarf(cv, x0, x1, y, flap):
    for x in range(int(x0), int(x1) + 1):
        cv.set(x, int(y), H["scarf"]), cv.set(x, int(y) + 1, H["scarfd"])
    fx, fy = int(x0), int(y) + 1                                      # the knot's end flutters behind
    for k, (dx, dy) in enumerate(flap):
        cv.set(fx + dx, fy + dy, H["scarf"] if k % 2 == 0 else H["scarfd"])


# pose: (tail points, far arm, far leg, body, near leg, head, near arm, scarf flap, eyes, mouth)
POSES = {
    "up": dict(tail=[(9, 18), (7, 20), (5, 22), (3, 20), (2, 16), (3, 12)], farm=(10, 12, 5.5, 4.5), farl=(11, 18, 10.5, 21.5),
               body=(12, 14, 3.8, 5.4), nearl=(13.5, 18, 13.8, 21.8), head=(12.5, 7.5), nearm=(14.5, 12, 19.5, 4.5),
               flap=[(-1, 0), (-2, 1), (-3, 1)], eyes="up", mouth="smile", behind=True),
    "down": dict(tail=[(9, 17), (6, 15), (4, 11), (4, 7), (6, 4)], farm=(10, 12, 5, 11), farl=(11, 18, 8.5, 21.5),
                 body=(12, 14.5, 4.2, 5.2), nearl=(13.5, 18, 16, 21.8), head=(12.5, 8), nearm=(14.5, 12, 19.5, 11),
                 flap=[(-1, -1), (-2, -2), (-3, -2)], eyes="down", mouth="o"),
    "squash": dict(tail=[(8, 19), (5, 20), (3, 18), (2, 14), (4, 11)], farm=(10, 15, 7, 20), farl=(10.5, 20, 8, 22),
                   body=(12, 17.2, 5.4, 4.0), nearl=(14, 20, 16.5, 22), head=(12.5, 12), nearm=(15, 15, 17.5, 20.5),
                   flap=[(-1, 0), (-2, 0), (-3, 1)], eyes="shut", mouth="smile"),
    "shoot": dict(tail=[(9, 18), (6, 20), (4, 21), (2, 18), (2, 14)], farm=(10, 12, 8, 15.5), farl=(11, 18, 10.5, 21.5),
                  body=(12, 14, 3.8, 5.4), nearl=(13.5, 18, 14, 21.8), head=(12.5, 7.5), nearm=(14.5, 11, 19, 2.5),
                  flap=[(-1, 0), (-2, 1), (-3, 1)], eyes="up", mouth="o", behind=True),
    "hurt": dict(tail=[(9, 17), (6, 18), (4, 21), (2, 22)], farm=(10, 12, 6, 6), farl=(11, 18, 9, 22),
                 body=(12, 14.5, 4.0, 5.2), nearl=(13.5, 18, 15.5, 22), head=(12.5, 8), nearm=(14.5, 12, 19, 6),
                 flap=[(-1, -1), (-2, -1), (-3, 0)], eyes="x", mouth="o"),
    "fly": dict(tail=[(10, 18), (9, 20), (8, 22), (6, 23)], farm=(10.5, 11, 6.5, 3), farl=(11.5, 18, 11, 22),
                body=(12, 14, 3.6, 5.4), nearl=(13.5, 18, 13.5, 22.5), head=(12.5, 7.5), nearm=(14, 11, 18.5, 3),
                flap=[(-1, 1), (-2, 2), (-2, 3)], eyes="open", mouth="smile", behind=True),
}
HERO_FRAMES = ["up", "down", "squash", "shoot", "curl", "hurt", "fly"]


def lemur(name):
    cv = Canvas(24, 24)
    if name == "curl":                                                # a ball, the tail wrapped round it
        hs.ellipse(cv, 12, 12.5, 6.4, 6.2, FUR4)
        hs.ellipse(cv, 13.5, 13.5, 3.6, 3.2, FACE4)
        tail(cv, [(7, 16), (8, 19.5), (12, 20.5), (16, 19), (18.5, 15), (18, 10)])
        ex, ey = 13, 11
        for x0 in (ex - 1, ex + 3):
            cv.rect(x0 - 1, ey - 1, x0 + 1, ey + 1, H["mask"])
            cv.set(x0, ey, H["eye"])
        cv.set(16, 14, H["mask"])
        cv.outline(H["out"])
        return cv
    p = POSES[name]
    tail(cv, p["tail"])
    limb(cv, *p["farm"], far=True)
    paw(cv, p["farm"][2] - 0.5, p["farm"][3])
    limb(cv, *p["farl"], far=True)
    paw(cv, p["farl"][2] - 0.5, p["farl"][3] + 0.5)
    bx, by, rx, ry = p["body"]
    hs.ellipse(cv, bx, by, rx, ry, FUR4)
    hs.ellipse(cv, bx + 0.8, by + 0.8, rx * 0.55, ry * 0.62, FACE4)  # the pale belly
    limb(cv, *p["nearl"])
    paw(cv, p["nearl"][2] - 0.5, p["nearl"][3] + 0.5)
    if p.get("behind"):                                              # raised arms: behind the head
        limb(cv, *p["nearm"])
        paw(cv, p["nearm"][2] - 0.5, p["nearm"][3])
    hx, hy = p["head"]
    head(cv, hx, hy, p["eyes"], p["mouth"])
    scarf(cv, bx - 3, bx + 3, hy + 4.5, p["flap"])
    if not p.get("behind"):
        limb(cv, *p["nearm"])
        paw(cv, p["nearm"][2] - 0.5, p["nearm"][3])
    cv.outline(H["out"])
    return cv


def head_anchor(name):
    """where a hat sits: the top of the head (its centre x, its top y) in the 24x24 frame"""
    if name == "curl":
        return (12, 7)
    hx, hy = POSES[name]["head"]
    return (int(round(hx)), int(round(hy - 4.6)))


# ---- hats (16x16: the brim's centre at x 8, y 13 sits on the head's top) ----------------------------------------------
def hat(kind):
    cv = Canvas(16, 16)
    d = HT
    if kind == "leaf":                                                # a big leaf cap, its stem up
        hs.ellipse(cv, 8, 12.5, 6.0, 2.6, [d["gl"], d["gl"], d["gm"], d["gd"]])
        for x in range(3, 13):
            cv.set(x, 12, d["gl"] if x < 9 else d["gm"])
        cv.set(8, 9, d["gd"]), cv.set(9, 8, d["gd"]), cv.set(10, 7, d["gd"])
    elif kind == "flower":                                            # a crown of little flowers
        for i, x in enumerate((3, 6, 9, 12)):
            c = d["pink"] if i % 2 == 0 else d["yellow"]
            for dx, dy in ((0, -1), (-1, 0), (1, 0), (0, 1)):
                cv.set(x + dx, 12 + dy, c)
            cv.set(x, 12, d["white"])
        for x in range(2, 14):
            cv.set(x, 14, d["gm"])
    elif kind == "party":                                             # a striped cone, a pompom
        for y in range(4, 15):
            half = (y - 3) * 0.45
            for x in range(int(8 - half), int(8 + half) + 1):
                cv.set(x, y, d["bl"] if (y // 2) % 2 == 0 else d["yellow"])
        cv.set(8, 3, d["pink"]), cv.set(7, 2, d["pink"]), cv.set(8, 2, d["white"]), cv.set(9, 2, d["pink"])
        cv.set(8, 1, d["pink"])
    elif kind == "beanie":                                            # a red knit beanie
        hs.ellipse(cv, 8, 12, 6.2, 4.5, [d["red"], d["red"], d["red"], d["redd"]])
        for y in range(13, 15):
            for x in range(2, 15):
                cv.set(x, y, d["white"] if (x + y) % 2 else d["red"])
        for y in range(0, 16):
            for x in range(16):
                if y > 14:
                    cv.set(x, y, None)
        hs.ellipse(cv, 8, 6.5, 1.8, 1.6, [d["white"], d["white"], d["white"], d["white"]])
    elif kind == "pirate":                                            # a black tricorn, a white mark
        for x in range(1, 15):
            cv.set(x, 13, d["black"]), cv.set(x, 14, d["black"])
        hs.ellipse(cv, 8, 10.5, 5.0, 3.5, [d["black"], d["black"], d["black"], d["black"]])
        cv.set(1, 12, d["black"]), cv.set(14, 12, d["black"]), cv.set(0, 11, d["black"]), cv.set(15, 11, d["black"])
        cv.set(7, 10, d["white"]), cv.set(8, 10, d["white"]), cv.set(7, 11, d["white"]), cv.set(8, 11, d["white"])
        for x in range(2, 14):
            cv.set(x, 12, d["goldd"])
    elif kind == "crown":                                             # a gold crown with a red jewel
        for y in range(9, 15):
            for x in range(3, 13):
                cv.set(x, y, d["goldl"] if x < 8 else d["goldd"])
        for x in (3, 6, 9, 12):
            cv.set(x, 8, d["goldl"]), cv.set(x, 7, d["goldl"])
        cv.set(7, 11, d["red"]), cv.set(8, 11, d["redd"]), cv.set(4, 9, d["white"])
    elif kind == "helmet":                                            # the astronaut's glass bubble (round the head)
        for y in range(16):
            for x in range(16):
                r = math.hypot(x + 0.5 - 8, y + 0.5 - 9.5)
                if 6.3 <= r < 7.6 and y < 13:
                    cv.set(x, y, d["glass"])
        cv.set(4, 5, d["white"]), cv.set(5, 4, d["white"]), cv.set(6, 3, d["white"])
        for x in range(1, 15):
            cv.set(x, 14, d["white"] if x % 3 else d["bd"]), cv.set(x, 15, d["bd"])
    cv.outline(d["out"])
    return cv


HATS = ["leaf", "flower", "party", "beanie", "pirate", "crown", "helmet"]


# ---- the leaves (platforms) ----------------------------------------------------------------------------------------------
def leaf(bent=False, dry=False):
    """a flat leaf seen from the side, 32 x 8: a fat lens, its vein, a stalk at the left; bent: its tips droop"""
    cv = Canvas(32, 8)
    L = [P["dl"], P["dl"], P["dm"], P["dd"]] if dry else [P["ll"], P["ll"], P["lm"], P["ld"]]
    for x in range(1, 31):
        t = (x - 15.5) / 15.0
        half = 2.6 * (1 - t * t) ** 0.6 + 0.5
        cy = 3.0 + (1.6 * t * t if bent else 0.4 * t * t)
        for y in range(8):
            dy = (y + 0.5 - cy) / half
            if abs(dy) <= 1:
                nd = -dy * 0.9 - t * 0.3
                cv.set(x, y, hs.shade_nd(nd, L))
        if 3 <= x <= 28:                                              # the vein
            cv.set(x, int(round(cy)), P["dl"] if dry else P["vein"])
    if dry:                                                           # a dry leaf: its cracks
        for x, y in ((9, 2), (10, 3), (10, 4), (21, 3), (22, 2), (22, 4), (15, 4)):
            cv.set(x, y, P["dd"])
    cv.outline(P["out"])
    return cv


def half_leaf(side):
    full = leaf(dry=True)
    cv = Canvas(16, 8)
    for y in range(8):
        for x in range(16):
            cv.p[y][x] = full.p[y][x + (16 if side else 0)]
    return cv


def wing_leaf(f):
    """the moving leaf, 32 x 16: a leaf (its top at y 8) carried by two little wings that flap"""
    cv = Canvas(32, 16)
    base = leaf()
    cv.paste(base, 0, 8)
    for wx in (8, 23):
        if f == 0:
            hs.ellipse(cv, wx - 2.5, 5.5, 3.2, 2.0, [P["white"], P["white"], P["white"], P["cshade"]])
            hs.ellipse(cv, wx + 2.5, 5.5, 3.2, 2.0, [P["white"], P["white"], P["white"], P["cshade"]])
        else:
            hs.ellipse(cv, wx - 3.5, 8.5, 3.4, 1.6, [P["white"], P["white"], P["white"], P["cshade"]])
            hs.ellipse(cv, wx + 3.5, 8.5, 3.4, 1.6, [P["white"], P["white"], P["white"], P["cshade"]])
        cv.set(wx, 8, P["cshade"]), cv.set(wx, 7, P["cshade"])
    cv.paste(base, 0, 8)
    cv.outline(P["out"])
    return cv


def puff():
    """a dandelion puff, 32 x 8: a row of round white seed heads, grey-blue below, on a thin green stem"""
    cv = Canvas(32, 8)
    for x in range(1, 31):
        cv.set(x, 6, P["lm"])
    for cx in (4.5, 11.5, 18.5, 25.5):
        hs.ellipse(cv, cx + 0.5, 3.2, 3.6, 2.9, [P["white"], P["white"], P["white"], P["cshade"]])
    for cx in (5, 12, 19, 26):
        cv.set(cx, 3, P["cream"]), cv.set(cx - 2, 1, P["white"])
    cv.outline(P["out"])
    return cv


def shroom(f):
    """a red mushroom cap (a trampoline), 32 x 16; f 1: squashed"""
    cv = Canvas(32, 16)
    sq = 1.6 if f else 0.0
    top = 2 + sq
    for x in range(1, 31):
        t = (x - 15.5) / 15.0
        h = (1 - t * t) ** 0.5 * (6.0 - sq)
        for y in range(16):
            if 8 - h <= y + (sq / 2) <= 8 and y >= top - 1:
                nd = -((y - (8 - h)) / max(h, 1)) * 0.5 - t * 0.5 + 0.4
                cv.set(x, y, hs.shade_nd(nd, [P["white"], P["red"], P["red"], P["redd"]]))
    for sx, sy in ((7, 4 + int(sq)), (15, 3 + int(sq)), (23, 5 + int(sq)), (11, 6), (20, 7)):
        cv.set(sx, sy, P["white"]), cv.set(sx + 1, sy, P["white"])
    for x in range(4, 28):                                            # the gills under the cap
        cv.set(x, 8, P["cream"] if x % 2 else P["cshade"])
    for y in range(9, 16):                                            # the stalk
        for x in range(12, 20):
            cv.set(x, y, P["cream"] if x < 17 else P["cshade"])
    cv.outline(P["out"])
    return cv


def cloud_plat(f):
    """a soft cloud, 32 x 16 (the feet land on its top at y 4); f 1: pressed down"""
    cv = Canvas(32, 16)
    dy = 2 if f else 0
    blobs = [(7, 8, 6, 4.5), (16, 6.5, 7, 5), (25, 8, 6, 4.5), (12, 10, 7, 3.5), (21, 10, 7, 3.5)]
    for bx, by, rx, ry in blobs:
        hs.ellipse(cv, bx, by + dy, rx, ry, [P["white"], P["white"], P["white"], P["cshade"]])
    for x in range(3, 29):
        cv.set(x, 13 + dy if 13 + dy < 16 else 15, P["cshade"])
    cv.outline(P["cshade"])
    return cv


def spring(f):
    """a coiled vine tendril with a flower pad on top, 16x16 (its foot on the leaf's top at y 15); f 1: sprung"""
    cv = Canvas(16, 16)
    top = 9 if f == 0 else 3
    n = 3 if f == 0 else 5
    for k in range(n):
        y = 15 - (15 - top - 2) * k / max(1, n - 1) - 1
        for x in range(5, 11):
            cv.set(x, int(y), P["lm"] if (x + k) % 2 else P["ld"])
        cv.set(4 + (k % 2) * 7, int(y) + 1, P["ld"])
    for x in range(3, 13):                                            # the pad: a red flower
        cv.set(x, top, P["red"]), cv.set(x, top + 1, P["redd"])
    cv.set(6, top - 1, P["red"]), cv.set(9, top - 1, P["red"]), cv.set(8, top, P["cream"])
    cv.outline(P["out"])
    return cv


def ground(v):
    """the garden's ground, 64 x 32: grass on top, rich soil, pebbles, a few flowers"""
    cv = Canvas(64, 32)
    for y in range(32):
        for x in range(64):
            h = ((x * 7 + y * 13 + v * 31) * 2654435761 >> 7) % 97
            if y < 3:
                c = P["ll"] if y == 0 else P["lm"]
            elif y < 6:
                c = P["lm"] if (x + y) % 3 else P["ld"]
            else:
                c = P["soil"] if h > 10 else P["dd"]
            cv.set(x, y, c)
    for x in range(0, 64, 3):                                         # grass blades
        cv.set(x + (v % 2), 0, None) if x % 2 else cv.set(x, 0, P["ll"])
    for px, py in ((10 + v * 7, 14), (40 - v * 5, 22), (54, 12 + v * 4), (24, 26)):
        cv.set(px, py, P["metal"]), cv.set(px + 1, py, P["cshade"]), cv.set(px, py + 1, P["dd"])
    for fx in ((8, 30, 51) if v == 0 else (17, 44)):
        cv.set(fx, 1, P["ld"]), cv.set(fx, 0, P["ld"])
        for dx, dy in ((0, -2), (-1, -1), (1, -1), (0, -3)):
            pass
    return cv


def ground_flowers():
    """flowers standing on the ground's top edge, 64 x 8 (drawn just above it)"""
    cv = Canvas(64, 8)
    for i, fx in enumerate((6, 19, 33, 47, 58)):
        cv.set(fx, 7, P["ld"]), cv.set(fx, 6, P["ld"]), cv.set(fx, 5, P["ld"])
        c = P["red"] if i % 2 == 0 else P["white"]
        for dx, dy in ((0, -1), (-1, 0), (1, 0), (0, 1)):
            cv.set(fx + dx, 3 + dy, c)
        cv.set(fx, 3, P["cream"])
    for x in range(0, 64, 2):
        if x % 7 < 3:
            cv.set(x, 7, P["lm"]), cv.set(x, 6, P["ll"])
    cv.outline(P["out"])
    return cv


# ---- items ------------------------------------------------------------------------------------------------------------
def coin(f):
    """a gold coin with a leaf stamped on it, spinning (4 frames), 8x8"""
    cv = Canvas(8, 8)
    w = [3.4, 2.4, 1.0, 2.4][f]
    hs.ellipse(cv, 4, 4, w, 3.4, [I["gl"], I["gl"], I["gm"], I["gd"]])
    if f == 0:
        cv.set(4, 3, I["gd"]), cv.set(3, 4, I["gd"]), cv.set(4, 4, I["gl"]), cv.set(5, 3, I["gl"])
    cv.set(2 if f != 2 else 4, 2, I["white"])
    cv.outline(I["out"])
    return cv


def prop_hat(f):
    """the propeller cap, 16x16: a little green cap, its propeller (2 frames)"""
    cv = Canvas(16, 16)
    hs.ellipse(cv, 8, 12, 5.0, 3.6, [I["bl"], I["bl"], I["bm"], I["bm"]])
    for x in range(3, 14):
        cv.set(x, 14, I["red"])
    cv.set(8, 8, I["steel"]), cv.set(8, 7, I["steel"])
    blades = [(-5, 0), (-4, 0), (-3, 0), (-2, 0), (2, 0), (3, 0), (4, 0), (5, 0)] if f == 0 else \
        [(-2, -1), (-1, 0), (1, 0), (2, 1), (-2, 1), (2, -1)]
    for dx, dy in blades:
        cv.set(8 + dx, 6 + dy, I["gm"])
    cv.set(8, 6, I["red"])
    cv.outline(I["out"])
    return cv


def rocket():
    """the acorn rocket, 16x16"""
    cv = Canvas(16, 16)
    hs.ellipse(cv, 8, 9.5, 4.2, 5.0, [I["acorn"], I["acorn"], I["acorn"], I["acornd"]])
    for x in range(4, 13):                                            # the acorn's cap
        for y in range(3, 6):
            cv.set(x, y, I["acornd"] if (x + y) % 2 else I["gd"])
    cv.set(8, 2, I["acornd"]), cv.set(8, 1, I["acornd"])
    cv.set(4, 13, I["red"]), cv.set(12, 13, I["red"]), cv.set(3, 14, I["red"]), cv.set(13, 14, I["red"])
    cv.set(6, 7, I["white"]), cv.set(7, 8, I["silver"])
    cv.outline(I["out"])
    return cv


def shield_item():
    cv = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            r = math.hypot(x + 0.5 - 8, y + 0.5 - 8)
            if r < 6.5:
                cv.set(x, y, I["bl"] if r < 5.2 else I["bm"])
    cv.set(5, 5, I["white"]), cv.set(6, 4, I["white"]), cv.set(5, 6, I["white"])
    cv.set(9, 9, I["white"])
    cv.outline(I["out"])
    return cv


def magnet():
    cv = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            r = math.hypot(x + 0.5 - 8, y + 0.5 - 7)
            if 2.6 <= r < 6.2 and y < 9:
                cv.set(x, y, I["red"] if x < 9 else I["redd"])
    for x in list(range(2, 6)) + list(range(10, 14)):
        for y in range(8, 13):
            cv.set(x, y, I["red"] if x < 9 else I["redd"])
        for y in range(11, 14):
            cv.set(x, y, I["silver"] if x < 9 else I["steel"])
    cv.set(4, 3, I["white"])
    cv.outline(I["out"])
    return cv


def seed():
    """the seed shot, 8x8: a striped sunflower seed flying up"""
    cv = Canvas(8, 8)
    hs.ellipse(cv, 4, 4, 1.8, 3.3, [I["white"], I["silver"], I["steel"], I["steel"]])
    cv.set(4, 2, I["out"]), cv.set(4, 4, I["out"])
    cv.outline(I["out"])
    return cv


def rope():
    """the record's line across the column: a rope with little pennants, 64 x 8"""
    cv = Canvas(64, 8)
    for x in range(64):
        y = 2 + (1 if (x % 16) in (6, 7, 8, 9) else 0)
        cv.set(x, y, I["gm"] if x % 4 < 2 else I["gd"])
    for k, px in enumerate(range(4, 64, 16)):
        c = I["red"] if k % 2 == 0 else I["bl"]
        for y in range(3, 7):
            for x in range(px, px + 7 - (y - 3) * 2):
                cv.set(x, y, c)
    return cv


def flag():
    """the record's flag on its pole, 16x16 (a star on it)"""
    cv = Canvas(16, 16)
    for y in range(1, 16):
        cv.set(2, y, I["silver"])
    for y in range(1, 8):
        for x in range(3, 14 - (y > 4) * (y - 4) * 2):
            cv.set(x, y, I["red"] if y < 6 else I["redd"])
    for dx, dy in ((7, 3), (6, 4), (8, 4), (7, 4), (7, 5)):
        cv.set(dx, dy, I["gl"])
    cv.outline(I["out"])
    return cv


def jet(f):
    """the acorn rocket worn on the back, its flame (2 frames), 16x16"""
    cv = Canvas(16, 16)
    hs.ellipse(cv, 8, 6.5, 3.2, 4.2, [I["acorn"], I["acorn"], I["acorn"], I["acornd"]])
    for x in range(5, 12):
        cv.set(x, 2, I["acornd"]), cv.set(x, 3, I["gd"])
    ln = 4 if f == 0 else 6
    for y in range(11, 11 + ln):
        w = 2 - (y - 11) * 2 // ln
        for x in range(8 - w, 8 + w + 1):
            cv.set(x, y, I["fyellow"] if abs(x - 8) < 1 and y < 13 else I["flame"])
    cv.outline(I["out"])
    return cv


def propeller(f):
    """the propeller turning over the head while flying, 16x8"""
    cv = Canvas(16, 8)
    cv.set(8, 6, I["steel"]), cv.set(8, 7, I["steel"]), cv.set(8, 5, I["red"])
    if f == 0:
        for x in list(range(1, 7)) + list(range(10, 16)):
            cv.set(x, 4, I["gm"]), cv.set(x, 5, I["gd"] if x % 3 == 0 else I["gm"])
    else:
        for x in range(5, 12):
            cv.set(x, 4, I["gm"])
        cv.set(4, 5, I["gd"]), cv.set(12, 5, I["gd"])
    cv.outline(I["out"])
    return cv


def hud_coin():
    cv = coin(0)
    return cv


def hud_up():
    """the HUD's height mark: an arrow up, 8x8"""
    cv = Canvas(8, 8)
    for y in range(1, 7):
        for x in range(4 - (y if y < 4 else 1), 4 + (y if y < 4 else 1)):
            if 0 <= x < 8:
                cv.set(x, y, I["gl"] if x < 4 else I["gm"])
    cv.outline(I["out"])
    return cv


def lock():
    cv = Canvas(8, 8)
    for x in (2, 5):
        cv.set(x, 1, I["steel"]), cv.set(x, 2, I["steel"])
    cv.set(3, 0, I["steel"]), cv.set(4, 0, I["steel"])
    for y in range(3, 8):
        for x in range(1, 7):
            cv.set(x, y, I["gm"] if x < 4 else I["gd"])
    cv.set(3, 5, I["out"]), cv.set(4, 5, I["out"])
    return cv


# ---- pests (24x24) ------------------------------------------------------------------------------------------------------
def eyes2(cv, x, y, angry=False):
    for ex in (x, x + 4):
        cv.rect(ex, y, ex + 2, y + 2, E["white"])
        cv.set(ex + 1, y + 1, E["ink"]), cv.set(ex + 1, y + 2, E["ink"])
    if angry:
        cv.set(x, y - 1, E["ink"]), cv.set(x + 1, y - 1, E["ink"]), cv.set(x + 2, y, E["ink"])
        cv.set(x + 6, y - 1, E["ink"]), cv.set(x + 5, y - 1, E["ink"]), cv.set(x + 4, y, E["ink"])


def bee(f):
    cv = Canvas(24, 24)
    wy = 5.5 if f == 0 else 8.5
    W = [E["white"], E["wing"], E["wing"], E["gl"]]
    hs.ellipse(cv, 10, wy, 3.0, 2.3 if f == 0 else 1.6, W)
    hs.ellipse(cv, 15.5, wy + 0.5, 3.0, 2.3 if f == 0 else 1.6, W)
    hs.ellipse(cv, 12, 13.5, 7.0, 5.4, [E["yellow"], E["yellow"], E["yellow"], E["amber"]])
    for x in (10, 11, 15, 16):                                        # the stripes
        for y in range(8, 20):
            if cv.get(x, y) in (E["yellow"], E["amber"]):
                cv.set(x, y, E["ink"])
    cv.set(19, 14, E["ink"]), cv.set(20, 14, E["ink"])                # the sting
    eyes2(cv, 5, 11, angry=True)
    cv.set(6, 16, E["ink"]), cv.set(7, 16, E["ink"])
    cv.set(6, 7, E["ink"]), cv.set(5, 6, E["ink"]), cv.set(8, 7, E["ink"]), cv.set(9, 6, E["ink"])   # antennae
    cv.outline(E["out"])
    return cv


def storm(f):
    cv = Canvas(24, 24)
    for bx, by, rx, ry in ((7, 11, 5, 4.5), (13, 8, 6.5, 5.5), (18, 11, 5, 4.5), (12, 13, 8, 4)):
        hs.ellipse(cv, bx, by, rx, ry, [E["gl"], E["gl"], E["gm"], E["gd"]])
    eyes2(cv, 8, 9, angry=True)
    cv.set(10, 14, E["ink"]), cv.set(11, 13, E["ink"]), cv.set(12, 13, E["ink"]), cv.set(13, 14, E["ink"])
    bolt = [(12, 17), (11, 18), (11, 19), (12, 19), (13, 19), (12, 20), (11, 21)] if f == 0 else \
           [(15, 17), (14, 18), (14, 19), (15, 19), (16, 19), (15, 20), (14, 21), (8, 17), (8, 18), (9, 19)]
    for x, y in bolt:
        cv.set(x, y, E["yellow"])
    cv.outline(E["out"])
    return cv


def bat(f):
    cv = Canvas(24, 24)
    up = f == 0
    for side in (-1, 1):                                              # the wings
        for k in range(9):
            x = 12 + side * (3 + k)
            y0 = 9 - (k // 2 if up else -k // 3)
            for y in range(y0, y0 + 4 - k // 3):
                cv.set(x, y, E["pm"] if k < 6 else E["pd"])
        cv.set(12 + side * 12, 9 - (4 if up else -3), E["pd"])
    hs.ellipse(cv, 12, 12, 4.5, 5.0, [E["pl"], E["pl"], E["pm"], E["pd"]])
    cv.set(9, 6, E["pm"]), cv.set(9, 7, E["pm"]), cv.set(15, 6, E["pm"]), cv.set(15, 7, E["pm"])   # ears
    eyes2(cv, 8, 10)
    cv.set(11, 15, E["white"]), cv.set(13, 15, E["white"])          # little fangs
    cv.outline(E["out"])
    return cv


def ufo(f):
    cv = Canvas(24, 24)
    hs.ellipse(cv, 12, 9, 4.5, 4.5, [E["wing"], E["wing"], E["gl"], E["gm"]])   # the dome
    hs.ellipse(cv, 12, 10, 2.5, 2.5, [E["green"], E["green"], E["green"], E["green"]])   # the pilot
    cv.set(11, 9, E["ink"]), cv.set(13, 9, E["ink"])
    hs.ellipse(cv, 12, 14, 10, 3.2, [E["silver"], E["silver"], E["gm"], E["gd"]])
    for k, x in enumerate((5, 9, 13, 17)):
        cv.set(x + (f * 2) % 4, 15, E["yellow"] if (k + f) % 2 else E["red"])
    cv.outline(E["out"])
    return cv


# ---- effects --------------------------------------------------------------------------------------------------------------
def dust(f):
    cv = Canvas(8, 8)
    r = [2.0, 3.0, 3.6][f]
    for y in range(8):
        for x in range(8):
            d = math.hypot(x + 0.5 - 4, y + 0.5 - 4)
            if d < r and (f < 2 or hs.checker(x, y)):
                cv.set(x, y, F["white"] if d < r - 1 else F["cream"])
    return cv


def spark(f):
    cv = Canvas(8, 8)
    n = 3 if f == 0 else 2
    for k in range(-n, n + 1):
        cv.set(4 + k, 4, F["yellow"]), cv.set(4, 4 + k, F["yellow"])
    cv.set(4, 4, F["white"])
    return cv


def tuft(f):
    """a dandelion seed drifting, 8x8"""
    cv = Canvas(8, 8)
    for x, y in ((3, 1), (2, 2), (4, 2), (1, 2), (5, 2), (3, 2)) if f == 0 else ((3, 2), (2, 1), (4, 1), (1, 3), (5, 3)):
        cv.set(x, y, F["white"])
    cv.set(3, 4, F["greyl"]), cv.set(3, 5, F["brown"])
    return cv


def leafbit(f):
    cv = Canvas(8, 8)
    pts = ((2, 3), (3, 3), (4, 4), (5, 4), (3, 4)) if f == 0 else ((3, 2), (3, 3), (4, 4), (4, 5), (3, 4))
    for x, y in pts:
        cv.set(x, y, F["gl"] if x < 4 else F["gm"])
    return cv


def drybit(f):
    cv = Canvas(8, 8)
    pts = ((2, 3), (3, 3), (4, 4), (5, 4)) if f == 0 else ((3, 2), (3, 3), (4, 4), (4, 5))
    for x, y in pts:
        cv.set(x, y, F["brown"])
    return cv


def star_fx():
    cv = Canvas(8, 8)
    for x, y in ((3, 0), (3, 1), (2, 2), (3, 2), (4, 2), (0, 3), (1, 3), (2, 3), (3, 3), (4, 3), (5, 3), (6, 3), (2, 4),
                 (3, 4), (4, 4), (1, 5), (2, 5), (4, 5), (5, 5), (1, 6), (5, 6)):
        cv.set(x, y, F["yellow"])
    cv.set(3, 3, F["white"])
    return cv


def bubble():
    """the shield: a soap bubble round the lemur, 32x32 (dithered rim, a glint)"""
    cv = Canvas(32, 32)
    for y in range(32):
        for x in range(32):
            r = math.hypot(x + 0.5 - 16, y + 0.5 - 16)
            if 13.5 <= r < 15.2:
                cv.set(x, y, F["bubble"] if (x + y) % 3 else F["white"])
            elif 12.6 <= r < 13.5 and hs.checker(x, y):
                cv.set(x, y, F["bl"])
    for x, y in ((8, 7), (7, 8), (9, 6), (6, 9), (10, 6)):
        cv.set(x, y, F["white"])
    return cv


def ring(f):
    """a stomp's ring, 16x16"""
    cv = Canvas(16, 16)
    r0 = 4.0 if f == 0 else 6.5
    for y in range(16):
        for x in range(16):
            r = math.hypot(x + 0.5 - 8, (y + 0.5 - 8) * 1.6)
            if r0 <= r < r0 + 1.3 and (f == 0 or hs.checker(x, y)):
                cv.set(x, y, F["white"])
    return cv


# ---- the side panels (BG 1): the giant beanstalk ------------------------------------------------------------------
PANEL_W, PANEL_H = 168, 256


def panel():
    """the left panel, 168 x 256, its right edge on the column: a vine along the edge, the stalk, big leaves, the
    shadowed jungle behind (tiles; the pattern repeats every 256 px up)"""
    cv = Canvas(PANEL_W, PANEL_H)
    W = PANEL_W
    for y in range(PANEL_H):                                          # the background: the jungle in the shade
        for x in range(W):
            cv.set(x, y, ST["bg0"])
    for k in range(9):                                                # hanging strands, gently waving
        x0 = 8 + k * 19 + (k * 7) % 5
        for y in range(PANEL_H):
            x = x0 + (1 if (y + k * 40) % 128 < 64 else 0)
            for dx in (0, 1):
                if 0 <= x + dx < W - 8:
                    cv.set(x + dx, y, ST["bg1"])
    for i, (lx, ly, rx) in enumerate(((W - 110, 30, 16), (W - 150, 120, 14), (W - 80, 190, 12), (W - 140, 230, 15),
                                      (W - 60, 96, 11), (20, 160, 13), (34, 60, 12))):   # far leaves, two tones
        for y in range(-rx, rx + 1):
            for x in range(-rx - 6, rx + 7):
                t = x / float(rx + 6)
                if abs(y) <= rx * 0.55 * (1 - t * t) * 1.6 and 0 <= lx + x < W - 8:
                    cv.set(lx + x, (ly + y) % PANEL_H, ST["bg2"] if y < 0 else ST["bg1"])
    # the stalk: a thick cylinder, its nodes every 64 px, centred 26 px from the column
    sx, sw = W - 30, 22
    for y in range(PANEL_H):
        for x in range(sx - sw // 2, sx + sw // 2):
            f = (x - (sx - sw // 2)) / float(sw - 1)
            c = ST["sh"] if f < 0.12 else ST["sl"] if f < 0.38 else ST["sm"] if f < 0.72 else ST["sd"] if f < 0.92 else ST["sdd"]
            if (x - y // 2) % 16 == 0 and 0.2 < f < 0.95:            # the twist
                c = ST["sd"] if f < 0.6 else ST["sdd"]
            if y % 64 in (0, 1):
                c = ST["sdd"] if y % 64 == 0 else ST["sd"]
            cv.set(x, y, c)
        if y % 16 == 8:                                               # little ribs on the stalk
            cv.set(sx - 3, y, ST["sm"]), cv.set(sx + 4, y, ST["sdd"])
    # big leaves sprouting from the nodes, alternately out and in
    for n, ny in enumerate(range(0, PANEL_H, 64)):
        out = n % 2 == 0
        cx, cy = (sx - 34, ny + 18) if out else (sx + 20, ny + 44)
        for y in range(-9, 10):
            for x in range(-24, 25):
                t = x / 24.0
                if abs(y) <= 9 * (1 - t * t) and 0 <= cx + x < W:
                    nd = -y / 9.0 - t * 0.3
                    c = ST["ll"] if nd > 0.4 else ST["lm"] if nd > -0.3 else ST["ld"]
                    if y == 0 or (abs(y) == abs(x) // 3 and x % 2 == 0):
                        c = ST["ld"] if nd > 0 else ST["sdd"]
                    cv.set(cx + x, (cy + y) % PANEL_H, c)
        for k in range(10):                                            # its stem to the stalk
            x = sx - sw // 2 - k if out else sx + sw // 2 + k - 10
            if out and x > cx + 20:
                cv.set(x, (ny + 10 + k // 2) % PANEL_H, ST["sd"])
    # the vine along the column's edge, with small leaves and pink buds
    for y in range(PANEL_H):
        vx = W - 5 + int(round(1.5 * math.sin(y * 2 * math.pi / 64)))
        for x in range(vx - 1, vx + 2):
            cv.set(x, y, ST["vine"] if x < vx + 1 else ST["vined"])
        cv.set(W - 1, y, ST["edge"])
        if y % 32 == 12:
            for dy in range(-2, 3):
                for dx in range(-4, 0):
                    if abs(dy) <= 2 + dx // 2:
                        cv.set(vx + dx, y + dy, ST["lm"])
        if y % 64 == 40:
            cv.set(vx - 2, y, ST["bud"]), cv.set(vx - 3, y, ST["bud"]), cv.set(vx - 2, y - 1, ST["bud"])
    return cv


def clouds():
    """the far clouds (BG 4), 256 x 256: a few flat-bottomed puffs, 3 tones, no outline (far layer)"""
    cv = Canvas(256, 256)
    puffs = [(56, 48, 26), (176, 112, 30), (84, 200, 20)]
    for px, py, r in puffs:
        for bx, by, rr in ((px - r * 0.6, py + 2, r * 0.55), (px, py - r * 0.15, r * 0.7), (px + r * 0.65, py + 3, r * 0.5)):
            for y in range(int(by - rr), int(by + rr) + 1):
                for x in range(int(bx - rr), int(bx + rr) + 1):
                    if (x - bx) ** 2 + (y - by) ** 2 <= rr * rr and y <= py + r * 0.35:
                        d = (y - (by - rr)) / (2 * rr)
                        c = CLOUD_PAL[0] if d < 0.4 else CLOUD_PAL[1] if d < 0.75 else CLOUD_PAL[2]
                        cv.set(x % 256, y % 256, c)
    return cv


def stars():
    """the stars and planets (BG 3), 256 x 256"""
    cv = Canvas(256, 256)
    S = STAR_PAL
    k = 1
    for i in range(70):
        k = (k * 1103515245 + 12345) & 0x7fffffff
        x, y = k % 256, (k // 256) % 256
        b = (k >> 20) % 3
        cv.set(x, y, S[b])
        if b == 0 and i % 5 == 0:
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                cv.set((x + dx) % 256, (y + dy) % 256, S[2])
    # a ringed planet and a little moon
    for y in range(-12, 13):
        for x in range(-12, 13):
            if x * x + y * y <= 100:
                nd = (-x * 0.6 - y * 0.8) / 10.0
                cv.set(190 + x, 60 + y, S[3] if nd > 0.3 else S[4] if nd > -0.4 else S[5])
    for x in range(-20, 21):
        y = int(round(x * 0.25))
        if not (x * x + y * y <= 100 and y < 0):
            cv.set(190 + x, 60 + y, S[6]), cv.set(190 + x, 61 + y, S[7])
    for y in range(-6, 7):
        for x in range(-6, 7):
            if x * x + y * y <= 36:
                nd = (-x * 0.6 - y * 0.8) / 6.0
                cv.set(60 + x, 180 + y, S[8] if nd > 0.2 else S[9] if nd > -0.5 else S[10])
    cv.set(58, 178, S[10]), cv.set(62, 182, S[10]), cv.set(61, 177, S[9])
    return cv


# ---- writing ---------------------------------------------------------------------------------------------------------
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


def picture(cv, pal, name):
    im = Image.new("P", (cv.w, cv.h), 0)
    im.putpalette([255, 0, 255] + [c for p in pal for c in p] + [0] * (3 * (255 - len(pal))))
    px = im.load()
    idx = indexed(cv, pal, name)
    for y in range(cv.h):
        for x in range(cv.w):
            px[x, y] = idx[y][x]
    return im


def preview(im, path, bg=(140, 196, 240), k=3):
    rgb = im.convert("RGB")
    out = Image.new("RGB", rgb.size, bg)
    mask = Image.frombytes("L", im.size, bytes(255 if b else 0 for b in im.tobytes()))
    out.paste(rgb, (0, 0), mask)
    out.resize((rgb.width * k, rgb.height * k), Image.NEAREST).save(path)


HERO_ENTRIES = [(n, lambda f, n=n: lemur(n), 24, 24, 1) for n in HERO_FRAMES]
HAT_ENTRIES = [(n, lambda f, n=n: hat(n), 16, 16, 1) for n in HATS]
PLAT_ENTRIES = [("leaf", lambda f: leaf(bent=f == 1), 32, 8, 2), ("move", wing_leaf, 32, 16, 2),
                ("dry", lambda f: leaf(dry=True), 32, 8, 1), ("dryhalf", half_leaf, 16, 8, 2),
                ("puff", lambda f: puff(), 32, 8, 1), ("shroom", shroom, 32, 16, 2), ("cloud", cloud_plat, 32, 16, 2),
                ("spring", spring, 16, 16, 2), ("ground", ground, 64, 32, 2), ("flowers", lambda f: ground_flowers(), 64, 8, 1)]
ITEM_ENTRIES = [("coin", coin, 8, 8, 4), ("hud_coin", lambda f: hud_coin(), 8, 8, 1), ("hud_up", lambda f: hud_up(), 8, 8, 1),
                ("seed", lambda f: seed(), 8, 8, 1), ("lock", lambda f: lock(), 8, 8, 1),
                ("prop", prop_hat, 16, 16, 2), ("rocket", lambda f: rocket(), 16, 16, 1),
                ("shield", lambda f: shield_item(), 16, 16, 1), ("magnet", lambda f: magnet(), 16, 16, 1),
                ("flag", lambda f: flag(), 16, 16, 1), ("jet", jet, 16, 16, 2), ("propeller", propeller, 16, 8, 2),
                ("rope", lambda f: rope(), 64, 8, 1)]
PEST_ENTRIES = [("bee", bee, 24, 24, 2), ("storm", storm, 24, 24, 2), ("bat", bat, 24, 24, 2), ("ufo", ufo, 24, 24, 2)]
FX_ENTRIES = [("dust", dust, 8, 8, 3), ("spark", spark, 8, 8, 2), ("tuft", tuft, 8, 8, 2), ("leafbit", leafbit, 8, 8, 2),
              ("drybit", drybit, 8, 8, 2), ("star", lambda f: star_fx(), 8, 8, 1), ("ring", ring, 16, 16, 2),
              ("bubble", lambda f: bubble(), 32, 32, 1)]


def write_all(out):
    os.makedirs(os.path.join(out, "preview"), exist_ok=True)
    layouts = {}
    jobs = [("hero", HERO_ENTRIES, HERO_PAL, 96), ("hats", HAT_ENTRIES, HATS_PAL, 128), ("plats", PLAT_ENTRIES, PLAT_PAL, 128),
            ("items", ITEM_ENTRIES, ITEM_PAL, 128), ("pests", PEST_ENTRIES, PEST_PAL, 96), ("fx", FX_ENTRIES, FX_PAL, 64)]
    for name, entries, pal, width in jobs:
        im, layout = sheet(entries, pal, width)
        im.save(os.path.join(out, name + ".png"))
        preview(im, os.path.join(out, "preview", name + ".png"))
        layouts[name] = layout
    for name, fn, pal in (("panel", panel, STALK_PAL), ("clouds", clouds, CLOUD_PAL), ("stars", stars, STAR_PAL)):
        im = picture(fn(), pal, name)
        im.save(os.path.join(out, name + ".png"))
        preview(im, os.path.join(out, "preview", name + ".png"), k=2)
    # the hero with every fur, for review
    for fname, fp in FURS.items():
        im, _ = sheet(HERO_ENTRIES, HERO_PAL, 96)
        im.putpalette([255, 0, 255] + [c for p in fp for c in p] + [0] * (3 * (255 - len(fp))))
        preview(im, os.path.join(out, "preview", "hero_" + fname + ".png"))
    anchors = {n: head_anchor(n) for n in HERO_FRAMES}
    pals = {"hero": HERO_PAL, "hats": HATS_PAL, "plats": PLAT_PAL, "items": ITEM_PAL, "pests": PEST_PAL, "fx": FX_PAL,
            "stalk": STALK_PAL, "clouds": CLOUD_PAL, "stars": STAR_PAL}
    for k, v in FURS.items():
        pals["fur_" + k] = v
    for z, v in STALK_ZONES.items():
        pals["stalk_" + z] = v + STALK_PAL[3:]
    for z, v in CLOUD_ZONES.items():
        pals["clouds_" + z] = v + CLOUD_PAL[3:]
    with open(os.path.join(out, "palettes.json"), "w") as f:
        json.dump({"palettes": pals, "layouts": layouts, "anchors": anchors, "furs": list(FURS.keys()),
                   "hats": HATS, "hero_frames": HERO_FRAMES}, f, indent=1, sort_keys=True)
    return layouts


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", default=os.path.join(GAME, "art"))
    a = ap.parse_args()
    write_all(a.out)
    print("art written to", a.out)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Duck Parade: the code-drawn art, in the 8BCraft house style (docs/art-direction.md).

Code-drawn only, no image generation: every sprite and tile is drawn here with
games/common/tools/house_style.py (1-px outlines outside the shape, 3-4 flat shades lit from the top-left,
squash and stretch, checker dither only for shadows and fades). The layouts (SPRITES, METAS) are shared with
build_assets.py, which imports this file.

The view is top-down (the lanes are columns 16 px wide, traffic runs up and down), with a little 3/4 feel:
the characters show their eye, a hop lifts the sprite off its shadow.

Writes, in the art folder (default games/duckparade/art):
  sprites.png          every sprite (packed, the layout is SPRITES)
  tiles/field.png      the 16x16 lane cells (METAS), tiles/river.png the rapids' 16 shifts
  tiles/sky.png        the clouds of the sky band, tiles/shadows.png the cloud shadows

    python3 games/duckparade/tools/make_art.py [--out DIR] [--preview FILE]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/duckparade/LICENSE.
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
LD = hs.LIGHT_DIR


def shade4(nx, ny, r4, bias=0.0):
    """One of the 4 shades of r4 for a surface normal (nx, ny) in screen space: the house light (top-left)."""
    return hs.shade_nd(nx * LD[0] + ny * LD[1] + bias, r4)


# =========================================================================================================
# palettes: explicit, so that player 2's palette swap and the kit's medal palette line up entry by entry
# =========================================================================================================
DUCK = [                                   # OBJ 0: Mother Duck (a white farm duck with a pink ribbon)
    (46, 38, 70),                          # 1 outline: the white's darkest shade pushed to violet
    (255, 255, 252), (238, 236, 226), (206, 202, 196), (158, 152, 160),   # 2-5 body
    (255, 253, 244), (242, 238, 230), (204, 198, 194),                    # 6-8 head (Father: green)
    (255, 178, 66), (222, 118, 34),        # 9-10 beak and feet
    (84, 124, 222),                        # 11 the wing's speculum (blue)
    (255, 150, 182), (214, 84, 128),       # 12-13 the ribbon (Father: the drake curl, head colour)
    (18, 8, 26),                           # 14 eye
    (230, 226, 216),                       # 15 the collar (Father: white)
]
FATHER = [                                 # OBJ 1: Father Duck, a mallard drake: the same tiles
    (40, 34, 52),
    (214, 202, 184), (176, 162, 142), (136, 120, 106), (92, 80, 72),      # grey-brown body
    (120, 210, 130), (44, 152, 84), (22, 96, 62),                         # the green head
    (244, 206, 78), (196, 150, 40),        # the yellow bill (and feet)
    (84, 124, 222),
    (44, 152, 84), (22, 96, 62),           # the curl: head green
    (18, 8, 26),
    (250, 250, 250),                       # the white collar
]
D = {k: DUCK[i] for i, k in enumerate(
    ["out", "b0", "b1", "b2", "b3", "h0", "h1", "h2", "k0", "k1", "spec", "r0", "r1", "eye", "collar"])}

LING = [                                   # OBJ 2: the ducklings and the effects
    (92, 56, 40),                          # 1 outline (warm, towards violet)
    (255, 250, 196), (255, 232, 110), (246, 200, 56), (212, 150, 32),     # 2-5 fluffy yellow
    (255, 148, 44),                        # 6 beak
    (18, 8, 26),                           # 7 eye
    (232, 248, 255), (132, 198, 238),      # 8-9 water (splash, ripples)
    (32, 44, 40),                          # 10 shadow (checker-dithered)
    (236, 72, 92), (255, 156, 176),        # 11-12 heart
    (250, 250, 250),                       # 13 feathers, white
    (214, 198, 160),                       # 14 dust
    (42, 68, 118),                         # 15 the "!" bubble's ink (UI navy)
]
L_ = {k: LING[i] for i, k in enumerate(
    ["out", "y0", "y1", "y2", "y3", "beak", "eye", "w0", "w1", "shadow", "red", "pink", "white", "dust", "navy"])}
LY = [L_["y0"], L_["y1"], L_["y2"], L_["y3"]]

CARS_A = [                                 # OBJ 4: red, blue and yellow cars, the bus, the bikes
    (30, 26, 44), (196, 230, 252), (92, 132, 176), (40, 40, 50), (255, 246, 190), (232, 44, 52),
    (252, 118, 104), (222, 58, 58), (150, 30, 46),
    (132, 192, 255), (64, 120, 222), (34, 62, 150),
    (255, 238, 124), (250, 196, 40), (196, 130, 22),
]
CARS_B = [                                 # OBJ 5: green, pink and white cars, joggers, the lawnmower
    (30, 26, 44), (196, 230, 252), (92, 132, 176), (40, 40, 50), (255, 246, 190),
    (152, 232, 122), (76, 176, 70), (36, 110, 50),
    (255, 178, 214), (236, 110, 170), (170, 56, 120),
    (250, 250, 246), (192, 198, 210),
    (255, 214, 180), (214, 150, 110),
]
RIVER = [                                  # OBJ 6: logs, lily pads, the swan boat
    (212, 162, 102), (174, 122, 70), (130, 86, 46), (86, 52, 28), (238, 204, 144), (46, 28, 16),
    (152, 216, 92), (84, 168, 60), (40, 110, 44), (20, 64, 34), (255, 172, 202),
    (250, 250, 250), (190, 198, 214), (250, 150, 40), (70, 130, 222),
]
RAIL = [                                   # OBJ 7: the train, the crossing light, the fox
    (24, 22, 36), (120, 204, 142), (50, 142, 92), (26, 86, 60), (222, 62, 52), (250, 236, 204),
    (126, 184, 232), (152, 158, 172), (255, 64, 40), (92, 32, 32), (252, 172, 92), (232, 122, 42),
    (170, 70, 32), (255, 236, 150),
]
R_ = {k: RAIL[i] for i, k in enumerate(["out", "g0", "g1", "g2", "red", "cream", "glass", "steel", "lamp", "lampoff",
                                        "f0", "f1", "f2", "head"])}
KIT = hs.UI                                # OBJ 3: the kit's palette (the egg medals use it, entry for entry)
KIT_ORDER = [KIT["out"], KIT["white"], KIT["shade"], KIT["bd"], KIT["bl"], KIT["sd"], KIT["sl"], KIT["gd"],
             KIT["gl"], KIT["pd"], KIT["pl"], KIT["pearl"], hs.SPARK["spark"], hs.SPARK["white"]]

# ---- BG palettes (the field on BG3, the clouds on BG4 and BG2) --------------------------------------------
GRASS = [(172, 216, 104), (142, 198, 82), (114, 174, 64), (86, 142, 50)]
TREE = [(128, 196, 82), (72, 150, 58), (40, 104, 46), (24, 64, 34)]
FLOWER = [(250, 250, 240), (255, 222, 72), (250, 142, 182)]
BERRY, ROCK = (222, 62, 62), [(176, 176, 172), (124, 124, 132)]
GRASS_SHADOW = (66, 116, 46)
ASPHALT = [(104, 108, 122), (90, 94, 106), (76, 80, 92), (60, 62, 74)]
LINE = [(242, 242, 232), (196, 196, 190)]
KERB = [(206, 206, 212), (164, 164, 174), (114, 116, 128)]
BIKE = [(196, 96, 84), (164, 74, 68), (132, 58, 56)]
RAPID = [(236, 248, 255), (172, 218, 250), (102, 174, 234), (62, 138, 216), (44, 106, 192), (30, 78, 152)]
GRAVEL = [(152, 140, 126), (124, 112, 100), (98, 88, 80)]
SLEEPER = [(164, 112, 72), (126, 84, 52), (88, 56, 34)]
STEEL = [(212, 216, 228), (122, 126, 142)]
SAND = [(238, 216, 166), (216, 190, 138), (188, 160, 112)]
POND = [(84, 168, 162), (58, 138, 142), (42, 112, 122), (124, 204, 192)]
PAD = [(152, 216, 92), (84, 168, 60), (40, 110, 44)]
REED = [(98, 152, 62), (142, 98, 52)]
STRAW = [(242, 210, 122), (208, 166, 82), (152, 112, 52)]
EGG = (250, 244, 230)
CLOUD = [(252, 252, 255), (218, 230, 248)]
SKY_KEY = (104, 176, 232)                  # the band's sky entry (the raster replaces it line by line)
SHADOW_SUB = (60, 74, 104)                 # a cloud's shadow: blended half and half with the field


# =========================================================================================================
# the ducks
# =========================================================================================================
def rot(u, v, facing):
    """Local (u = forward, v = to the duck's right) -> screen (x, y) offsets for a facing."""
    if facing == "right":
        return u, v
    if facing == "left":
        return -u, -v
    if facing == "up":
        return v, -u
    return -v, u                                                   # down


def local(x, y, facing):
    """Screen offsets -> local (u, v)."""
    if facing == "right":
        return x, y
    if facing == "left":
        return -x, -y
    if facing == "up":
        return -y, x
    return y, -x


def duck(facing="right", pose="rest", blink=False, size=1.0):
    """Mother Duck, 16x16, in the classic 3/4 view of top-down games: seen from the side when she goes right
    or left, from the front (down) or the back (up). A round white body, the head with a pink ribbon, an
    orange bill and feet, a blue speculum on the wing. Father Duck is the same tiles, recoloured (FATHER)."""
    cv = Canvas(16, 16)
    B = [D["b0"], D["b1"], D["b2"], D["b3"]]
    H = [D["h0"], D["h1"], D["h1"], D["h2"]]
    side = facing in ("right", "left")
    if side:
        # feet, then the body, the tail, the wing, the neck and head, the bill, the ribbon
        if pose == "hop":
            cv.set(5, 15, D["k1"]), cv.set(6, 15, D["k0"]), cv.set(9, 14, D["k1"]), cv.set(10, 14, D["k0"])
        else:
            for x in (5, 6, 9, 10):
                cv.set(x, 15, D["k0"] if x in (6, 10) else D["k1"])
        hs.ellipse(cv, 7.0, 10.6, 6.2, 4.3, B)
        for y in range(5, 10):                                       # the tail, up at the back
            for x in range(0, 4):
                if x + (9 - y) * 0.7 < 3.6 and x >= (9 - y) * 0.35:
                    cv.set(x, y, B[1] if y < 8 else B[2])
        for y in range(9, 14):                                       # the wing
            for x in range(2, 10):
                d = ((x + 0.5 - 6.0) / 4.2) ** 2 + ((y + 0.5 - 11.0) / 2.3) ** 2
                if d <= 1.0 and not (d < 0.55 and y < 11):
                    cv.set(x, y, B[2] if d > 0.55 else B[1])
        cv.set(3, 11, D["spec"]), cv.set(4, 11, D["spec"]), cv.set(3, 12, D["spec"])
        hs.ellipse(cv, 10.0, 7.8, 1.9, 2.2, B)                       # the neck
        for x in range(8, 12):
            if cv.get(x, 7) is not None:
                cv.set(x, 7, D["collar"])
        hs.ellipse(cv, 10.6, 4.8, 3.3, 3.1, H)                       # the head
        for x, y, c in ((13, 5, D["k0"]), (14, 5, D["k0"]), (15, 5, D["k1"]), (13, 6, D["k1"]), (14, 6, D["k1"])):
            cv.set(x, y, c)                                          # the bill
        if blink:
            cv.set(11, 4, D["h2"]), cv.set(12, 4, D["h2"])
        else:
            cv.set(11, 3, D["eye"]), cv.set(11, 4, D["eye"])
        for x, y in ((7, 2), (8, 3), (7, 4), (6, 3), (6, 2), (6, 4)):  # the ribbon, a bow at the back
            cv.set(x, y, D["r1"] if y == 4 or x == 6 else D["r0"])
        if facing == "left":
            cv = hs.flip_h(cv)
        return cv
    # front (down) and back (up)
    if pose == "hop":
        cv.set(5, 15, D["k1"]), cv.set(6, 15, D["k0"]), cv.set(9, 14, D["k1"]), cv.set(10, 14, D["k0"])
    else:
        for x in (5, 6, 9, 10):
            cv.set(x, 15, D["k0"] if x in (5, 9) else D["k1"])
    hs.ellipse(cv, 7.5, 10.8, 5.8, 4.3, B)
    for y in range(8, 14):                                           # the wings at the sides
        for x in (1, 2, 13, 14):
            if cv.get(x, y) is not None:
                cv.set(x, y, B[2] if x > 7 or y > 10 else B[1])
    cv.set(2, 11, D["spec"]), cv.set(13, 11, D["spec"])
    hs.ellipse(cv, 7.5, 7.6, 2.2, 2.0, B)
    for x in range(5, 11):
        if cv.get(x, 7) is not None:
            cv.set(x, 7, D["collar"])
    hs.ellipse(cv, 7.5, 4.6, 3.4, 3.2, H)
    if facing == "down":
        for x, y, c in ((7, 6, D["k0"]), (8, 6, D["k0"]), (6, 6, D["k1"]), (9, 6, D["k1"]), (7, 7, D["k1"]), (8, 7, D["k1"])):
            cv.set(x, y, c)
        if blink:
            cv.set(5, 4, D["h2"]), cv.set(10, 4, D["h2"])
        else:
            for x in (5, 10):
                cv.set(x, 3, D["eye"]), cv.set(x, 4, D["eye"])
        for x, y in ((10, 1), (11, 1), (11, 2), (10, 0)):            # the ribbon on the side of the head
            cv.set(x, y, D["r0"])
        cv.set(12, 1, D["r1"]), cv.set(12, 2, D["r1"])
    else:
        for x, y in ((6, 3), (7, 4), (8, 4), (9, 3), (5, 2), (5, 4), (10, 2), (10, 4)):   # the bow, from behind
            cv.set(x, y, D["r0"] if x in (7, 8) or y == 2 else D["r1"])
        for x in (6, 7, 8, 9):                                       # the tail tip, up at the back
            cv.set(x, 14, D["b1"])
        cv.set(7, 13, D["b0"]), cv.set(8, 13, D["b0"])
    return cv


def finish(cv, out, pose):
    if pose == "stretch":
        cv = hs.squash(cv, 0.86, 1.14)
    elif pose == "squash":
        cv = hs.squash(cv, 1.18, 0.84)
    elif pose == "hit":
        cv = hs.squash(cv, 1.4, 0.5)
    cv.outline(out)
    return cv


def duck_frame(facing, pose, blink=False):
    base = duck(facing, "hop" if pose in ("stretch", "hop") else "rest", blink)
    if pose == "hit":
        base = duck("right", "rest")                                 # X eyes, then flattened
        for d in range(3):
            base.set(10 + d, 3 + d, D["eye"]), base.set(12 - d, 3 + d, D["eye"])
    return finish(base, D["out"], pose)


def duck_swept(frame):
    """Mother Duck swept away: only her head above the churning water."""
    cv = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            r = math.hypot(x + 0.5 - 8, (y + 0.5 - 11) * 1.6)
            if abs(r - (6.0 + frame)) < 0.8 and (x + y + frame) % 3:
                cv.set(x, y, L_["w0"])
    hs.ellipse(cv, 8.0, 8.5 + frame * 0.5, 3.3, 3.1, [D["h0"], D["h1"], D["h1"], D["h2"]])
    cv.set(11, 9 + frame // 2, D["k0"]), cv.set(12, 9 + frame // 2, D["k1"])
    cv.set(9, 7 + frame // 2, D["eye"]), cv.set(9, 8 + frame // 2, D["eye"])
    for x, y in ((5, 6), (5, 7), (4, 6)):
        cv.set(x, y + frame // 2, D["r0"])
    cv.outline(D["out"])
    return cv


def duckling(facing="right", pose="rest", frame=0):
    """A lost duckling (about 10x10 in a 16x16 cell): fluffy yellow, an orange beak, a big head. The same 3/4
    views as Mother Duck."""
    cv = Canvas(16, 16)
    side = facing in ("right", "left") or pose == "tumble"
    if side:
        cv.set(6, 15, L_["beak"]), cv.set(9, 15, L_["beak"])
        hs.ellipse(cv, 7.0, 12.0, 4.4, 3.2, LY)
        cv.set(2, 10, LY[1]), cv.set(3, 10, LY[1]), cv.set(2, 9, LY[0])          # a fluffy tail
        hs.ellipse(cv, 9.4, 7.8, 3.0, 2.9, LY)
        cv.set(12, 8, L_["beak"]), cv.set(13, 8, L_["beak"]), cv.set(12, 9, LY[3])
        cv.set(10, 7, L_["eye"])
        cv.set(8, 5, LY[0]), cv.set(9, 4, LY[1])                                  # the tuft on top
        if pose == "flutter":
            for x, y in ((5, 9), (6, 8), (7, 8), (4, 10)):
                cv.set(x, y - frame, LY[0])
        else:
            for x in range(5, 9):
                cv.set(x, 12, LY[2])                                              # the wing nub
        if facing == "left":
            cv = hs.flip_h(cv)
    else:
        cv.set(5, 15, L_["beak"]), cv.set(10, 15, L_["beak"])
        hs.ellipse(cv, 7.5, 12.0, 4.4, 3.2, LY)
        hs.ellipse(cv, 7.5, 7.8, 3.2, 2.9, LY)
        cv.set(7, 4, LY[0]), cv.set(8, 3, LY[1])
        if facing == "down" or pose in ("peep", "flutter"):
            cv.set(6, 7, L_["eye"]), cv.set(9, 7, L_["eye"])
            if pose == "peep" and frame == 1:
                cv.set(7, 9, L_["beak"]), cv.set(8, 9, L_["beak"]), cv.set(7, 10, L_["out"]), cv.set(8, 10, L_["beak"])
            else:
                cv.set(7, 9, L_["beak"]), cv.set(8, 9, L_["beak"])
        if pose == "flutter":
            for x, y in ((2, 9), (1, 8), (13, 9), (14, 8)):
                cv.set(x, y + (1 if frame else -1) * (1 if y == 8 else 0), LY[1])
            cv.set(2, 10, LY[2]), cv.set(13, 10, LY[2])
    if pose == "hop":
        cv = hs.squash(cv, 0.86, 1.16)
    elif pose == "tumble":
        cv = rotate90(cv, frame + 1)
    cv.outline(L_["out"])
    return cv


def duckling_paddle(frame):
    """Paddling back (in the water) or swimming in a nest pond: half under, ripples around."""
    cv = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            r = math.hypot(x + 0.5 - 8, (y + 0.5 - 10) * 1.5)
            if abs(r - (5.8 + frame * 0.7)) < 0.7 and (x + frame) % 2 == 0:
                cv.set(x, y, L_["w0"])
    hs.ellipse(cv, 7.5, 9.5, 3.6, 2.4, LY)
    hs.ellipse(cv, 9.5, 7.5, 2.1, 2.0, LY)
    cv.set(11, 7, L_["beak"]), cv.set(12, 7, L_["beak"])
    cv.set(10, 6, L_["eye"])
    cv.outline(L_["out"])
    return cv


def rotate90(cv, k):
    out = cv
    for _ in range(k % 4):
        n = Canvas(cv.w, cv.h)
        for y in range(cv.h):
            for x in range(cv.w):
                n.p[x][cv.w - 1 - y] = out.p[y][x]
        out = n
    return out


def splash(frame):
    cv = Canvas(16, 16)
    r = 3.0 + frame * 2.2
    for y in range(16):
        for x in range(16):
            d = math.hypot(x + 0.5 - 8, (y + 0.5 - 9) * 1.3)
            if abs(d - r) < 0.9 and (frame < 2 or hs.checker(x, y)):
                cv.set(x, y, L_["w0"] if (x + y) % 3 else L_["w1"])
    for k in range(5):                               # droplets thrown up
        a = k * 1.3 + 0.4
        dx, dy = math.cos(a) * (2 + frame * 2.5), -abs(math.sin(a)) * (3 + frame * 1.5) - frame
        cv.set(int(8 + dx), int(6 + dy), L_["w0"])
    if frame == 0:
        hs.ellipse(cv, 8, 9, 3, 2, None, flat=L_["w1"])
    return cv


def shadow(wide):
    cv = Canvas(16, 8)
    rx = 5.5 if wide else 4.0
    for y in range(8):
        for x in range(16):
            dx, dy = (x + 0.5 - 8) / rx, (y + 0.5 - 4) / 2.2
            if dx * dx + dy * dy <= 1.0 and hs.checker(x, y):
                cv.set(x, y, L_["shadow"])
    return cv


def feather(frame):
    cv = Canvas(8, 8)
    pts = [(1, 5), (2, 4), (3, 3), (4, 3), (5, 2), (6, 1)] if frame == 0 else [(1, 2), (2, 3), (3, 4), (4, 4), (5, 5), (6, 6)]
    for i, (x, y) in enumerate(pts):
        cv.set(x, y, L_["white"])
        if 1 <= i <= 4:
            cv.set(x, y + (1 if frame == 0 else -1), L_["w1"])
    cv.outline(L_["out"])
    return cv


def puff(frame):
    cv = Canvas(8, 8)
    r = 1.6 + frame * 1.1
    for y in range(8):
        for x in range(8):
            d = math.hypot(x + 0.5 - 4, y + 0.5 - 4)
            if d < r and (frame == 0 or hs.checker(x, y, frame)):
                cv.set(x, y, L_["dust"] if d < r - 0.8 else L_["white"])
    return cv


def heart(frame):
    cv = Canvas(8, 8)
    rows = ["        ", " ## ##  ", "####### ", "####### ", " #####  ", "  ###   ", "   #    ", "        "]
    for y, r in enumerate(rows):
        for x, ch in enumerate(r):
            if ch == "#":
                cv.set(x, y + (frame and y > 0 and 0), L_["red"] if (x + y) > 4 else L_["pink"])
    cv.set(1, 2, L_["white"])
    cv.outline(L_["out"])
    return cv


def note():
    cv = Canvas(8, 8)
    for y in range(1, 6):
        cv.set(5, y, L_["navy"])
    cv.set(6, 1, L_["navy"]), cv.set(6, 2, L_["navy"])
    for x, y in ((3, 5), (4, 5), (3, 6), (4, 6), (5, 6)):
        cv.set(x, y, L_["navy"])
    return cv


def exclaim(frame):
    """The fox's "!" bubble: white, the UI navy mark."""
    cv = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            if math.hypot(x + 0.5 - 8, y + 0.5 - 7) < 6.0:
                cv.set(x, y, L_["white"])
    for x, y in ((6, 12), (7, 13), (7, 12)):
        cv.set(x, y + 0, L_["white"])
    for y in range(3, 9):
        cv.set(7, y, L_["red"]), cv.set(8, y, L_["red"])
    cv.set(7, 10, L_["red"]), cv.set(8, 10, L_["red"])
    cv.outline(L_["out"])
    if frame:
        cv = hs.squash(cv, 1.1, 0.92)
        cv.outline(L_["out"])
    return cv


# =========================================================================================================
# traffic (top-down, facing down or up; drawn in both directions so the light stays top-left)
# =========================================================================================================
def rounded_rect(x, y, x0, y0, x1, y1, r):
    """Is pixel (x, y) inside the rounded rectangle [x0, x1] x [y0, y1] (corner radius r)?"""
    cx = min(max(x, x0 + r), x1 - r)
    cy = min(max(y, y0 + r), y1 - r)
    return (x - cx) ** 2 + (y - cy) ** 2 <= r * r


def car(ramp, pal, facing="down", length=24, bus=False):
    """A cute rounded car: bonnet, windscreen, roof, rear window; wheels peeking out, lights front and back."""
    out, glass0, glass1, tyre, head = pal[0], pal[1], pal[2], pal[3], pal[4]
    tail = pal[5] if len(pal) > 14 and pal is CARS_A else ramp[2]
    L = length
    cv = Canvas(16, L)
    front = L - 1 if facing == "down" else 0
    for y in range(L):
        for x in range(16):
            fy = y if facing == "down" else L - 1 - y             # distance from the back
            # wheels
            for wy in ((4, 8) if not bus else (5, 9)), ((L - 9, L - 5) if not bus else (L - 11, L - 7)):
                if wy[0] <= fy <= wy[1] and x in (1, 14):
                    cv.set(x, y, tyre)
            xc = x + 0.5
            if rounded_rect(xc, y + 0.5, 2.0, 0.5, 14.0, L - 0.5, 3.4 if not bus else 2.2):
                nx = (xc - 8) / 6.0
                ny = ((y + 0.5) - L / 2.0) / (L / 2.0)
                col = shade4(nx * 0.9, ny * 0.5, ramp + [ramp[-1]] if len(ramp) == 3 else ramp)
                if len(ramp) == 3:
                    col = ramp[0] if nx < -0.55 else ramp[1] if nx < 0.45 else ramp[2]
                if bus:
                    # the bus: a long roof, rows of side windows
                    if 2 <= x <= 13 and 3 <= fy <= L - 4 and x in (3, 12) and fy % 5 in (1, 2, 3):
                        col = glass1 if x == 12 else glass0
                    if 5 <= x <= 10 and fy in (L - 6, L - 5):
                        col = glass0 if x < 8 else glass1
                    if fy % 10 == 0 and 5 <= x <= 10:
                        col = ramp[2]
                else:
                    # a bubble car: the bonnet, a wide windscreen, a domed roof, the rear window, the boot
                    ws0, ws1 = L - 11, L - 8                         # the windscreen (towards the front)
                    rw0, rw1 = 4, 5                                  # the rear window
                    inset = 1 if fy in (ws0, rw1) else 0             # the glass is narrower where it meets the roof
                    if ws0 <= fy <= ws1 and 3.5 + inset <= xc <= 12.5 - inset:
                        col = glass1
                        if (xc - 3.5) + (fy - ws0) * 1.2 < 3.2 or 5.2 < (xc - 3.5) + (fy - ws0) * 1.2 < 6.4:
                            col = glass0                             # the glint
                    elif rw0 <= fy <= rw1 and 4.5 + inset <= xc <= 11.5 - inset:
                        col = glass0 if xc < 6.5 else glass1
                    elif rw1 < fy < ws0 and 3.5 <= xc <= 12.5:
                        col = ramp[0] if xc < 6.5 and fy > rw1 + 1 else ramp[1] if xc < 11 else ramp[2]   # the roof
                        if xc > 12.0 or (rw1 < fy < ws0 and xc < 4.0):
                            col = ramp[2]
                    elif fy == ws1 + 1 or fy == rw0 - 1:
                        col = ramp[2] if 4 <= xc <= 12 else col      # the seams of the bonnet and the boot
                cv.set(x, y, col)
    if not bus:
        # the side mirrors, a glint on the bonnet
        my = (L - 12) if facing == "down" else 11
        cv.set(1, my, ramp[2]), cv.set(14, my, ramp[2])
        gy = L - 5 if facing == "down" else 4
        cv.set(5, gy, hs.HOUSE["white"][0])
    # lights
    fy_front, fy_back = (L - 1, 0) if facing == "down" else (0, L - 1)
    for x in (4, 5, 10, 11):
        cv.set(x, fy_front, head)
    for x in (4, 11):
        cv.set(x, fy_back, tail)
    cv.outline(out)
    return cv


def bike(facing="down", frame=0):
    """A cyclist from above: a red helmet, blue shoulders, the wheels front and back."""
    pal = CARS_A
    cv = Canvas(16, 16)
    sgn = 1 if facing == "down" else -1
    for y in range(16):
        for x in range(16):
            fy = y if facing == "down" else 15 - y
            if 7 <= x <= 8 and (fy <= 3 or fy >= 12):
                cv.set(x, y, pal[3])                                     # the wheels
    hs.ellipse(cv, 8.0, 8.0, 4.2, 2.6, [pal[9], pal[10], pal[10], pal[11]])   # shoulders
    arm = 1 if frame else 0
    for x in (4, 11):
        for k in range(3):
            cv.set(x, 8 + sgn * (k + 1 + arm), pal[10])
    hs.ellipse(cv, 8.0, 8.0 + sgn * 0.5, 2.2, 2.2, [pal[6], pal[7], pal[7], pal[8]])  # the helmet
    for x in range(5, 11):
        cv.set(x, 8 + sgn * 5, pal[7])                                   # the handlebar
    cv.outline(pal[0])
    return cv


def jogger(facing="down", frame=0, shirt=0):
    """A jogger in the 3/4 view: coming towards us (down) or running away (up); arms and legs swing."""
    pal = CARS_B
    shirts = [[pal[8], pal[9], pal[9], pal[10]], [pal[5], pal[6], pal[6], pal[7]]]
    skin, skin_d, hair, shorts = pal[13], pal[14], pal[3], pal[11]
    cv = Canvas(16, 16)
    sw = 1 if frame else -1
    # legs (skin), the shorts, the shirt, the arms, the head
    cv.set(6, 13 + (sw > 0), skin), cv.set(6, 12, skin), cv.set(9, 13 + (sw < 0), skin), cv.set(9, 12, skin)
    cv.set(6, 14 + (sw > 0), pal[3]), cv.set(9, 14 + (sw < 0), pal[3])            # shoes
    for x in range(5, 11):
        cv.set(x, 11, shorts), cv.set(x, 10, shorts)
    for y in range(6, 10):
        for x in range(5, 11):
            cv.set(x, y, shirts[shirt % 2][0 if x < 7 else 1 if x < 9 else 3])
    for k in range(3):
        cv.set(4, 7 + k - sw, skin), cv.set(11, 7 + k + sw, skin_d)
    hs.ellipse(cv, 8.0, 3.6, 2.6, 2.6, [skin, skin, skin_d, skin_d])
    if facing == "down":
        cv.set(7, 3, pal[0]), cv.set(9, 3, pal[0])                               # the eyes
        for x in range(6, 11):
            cv.set(x, 1, hair)
    else:
        for y in range(1, 5):
            for x in range(6, 11):
                if cv.get(x, y) is not None:
                    cv.set(x, y, hair)                                           # the back of the head
    cv.outline(pal[0])
    return cv


def mower(facing="down", frame=0):
    """A ride-on lawnmower: a green body, the gardener's straw hat, a spray of clippings behind."""
    pal = CARS_B
    cv = Canvas(16, 24)
    green = [pal[5], pal[6], pal[6], pal[7]]
    for y in range(24):
        for x in range(16):
            fy = y if facing == "down" else 23 - y
            if rounded_rect(x + 0.5, y + 0.5, 2, 3, 14, 21, 3):
                nx = (x + 0.5 - 8) / 6.0
                cv.set(x, y, green[0] if nx < -0.5 else green[1] if nx < 0.4 else green[3])
            if fy in (4, 5, 6, 17, 18, 19) and x in (1, 14):
                cv.set(x, y, pal[3])
    # the cutting deck (front): white stripes shimmering
    for x in range(3, 13):
        yy = 20 if facing == "down" else 3
        cv.set(x, yy, pal[11] if (x + frame) % 2 else pal[12])
    # the gardener: a straw hat (skin light as straw), shoulders
    hs.ellipse(cv, 8.0, 10.5 if facing == "down" else 13.5, 3.4, 3.0, [pal[13], pal[13], pal[14], pal[14]])
    cv.set(7, 10 if facing == "down" else 13, pal[11])
    cv.outline(pal[0])
    return cv


# =========================================================================================================
# river things
# =========================================================================================================
def log(cells):
    L = cells * 16
    cv = Canvas(16, L)
    wood = RIVER[0:4]
    for y in range(L):
        for x in range(16):
            xc = x + 0.5
            if 2 <= x <= 13:
                col = hs.shade3(hs.ramp_dict(wood), x, y, 12, 2)
                if (x in (5, 9) and (y * 3 + x) % 7 < 5) or (x == 11 and y % 9 < 6):
                    col = wood[2] if col != wood[3] else wood[3]      # bark grooves
                if y < 3 or y >= L - 3:                               # the cut ends: rings
                    e = y if y < 3 else L - 1 - y
                    r = math.hypot(xc - 8, (2.5 - e) * 1.6)
                    col = RIVER[4] if r < 3.2 else wood[1] if r < 4.6 else wood[2]
                    if abs(r - 2.0) < 0.5:
                        col = wood[1]
                if y in (0, L - 1) and x in (2, 13):
                    continue
                cv.set(x, y, col)
    # a knot and a twig
    kx, ky = 6, L // 2 + 3
    cv.set(kx, ky, wood[3]), cv.set(kx + 1, ky, wood[2])
    cv.outline(RIVER[5])
    return cv


def lilypad(variant):
    cv = Canvas(16, 16)
    g = RIVER[6:9]
    ang0 = [0.4, 2.2, 4.0][variant % 3]
    for y in range(16):
        for x in range(16):
            dx, dy = x + 0.5 - 8, y + 0.5 - 8
            r = math.hypot(dx, dy)
            a = (math.atan2(dy, dx) - ang0) % (2 * math.pi)
            if r < 6.8 and not (a < 0.45 and r > 1.0):
                col = shade4(dx / 6.8, dy / 6.8, [g[0], g[0], g[1], g[2]])
                if abs(a - math.pi) < 0.08 or abs(a - math.pi * 0.5) < 0.1 or abs(a - math.pi * 1.5) < 0.1:
                    col = g[2] if r > 2 else col                          # the veins
                cv.set(x, y, col)
    if variant == 1:                                                        # a pink flower
        hs.ellipse(cv, 8.0, 7.5, 2.3, 2.3, None, flat=RIVER[10])
        cv.set(8, 7, RIVER[11]), cv.set(7, 8, RIVER[11])
    cv.outline(RIVER[9])
    return cv


def swan_boat(facing="down", frame=0):
    """The swan paddle boat: a white swan from above, her wings raised along the sides, a blue bench between
    them, the long neck reaching forward with the head and its orange beak; a paddle wheel splashing."""
    cv = Canvas(16, 32)
    white = [RIVER[11], RIVER[11], RIVER[12], RIVER[12]]
    grey = RIVER[12]
    fl = (lambda fy: fy) if facing == "down" else (lambda fy: 31 - fy)   # fy (0 = the tail) -> y
    # the hull: an egg, pointed at the tail
    for fy in range(1, 26):
        half = 6.6 * math.sin(min(1.0, (fy + 1) / 19.0) * math.pi / 2) if fy < 19 else 6.6 - (fy - 19) * 0.75
        for x in range(16):
            if abs(x + 0.5 - 8) <= half:
                cv.set(x, fl(fy), shade4((x + 0.5 - 8) / 6.6, -0.2, white))
    # the wings raised along the sides: grey scallops
    for fy in range(5, 20):
        for side in (1, 14):
            if (fy + (0 if side == 1 else 1)) % 3:
                cv.set(side + (1 if side == 1 else -1), fl(fy), grey)
    # the bench and its back
    for fy in range(9, 17):
        for x in range(5, 11):
            cv.set(x, fl(fy), RIVER[14] if fy < 15 else grey)
    # the neck, the head, the beak and the eye
    for fy in range(24, 29):
        cv.set(7, fl(fy), RIVER[11]), cv.set(8, fl(fy), RIVER[12])
    hs.ellipse(cv, 8.0, fl(29) + 0.5, 2.0, 1.8, white)
    cv.set(8, fl(31), RIVER[13]), cv.set(7, fl(31), RIVER[13])
    cv.set(9, fl(29), RIVER[5])
    # the paddle wheel splashing at the back
    wy = fl(4 + frame)
    cv.set(1, wy, RIVER[12]), cv.set(14, fl(5 - frame), RIVER[12])
    cv.outline(RIVER[5])
    return cv


# =========================================================================================================
# the railway and the fox
# =========================================================================================================
def train_car(kind, facing="down"):
    """The locomotive (a nose with a headlight) or a carriage, 16x48 from above."""
    L = 48
    cv = Canvas(16, L)
    g = [R_["g0"], R_["g1"], R_["g1"], R_["g2"]]
    for y in range(L):
        for x in range(16):
            fy = y if facing == "down" else L - 1 - y            # 0 = the back
            nose = kind == "loco" and fy > L - 9
            half = 6.5 if not nose else 6.5 - (fy - (L - 9)) * 0.55
            if abs(x + 0.5 - 8) <= half and 1 <= fy <= L - 2:
                nx = (x + 0.5 - 8) / 6.5
                col = g[0] if nx < -0.55 else g[1] if nx < 0.45 else g[3]
                if x in (5, 10) and fy % 8 in (2, 3, 4, 5) and not nose:
                    col = R_["glass"]                              # windows along the roof edges
                if kind == "loco" and L - 16 <= fy <= L - 12 and 4 <= x <= 11:
                    col = R_["glass"]                              # the cab
                if 7 <= x <= 8 and not nose:
                    col = R_["red"]                                # the stripe
                if kind == "car" and fy in (2, L - 3):
                    col = R_["steel"]
                cv.set(x, y, col)
    if kind == "loco":
        hy = L - 2 if facing == "down" else 1
        cv.set(7, hy, R_["head"]), cv.set(8, hy, R_["head"])
    cv.outline(R_["out"])
    return cv


def signal(frame):
    """The crossing light in the sky band: a black post, two red lamps flashing in turn (frame 0 off)."""
    cv = Canvas(16, 16)
    for y in range(8, 16):
        cv.set(7, y, R_["steel"]), cv.set(8, y, R_["out"])
    for i, lx in enumerate((4, 11)):
        lit = (frame == 1 and i == 0) or (frame == 2 and i == 1)
        hs.ellipse(cv, lx + 0.5, 5.5, 2.6, 2.6, None, flat=R_["lamp"] if lit else R_["lampoff"])
        if lit:
            cv.set(lx - 1, 4, R_["cream"])
    for x in range(3, 13):
        cv.set(x, 9, R_["out"])
    cv.outline(R_["out"])
    return cv


def fox(pose, frame=0):
    """The hungry fox (24x24): 'watch' (crouched at the edge, ears up, eyes on the duck), 'pounce' (a long leap),
    'catch' (carrying Mother Duck away)."""
    cv = Canvas(24, 24)
    o = [R_["f0"], R_["f1"], R_["f1"], R_["f2"]]
    if pose == "watch":
        hs.ellipse(cv, 8.0, 14.5, 6.5, 4.6, o)                  # the body, crouched
        for k in range(6):                                       # the tail, curled
            hs.ellipse(cv, 2.0 + k * 0.3, 18.5 - k * 0.9, 2.2, 1.6, o)
        cv.set(1, 13, R_["cream"]), cv.set(2, 13, R_["cream"])
        hs.ellipse(cv, 15.5, 10.5 - frame * 0.5, 4.6, 4.0, o)    # the head
        for ex in (13, 18):                                      # the ears
            for k in range(4):
                cv.set(ex + (k // 2), 5 - k + (frame and 0), o[1] if k < 3 else R_["out"])
                cv.set(ex + 1, 6 - k, o[2])
        for x in range(17, 22):                                  # the white muzzle, the nose
            cv.set(x, 12, R_["cream"]), cv.set(x, 13, R_["cream"])
        cv.set(21, 11, R_["out"])
        cv.set(16, 9, R_["out"]), cv.set(19, 9, R_["out"])       # the eyes, looking right
        cv.set(17, 9, R_["cream"]) if frame else None
    elif pose == "pounce":
        for k in range(24):                                      # a stretched leap
            y = 13 - math.sin(k / 23.0 * math.pi) * 5
            hs.ellipse(cv, k * 0.72 + 3, y + 1.5, 3.0 if 4 < k < 20 else 2.0, 2.8, o)
        hs.ellipse(cv, 19.5, 8.5, 3.8, 3.4, o)
        for x in range(20, 24):
            cv.set(x, 10, R_["cream"])
        cv.set(23, 9, R_["out"]), cv.set(20, 7, R_["out"])
        cv.set(18, 4, o[1]), cv.set(19, 5, o[1]), cv.set(21, 4, o[1])
    else:                                                        # catch: trotting away with the duck
        hs.ellipse(cv, 11.0, 15.5, 7.0, 4.2, o)
        hs.ellipse(cv, 4.5, 11.5, 4.2, 3.8, o)
        cv.set(3, 9, R_["out"])
        for k in range(4):
            cv.set(1 + k, 16 + (k + frame) % 2, o[2])
        hs.ellipse(cv, 1.5, 13.5, 2.6, 2.0, None, flat=R_["cream"])   # the duck in its jaws (white)
        for x in range(12, 22):
            cv.set(x, 19 + (x + frame) % 2, o[3])
    cv.outline(R_["out"])
    return cv


# =========================================================================================================
# the egg medals (the house tiers, the kit's palette) and small UI bits
# =========================================================================================================
def egg_medal(tier):
    cv = Canvas(24, 24)
    dark, light = [(KIT["bd"], KIT["bl"]), (KIT["sd"], KIT["sl"]), (KIT["gd"], KIT["gl"]), (KIT["pd"], KIT["pl"])][tier]
    for y in range(24):
        for x in range(24):
            dx, dy = (x + 0.5 - 12) / 7.2, (y + 0.5 - 13.5)
            ry = 9.5 if dy < 0 else 8.0                           # an egg: pointier at the top
            dy /= ry
            if dx * dx + dy * dy <= 1.0:
                nd = -dx * LD[0] * -1 * -1 + 0
                col = light if (dx * LD[0] + dy * LD[1]) > -0.25 else dark
                if dx * dx + dy * dy > 0.82 and (dx * LD[0] + dy * LD[1]) < 0.2:
                    col = dark
                cv.set(x, y, col)
    for k, (sx, sy) in enumerate(((9, 8), (10, 7), (9, 9))):     # the shine, top-left
        cv.set(sx, sy, KIT["white"])
    if tier == 3:                                                  # the pearl egg: speckles and a sheen
        for sx, sy in ((14, 9), (11, 14), (15, 16), (9, 17), (13, 19), (16, 12)):
            cv.set(sx, sy, KIT["pd"])
        for sx, sy in ((12, 6), (13, 6)):
            cv.set(sx, sy, KIT["pearl"])
    cv.outline(KIT["out"])
    return cv


def nest_sign(frame):
    """The nest pond's sign in the sky band: a little flag with a heart."""
    cv = Canvas(16, 16)
    for y in range(3, 16):
        cv.set(4, y, R_["steel"])
    for y in range(3, 10):
        for x in range(5, 5 + 8 - abs(y - 6)):
            cv.set(x + (frame if x > 8 else 0), y, R_["cream"])
    for x, y in ((7, 5), (9, 5), (6, 6), (7, 6), (8, 6), (9, 6), (10, 6), (7, 7), (8, 7), (9, 7), (8, 8)):
        cv.set(x + (frame if x > 8 else 0), y, R_["red"])
    cv.outline(R_["out"])
    return cv


# =========================================================================================================
# the sprite sheet layout: (name, w, h, frames, group, draw(frame) -> Canvas)
# =========================================================================================================
FACINGS = ["right", "left", "up", "down"]


def _duck_entries():
    e = []
    for f in FACINGS:
        e.append(("duck_%s" % f, 16, 16, 4, "duck",
                  lambda k, f=f: duck_frame(f, ["rest", "stretch", "squash", "rest"][k], blink=(k == 3))))
    e.append(("duck_hit", 16, 16, 1, "duck", lambda k: duck_frame("right", "hit")))
    e.append(("duck_swept", 16, 16, 2, "duck", duck_swept))
    return e


def _ling_entries():
    e = []
    for f in FACINGS:
        e.append(("ling_%s" % f, 16, 16, 2, "ling", lambda k, f=f: duckling(f, ["rest", "hop"][k])))
    e += [("ling_peep", 16, 16, 2, "ling", lambda k: duckling("down", "peep", k)),
          ("ling_tumble", 16, 16, 4, "ling", lambda k: duckling("right", "tumble", k)),
          ("ling_flutter", 16, 16, 2, "ling", lambda k: duckling("up", "flutter", k)),
          ("ling_paddle", 16, 16, 2, "ling", duckling_paddle),
          ("splash", 16, 16, 3, "ling", splash),
          ("shadow", 16, 8, 2, "ling", lambda k: shadow(k == 1)),
          ("exclaim", 16, 16, 2, "ling", exclaim),
          ("feather", 8, 8, 2, "ling", feather),
          ("puff", 8, 8, 3, "ling", puff),
          ("heart", 8, 8, 1, "ling", heart),
          ("note", 8, 8, 1, "ling", lambda k: note())]
    return e


def _traffic_entries():
    red, blue, yellow = CARS_A[6:9], CARS_A[9:12], CARS_A[12:15]
    green, pink, white = CARS_B[5:8], CARS_B[8:11], [CARS_B[11], CARS_B[11], CARS_B[12]]
    e = []
    for i, (ramp, pal, grp) in enumerate(((red, CARS_A, "cars_a"), (blue, CARS_A, "cars_a"), (yellow, CARS_A, "cars_a"),
                                          (green, CARS_B, "cars_b"), (pink, CARS_B, "cars_b"), (white, CARS_B, "cars_b"))):
        e.append(("car%d" % i, 16, 24, 2, grp,
                  lambda k, ramp=ramp, pal=pal: car(ramp, pal, ["down", "up"][k])))
    e.append(("bus", 16, 40, 2, "cars_a", lambda k: car(CARS_A[12:15], CARS_A, ["down", "up"][k], 40, bus=True)))
    e.append(("bike", 16, 16, 4, "cars_a", lambda k: bike(["down", "up"][k // 2], k % 2)))
    e.append(("jogger", 16, 16, 8, "cars_b", lambda k: jogger(["down", "up"][k // 4], k % 2, (k // 2) % 2)))
    e.append(("mower", 16, 24, 4, "cars_b", lambda k: mower(["down", "up"][k // 2], k % 2)))
    return e


def _river_entries():
    return [("log2", 16, 32, 1, "river", lambda k: log(2)),
            ("log3", 16, 48, 1, "river", lambda k: log(3)),
            ("log4", 16, 64, 1, "river", lambda k: log(4)),
            ("pad", 16, 16, 3, "river", lilypad),
            ("boat", 16, 32, 4, "river", lambda k: swan_boat(["down", "up"][k // 2], k % 2))]


def _rail_entries():
    return [("loco", 16, 48, 2, "rail", lambda k: train_car("loco", ["down", "up"][k])),
            ("carriage", 16, 48, 1, "rail", lambda k: train_car("car")),
            ("signal", 16, 16, 3, "rail", signal),
            ("nest_sign", 16, 16, 2, "rail", nest_sign),
            ("fox_watch", 24, 24, 2, "rail", lambda k: fox("watch", k)),
            ("fox_pounce", 24, 24, 1, "rail", lambda k: fox("pounce")),
            ("fox_catch", 24, 24, 2, "rail", lambda k: fox("catch", k))]


def _kit_entries():
    return [("egg", 24, 24, 4, "kit", egg_medal)]


GROUPS = ["duck", "ling", "cars_a", "cars_b", "river", "rail", "kit"]
PALETTES = {"duck": DUCK, "ling": LING, "cars_a": CARS_A, "cars_b": CARS_B, "river": RIVER, "rail": RAIL,
            "kit": KIT_ORDER}
OBJ_PAL = {"duck": 0, "ling": 2, "kit": 3, "cars_a": 4, "cars_b": 5, "river": 6, "rail": 7}
SHEET_W = 256


def sprite_entries():
    return _duck_entries() + _ling_entries() + _traffic_entries() + _river_entries() + _rail_entries() + _kit_entries()


def layout():
    """Packs the entries into a 256-wide sheet: shelves, left to right. Returns [(entry, x, y)], height."""
    out, x, y, shelf = [], 0, 0, 0
    for e in sprite_entries():
        name, w, h, n, g, fn = e
        if x + w * n > SHEET_W:
            x, y, shelf = 0, y + shelf, 0
        out.append((e, x, y))
        x += w * n
        shelf = max(shelf, h)
    return out, y + shelf


# =========================================================================================================
# the field: 16x16 cells (METAS), each with its BG palette group
# =========================================================================================================
def grass_cell(seed, flowers=True, fringe=None):
    rng = random.Random(seed)
    cv = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            v = (x * 7 + y * 13 + seed * 5) % 23
            col = GRASS[1] if v < 17 else GRASS[2]
            cv.set(x, y, col)
    for k in range(5):                                            # tufts: little "v" strokes
        tx, ty = rng.randrange(1, 15), rng.randrange(2, 15)
        cv.set(tx, ty, GRASS[3]), cv.set(tx - 1, ty - 1, GRASS[2]), cv.set(tx + 1, ty - 1, GRASS[2])
        cv.set(tx + 1, ty - 2, GRASS[0])
    if flowers:
        for k in range(rng.randrange(1, 3)):
            fx, fy = rng.randrange(2, 14), rng.randrange(2, 14)
            c = FLOWER[rng.randrange(3)]
            cv.set(fx, fy, c), cv.set(fx + 1, fy, c), cv.set(fx, fy + 1, c), cv.set(fx + 1, fy + 1, FLOWER[1] if c != FLOWER[1] else FLOWER[0])
    return cv


def tree_cell(seed):
    cv = grass_cell(seed, flowers=False)
    for y in range(16):                                            # its shadow, bottom-right
        for x in range(16):
            if math.hypot(x + 0.5 - 9.5, (y + 0.5 - 10.5) * 1.1) < 6.2 and hs.checker(x, y):
                cv.set(x, y, GRASS_SHADOW)
    blobs = [(7.5, 7.5, 6.3), (5.5, 6.0, 3.6), (9.5, 6.2, 3.6), (7.5, 10.0, 3.8)]
    for y in range(16):
        for x in range(16):
            best = None
            for bx, by, r in blobs:
                d = math.hypot(x + 0.5 - bx, y + 0.5 - by)
                if d <= r:
                    nd = ((x + 0.5 - bx) * LD[0] + (y + 0.5 - by) * LD[1]) / r
                    c = hs.shade_nd(nd * 0.9 + 0.05, TREE)
                    best = c if best is None or TREE.index(c) < TREE.index(best) else best
            if best is not None and math.hypot(x + 0.5 - 7.5, y + 0.5 - 7.5) <= 6.8:
                cv.set(x, y, best)
    # the outline: the canopy's darkest shade, inside the cell
    edge = []
    for y in range(16):
        for x in range(16):
            if cv.get(x, y) in TREE:
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    if cv.get(x + dx, y + dy) not in TREE and cv.get(x + dx, y + dy) is not None:
                        edge.append((x + dx, y + dy))
    for x, y in edge:
        cv.set(x, y, TREE[3])
    if seed % 3 == 0:                                               # red apples now and then
        for ax, ay in ((5, 6), (10, 8), (7, 11)):
            cv.set(ax, ay, BERRY)
    return cv


def bush_cell(seed, hedge=False):
    cv = grass_cell(seed, flowers=False)
    rng = random.Random(seed)
    blobs = [(4.5, 9.5, 3.8), (11.0, 9.0, 4.0), (7.8, 6.2, 4.2)] if not hedge else \
        [(3.5, 3.5, 4.3), (11.5, 4.0, 4.4), (4.0, 11.5, 4.4), (11.5, 12.0, 4.3), (8.0, 8.0, 4.5)]
    for y in range(16):
        for x in range(16):
            for bx, by, r in blobs:
                d = math.hypot(x + 0.5 - bx, y + 0.5 - by)
                if d <= r:
                    nd = ((x + 0.5 - bx) * LD[0] + (y + 0.5 - by) * LD[1]) / r
                    cv.set(x, y, hs.shade_nd(nd, TREE))
    for k in range(3 if not hedge else 2):
        bx, by = rng.randrange(3, 13), rng.randrange(4, 12)
        cv.set(bx, by, BERRY)
    if not hedge:
        for x in range(16):
            for y in range(16):
                c = cv.get(x, y)
                if c in TREE and any(cv.get(x + dx, y + dy) in GRASS + [GRASS_SHADOW] for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                    cv.set(x, y, TREE[3])
    return cv


def rock_cell(seed):
    cv = grass_cell(seed, flowers=False)
    hs.ellipse(cv, 8.0, 9.0, 5.5, 4.2, [ROCK[0], ROCK[0], ROCK[1], ROCK[1]])
    for y in range(16):
        for x in range(16):
            if cv.get(x, y) in ROCK and any(cv.get(x + dx, y + dy) not in ROCK for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                cv.set(x, y, TREE[3])
    cv.set(6, 7, FLOWER[0])
    return cv


def road_cell(left, right, dash, bike=False, symbol=False):
    """left: 'kerb' | 'dash' | 'plain'; right: 'kerb' | 'plain'; dash: the lane dash is painted in this cell."""
    A = BIKE + [BIKE[2]] if bike else ASPHALT
    cv = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            v = (x * 5 + y * 11 + (x * y) % 7) % 13
            col = A[1] if v < 9 else A[2] if v < 12 else A[0]
            cv.set(x, y, col)
    if left == "kerb":
        for y in range(16):
            cv.set(0, y, KERB[1] if y % 8 else KERB[2]), cv.set(1, y, KERB[0] if y % 8 else KERB[1])
            cv.set(2, y, ASPHALT[3])
    elif left == "dash" and dash:
        for y in range(3, 13):
            cv.set(0, y, LINE[1]), cv.set(1, y, LINE[0])
    if right == "kerb":
        for y in range(16):
            cv.set(15, y, KERB[2] if y % 8 else KERB[2]), cv.set(14, y, KERB[1] if y % 8 else KERB[2])
            cv.set(13, y, ASPHALT[3])
    if symbol:                                                      # a bike painted on the lane
        for x, y in ((5, 5), (5, 6), (5, 7), (10, 5), (10, 6), (10, 7), (6, 8), (9, 8), (7, 7), (8, 6), (7, 5)):
            cv.set(x, y + 2, LINE[0])
    return cv


def rail_cell(seed):
    cv = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            v = (x * 3 + y * 7 + (x * x + y) % 5) % 9
            cv.set(x, y, GRAVEL[0] if v < 3 else GRAVEL[1] if v < 7 else GRAVEL[2])
    for sy in (2, 10):                                              # sleepers
        for y in range(sy, sy + 4):
            for x in range(1, 15):
                cv.set(x, y, SLEEPER[0] if y == sy else SLEEPER[2] if y == sy + 3 else SLEEPER[1])
    for rx in (4, 11):                                              # the rails
        for y in range(16):
            cv.set(rx, y, STEEL[0]), cv.set(rx + 1, y, STEEL[1])
    if seed == 1:
        cv.set(1, 7, GRASS[2]), cv.set(2, 6, GRASS[1]), cv.set(14, 15, GRASS[2])
    return cv


def path_cell(seed, left_grass, right_grass):
    rng = random.Random(seed)
    cv = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            v = (x * 11 + y * 5 + seed) % 17
            cv.set(x, y, SAND[0] if v < 10 else SAND[1] if v < 15 else SAND[2])
    for k in range(3):
        px, py = rng.randrange(3, 13), rng.randrange(1, 15)
        cv.set(px, py, SAND[2]), cv.set(px + 1, py, SAND[1])
    for side, on in ((0, left_grass), (15, right_grass)):
        if not on:
            continue
        for y in range(16):
            cv.set(side, y, GRASS[2])
            w = 1 + ((y * 7 + side + seed) % 3 == 0)
            for k in range(w):
                cv.set(side + (k if side == 0 else -k), y, GRASS[2] if k == 0 else GRASS[3])
    return cv


def pond_cell(pad, variant, top=False, bottom=False):
    cv = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            v = (x * 3 + y * 5 + variant * 7) % 11
            cv.set(x, y, POND[1] if v < 8 else POND[2])
            if (y + variant * 3) % 8 == 0 and 3 < (x + variant * 5) % 16 < 9:
                cv.set(x, y, POND[0])                                # ripples
    if pad:
        c = lilypad(variant % 3)
        for y in range(16):
            for x in range(16):
                p = c.get(x, y)
                if p is not None:
                    m = {RIVER[6]: PAD[0], RIVER[7]: PAD[1], RIVER[8]: PAD[2], RIVER[9]: PAD[2],
                         RIVER[10]: FLOWER[2], RIVER[11]: FLOWER[0]}
                    cv.set(x, y, m.get(p, PAD[1]))
    return cv


def nest_cell(part, variant):
    """The nest pond: shallow light water, reeds at the top and bottom, the island with the nest in the middle."""
    cv = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            v = (x * 5 + y * 3 + variant) % 13
            cv.set(x, y, POND[3] if v < 9 else POND[0])
    if part in ("reeds_top", "reeds_bottom"):
        rng = random.Random(variant * 11 + (part == "reeds_top"))
        for k in range(6):
            rx = rng.randrange(1, 15)
            h = rng.randrange(6, 13)
            for y in range(h):
                yy = y if part == "reeds_top" else 15 - y
                cv.set(rx, yy, REED[0] if y < h - 2 else REED[1])
                if k % 2 and y < h - 3:
                    cv.set(rx + 1, yy, REED[0])
    if part in ("island_top", "island_bottom"):
        base = 0 if part == "island_top" else -16
        for y in range(16):
            for x in range(16):
                Y = y - base                                          # 0..31 over the two cells
                if math.hypot(x + 0.5 - 8, (Y + 0.5 - 16) * 0.55) < 7.2:
                    cv.set(x, y, PAD[0] if x + Y < 18 else PAD[1])      # a grassy island
                if math.hypot(x + 0.5 - 8, (Y + 0.5 - 16) * 0.8) < 5.6:
                    r = math.hypot(x + 0.5 - 8, (Y + 0.5 - 16) * 0.8)
                    cv.set(x, y, STRAW[0] if r > 4.4 and (x + Y) % 3 else STRAW[1] if r > 3.2 else STRAW[2])
                for ex, ey in ((6.5, 15.0), (9.5, 16.5), (7.5, 18.0)):   # eggs
                    if math.hypot(x + 0.5 - ex, (Y + 0.5 - ey) * 1.2) < 1.5:
                        cv.set(x, y, EGG)
    return cv


def metas():
    """name -> (Canvas 16x16, palette group). Groups: grass, road, rail (rail + park), pond (pond + nest)."""
    m = {}
    for i in range(6):
        m["grass%d" % i] = (grass_cell(i * 17 + 3, flowers=i < 4), "grass")
    for i in range(3):
        m["tree%d" % i] = (tree_cell(i * 7 + 1), "grass")
    for i in range(2):
        m["bush%d" % i] = (bush_cell(i * 5 + 2), "grass")
    m["rock"] = (rock_cell(9), "grass")
    for i in range(2):
        m["hedge%d" % i] = (bush_cell(40 + i, hedge=True), "grass")
    for left in ("kerb", "dash", "plain"):
        for right in ("kerb", "plain"):
            for dash in (0, 1):
                if left != "dash" and dash:
                    continue
                m["road_%s_%s_%d" % (left, right, dash)] = (road_cell(left, right, dash), "road")
                m["bike_%s_%s_%d" % (left, right, dash)] = (road_cell(left, right, dash, bike=True), "road")
    m["bike_sym"] = (road_cell("kerb", "kerb", 0, bike=True, symbol=True), "road")
    for i in range(2):
        m["rail%d" % i] = (rail_cell(i), "rail")
    for lg in (0, 1):
        for rg in (0, 1):
            for i in range(2):
                m["path_%d%d_%d" % (lg, rg, i)] = (path_cell(i * 3 + lg * 5 + rg, lg, rg), "rail")
    for i in range(3):
        m["pond%d" % i] = (pond_cell(False, i), "pond")
        m["pondpad%d" % i] = (pond_cell(True, i), "pond")
    for i in range(2):
        m["nest_water%d" % i] = (nest_cell("water", i), "pond")
        m["nest_reeds_top%d" % i] = (nest_cell("reeds_top", i), "pond")
        m["nest_reeds_bottom%d" % i] = (nest_cell("reeds_bottom", i), "pond")
    m["nest_island_top"] = (nest_cell("island_top", 0), "pond")
    m["nest_island_bottom"] = (nest_cell("island_bottom", 0), "pond")
    return m


META_PAL = {"grass": 1, "road": 2, "water": 3, "rail": 4, "pond": 6}
PAL_LOGO, PAL_CLOUD = 5, 7


def river_windows():
    """The rapids: a 16x16 pattern repeating along the lane; 16 vertical shifts x (left bank, right bank)."""
    rng = random.Random(77)
    pat = [[RAPID[3] if (x * 5 + y * 3) % 7 else RAPID[4] for x in range(16)] for y in range(16)]
    for k in range(9):                                              # streaks along the flow
        sx, sy, ln = rng.randrange(1, 15), rng.randrange(16), rng.randrange(3, 7)
        c = RAPID[1] if k % 3 else RAPID[0]
        for i in range(ln):
            pat[(sy + i) % 16][sx] = c if i < ln - 1 else RAPID[2]
    for k in range(4):
        sx, sy = rng.randrange(2, 14), rng.randrange(16)
        pat[sy][sx] = RAPID[2]
        pat[(sy + 1) % 16][sx] = RAPID[2]
    out = {}
    for s in range(16):
        for lb in (0, 1):
            for rb in (0, 1):
                cv = Canvas(16, 16)
                for y in range(16):
                    for x in range(16):
                        c = pat[(y - s) % 16][x]
                        if lb and x == 0:
                            c = RAPID[5]
                        elif lb and x == 1:
                            c = RAPID[0] if (y - s) % 4 else RAPID[1]
                        if rb and x == 15:
                            c = RAPID[5]
                        elif rb and x == 14:
                            c = RAPID[4]
                        cv.set(x, y, c)
                out[(s, lb, rb)] = cv
    return out


def sky_clouds():
    """The sky band's clouds (BG4, far: two tones, no outline): 512x16."""
    cv = Canvas(512, 16)
    for y in range(16):
        for x in range(512):
            cv.set(x, y, SKY_KEY)                                 # the sky: its colour is set per line (raster)
    rng = random.Random(5)
    x = 10
    while x < 500:
        w = rng.randrange(26, 60)
        for y in range(16):
            for xx in range(w):
                bump = 2.5 * math.sin(xx / w * math.pi * 3 + x) + 5.5 * math.sin(xx / w * math.pi)
                top = 13 - bump
                if y >= top and y < 15:
                    cv.set(x + xx, y, CLOUD[0] if y < 12 else CLOUD[1])
        x += w + rng.randrange(30, 90)
    return cv


def cloud_shadows():
    """Soft cloud shadows drifting over the field (BG2, subtracted): 256x256, one flat colour, checker edges."""
    cv = Canvas(256, 256)
    rng = random.Random(9)
    blobs = []
    for k in range(5):
        cx, cy = rng.randrange(20, 236), rng.randrange(20, 236)
        for j in range(4):
            blobs.append((cx + rng.randrange(-22, 22), cy + rng.randrange(-12, 12), rng.randrange(12, 22)))
    for y in range(256):
        for x in range(256):
            best = 99
            for bx, by, r in blobs:
                dx = min(abs(x - bx), 256 - abs(x - bx))
                dy = min(abs(y - by), 256 - abs(y - by))
                best = min(best, math.hypot(dx, dy * 1.3) - r)
            if best < -2 or (best < 1 and hs.checker(x, y)):
                cv.set(x, y, SHADOW_SUB)
    return cv


# =========================================================================================================
# writing
# =========================================================================================================
def sheet_image():
    lay, h = layout()
    cv = Canvas(SHEET_W, h)
    for (name, w, hh, n, g, fn), x, y in lay:
        for k in range(n):
            c = fn(k)
            assert c.w == w and c.h == hh, name
            cv.paste(c, x + k * w, y)
    return cv


def field_image():
    m = metas()
    names = sorted(m)
    cols = 16
    cv = Canvas(cols * 16, ((len(names) + cols - 1) // cols) * 16)
    for i, n in enumerate(names):
        cv.paste(m[n][0], (i % cols) * 16, (i // cols) * 16)
    return cv, names


def river_image():
    w = river_windows()
    cv = Canvas(16 * 16, 16 * 4)
    for (s, lb, rb), c in w.items():
        cv.paste(c, s * 16, (lb * 2 + rb) * 16)
    return cv


def write_all(out):
    os.makedirs(os.path.join(out, "tiles"), exist_ok=True)
    sheet_image().image().save(os.path.join(out, "sprites.png"))
    f, _ = field_image()
    f.image().save(os.path.join(out, "tiles", "field.png"))
    river_image().image().save(os.path.join(out, "tiles", "river.png"))
    sky_clouds().image().save(os.path.join(out, "tiles", "sky.png"))
    cloud_shadows().image().save(os.path.join(out, "tiles", "shadows.png"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(GAME, "art"))
    ap.add_argument("--preview", default="", help="also write a 3x preview of the sprite sheet")
    a = ap.parse_args()
    write_all(a.out)
    if a.preview:
        img = sheet_image().image()
        img.resize((img.width * 3, img.height * 3), Image.NEAREST).save(a.preview)
    print("art written to", a.out)


if __name__ == "__main__":
    main()

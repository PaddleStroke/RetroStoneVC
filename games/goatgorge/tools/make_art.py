#!/usr/bin/env python3
"""Goat Gorge: the code-drawn art, in the 8BCraft house style (docs/art-direction.md).

Code-drawn only, no image generation. Everything is drawn in PALETTE INDICES (1..15, 0 = clear), so that one drawing
serves every chapter (the terrain, its spikes and decorations take the chapter's colours) and the kid's scarf can
change colour with the dash (OBJ 0 red: the dash is ready; OBJ 1 blue: it is used), house style: 1-px outlines outside
the shapes, 3-4 flat shades lit from the top-left, squash and stretch.

Writes, in the art folder (default games/goatgorge/art), indexed PNGs (build_assets.py reads their indices):
  terrain.png     the rooms' 8x8 tiles (rock quarters, spikes, ledges, crumbling blocks, decorations)
  sprites.png     the kid, the props, the effects, the HUD icons (SPRITES: the layout)
  far_chN.png     each chapter's far layer (BG4), 256 x 96;   mid_chN.png  its middle layer (BG3), 512 x 112
  palettes.json   every palette (RGB), by chapter
and art/preview/*.png: the same in colour, each chapter's terrain, at 2x.

    python3 games/goatgorge/tools/make_art.py [--out DIR]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/goatgorge/LICENSE.
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
CHAPTERS = 5

# ---- palettes ------------------------------------------------------------------------------------------------------
# The terrain (BG palette 1): its indices are roles, the chapters give them colours.
T_OUT, T_RD, T_RM, T_RL, T_RH, T_CD, T_CM, T_CL, T_SD, T_SM, T_SL, T_FA, T_FB, T_CR, T_X = range(1, 16)
TERRAIN = [
    # outline, rock dark/mid/light/highlight, cap dark/mid/light, spike dark/mid/light, flower a/b, crack, extra
    # 1 the meadow: warm grey rock, green grass, grey stone points, daisies
    [(44, 30, 46), (104, 88, 92), (140, 124, 120), (176, 160, 148), (206, 194, 178), (58, 122, 52), (98, 168, 64),
     (156, 210, 92), (92, 92, 112), (156, 160, 176), (226, 230, 238), (252, 246, 236), (250, 200, 64), (78, 62, 70),
     (120, 104, 104)],
    # 2 the pine forest: mossy green-grey rock, deep moss, brambles' thorns, bluebells
    [(30, 34, 44), (70, 86, 84), (98, 118, 110), (132, 152, 138), (168, 186, 166), (40, 96, 66), (66, 140, 84),
     (118, 190, 108), (86, 50, 52), (148, 92, 80), (214, 168, 140), (132, 150, 236), (250, 236, 160), (48, 58, 60),
     (84, 102, 96)],
    # 3 the crystal cave: purple rock, glowing crystal moss, crystal points, glow-caps
    [(20, 14, 36), (60, 44, 92), (86, 66, 124), (116, 94, 156), (150, 130, 186), (56, 130, 168), (86, 192, 214),
     (170, 242, 246), (88, 46, 150), (170, 110, 230), (236, 200, 255), (250, 150, 210), (120, 240, 210), (38, 26, 60),
     (72, 54, 108)],
    # 4 the windy ridge: red sandstone, dry golden grass, stone points, little red flowers
    [(46, 22, 30), (130, 66, 54), (172, 96, 70), (206, 132, 92), (232, 170, 120), (170, 120, 40), (214, 168, 64),
     (246, 214, 120), (96, 72, 84), (160, 140, 150), (226, 218, 222), (236, 76, 72), (255, 220, 140), (96, 46, 42),
     (150, 80, 62)],
    # 5 the summit: blue-grey granite under snow, icicles, frost flowers
    [(26, 30, 56), (76, 88, 120), (106, 120, 154), (140, 156, 188), (176, 190, 216), (150, 178, 214), (210, 228, 246),
     (252, 254, 255), (70, 120, 180), (140, 200, 240), (228, 248, 255), (190, 210, 255), (250, 250, 255), (54, 62, 92),
     (96, 108, 140)],
]
# BG palette 2: the props drawn in the map (ledges: wood; crumbling blocks: dry stone), the same every chapter
P_OUT, P_WD, P_WM, P_WL, P_KD, P_KM, P_KL, P_KH = range(1, 9)
PROPS = [(48, 28, 30), (124, 76, 44), (176, 118, 66), (222, 170, 104), (150, 112, 86), (196, 156, 112),
         (230, 200, 150), (250, 232, 196)] + [(0, 0, 0)] * 7

# the sprites' palettes: OBJ 0 the kid (dash ready: the red scarf), OBJ 1 (dash used: blue), 2 the props, 4 the dash's
# trail (added colours, see draw.c), 5 the effects. OBJ 3 is the house kit's.
K_OUT, K_WH, K_WL, K_WM, K_WD, K_HL, K_HD, K_HOOF, K_INK, K_EYE, K_SL, K_SM, K_SD, K_PINK, K_FACE = range(1, 16)
KID = [(52, 26, 50), (255, 252, 244), (240, 230, 210), (212, 194, 170), (164, 140, 122), (236, 200, 138),
       (176, 126, 76), (96, 60, 54), (26, 12, 32), (250, 250, 250), (255, 120, 104), (228, 52, 62), (150, 22, 50),
       (246, 156, 152), (226, 210, 190)]
KID_NODASH = list(KID)
KID_NODASH[K_SL - 1], KID_NODASH[K_SM - 1], KID_NODASH[K_SD - 1] = (150, 216, 255), (64, 134, 230), (36, 70, 158)
TRAIL = [(30, 36, 70)] + [(40 + 6 * i, 70 + 8 * i, 120 + 7 * i) for i in range(14)]
O_OUT, O_RL, O_RM, O_RD, O_GL, O_GM, O_GD, O_CL, O_CM, O_CD, O_ML, O_MM, O_MD, O_WH, O_DK = range(1, 16)
PROP = [(44, 24, 40), (255, 120, 120), (226, 40, 66), (148, 20, 56), (156, 222, 92), (74, 160, 60), (34, 96, 44),
        (200, 255, 244), (90, 226, 206), (30, 150, 150), (230, 210, 160), (178, 140, 92), (110, 78, 52),
        (255, 255, 255), (70, 70, 96)]
F_OUT, F_W, F_L, F_M, F_D, F_R, F_RL, F_Y, F_G, F_B, F_SN = range(1, 12)
FX = [(60, 40, 60), (255, 255, 255), (240, 232, 214), (210, 196, 170), (160, 140, 120), (238, 64, 72),
      (255, 160, 140), (255, 230, 120), (140, 220, 120), (150, 210, 255), (220, 236, 255)] + [(0, 0, 0)] * 4

# the sky (the backdrop's gradient, draw.c) and the far and middle layers' colours, by chapter
SKY = [((92, 168, 236), (214, 240, 252)), ((96, 170, 186), (226, 240, 214)), ((18, 12, 38), (64, 40, 96)),
       ((112, 84, 168), (255, 196, 128)), ((20, 26, 70), (130, 112, 186))]
FAR = [[(150, 186, 226), (122, 160, 210), (236, 244, 252), (198, 222, 246)],
       [(120, 168, 168), (96, 142, 146), (210, 230, 220), (168, 204, 196)],
       [(44, 30, 72), (36, 24, 60), (90, 70, 130), (60, 44, 96)],
       [(176, 104, 140), (140, 82, 124), (255, 200, 150), (214, 140, 140)],
       [(70, 82, 138), (56, 64, 116), (220, 230, 255), (120, 130, 186)]]
MID = [[(104, 168, 96), (78, 140, 80), (52, 106, 66), (138, 196, 112), (36, 78, 54)],
       [(56, 110, 92), (40, 86, 74), (28, 64, 58), (84, 140, 110), (20, 46, 44)],
       [(80, 60, 130), (60, 44, 104), (130, 220, 230), (90, 170, 200), (40, 28, 70)],
       [(190, 106, 86), (156, 80, 72), (120, 58, 58), (222, 146, 106), (90, 40, 46)],
       [(150, 166, 206), (116, 130, 176), (236, 244, 255), (196, 212, 240), (84, 94, 140)]]


def h(*a):
    """a small deterministic hash"""
    v = 2166136261
    for x in a:
        v = ((v ^ (x & 0xffffffff)) * 16777619) & 0xffffffff
    return v


# ---- the terrain's tiles ------------------------------------------------------------------------------------------
# rock quarters: q 0 TL, 1 TR, 2 BL, 3 BR; kinds: 0..2 the inside (3 textures), 3 an inner corner, 4 the vertical side
# exposed (the top for q 0-1, the bottom for 2-3), 5 the horizontal side exposed (left 0/2, right 1/3), 6 an outer corner
ROCK_KINDS = 7


STRATA = [0, 0, 1, 1, 2, 2, 2, 1, 1, 0, 0, -1, -1, -1, 0, 0]       # the layers' wave across a cell (seamless)
CRACKS = [(3, 2, 5), (11, 8, 12), (6, 9, 13)]                       # a vertical crack per variant: x, from y, to y


def stone_pixel(gx, gy, v):
    """a pixel of a 16x16 cell of rock: wavy layers (their tops lit, their undersides dark), a crack, a few pebbles;
    the layers line up from cell to cell, the variants differ by their crack and pebbles"""
    k = (gy + STRATA[gx]) % 8
    c = T_RM
    if k == 7:
        c = T_RD
    elif k == 0:
        c = T_RL
    cx, y0, y1 = CRACKS[v]
    if gx == cx and y0 <= gy <= y1 and k not in (7, 0):
        c = T_CR
    if gx == cx + 1 and y0 <= gy <= y1 and k not in (7, 0):
        c = T_RL
    if h(gx, gy, v, 3) % 41 == 0 and k in (2, 3, 4):
        c = T_RH
    if h(gx, gy, v, 5) % 37 == 0 and k in (3, 4, 5):
        c = T_RD
    return c


def rock_quarter(q, kind):
    t = [[0] * 8 for _ in range(8)]
    top, left = q < 2, q % 2 == 0
    for y in range(8):
        for x in range(8):
            gx, gy = x + (0 if left else 8), y + (0 if top else 8)      # the pixel in its 16x16 cell
            dv = y if top else 7 - y                                    # to the exposed top / bottom
            ds = x if left else 7 - x                                   # to the exposed left / right
            vexp, hexp = kind in (4, 6), kind in (5, 6)
            # the inside: big stones (three layouts), each lit from the top-left, dark joints between them
            v = kind if kind < 3 else 0
            c = stone_pixel(gx, gy, v)
            if kind == 3:                                               # an inner corner: the corner's pixel
                if dv == 0 and ds == 0:
                    c = T_OUT
                elif dv + ds == 1:
                    c = T_CM if top else T_RD
            if hexp:
                if ds == 0:
                    c = T_OUT
                elif ds == 1:
                    c = T_RH if left else T_RD
                elif ds == 2 and not left:
                    c = T_RD if c == T_RM else c
            if vexp:
                if top:
                    depth = 3 + (h(gx, 3) % 2)                          # the cap's jagged edge
                    if dv == 0:
                        c = T_OUT
                    elif dv == 1:
                        c = T_CL
                    elif dv <= depth - 1:
                        c = T_CM
                    elif dv == depth:
                        c = T_CD
                    elif dv == depth + 1 and h(gx, 9) % 3 == 0:
                        c = T_CD
                else:
                    if dv == 0:
                        c = T_OUT
                    elif dv == 1:
                        c = T_RD
                if kind == 6:                                           # the outer corner, rounded
                    if dv + ds < 2:
                        c = 0
                    elif dv + ds == 2:
                        c = T_OUT
                    elif top and ds == 1 and dv <= 3:
                        c = T_CL if dv > 1 else T_OUT
            t[y][x] = c
    return t


def spike_up():
    """one spike, 8 wide, 7 tall, on the tile's bottom (a cell holds two)"""
    t = [[0] * 8 for _ in range(8)]
    for y in range(1, 8):
        half = (y + 1) / 2.0                                            # the half width at this row
        for x in range(8):
            d = abs(x + 0.5 - 4)
            if d <= half:
                c = T_SM
                if x < 4 and d > half - 1.2:
                    c = T_SL
                if x >= 4 and d > half - 1.2:
                    c = T_SD
                t[y][x] = c
    out = [row[:] for row in t]
    for y in range(8):
        for x in range(8):
            if t[y][x] == 0 and any(0 <= x + dx < 8 and 0 <= y + dy < 8 and t[y + dy][x + dx]
                                    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                out[y][x] = T_OUT
    return out


def spike_right():
    """the same spike lying down, pointing right (on a wall's right side): the up spike turned"""
    u = spike_up()
    return [[u[7 - x][y] for x in range(8)] for y in range(8)]


def ledge(part):
    """a one-way wooden plank on the cell's top: part 0 the left end, 1 the middle, 2 the right end"""
    t = [[0] * 8 for _ in range(8)]
    for y in range(6):
        for x in range(8):
            c = P_WM
            if y == 0 or y == 5:
                c = P_OUT
            elif y == 1:
                c = P_WL
            elif y == 4:
                c = P_WD
            elif (x + 3 * part) % 7 == 0 and y == 3:
                c = P_WD
            if (part == 0 and x == 0) or (part == 2 and x == 7):
                c = P_OUT
            t[y][x] = c
    if part == 0:
        t[0][0] = t[5][0] = 0
    if part == 2:
        t[0][7] = t[5][7] = 0
    if part in (0, 2):                                                  # a peg under the plank, at its ends
        px = 2 if part == 0 else 5
        t[6][px] = P_OUT
        t[6][px + (1 if part == 0 else -1)] = P_WD
    return t


def crumble(q):
    """a crumbling block (dry, cracked stone), quarter q of 16x16"""
    t = [[0] * 8 for _ in range(8)]
    top, left = q < 2, q % 2 == 0
    for y in range(8):
        for x in range(8):
            gx, gy = x + (0 if left else 8), y + (0 if top else 8)
            c = P_KM
            if gx in (0, 15) or gy in (0, 15):
                c = P_OUT
            elif gy == 1 or gx == 1:
                c = P_KH if gy == 1 else P_KL
            elif gy == 14 or gx == 14:
                c = P_KD
            if 2 <= gx <= 13 and 2 <= gy <= 13:                         # the cracks
                if gx == 7 + (gy // 3) % 2 and gy < 9:
                    c = P_KD
                if gy == 9 and 3 <= gx <= 12:
                    c = P_KD
                if gx == 4 and gy > 9:
                    c = P_KD
                if gx == 11 and 10 <= gy <= 13:
                    c = P_KD
            t[y][x] = c
    if q == 0:
        t[0][0] = 0
    if q == 1:
        t[0][7] = 0
    if q == 2:
        t[7][0] = 0
    if q == 3:
        t[7][7] = 0
    return t


def crumble_gone(q):
    """where a crumbled block will come back: a dotted outline"""
    t = [[0] * 8 for _ in range(8)]
    top, left = q < 2, q % 2 == 0
    for y in range(8):
        for x in range(8):
            gx, gy = x + (0 if left else 8), y + (0 if top else 8)
            if (gx in (1, 14) or gy in (1, 14)) and (gx + gy) % 3 == 0:
                t[y][x] = P_KD
    return t


def decor(kind):
    """a decoration on a rock's top (the lower half of the empty cell above): 0 a tuft, 1 a flower, 2 two flowers,
    3 a pebble"""
    t = [[0] * 8 for _ in range(8)]
    if kind == 0:
        for x, hgt in ((1, 3), (2, 5), (3, 2), (4, 4), (5, 6), (6, 3)):
            for y in range(8 - hgt, 8):
                t[y][x] = T_CM if y > 8 - hgt else T_CL
    elif kind in (1, 2):
        for fx, fh in ((3, 5),) if kind == 1 else ((1, 4), (5, 6)):
            for y in range(8 - fh + 1, 8):
                t[y][fx] = T_CD
            py = 8 - fh
            for dx, dy in ((0, -1), (-1, 0), (1, 0), (0, 1)):
                if 0 <= fx + dx < 8 and 0 <= py + dy < 8:
                    t[py + dy][fx + dx] = T_FA
            t[py][fx] = T_FB
            if fx + 1 < 8:
                t[7][fx + 1] = T_CM
    else:
        for y in range(5, 8):
            for x in range(2, 7):
                if (x - 4) ** 2 / 6.0 + (y - 6.5) ** 2 / 2.5 <= 1:
                    t[y][x] = T_RL if y == 5 or x == 2 else T_RM if y < 7 else T_RD
        for y in range(4, 8):
            for x in range(1, 8):
                if t[y][x] == 0 and any(0 <= x + dx < 8 and 0 <= y + dy < 8 and t[y + dy][x + dx] not in (0, T_OUT)
                                        for dx, dy in ((1, 0), (-1, 0), (0, -1))):
                    t[y][x] = T_OUT
    return t


def fill_tile(c):
    return [[c] * 8 for _ in range(8)]


def build_tiles():
    """the terrain atlas (draw.c's T_* tile numbers follow this order)"""
    tiles, names = [fill_tile(0)], ["blank"]
    for q in range(4):
        for k in range(ROCK_KINDS):
            tiles.append(rock_quarter(q, k))
            names.append("rock_%d_%d" % (q, k))
    tiles.append(spike_up()); names.append("spike_up")
    tiles.append(spike_right()); names.append("spike_right")
    for p in range(3):
        tiles.append(ledge(p)); names.append("ledge_%d" % p)
    for q in range(4):
        tiles.append(crumble(q)); names.append("crumble_%d" % q)
    for q in range(4):
        tiles.append(crumble_gone(q)); names.append("crumble_gone_%d" % q)
    for k in range(4):
        tiles.append(decor(k)); names.append("decor_%d" % k)
    tiles.append(fill_tile(T_RD)); names.append("deep")             # the mountain's inside (past the room)
    tiles.append(fill_tile(T_OUT)); names.append("dark")            # deeper still
    return tiles, names


# the props' tiles (BG palette 2) among them: the ledges and the crumbling blocks
def tile_palette(name):
    return 2 if name.startswith("ledge") or name.startswith("crumble") else 1


# ---- the kid -------------------------------------------------------------------------------------------------------
WOOL = [K_WH, K_WL, K_WM, K_WD]


def kid(pose="idle", frame=0):
    """the goat kid, 24 x 24, facing right, its hooves on the bottom row"""
    cv = Canvas(24, 24)
    bob = 0
    legs = [(7, 0), (9, 0), (14, 0), (16, 0)]                 # x of each leg, its lift (back, back, front, front)
    head = (16.5, 10.5)
    body = (11.0, 16.0, 6.2, 3.9)
    scarf_flap = frame % 2
    if pose == "idle":
        bob = 1 if frame == 1 else 0
    elif pose == "run":
        ph = frame % 6
        swing = [(-2, 1), (-1, 0), (1, 0), (2, 1), (1, 2), (-1, 2)][ph]
        legs = [(7 - swing[0], swing[1]), (9 + swing[0], 2 - swing[1]), (14 + swing[0], swing[1]),
                (16 - swing[0], 2 - swing[1])]
        bob = 1 if ph in (1, 4) else 0
    elif pose in ("jump", "up"):
        legs = [(7, 2), (9, 3), (14, 3), (16, 2)]
    elif pose == "fall":
        legs = [(6, 0), (8, 1), (15, 1), (17, 0)]
    elif pose == "slide":
        legs = [(8, 1), (10, 0), (18, 4), (18, 7)]
    elif pose == "dash":
        head = (18.0, 13.0)
        body = (11.0, 15.5, 7.0, 3.5)
        legs = [(5, 2), (7, 2), (15, 2), (17, 2)]
    elif pose == "dead":
        legs = [(6, 0), (9, 1), (14, 1), (17, 0)]
    bx, by, brx, bry = body
    by += bob
    for i, (lx, lift) in enumerate(legs):                     # the legs (the far pair darker), hooves at the bottom
        top = int(by + bry - 1)
        bottom = 21 - lift
        if pose == "slide" and i >= 2:                        # the front hooves on the wall
            yy = 11 + (i - 2) * 3
            cv.rect(19, yy, 20, yy, K_WD)
            cv.set(21, yy, K_HOOF)
            continue
        col = K_WD if i in (0, 2) else K_WM
        for y in range(top, bottom + 1):
            cv.set(lx, y, col)
            cv.set(lx + 1, y, col if i in (0, 2) else K_WL)
        cv.set(lx, bottom + 1, K_HOOF)
        cv.set(lx + 1, bottom + 1, K_HOOF)
    tx, ty = int(bx - brx), int(by - 2)                       # the tail
    cv.set(tx, ty, K_WL); cv.set(tx - 1, ty - 1, K_WH); cv.set(tx, ty - 1, K_WL)
    hs.ellipse(cv, bx, by, brx, bry, WOOL)                    # the body: wool, lit from the top-left
    for k in range(3):                                        # a few curls
        cv.set(int(bx - 3 + 3 * k), int(by - 1 + (k % 2)), K_WM)
    hx, hy = head
    hy += bob
    # the scarf: a band at the neck, its end flapping behind
    for x in range(int(hx) - 4, int(hx) + 1):
        cv.set(x, int(hy) + 2, K_SM)
        cv.set(x, int(hy) + 3, K_SD)
    cv.set(int(hx) - 4, int(hy) + 2, K_SL)
    tail_y = int(hy) + 2 - (1 if pose == "fall" else 0) + scarf_flap
    for k, x in enumerate(range(int(hx) - 8, int(hx) - 4)):
        yy = tail_y + (k % 2 if scarf_flap else 0) - (1 if pose in ("jump", "up", "dash") and k < 2 else 0)
        cv.set(x, yy, K_SM if k % 2 else K_SL)
        cv.set(x, yy + 1, K_SD)
    # the head: round, the muzzle a shade warmer, an ear back, two little horns
    hs.ellipse(cv, hx, hy, 3.9, 3.4, WOOL)
    hs.ellipse(cv, hx + 3.0, hy + 1.2, 2.0, 1.6, [K_FACE, K_FACE, K_WM, K_WM])
    cv.set(int(hx + 4.5), int(hy + 1), K_HOOF)                # the nose
    for x, y in ((int(hx) - 3, int(hy) - 1), (int(hx) - 4, int(hy)), (int(hx) - 5, int(hy))):
        cv.set(x, y, K_WD)                                    # the ear
    ix, iy = int(hx), int(hy)
    if pose == "dash":                                        # horns forward: a charge
        horns = ((ix + 1, iy - 4, K_HL), (ix + 2, iy - 4, K_HL), (ix + 3, iy - 4, K_HD), (ix + 4, iy - 3, K_HD),
                 (ix, iy - 3, K_HL))
    else:
        horns = ((ix - 1, iy - 4, K_HL), (ix - 2, iy - 5, K_HL), (ix - 3, iy - 5, K_HD), (ix + 1, iy - 4, K_HL),
                 (ix, iy - 5, K_HL), (ix - 1, iy - 6, K_HD))
    for x, y, c in horns:
        cv.set(x, y, c)
    cv.set(ix + 2, iy + 1, K_PINK)                            # the cheek
    ex, ey = ix + 1, iy - 2                                   # the eye (X when knocked out)
    if pose == "dead":
        for dx, dy in ((0, 0), (2, 0), (1, 1), (0, 2), (2, 2)):
            cv.set(ex + dx - 1, ey + dy, K_INK)
    else:
        cv.rect(ex, ey, ex + 1, ey + 2, K_EYE)
        cv.set(ex + 1, ey + 1, K_INK)
        cv.set(ex + 1, ey + 2, K_INK)
    return cv


def kid_frame(pose, frame=0):
    if pose == "squash":
        cv = hs.squash(kid("idle"), 1.3)
    elif pose == "stretch":
        cv = hs.squash(kid("jump"), 0.8)
    elif pose == "dash_up":
        cv = hs.squash(kid("up"), 0.72)
    else:
        cv = kid(pose, frame)
    cv.outline(K_OUT)
    return cv


# ---- the props ------------------------------------------------------------------------------------------------------
def berry(frame):
    """a wild berry: drupelets lit from the top-left, two leaves on top"""
    cv = Canvas(16, 16)
    for (cx, cy) in ((6, 8), (9, 8), (7.5, 10.5), (5, 11), (10, 11), (7.5, 13)):
        hs.ellipse(cv, cx + 0.5, cy + 0.5, 2.2, 2.2, [O_RL, O_RM, O_RM, O_RD])
    for (x, y) in ((6, 7), (9, 7), (5, 10), (8, 10)):
        cv.set(x, y, O_RL)
    cv.set(5, 7, O_WH)
    lean = 1 if frame else 0
    for x, y in ((7, 5), (8, 5), (6, 4 - lean), (5, 4 - lean), (9, 4 + lean), (10, 4 + lean), (8, 4), (8, 3)):
        cv.set(x, y, O_GM)
    cv.set(5, 3 - lean, O_GL)
    cv.set(10, 3 + lean, O_GL)
    cv.outline(O_OUT)
    return cv


def gem(frame):
    """the dash's refill: a green-cyan crystal; frame 4: the empty place (it comes back)"""
    cv = Canvas(16, 16)
    pts = set()
    for y in range(2, 15):
        half = min((y - 1) if y < 8 else (14 - y), 5) * 0.8
        for x in range(16):
            if abs(x + 0.5 - 8) <= half + 0.3:
                pts.add((x, y))
    if frame == 4:
        edge = sorted(p for p in pts if any((p[0] + dx, p[1] + dy) not in pts for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))))
        for i, p in enumerate(edge):
            if (p[0] + p[1]) % 2 == 0:
                cv.set(p[0], p[1], O_CD)
        return cv
    for x, y in pts:
        c = O_CM
        if x < 8 and y < 8:
            c = O_CL
        elif x >= 8 and y >= 8:
            c = O_CD
        cv.set(x, y, c)
    for y in range(3, 14):
        cv.set(8, y, O_CL if y < 8 else O_CM)
    gl = [(5, 6), (6, 5), (10, 9), (9, 11)][frame % 4]
    cv.set(gl[0], gl[1], O_WH)
    cv.outline(O_OUT)
    return cv


def spring(frame):
    """the spring: a bouncy mushroom on the rock; 0 at rest, 1 pressed, 2 springing"""
    cv = Canvas(16, 16)
    capy = [10, 12, 7][frame]
    for y in range(capy + 2, 16):
        for x in (6, 7, 8, 9):
            cv.set(x, y, O_ML if x < 8 else O_MM)
    cv.set(9, 15, O_MD)
    rx = 6.8 if frame != 1 else 7.5
    ry = 3.4 if frame != 1 else 2.6
    for y in range(capy - 4, capy + 3):
        for x in range(16):
            dx, dy = (x + 0.5 - 8) / rx, (y + 0.5 - (capy + 1)) / ry
            if dx * dx + dy * dy <= 1 and y <= capy + 1:
                c = O_RM
                if dx < -0.2 and dy < -0.1:
                    c = O_RL
                if dy > 0.45:
                    c = O_RD
                cv.set(x, y, c)
    for x, y in ((5, capy - 1), (10, capy), (8, capy - 2)):
        cv.set(x, y, O_WH)
    cv.outline(O_OUT)
    return cv


def plank(part):
    """a moving platform: a wooden raft, 16 x 8 pieces (0 left, 1 middle, 2 right)"""
    cv = Canvas(16, 8)
    for y in range(8):
        for x in range(16):
            c = O_MM
            if y == 0 or y == 7:
                c = O_OUT
            elif y == 1:
                c = O_ML
            elif y == 6:
                c = O_MD
            elif x % 8 == 7:
                c = O_MD
            if (part == 0 and x == 0) or (part == 2 and x == 15):
                c = O_OUT
            cv.set(x, y, c)
    if part == 0:
        cv.set(0, 0, None); cv.set(0, 7, None)
    if part == 2:
        cv.set(15, 0, None); cv.set(15, 7, None)
    for x in (3, 11):
        cv.set(x, 3, O_DK)                                    # the nails
    return cv


def crumble_sprite():
    """the crumbling block as a sprite (it shakes before it falls): the map's tiles, in the props' palette"""
    cv = Canvas(16, 16)
    remap = {P_OUT: O_OUT, P_KD: O_MD, P_KM: O_MM, P_KL: O_ML, P_KH: O_WH}
    for q in range(4):
        t = crumble(q)
        for y in range(8):
            for x in range(8):
                if t[y][x]:
                    cv.set(x + (q % 2) * 8, y + (q // 2) * 8, remap[t[y][x]])
    return cv


def dust(frame):
    cv = Canvas(8, 8)
    r = [1.6, 2.6, 3.2][frame]
    for y in range(8):
        for x in range(8):
            d = math.hypot(x + 0.5 - 4, y + 0.5 - 4.5)
            if d <= r and (frame < 2 or hs.checker(x, y)):
                cv.set(x, y, F_L if d < r - 1 else F_M)
    return cv


def burst(frame):
    """the knock-out: a ring of puffs (drawn 8 times round the kid by draw.c)"""
    cv = Canvas(8, 8)
    r = [3.4, 2.4][frame]
    for y in range(8):
        for x in range(8):
            d = math.hypot(x + 0.5 - 4, y + 0.5 - 4)
            if d <= r:
                cv.set(x, y, F_W if d < r - 1.2 else F_RL if frame == 0 else F_R)
    return cv


def streak(frame):
    """the wind: a thin streak"""
    cv = Canvas(8, 8)
    n = 7 if frame == 0 else 5
    for x in range(n):
        cv.set(x, 3 + (1 if x > 4 and frame else 0), F_SN if x in (0, n - 1) else F_W)
    return cv


def flake(frame):
    """snow (the summit), petals (the meadow): a dot that turns"""
    cv = Canvas(8, 8)
    if frame == 0:
        for x, y in ((3, 3), (4, 3), (3, 4), (4, 4)):
            cv.set(x, y, F_W)
    else:
        for x, y in ((3, 3), (4, 4)):
            cv.set(x, y, F_RL)
        cv.set(4, 3, F_W)
    return cv


def icon(kind):
    """8x8 HUD icons: 0 the knock-outs (a little skull), 1 a berry, 2 the clock"""
    cv = Canvas(8, 8)
    if kind == 0:
        for y in range(1, 6):
            for x in range(1, 7):
                if (x + 0.5 - 4) ** 2 / 7.0 + (y + 0.5 - 3.5) ** 2 / 5.0 <= 1:
                    cv.set(x, y, F_W)
        cv.rect(2, 5, 5, 6, F_W)
        cv.set(2, 3, F_OUT); cv.set(5, 3, F_OUT); cv.set(3, 6, F_OUT); cv.set(4, 6, F_OUT)
    elif kind == 1:
        hs.ellipse(cv, 4, 4.5, 2.8, 2.8, [F_RL, F_R, F_R, F_R])
        cv.set(4, 1, F_G); cv.set(3, 1, F_G); cv.set(5, 2, F_G)
        cv.set(3, 3, F_W)
    else:
        hs.ellipse(cv, 4, 4, 3.0, 3.0, [F_W, F_W, F_L, F_M])
        cv.set(4, 2, F_OUT); cv.set(4, 3, F_OUT); cv.set(5, 4, F_OUT)
    cv.outline(F_OUT)
    return cv


# name, x, y, w, h, frames, OBJ palette (build_assets.py cuts the sheet with this)
SPRITES = [
    ("kid_idle", 0, 0, 24, 24, 2, 0),
    ("kid_run", 48, 0, 24, 24, 6, 0),
    ("kid_jump", 192, 0, 24, 24, 1, 0),
    ("kid_fall", 216, 0, 24, 24, 1, 0),
    ("kid_slide", 0, 24, 24, 24, 1, 0),
    ("kid_dash", 24, 24, 24, 24, 1, 0),
    ("kid_dash_up", 48, 24, 24, 24, 1, 0),
    ("kid_squash", 72, 24, 24, 24, 1, 0),
    ("kid_stretch", 96, 24, 24, 24, 1, 0),
    ("kid_dead", 120, 24, 24, 24, 1, 0),
    ("icon", 144, 24, 8, 8, 3, 5),
    ("berry", 0, 48, 16, 16, 2, 2),
    ("gem", 32, 48, 16, 16, 5, 2),
    ("spring", 112, 48, 16, 16, 3, 2),
    ("crumble", 160, 48, 16, 16, 1, 2),
    ("plank", 176, 48, 16, 8, 3, 2),
    ("dust", 176, 56, 8, 8, 3, 5),
    ("burst", 200, 56, 8, 8, 2, 5),
    ("streak", 216, 56, 8, 8, 2, 5),
    ("flake", 232, 56, 8, 8, 2, 5),
]
SHEET_W, SHEET_H = 256, 64


def sprite_frame(name, i):
    if name.startswith("kid_"):
        return kid_frame(name[4:], i)
    return {"berry": berry, "gem": gem, "spring": spring, "crumble": lambda f: crumble_sprite(), "plank": plank,
            "dust": dust, "burst": burst, "streak": streak, "flake": flake, "icon": icon}[name](i)


def build_sheet():
    sheet = Canvas(SHEET_W, SHEET_H)
    for name, x, y, w, hh, n, _p in SPRITES:
        for i in range(n):
            sheet.paste(sprite_frame(name, i), x + i * w, y)
    return sheet


# ---- the far and middle layers ---------------------------------------------------------------------------------------
FAR_W, FAR_H = 256, 96            # BG4: the far mountains (palette 7): 1 face, 2 shade, 3 snow, 4 haze
MID_W, MID_H = 512, 112           # BG3: the middle ground (palette 6): 1 light, 2 mid, 3 dark, 4 highlight, 5 deep


def ridge(x, w, seed, base, amp, k=3):
    """a wrapping ridge line: the sum of a few sines (seamless over w)"""
    rng = random.Random(seed)
    v = base
    for i in range(k):
        f = (i + 1) * (1 + rng.randint(0, 2))
        v += amp / (i + 1) * math.sin(2 * math.pi * f * x / w + rng.random() * 6.28)
    return v


def far(ch):
    cv = Canvas(FAR_W, FAR_H)
    if ch == 2:                                             # the cave: the far wall, stalactites from the top
        for x in range(FAR_W):
            top = ridge(x, FAR_W, 31, 34, 14)
            drip = 10 + (h(x // 6, 5) % 30 if (x // 6) % 3 == 0 else 0) - abs((x % 6) - 2.5) * 3
            for y in range(FAR_H):
                if y > top:
                    c = 1 if not (y > top + 24 and hs.dither_ok(x, y, min(16, int(y - top - 24) // 2))) else 4
                    cv.set(x, y, c)
                elif y < drip:
                    cv.set(x, y, 2)
        return cv
    for x in range(FAR_W):
        top = ridge(x, FAR_W, 11 + ch, 40, 18)
        shaded = ridge(x + 1, FAR_W, 11 + ch, 40, 18) < top - 0.1
        for y in range(int(top), FAR_H):
            c = 2 if shaded else 1
            if y < top + 4 + (h(x, ch) % 3):
                c = 3 if not shaded else 4                  # snow on the crests (in shade: the haze's colour)
            if y > FAR_H - 34 and hs.dither_ok(x, y, min(16, (y - (FAR_H - 34)) // 2)):
                c = 4
            cv.set(x, y, c)
    return cv


def mid(ch):
    cv = Canvas(MID_W, MID_H)
    rng = random.Random(100 + ch)
    for x in range(MID_W):
        top = ridge(x, MID_W, 21 + ch, 60, 16, 4)
        for y in range(int(top), MID_H):
            c = 2
            if y < top + 2:
                c = 4
            elif y < top + 7:
                c = 1
            if y > top + 30:
                c = 3
            if y > top + 52:
                c = 5
            cv.set(x, y, c)
    # the chapter's things on the ridge: firs (1, 2, 5), crystals (3), stone spires (4)
    n = 26 if ch in (0, 1) else 14
    for i in range(n):
        x0 = int(i * MID_W / n + rng.randint(-6, 6)) % MID_W
        top = int(ridge(x0, MID_W, 21 + ch, 60, 16, 4))
        hgt = rng.randint(18, 34) if ch != 2 else rng.randint(16, 40)
        for dy in range(hgt):
            y = top - dy + 2
            if ch in (0, 1, 4):                             # a fir: a stack of widening rows
                half = int((hgt - dy) * 0.32) + 1
                if dy % 6 == 5:
                    half -= 1
                for dx in range(-half, half + 1):
                    c = 3 if dx > 0 else 2
                    if ch == 4 and (dy % 6 in (0, 1)):
                        c = 4
                    if ch == 0 and dx < -half + 2 and dy % 6 < 3:
                        c = 1
                    cv.set((x0 + dx) % MID_W, y, c)
            elif ch == 2:                                   # a crystal: a glowing column
                half = 2 if dy < hgt - 4 else 1
                for dx in range(-half, half + 1):
                    cv.set((x0 + dx) % MID_W, y, 4 if dx < 0 else 3)
            else:                                           # a stone spire
                half = max(1, int((hgt - dy) * 0.18) + 1)
                for dx in range(-half, half + 1):
                    cv.set((x0 + dx) % MID_W, y, 1 if dx < 0 else 3)
    return cv


# ---- writing --------------------------------------------------------------------------------------------------------
def to_p(cv, pal15):
    """a P-mode image of an index canvas (index 0: clear, magenta in viewers)"""
    im = Image.new("P", (cv.w, cv.h), 0)
    flat = [255, 0, 255]
    for c in pal15[:15]:
        flat += list(c)
    flat += [0, 0, 0] * (256 - len(flat) // 3)
    im.putpalette(flat)
    im.putdata([v if v else 0 for row in cv.p for v in row])
    return im


def to_rgb(cv, pal15, scale=1, bg=(255, 0, 255)):
    im = Image.new("RGB", (cv.w, cv.h), bg)
    px = im.load()
    for y in range(cv.h):
        for x in range(cv.w):
            v = cv.p[y][x]
            if v:
                px[x, y] = tuple(pal15[v - 1])
    return im.resize((cv.w * scale, cv.h * scale), Image.NEAREST) if scale > 1 else im


def tiles_canvas(tiles):
    cols = 16
    cv = Canvas(cols * 8, ((len(tiles) + cols - 1) // cols) * 8)
    for i, t in enumerate(tiles):
        for y in range(8):
            for x in range(8):
                if t[y][x]:
                    cv.set((i % cols) * 8 + x, (i // cols) * 8 + y, t[y][x])
    return cv


def palettes():
    return {"terrain": TERRAIN, "props": PROPS, "kid": KID, "kid_nodash": KID_NODASH, "trail": TRAIL, "prop": PROP,
            "fx": FX, "sky": SKY, "far": FAR, "mid": MID}


def write_all(out):
    os.makedirs(os.path.join(out, "preview"), exist_ok=True)
    tiles, names = build_tiles()
    tc = tiles_canvas(tiles)
    to_p(tc, TERRAIN[0]).save(os.path.join(out, "terrain.png"))
    for ch in range(CHAPTERS):
        cv = Canvas(tc.w, tc.h)
        prev = Image.new("RGB", (tc.w, tc.h), (255, 0, 255))
        for i, t in enumerate(tiles):
            pal = PROPS if tile_palette(names[i]) == 2 else TERRAIN[ch]
            one = Canvas(8, 8)
            one.p = [row[:] for row in t]
            prev.paste(to_rgb(one, pal, bg=SKY[ch][1]), ((i % 16) * 8, (i // 16) * 8))
        del cv
        prev.resize((tc.w * 3, tc.h * 3), Image.NEAREST).save(os.path.join(out, "preview", "terrain_ch%d.png" % (ch + 1)))
        to_p(far(ch), FAR[ch]).save(os.path.join(out, "far_ch%d.png" % (ch + 1)))
        to_p(mid(ch), MID[ch]).save(os.path.join(out, "mid_ch%d.png" % (ch + 1)))
    sheet = build_sheet()
    to_p(sheet, KID).save(os.path.join(out, "sprites.png"))
    pals = {0: KID, 2: PROP, 5: FX}
    prev = Image.new("RGB", (SHEET_W, SHEET_H), (64, 80, 120))
    for name, x, y, w, hh, n, p in SPRITES:
        for i in range(n):
            fr = sprite_frame(name, i)
            for yy in range(hh):
                for xx in range(w):
                    v = fr.p[yy][xx]
                    if v:
                        prev.putpixel((x + i * w + xx, y + yy), tuple(pals[p][v - 1]))
    prev.resize((SHEET_W * 3, SHEET_H * 3), Image.NEAREST).save(os.path.join(out, "preview", "sprites.png"))
    with open(os.path.join(out, "palettes.json"), "w") as f:
        json.dump(palettes(), f, indent=1)


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(GAME, "art"))
    a = ap.parse_args(argv)
    write_all(a.out)
    print("art written to %s" % a.out)


if __name__ == "__main__":
    main()

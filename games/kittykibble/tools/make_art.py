#!/usr/bin/env python3
"""Kitty Kibble: the code-drawn art, in the 8BCraft house style (docs/art-direction.md).

Code-drawn art only, no image generation: every sprite, tile and panorama is drawn here with
games/common/tools/house_style.py (1-px outlines, flat shades lit from the top-left). The sheet layout (SPRITES),
the rooms (ROOMS) and the wall tiles' masks (BLOB) are shared with build_assets.py.

Writes, in the art folder (default games/kittykibble/art):
  sprites.png (256x96): the kitten (ginger; players 2-4 recolour its fur and collar: silver, snow white,
      chocolate) walking right, down and up, munching, dizzy, its icon (OBJ 0); the balls of yarn, the fish treat,
      a caught vacuum's eyes zipping home, dust, small digits, sparkles and dizzy stars (OBJ 2); the four robot
      vacuums (Dash red, Sneak violet, Patrol teal, Wobble yellow: side, front and back, two frames each) and the
      scared one (OBJ 6; OBJ 7 its flash)
  tiles/walls.png (376x40): one row per room, the 47 shapes of a wall cell (by its 8 neighbours: the furniture's
      rounded blocks, lit from the top-left, each room its material: kitchen counters, plum sofas, quilts, hedges,
      cardboard boxes)
  tiles/items.png (32x8): the kibble (two kinds), the dock's gate and its charging floor (BG2)
  tiles/<room>.png (576x240): each room around the maze: the floor, the back wall with its windows, furniture seen
      from above in the margins (more of it on wide screens); x 0 is stage x -128 (BG3)

    python3 games/kittykibble/tools/make_art.py [--out DIR] [--preview]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/kittykibble/LICENSE.
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
SHEET_W, SHEET_H = 256, 96
# name, x, y, w, h, frames, palette group (hero: OBJ 0, players 2-4 = OBJ 1, 4, 5 recoloured; props: OBJ 2;
# vac: OBJ 6, its flash OBJ 7)
SPRITES = [
    ("kit_side", 0, 0, 16, 16, 3, "hero"),      # walking right (flipped: left): two steps, a munch
    ("kit_down", 48, 0, 16, 16, 3, "hero"),     # walking down (its face): two steps, a munch
    ("kit_up", 96, 0, 16, 16, 2, "hero"),       # walking up (its back): two steps
    ("kit_dizzy", 128, 0, 16, 16, 2, "hero"),   # caught: spinning, dizzy
    ("kit_icon", 160, 0, 16, 16, 1, "hero"),    # the title's player slots, the results
    ("yarn", 0, 16, 16, 16, 2, "props"),        # a ball of yarn (it pulses)
    ("fish", 32, 16, 16, 16, 2, "props"),       # the fish treat (its tail flaps)
    ("eyes", 64, 16, 16, 16, 4, "props"),       # a caught vacuum zipping home: right, down, left, up
    ("dust", 128, 16, 16, 16, 2, "props"),      # its puff of dust
    ("digit", 0, 32, 8, 8, 10, "props"),        # small digits (the points)
    ("sparkle", 80, 32, 8, 8, 2, "props"),
    ("star", 96, 32, 8, 8, 2, "props"),         # dizzy stars
    ("dash", 0, 48, 16, 16, 6, "vac"),          # side (flipped: left) x2, front x2, back x2
    ("sneak", 96, 48, 16, 16, 6, "vac"),
    ("patrol", 0, 64, 16, 16, 6, "vac"),
    ("wobble", 96, 64, 16, 16, 6, "vac"),
    ("scared", 0, 80, 16, 16, 2, "vac"),        # yarn time: blue and trembling
]
ROOMS = ["kitchen", "living", "bedroom", "garden", "attic"]
PANO_W, PANO_H, PANO_X0 = 576, 240, -128
MAZE_X, MAZE_Y, MAZE_W, MAZE_H, CELL = 52, 28, 27, 24, 8

# ---- colours ------------------------------------------------------------------------------------------------------
WHITE = (250, 250, 250)
INK = hs.HOUSE["ink"][0]
# the kitten (each group fits one 15-colour sprite palette): fur (swapped for players 2-4), cream, pink, the collar
OUTK = (54, 26, 40)
FUR = [(255, 214, 150), (246, 168, 82), (214, 118, 52), (156, 74, 36)]           # P1 ginger
FUR_P = {1: [(236, 240, 250), (190, 198, 218), (140, 148, 176), (96, 102, 132)],  # P2 silver
         2: [(255, 255, 250), (240, 236, 226), (210, 202, 192), (160, 150, 146)],  # P3 snow white
         3: [(170, 126, 104), (126, 88, 70), (90, 60, 50), (62, 40, 38)]}          # P4 chocolate
CREAM = [(255, 246, 228), (238, 216, 190)]
PINK = (255, 150, 164)
COLLAR = (232, 64, 72)                                     # P1 red ...
COLLAR_P = {1: (72, 142, 238), 2: (86, 192, 96), 3: (178, 100, 224)}   # ... P2 blue, P3 green, P4 purple
BELL = (250, 210, 90)
# the props
OUTP = (36, 22, 44)
YARN = [(255, 196, 218), (246, 124, 170), (196, 66, 120)]
FISHC = [(220, 240, 255), (134, 186, 232), (72, 114, 180)]
GOLD = [(255, 232, 124), (232, 168, 40)]
DUST = [(212, 206, 202), (150, 144, 148)]
# the vacuums (all in one palette: OBJ 6)
OUTV = (28, 22, 48)
GREY = [(208, 214, 228), (134, 140, 162)]
VCOL = {"dash": [(255, 122, 106), (206, 62, 66)], "sneak": [(198, 152, 255), (128, 88, 208)],
        "patrol": [(112, 222, 206), (38, 152, 150)], "wobble": [(255, 222, 98), (218, 160, 36)]}
BLUE = [(122, 152, 255), (64, 84, 202)]
FLASH = [(250, 250, 250), (212, 216, 238)]                 # OBJ 7: BLUE -> FLASH, WHITE -> the red (its face)
# BG2: the kibble, the gate, the dock's floor (one palette for every room)
KIB_OUT = (92, 48, 30)
KIB = [(246, 200, 120), (212, 142, 66), (156, 92, 42)]
GATE = [(250, 250, 250), (176, 184, 206)]
DOCKF = [(62, 68, 98), (90, 98, 132)]
LED = (112, 232, 146)
ITEM_COLOURS = [KIB_OUT] + KIB + GATE + DOCKF + [LED]
# the walls: outline, front (dark), body, light, highlight, texture A, texture B
WALLS = {
    "kitchen": [(40, 56, 72), (104, 146, 158), (182, 222, 220), (226, 246, 240), (255, 255, 255), (146, 194, 196),
                (206, 238, 232)],
    "living": [(52, 22, 48), (122, 52, 98), (178, 88, 142), (216, 132, 178), (246, 190, 218), (146, 66, 118),
               (204, 112, 162)],
    "bedroom": [(40, 44, 92), (98, 110, 184), (162, 178, 238), (204, 214, 250), (246, 248, 255), (130, 146, 214),
                (250, 206, 226)],
    "garden": [(20, 48, 30), (40, 98, 52), (78, 152, 72), (132, 198, 98), (190, 228, 132), (54, 120, 58),
               (164, 216, 112)],
    "attic": [(58, 38, 24), (132, 94, 56), (198, 152, 100), (228, 192, 138), (248, 226, 180), (168, 124, 76),
              (236, 220, 176)],
}
W_OUT, W_FRONT, W_BODY, W_LIGHT, W_HI, W_TA, W_TB = range(1, 8)


def dist(x, y, cx, cy):
    return math.hypot(x + 0.5 - cx, y + 0.5 - cy)


def disc(cv, cx, cy, r, col):
    for y in range(int(cy - r) - 1, int(cy + r) + 2):
        for x in range(int(cx - r) - 1, int(cx + r) + 2):
            if dist(x, y, cx, cy) <= r:
                cv.set(x, y, col)


def oval(cv, cx, cy, rx, ry, col):
    hs.ellipse(cv, cx, cy, rx, ry, None, flat=col)


def tri(cv, pts, col):
    """A filled triangle (pixel centres inside)."""
    (ax, ay), (bx, by), (cx, cy) = pts
    for y in range(int(min(ay, by, cy)) - 1, int(max(ay, by, cy)) + 2):
        for x in range(int(min(ax, bx, cx)) - 1, int(max(ax, bx, cx)) + 2):
            px, py = x + 0.5, y + 0.5
            d1 = (px - bx) * (ay - by) - (ax - bx) * (py - by)
            d2 = (px - cx) * (by - cy) - (bx - cx) * (py - cy)
            d3 = (px - ax) * (cy - ay) - (cx - ax) * (py - ay)
            neg = d1 < 0 or d2 < 0 or d3 < 0
            pos = d1 > 0 or d2 > 0 or d3 > 0
            if not (neg and pos):
                cv.set(x, y, col)


def rect(cv, x0, y0, x1, y1, col):
    cv.rect(int(x0), int(y0), int(x1), int(y1), col)


def ramp4(c2):
    """A 2-shade ramp as the 4 shades of hs.ellipse (light, light, dark, dark)."""
    return [c2[0], c2[0], c2[1], c2[1]]


# ---- the kitten -----------------------------------------------------------------------------------------------------
def ears(cv, cx, top, spread, inner=True, back=False):
    for s in (-1, 1):
        x0 = cx + s * spread
        tri(cv, [(x0 - 1.9, top + 3.2), (x0 + 0.2 * s, top), (x0 + 1.9, top + 3.2)], FUR[2] if back else FUR[1])
        if inner and not back:
            tri(cv, [(x0 - 0.8, top + 3.0), (x0 + 0.2 * s, top + 1.3), (x0 + 0.8, top + 3.0)], PINK)


def kitten_side(frame):
    """Walking right: frames 0, 1 the steps, 2 a munch (the mouth open)."""
    cv = Canvas(16, 16)
    sway = 0.7 if frame == 1 else 0.0
    hs.capsule(cv, 3.6, 10.0, 1.6 - sway, 4.6, 1.05, [FUR[1], FUR[1], FUR[2], FUR[2]])    # the tail, up
    step = frame % 2
    for lx, dy in ((4, step), (6, 1 - step), (9, step), (11, 1 - step)):              # the legs
        rect(cv, lx, 12, lx + 1, 13 + dy, FUR[2])
        cv.set(lx, 13 + dy, CREAM[1]), cv.set(lx + 1, 13 + dy, CREAM[1])
    hs.ellipse(cv, 7.6, 10.2, 4.8, 3.0, FUR)                                           # the body
    oval(cv, 8.4, 11.6, 3.0, 1.0, CREAM[1])
    for x in (6, 8):                                                                    # tabby stripes
        cv.set(x, 8, FUR[2]), cv.set(x, 9, FUR[2])
    ears(cv, 11.4, 1.6, 1.8)
    hs.ellipse(cv, 11.4, 6.6, 3.8, 3.3, FUR)                                           # the head
    oval(cv, 13.0, 7.9, 1.8, 1.3, CREAM[0])                                            # the muzzle
    cv.set(10, 3, FUR[2]), cv.set(11, 3, FUR[2])
    cv.rect(11, 4, 12, 6, WHITE)                                                        # the eye, looking ahead
    cv.set(12, 5, INK), cv.set(12, 6, INK)
    cv.set(14, 7, PINK)                                                                 # the nose
    if frame == 2:                                                                      # munch!
        cv.set(13, 9, INK), cv.set(14, 9, INK), cv.set(14, 8, INK)
        cv.set(13, 10, PINK)
    for y in (8, 9, 10):                                                                # the collar and its bell
        cv.set(9, y, COLLAR)
    cv.set(10, 10, BELL)
    cv.outline(OUTK)
    return cv


def kitten_front(frame, face="walk"):
    """Walking down (its face): frames 0, 1 the steps, 2 a munch; face 'dizzy': X eyes."""
    cv = Canvas(16, 16)
    sway = 0.8 if frame == 1 else 0.0
    hs.capsule(cv, 11.0, 12.0, 13.6 + sway, 7.4, 1.0, [FUR[1], FUR[1], FUR[2], FUR[2]])   # the tail, behind
    hs.ellipse(cv, 8.0, 11.3, 4.3, 3.4, FUR)                                                # the body
    oval(cv, 8.0, 12.2, 2.2, 1.9, CREAM[1])
    up = frame % 2
    oval(cv, 6.0, 14.2 - up, 1.4, 1.0, CREAM[0])                                            # the front paws
    oval(cv, 10.0, 13.2 + up, 1.4, 1.0, CREAM[0])
    ears(cv, 8.0, 0.8, 3.0)
    hs.ellipse(cv, 8.0, 6.2, 4.7, 4.0, FUR)                                                 # the head
    cv.set(7, 2, FUR[2]), cv.set(8, 2, FUR[2]), cv.set(7, 3, FUR[2]), cv.set(8, 3, FUR[2])
    oval(cv, 8.0, 8.2, 2.3, 1.4, CREAM[0])                                                  # the muzzle
    if face == "dizzy":
        for ex in (4, 9):
            for d in range(3):
                cv.set(ex + d, 4 + d, INK), cv.set(ex + 2 - d, 4 + d, INK)
    else:
        for ex in (5, 9):
            cv.rect(ex, 4, ex + 1, 6, WHITE)
        for px in (6, 9):                                                                    # pupils: down-inside
            cv.set(px, 5, INK), cv.set(px, 6, INK)
    cv.set(7, 7, PINK), cv.set(8, 7, PINK)                                                  # the nose
    if frame == 2 or face == "dizzy":
        cv.set(7, 9, INK), cv.set(8, 9, INK)
        if frame == 2:
            cv.set(7, 10, PINK)
    for x in range(6, 11):                                                                  # the collar
        cv.set(x, 10, COLLAR)
    cv.set(8, 11, BELL)
    cv.outline(OUTK)
    return cv


def kitten_back(frame):
    """Walking up (its back): two steps, the tail swishing."""
    cv = Canvas(16, 16)
    step = frame % 2
    oval(cv, 6.0, 14.0 + step * 0.6, 1.3, 1.0, FUR[2])                                      # the hind paws
    oval(cv, 10.0, 14.6 - step * 0.6, 1.3, 1.0, FUR[2])
    hs.ellipse(cv, 8.0, 11.2, 4.2, 3.5, FUR)                                                # the body
    for y in (9, 11):
        cv.set(7, y, FUR[2]), cv.set(8, y, FUR[2])
    hs.ellipse(cv, 8.0, 6.4, 4.6, 3.7, FUR)                                                 # the back of the head
    ears(cv, 8.0, 0.6, 3.0, back=True)
    cv.set(7, 3, FUR[2]), cv.set(8, 3, FUR[2]), cv.set(8, 5, FUR[2]), cv.set(7, 5, FUR[2])
    for x in range(5, 12):                                                                  # the collar
        cv.set(x, 9, COLLAR)
    tip = 11.5 if frame else 4.5                                                            # the tail, up
    hs.capsule(cv, 8.0, 13.0, tip, 9.0, 1.05, [FUR[1], FUR[1], FUR[2], FUR[2]])
    cv.outline(OUTK)
    return cv


def kitten_dizzy(frame):
    cv = kitten_front(0, "dizzy")
    return hs.flip_h(cv) if frame else cv


def kitten_icon():
    """The 16x16 face of the title's player slots and the results (the same colours: the players' swaps apply)."""
    cv = Canvas(16, 16)
    ears(cv, 8.0, 0.6, 4.2)
    hs.ellipse(cv, 8.0, 8.6, 6.4, 5.4, FUR)
    for x in (7, 8):
        cv.set(x, 4, FUR[2]), cv.set(x, 5, FUR[2])
    oval(cv, 8.0, 11.2, 3.0, 1.8, CREAM[0])
    for ex in (4, 10):
        cv.rect(ex, 7, ex + 1, 9, WHITE)
        cv.set(ex + 1, 8, INK), cv.set(ex + 1, 9, INK)
    cv.set(7, 10, PINK), cv.set(8, 10, PINK)
    for x in range(5, 11):
        cv.set(x, 14, COLLAR)
    cv.set(8, 15, BELL)
    cv.outline(OUTK)
    return cv


# ---- the props ------------------------------------------------------------------------------------------------------
def yarn(frame):
    cv = Canvas(16, 16)
    r = 5.4 if frame == 0 else 5.9
    hs.ellipse(cv, 8.0, 8.4, r, r, [YARN[0], YARN[1], YARN[1], YARN[2]])
    for k in range(-2, 3):                                     # the wraps: curved strands across the ball
        for t in range(-6, 7):
            a = t / 6.0
            x = 8.0 + a * r * 0.9
            y = 8.4 + k * 2.0 + (a * a) * 2.2 - 1.0
            px, py = int(x), int(y)
            if dist(px, py, 8.0, 8.4) < r - 0.6:
                cv.set(px, py, YARN[2] if k % 2 else YARN[0])
    hs.capsule(cv, 12.6, 12.0, 14.6, 14.4, 0.5, None, flat=YARN[1])     # the loose end
    cv.set(5, 5, WHITE)
    cv.outline(OUTP)
    return cv


def fish(frame):
    cv = Canvas(16, 16)
    tail = 1 if frame else 0
    tri(cv, [(1.0, 5.0 - tail), (4.6, 8.5), (1.0, 12.0 + tail)], GOLD[1])           # the tail
    hs.ellipse(cv, 8.4, 8.5, 5.4, 3.6, [FISHC[0], FISHC[1], FISHC[1], FISHC[2]])  # the body
    tri(cv, [(7.0, 5.4), (9.2, 2.6), (10.6, 5.6)], GOLD[0])                         # the fin
    for x in (6, 8):
        cv.set(x, 9, FISHC[2]), cv.set(x + 1, 10, FISHC[2])
    cv.rect(11, 7, 12, 8, WHITE)
    cv.set(12, 8, INK)
    cv.set(13, 10, FISHC[2])
    cv.outline(OUTP)
    return cv


def zip_eyes(d):
    """A caught vacuum zipping home: its two eyes (looking where it goes) and speed lines behind."""
    cv = Canvas(16, 16)
    dx, dy = [(1, 0), (0, 1), (-1, 0), (0, -1)][d]
    for ex in (4.5, 10.5):
        disc(cv, ex, 7.5, 2.3, WHITE)
        cv.set(int(ex + dx * 1.2), int(7.5 + dy * 1.2), INK)
    for k in range(3):                                         # speed lines, behind
        if dx:
            y = 4 + k * 4
            for i in range(3):
                cv.set(int(8 - dx * (6 + i)) % 16, y, DUST[1])
        else:
            x = 3 + k * 5
            for i in range(2):
                cv.set(x, int(7.5 - dy * (5 + i)), DUST[1])
    cv.outline(OUTP)
    return cv


def dust(frame):
    cv = Canvas(16, 16)
    rr = random.Random(5 + frame)
    for _ in range(5):
        cx, cy = rr.uniform(4, 12), rr.uniform(5, 12)
        r = rr.uniform(1.6, 2.8) * (1.0 if frame == 0 else 0.7)
        hs.ellipse(cv, cx, cy, r, r, [DUST[0], DUST[0], DUST[1], DUST[1]])
    if frame:
        for x, y in ((2, 3), (13, 4), (3, 13), (13, 13)):
            cv.set(x, y, DUST[0])
    cv.outline(OUTP)
    return cv


SMALL_DIGITS = ["111101101101111", "010110010010111", "111001111100111", "111001011001111", "101101111001001",
                "111100111001111", "111100111101111", "111001010010010", "111101111101111", "111101111001111"]


def small_digit(n):
    cv = Canvas(8, 8)
    g = SMALL_DIGITS[n]
    for i, ch in enumerate(g):
        if ch == "1":
            cv.set(2 + i % 3, 1 + i // 3, WHITE)
    cv.outline(OUTP, diag=True)
    return cv


def sparkle(frame):
    cv = Canvas(8, 8)
    r = 3 if frame == 0 else 2
    for i in range(-r, r + 1):
        cv.set(4 + i, 4, GOLD[0]), cv.set(4, 4 + i, GOLD[0])
    cv.set(4, 4, WHITE)
    return cv


def star(frame):
    cv = Canvas(8, 8)
    pts = []
    for k in range(10):
        a = -math.pi / 2 + k * math.pi / 5 + (0.3 if frame else 0.0)
        r = 3.2 if k % 2 == 0 else 1.4
        pts.append((4.0 + r * math.cos(a), 4.0 + r * math.sin(a)))
    for k in range(10):
        tri(cv, [(4.0, 4.0), pts[k], pts[(k + 1) % 10]], GOLD[0])
    cv.set(3, 3, WHITE)
    cv.outline(OUTP)
    return cv


# ---- the robot vacuums ----------------------------------------------------------------------------------------------
def vacuum(kind, view, frame):
    """A round robot vacuum (3/4 view, 16x16): view 'side' (going right), 'front' (coming down), 'back' (going up);
    frame: its side brushes spin. kind: dash, sneak, patrol, wobble or scared. A shell lit from the top-left, a grey
    bumper on its rim, a dark visor with two bright eyes looking where it goes, and each one's own mark."""
    cv = Canvas(16, 16)
    col = BLUE if kind == "scared" else VCOL[kind]
    ox = frame if kind == "scared" else 0
    cx, cy, rx, ry = 8.0 + ox, 9.0, 6.9, 5.4
    for y in range(16):
        for x in range(16):
            dx, dy = (x + 0.5 - cx) / rx, (y + 0.5 - cy) / ry
            if dx * dx + dy * dy > 1.0:
                continue
            c = col[0] if dy < 0.40 else col[1]
            front = (view == "front" and abs(dx) < 0.82) or (view == "side" and dx > 0.05) or \
                    (view == "back" and abs(dx) < 0.35) or kind == "scared"
            if 0.40 <= dy < 0.68 and front:
                c = GREY[0] if dy < 0.56 else GREY[1]
            cv.set(x, y, c)
    cv.set(int(cx) - 4, 5, WHITE), cv.set(int(cx) - 5, 6, WHITE)          # the shine, top-left
    for bx in ((2, 13) if view != "side" else (13,)):                     # the side brushes, spinning
        if frame == 0:
            cv.set(bx - 1 + ox, 14, GREY[1]), cv.set(bx + 1 + ox, 14, GREY[1]), cv.set(bx + ox, 15, GREY[1])
        else:
            cv.set(bx - 1 + ox, 15, GREY[1]), cv.set(bx + 1 + ox, 15, GREY[1]), cv.set(bx + ox, 14, GREY[1])
    if kind == "scared":                                                  # big worried eyes, a wobbly mouth
        for ex in (5.4, 10.6):
            disc(cv, ex + ox, 6.8, 1.9, WHITE)
            cv.set(int(ex + ox), 7 - frame, INK)
        for i, x in enumerate(range(4, 13)):
            cv.set(x + ox, 11 + (i % 2), WHITE)
        cv.outline(OUTV)
        return cv
    if view == "back":                                                    # the dust bin's vent, a status light
        for x in range(5, 11):
            cv.set(x, 7, GREY[1] if x % 2 else GREY[0])
        cv.set(8, 9, VCOL["patrol"][0]), cv.set(7, 9, VCOL["patrol"][0])
    else:
        vx = 10.6 if view == "side" else 8.0
        oval(cv, vx, 8.0, 3.9 if view == "side" else 4.8, 2.3, OUTV)      # the visor
        exs = (9, 12) if view == "side" else (5, 9)
        for i, ex in enumerate(exs):
            top = 7
            h = 2 if kind == "sneak" else 3                               # Sneak: half-closed, sly
            if kind == "sneak":
                top = 8
            cv.rect(ex, top, ex + 1, top + h - 1 if h > 1 else top, WHITE)
            if kind == "wobble" and i == 1:                               # Wobble: one googly eye
                cv.rect(ex, 6, ex + 2, 9, WHITE)
                cv.set(ex + 2 if view == "side" else ex + 1, 9, INK)
                continue
            if view == "side":
                cv.set(ex + 1, top + h - 2 if h > 1 else top, INK), cv.set(ex + 1, top + h - 1, INK)
            else:
                cv.set(ex + (1 if i == 0 else 0), top + h - 1, INK)
                if h == 3:
                    cv.set(ex + (1 if i == 0 else 0), top + 1, INK)
            if kind == "dash":                                            # Dash: angry brows, in a hurry
                cv.set(ex + (1 if (i == 0 and view == "front") else 0), top, OUTV)
    if kind == "dash":                                                    # a white lightning bolt on its shell
        bx = 3 if view != "back" else 7
        for x, y in ((bx + 2, 4), (bx + 1, 5), (bx, 6), (bx + 1, 6), (bx + 2, 6), (bx + 1, 7), (bx, 8)):
            if cv.get(x, y) is not None:
                cv.set(x, y, WHITE)
    elif kind == "patrol":                                                # a siren on top: red, then yellow
        c = VCOL["dash"][0] if frame == 0 else VCOL["wobble"][0]
        cv.rect(7, 1, 8, 2, c)
        cv.set(7, 1, WHITE)
        cv.rect(6, 3, 9, 3, GREY[1])
    elif kind == "wobble":                                                # a springy antenna with a ball
        ax = 8 + (1 if frame else -1)
        cv.set(8, 3, GREY[1]), cv.set(8 + (1 if frame else -1) // 2, 2, GREY[1])
        disc(cv, ax + 0.5, 1.4, 1.2, VCOL["dash"][0])
    elif kind == "sneak":                                                 # a ninja band's knot behind
        kx = 2 if view == "side" else 13
        if view != "back":
            cv.set(kx, 6, VCOL["sneak"][1]), cv.set(kx + (-1 if view == "side" else 1), 5, VCOL["sneak"][1])
    cv.outline(OUTV)
    return cv


# ---- the walls: 47 shapes of a wall cell by its 8 neighbours --------------------------------------------------------
N_, NE, E_, SE, S_, SW, W_, NW = 1, 2, 4, 8, 16, 32, 64, 128
NB = {N_: (0, -1), NE: (1, -1), E_: (1, 0), SE: (1, 1), S_: (0, 1), SW: (-1, 1), W_: (-1, 0), NW: (-1, -1)}


def canon(m):
    """The mask with the diagonals that cannot change the cell's shape cleared (a corner counts only between two
    walls): 256 masks -> the 47 shapes."""
    if not (m & N_ and m & E_):
        m &= ~NE
    if not (m & S_ and m & E_):
        m &= ~SE
    if not (m & S_ and m & W_):
        m &= ~SW
    if not (m & N_ and m & W_):
        m &= ~NW
    return m


BLOB = sorted(set(canon(m) for m in range(256)))
INSET, ROUND = 1, 2.6


def wall_solid(mask, x, y):
    """Is the pixel (x, y) (tile coordinates, -8..15) inside the wall? The wall cells' squares, kept INSET px away
    from every corridor cell, the convex corners rounded."""
    cx, cy = x // 8, y // 8

    def wall(dx, dy):
        if dx == 0 and dy == 0:
            return True
        for b, (bx, by) in NB.items():
            if (bx, by) == (dx, dy):
                return bool(mask & b)
        return False
    if not wall(cx, cy):
        return False
    px, py = x + 0.5, y + 0.5
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            if wall(dx, dy):
                continue
            qx = min(max(px, dx * 8), dx * 8 + 8)
            qy = min(max(py, dy * 8), dy * 8 + 8)
            if math.hypot(px - qx, py - qy) < INSET:
                return False
    if cx == 0 and cy == 0:                                    # round the cell's convex corners
        for sx, sy in ((-1, -1), (1, -1), (-1, 1), (1, 1)):
            if wall(sx, 0) or wall(0, sy):
                continue
            ccx = INSET + ROUND if sx < 0 else 8 - INSET - ROUND
            ccy = INSET + ROUND if sy < 0 else 8 - INSET - ROUND
            if (px - ccx) * sx > 0 and (py - ccy) * sy > 0 and math.hypot(px - ccx, py - ccy) > ROUND:
                return False
    return True


def wall_texture(room, x, y):
    if room == "kitchen":                                      # tiled counters: grout lines, a shine
        if x % 4 == 3 or y % 4 == 3:
            return W_TA
        return W_TB if (x % 4 == 0 and y % 4 == 0) else W_BODY
    if room == "living":                                       # tufted sofas: buttons
        if x % 4 == 1 and y % 4 == 1:
            return W_TA
        return W_TB if (x % 4 == 0 and y % 4 == 0) else W_BODY
    if room == "bedroom":                                      # quilts: diagonal stitches, little hearts
        if (x + y) % 8 == 0 or (x - y) % 8 == 4:
            return W_TA
        return W_TB if (x % 8 == 4 and y % 8 == 0) else W_BODY
    if room == "garden":                                       # hedges: leaves
        h = (x * 5 + y * 3) % 7
        return W_TA if h == 0 else W_TB if h == 4 else W_BODY
    if x % 8 in (3, 4):                                        # attic: cardboard boxes, taped
        return W_TB
    return W_TA if (y % 8 == 6 and x % 2 == 0) else W_BODY


def wall_tile(room, mask):
    """8x8 palette indices (0 = see-through) of a wall cell with neighbours `mask`, in room's material."""
    px = []
    for y in range(8):
        for x in range(8):
            if wall_solid(mask, x, y):
                if not wall_solid(mask, x, y + 1) or not wall_solid(mask, x, y + 2):
                    v = W_FRONT                                # the front face, toward the viewer
                elif not wall_solid(mask, x, y - 1):
                    v = W_HI if not wall_solid(mask, x - 1, y - 1) else W_LIGHT
                elif not wall_solid(mask, x - 1, y):
                    v = W_LIGHT
                elif not wall_solid(mask, x + 1, y):
                    v = W_TA
                else:
                    v = wall_texture(room, x, y)
            elif any(wall_solid(mask, x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                v = W_OUT
            else:
                v = 0
            px.append(v)
    return px


def walls_sheet():
    """tiles/walls.png: one row of 47 tiles per room."""
    im = Image.new("RGB", (len(BLOB) * 8, len(ROOMS) * 8), hs.MAG)
    for r, room in enumerate(ROOMS):
        pal = WALLS[room]
        for i, m in enumerate(BLOB):
            t = wall_tile(room, m)
            for k, v in enumerate(t):
                if v:
                    im.putpixel((i * 8 + k % 8, r * 8 + k // 8), pal[v - 1])
    return im


# ---- the items (BG2): kibble, the gate, the dock's floor ---------------------------------------------------------
def items_sheet():
    cv = Canvas(56, 8)
    # kibble 1: a round biscuit; kibble 2: a little fish-shaped one
    hs.ellipse(cv, 4.0, 4.0, 1.9, 1.7, [KIB[0], KIB[1], KIB[1], KIB[2]])
    k2 = Canvas(8, 8)
    tri(k2, [(2.2, 2.6), (2.2, 5.4), (3.8, 4.0)], KIB[1])
    hs.ellipse(k2, 4.6, 4.0, 1.7, 1.4, [KIB[0], KIB[1], KIB[1], KIB[2]])
    k2.set(4, 3, KIB[0])
    k2.outline(KIB_OUT)
    cv.outline(KIB_OUT)
    cv.paste(k2, 8, 0)
    # the gate: a little white pet gate
    for x in range(16, 24):
        cv.set(x, 3, GATE[0]), cv.set(x, 4, GATE[1])
    for x in (16, 19, 20, 23):
        for y in range(2, 6):
            cv.set(x, y, GATE[0] if y < 5 else GATE[1])
    # the dock's charging floor: a dark plate, a stripe, a green light
    for y in range(8):
        for x in range(24, 32):
            cv.set(x, y, DOCKF[1] if (x + y) % 4 == 0 else DOCKF[0])
    cv.set(27, 3, LED), cv.set(28, 3, LED)
    # the pet door (left of the maze; flipped on the right): a frame, a dark opening, a flap hinged on top, 8x24
    door = Canvas(8, 24)
    door.rect(1, 3, 6, 20, KIB[2])
    door.rect(2, 5, 6, 19, DOCKF[0])
    door.rect(2, 5, 6, 11, GATE[1])
    door.rect(2, 5, 6, 5, GATE[0])
    door.outline(KIB_OUT)
    for t in range(3):
        for y in range(8):
            for x in range(8):
                if door.get(x, t * 8 + y) is not None:
                    cv.set(32 + t * 8 + x, y, door.get(x, t * 8 + y))
    return cv.image()


# ---- the rooms around the maze (BG3) ------------------------------------------------------------------------------
class Room:
    """A 576x240 panorama (x 0 = stage x -128): sx(x) converts a stage x."""

    def __init__(self):
        self.cv = Canvas(PANO_W, PANO_H)
        self.sh = None                  # the floor's shadow colours: props cast a shadow down-right

    @staticmethod
    def sx(x):
        return x - PANO_X0

    def put(self, x, y, c):
        self.cv.set(int(x) - PANO_X0, int(y), c)

    def get(self, x, y):
        return self.cv.get(int(x) - PANO_X0, int(y))

    def paste(self, other, x, y, flat=False):
        """A prop at stage (x, y); its silhouette darkens the floor 2 px right and 3 px down (unless flat)."""
        if self.sh and not flat:
            for yy in range(other.h):
                for xx in range(other.w):
                    if other.p[yy][xx] is not None:
                        c = self.get(x + xx + 2, y + yy + 3)
                        if c in self.sh:
                            self.put(x + xx + 2, y + yy + 3, self.sh[c])
        self.cv.paste(other, int(x) - PANO_X0, int(y))

    def fill(self, fn):
        for y in range(PANO_H):
            for x in range(PANO_W):
                c = fn(x + PANO_X0, y)
                if c is not None:
                    self.cv.p[y][x] = c


def stamp(fn, w, h, out):
    cv = Canvas(w, h)
    fn(cv)
    cv.outline(out)
    return cv


def wall_band(room, rm, pattern, base):
    """The back wall at the top (y 0..23) and the baseboard (24..27); the side walls on the far left and right."""
    def f(x, y):
        if y < 24 or x < -110 or x >= 430:
            return pattern(x, y)
        if y < 28:
            return base[0] if y < 26 else base[1]
        return None
    rm.fill(f)


def window(cv, w, h, frame, glass, curtain=None, night=False):
    """A window, face-on: its frame, two panes, the sky (a cloud or the moon), curtains."""
    rect(cv, 0, 0, w - 1, h - 1, frame[1])
    rect(cv, 1, 1, w - 2, h - 2, frame[0])
    for y in range(2, h - 2):
        for x in range(2, w - 2):
            if x != w // 2:
                cv.set(x, y, glass[0] if y < h // 2 else glass[1])
    if night:
        disc(cv, w - 7, 5, 2.5, (250, 240, 190))
        cv.set(4, 4, (250, 240, 190)), cv.set(9, 7, (250, 240, 190))
    else:
        for x in range(4, 10):
            cv.set(x, 5, (250, 250, 250))
        for x in range(5, 8):
            cv.set(x, 4, (250, 250, 250))
    if curtain:
        for y in range(0, h):
            for x in range(0, 4):
                cv.set(x, y, curtain[0] if x < 2 else curtain[1])
                cv.set(w - 1 - x, y, curtain[0] if x >= 2 else curtain[1])


def bowl(cv, cx, cy, r, ramp, content):
    hs.ellipse(cv, cx, cy, r, r * 0.8, [ramp[0], ramp[0], ramp[1], ramp[1]])
    oval(cv, cx, cy + 0.3, r - 1.6, r * 0.8 - 1.4, ramp[1])
    if content:
        hs.ellipse(cv, cx, cy + 0.4, r - 2.2, r * 0.8 - 2.0, content)


def plant(cv, cx, cy, r, pot, leaf):
    hs.ellipse(cv, cx, cy, r, r, [pot[0], pot[0], pot[1], pot[1]])
    rr = random.Random(int(cx * 7 + cy))
    for k in range(7):
        a = k * 2 * math.pi / 7 + rr.uniform(-0.2, 0.2)
        lx, ly = cx + math.cos(a) * r * 0.75, cy + math.sin(a) * r * 0.75
        hs.ellipse(cv, lx, ly, r * 0.5, r * 0.32 + 1, [leaf[0], leaf[0], leaf[1], leaf[1]])
    disc(cv, cx, cy, r * 0.35, leaf[1])


def rug(cv, cx, cy, rx, ry, c1, c2):
    for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
        for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
            d = ((x + 0.5 - cx) / rx) ** 2 + ((y + 0.5 - cy) / ry) ** 2
            if d <= 1:
                cv.set(x, y, c2 if 0.55 < d < 0.75 else c1)


def chair(cv, x, y, wood, cushion=None):
    rect(cv, x, y, x + 15, y + 15, wood[1])
    rect(cv, x + 1, y + 1, x + 14, y + 13, wood[0])
    if cushion:
        hs.ellipse(cv, x + 8, y + 8, 5.5, 5, [cushion[0], cushion[0], cushion[1], cushion[1]])
    rect(cv, x, y - 4, x + 15, y - 1, wood[2])                 # the back, seen from above
    rect(cv, x + 1, y - 4, x + 14, y - 3, wood[1])


def table(cv, x, y, w, h, wood, things=None):
    rect(cv, x, y, x + w - 1, y + h - 1, wood[2])
    rect(cv, x, y, x + w - 1, y + h - 3, wood[1])
    rect(cv, x + 1, y + 1, x + w - 2, y + 2, wood[0])
    for k in range(1, h // 6):
        for xx in range(x + 2, x + w - 2):
            if (xx * 3 + k * 7) % 11 < 7:
                cv.set(xx, y + k * 6, wood[2])
    if things:
        things(cv)


def box(cv, x, y, w, h, card, tape):
    rect(cv, x, y, x + w - 1, y + h - 1, card[2])
    rect(cv, x, y, x + w - 1, y + h - 3, card[1])
    rect(cv, x + 1, y + 1, x + w - 2, y + 1, card[0])
    for yy in range(y, y + h - 2):
        cv.set(x + w // 2, yy, tape), cv.set(x + w // 2 - 1, yy, tape)


def room_kitchen():
    rm = Room()
    fl = [(238, 232, 214), (198, 208, 216), (214, 206, 188)]      # cream and blue-grey squares; shadow
    rm.fill(lambda x, y: fl[0] if ((x - MAZE_X) // 16 + (y - MAZE_Y) // 16) % 2 == 0 else fl[1])
    tile = [(236, 242, 238), (198, 212, 208)]
    wall_band("kitchen", rm, lambda x, y: tile[1] if x % 8 == 0 or y % 6 == 5 else tile[0],
              [(176, 132, 96), (122, 86, 62)])
    out = (54, 40, 44)
    wood = [(222, 170, 112), (180, 126, 76), (128, 84, 50)]
    glass = [(178, 220, 244), (140, 194, 232)]
    cur = [(232, 116, 100), (184, 76, 72)]
    rm.paste(stamp(lambda cv: window(cv, 44, 22, [tile[0], wood[2]], glass, cur), 46, 24, out), 6, 0)
    rm.paste(stamp(lambda cv: window(cv, 44, 22, [tile[0], wood[2]], glass, cur), 46, 24, out), 226, 0)

    def clock(cv):
        disc(cv, 8, 8, 7, tile[0])
        disc(cv, 8, 8, 5.6, (250, 250, 250))
        for k in range(4):
            cv.set(8 + [0, 4, 0, -4][k], 8 + [-4, 0, 4, 0][k], out)
        for i in range(4):
            cv.set(8, 8 - i, out)
        for i in range(3):
            cv.set(8 + i, 8, cur[1])
    rm.paste(stamp(clock, 17, 17, out), 98, 3)

    def shelf(cv):
        rect(cv, 0, 14, 63, 16, wood[1])
        rect(cv, 0, 17, 63, 17, wood[2])
        for i, c in enumerate([cur, glass, [(246, 214, 120), (214, 168, 70)], cur]):
            x = 4 + i * 15
            rect(cv, x, 4, x + 9, 13, c[1])
            rect(cv, x + 1, 5, x + 4, 12, c[0])
            rect(cv, x + 1, 2, x + 8, 3, wood[0])
    rm.paste(stamp(shelf, 64, 19, out), -100, 2)
    rm.paste(stamp(shelf, 64, 19, out), 340, 2)
    # floor: the kitten's bowls (food and water), a chair, a table, a plant, a rug
    sh = {fl[0]: fl[2], fl[1]: (176, 186, 198)}
    rm.sh = sh
    rm.paste(stamp(lambda cv: bowl(cv, 14, 11, 12, [(120, 160, 226), (78, 108, 178)],
                                   [KIB[0], KIB[1], KIB[1], KIB[2]]), 30, 24, out), -1, 150)
    rm.paste(stamp(lambda cv: bowl(cv, 13, 10, 11, [(250, 250, 250), (196, 200, 210)],
                                   [glass[0], glass[0], glass[1], glass[1]]), 28, 22, out), 2, 184)
    for cx, cy in ((290, 70), (290, 128)):
        rm.paste(stamp(lambda cv: chair(cv, 1, 5, wood, cur), 18, 22, out), cx - 1, cy - 5)
    rm.paste(stamp(lambda cv: table(cv, 1, 1, 60, 100, wood,
                                    lambda c: (bowl(c, 18, 24, 7, [(250, 250, 250), (196, 200, 210)], None),
                                               bowl(c, 40, 70, 7, [(250, 250, 250), (196, 200, 210)], None))),
                   62, 102, out), -95, 68)
    rm.paste(stamp(lambda cv: plant(cv, 15, 15, 12, [(214, 122, 80), (162, 84, 56)],
                                    [(126, 192, 92), (70, 132, 62)]), 32, 32, out), 338, 160)
    rm.paste(stamp(lambda cv: rug(cv, 30, 16, 29, 15, (232, 150, 120), (196, 96, 86)), 62, 34, out), 330, 74, flat=True)
    return rm


def room_living():
    rm = Room()
    fl = [(196, 146, 98), (178, 128, 84), (150, 104, 68)]         # honey planks, seams
    def planks(x, y):
        yy = y - MAZE_Y
        row = yy // 8
        if yy % 8 == 7:
            return fl[2]
        if (x + row * 24) % 48 == 0:
            return fl[2]
        return fl[0] if row % 2 == 0 else fl[1]
    rm.fill(planks)
    paper = [(236, 222, 196), (214, 198, 172)]
    wall_band("living", rm, lambda x, y: paper[1] if x % 12 in (0, 1) else paper[0], [(150, 96, 66), (104, 64, 46)])
    out = (52, 30, 34)
    wood = [(176, 112, 70), (138, 84, 52), (96, 58, 38)]
    glass = [(178, 220, 244), (140, 194, 232)]
    cur = [(108, 158, 136), (72, 116, 100)]
    rm.paste(stamp(lambda cv: window(cv, 52, 22, [paper[0], wood[2]], glass, cur), 54, 24, out), 2, 0)
    rm.paste(stamp(lambda cv: window(cv, 52, 22, [paper[0], wood[2]], glass, cur), 54, 24, out), 230, 0)

    def picture(cv):
        rect(cv, 0, 0, 23, 17, (216, 172, 82))
        rect(cv, 2, 2, 21, 15, (150, 200, 230))
        rect(cv, 2, 11, 21, 15, (110, 170, 96))
        disc(cv, 15, 6, 2.2, (250, 230, 140))
    rm.paste(stamp(picture, 25, 19, out), 122, 2)
    rm.paste(stamp(picture, 25, 19, out), 176, 2)
    # an armchair, a round rug with a basket of yarn, a floor lamp, a plant, a coffee table
    sh = {fl[0]: (170, 122, 80), fl[1]: (154, 108, 72)}
    rm.sh = sh
    sofa = [(178, 88, 142), (122, 52, 98)]

    def armchair(cv):
        rect(cv, 0, 0, 31, 29, sofa[1])
        rect(cv, 4, 6, 27, 27, sofa[0])
        hs.ellipse(cv, 16, 16, 9, 8, [(216, 132, 178), (216, 132, 178), sofa[0], sofa[0]])
        rect(cv, 0, 0, 31, 4, (216, 132, 178))
    rm.paste(stamp(armchair, 32, 30, out), -92, 60)
    rm.paste(stamp(armchair, 32, 30, out), 290, 60)
    rm.paste(stamp(lambda cv: rug(cv, 26, 22, 25, 21, (120, 168, 200), (232, 214, 150)), 54, 46, out), -80, 128, flat=True)

    def basket(cv):
        hs.ellipse(cv, 10, 9, 9, 7, [wood[0], wood[0], wood[1], wood[1]])
        oval(cv, 10, 9, 7, 5, wood[2])
        for cx, c in ((7, YARN), (13, [(150, 200, 240), (90, 140, 210), (60, 96, 170)])):
            hs.ellipse(cv, cx, 8.5, 3.2, 3.2, [c[0], c[1], c[1], c[2]])
    rm.paste(stamp(basket, 22, 20, out), -66, 140)
    rm.paste(stamp(basket, 22, 20, out), 6, 196)

    def lamp(cv):
        disc(cv, 11, 11, 10, (250, 228, 150))
        disc(cv, 11, 11, 7.5, (252, 240, 196))
        disc(cv, 11, 11, 2, (176, 112, 70))
    rm.paste(stamp(lamp, 23, 23, out), 10, 40)
    rm.paste(stamp(lamp, 23, 23, out), 380, 196)
    rm.paste(stamp(lambda cv: plant(cv, 15, 15, 12, [(214, 122, 80), (162, 84, 56)],
                                    [(126, 192, 92), (70, 132, 62)]), 32, 32, out), 326, 156)
    rm.paste(stamp(lambda cv: table(cv, 1, 1, 24, 48, wood,
                                    lambda c: bowl(c, 12, 16, 5, [(250, 250, 250), (200, 200, 210)], None)),
                   26, 50, out), 281, 120)
    return rm


def room_bedroom():
    rm = Room()
    fl = [(238, 200, 176), (228, 188, 164)]                       # a peach carpet
    rm.fill(lambda x, y: fl[1] if (x * 3 + y * 5) % 11 == 0 or (x + y * 2) % 13 == 0 else fl[0])
    paper = [(64, 74, 132), (96, 108, 170), (250, 236, 170)]      # a night-blue wallpaper with little stars
    wall_band("bedroom", rm, lambda x, y: paper[2] if (x % 16 == 5 and y % 12 == 4) or (x % 16 == 13 and y % 12 == 10)
              else paper[1] if x % 16 == 8 else paper[0], [(170, 140, 200), (118, 94, 150)])
    out = (40, 30, 62)
    wood = [(230, 214, 236), (190, 168, 210), (140, 118, 168)]
    glass = [(40, 50, 104), (30, 38, 86)]
    cur = [(250, 190, 210), (220, 140, 172)]
    rm.paste(stamp(lambda cv: window(cv, 44, 22, [paper[1], wood[2]], glass, cur, night=True), 46, 24, out), 4, 0)
    rm.paste(stamp(lambda cv: window(cv, 44, 22, [paper[1], wood[2]], glass, cur, night=True), 46, 24, out), 228, 0)
    sh = {fl[0]: (214, 174, 152), fl[1]: (204, 164, 144)}
    rm.sh = sh
    quilt = [(162, 178, 238), (98, 110, 184)]

    def bed(cv):
        rect(cv, 0, 0, 51, 85, wood[2])
        rect(cv, 2, 2, 49, 83, wood[1])
        rect(cv, 3, 3, 48, 20, (250, 250, 250))
        hs.ellipse(cv, 15, 11, 10, 6, [(250, 250, 250), (250, 250, 250), (220, 224, 240), (220, 224, 240)])
        hs.ellipse(cv, 37, 11, 10, 6, [(250, 250, 250), (250, 250, 250), (220, 224, 240), (220, 224, 240)])
        rect(cv, 3, 24, 48, 82, quilt[0])
        for y in range(24, 83):
            for x in range(3, 49):
                if (x + y) % 10 == 0 or (x - y) % 10 == 0:
                    cv.set(x, y, quilt[1])
        rect(cv, 3, 24, 48, 26, (250, 206, 226))
    rm.paste(stamp(bed, 52, 86, out), -100, 36)

    def toybox(cv):
        rect(cv, 0, 0, 27, 21, (232, 120, 110))
        rect(cv, 1, 1, 26, 17, (250, 160, 140))
        for x in range(3, 25, 6):
            disc(cv, x + 2, 8, 1.6, (250, 230, 140))
    rm.paste(stamp(toybox, 28, 22, out), 290, 66)

    def slippers(cv):
        for x in (0, 9):
            hs.ellipse(cv, x + 3.5, 7, 3.5, 6.5, [cur[0], cur[0], cur[1], cur[1]])
            disc(cv, x + 3.5, 3.5, 1.5, (250, 250, 250))
    rm.paste(stamp(slippers, 17, 15, out), 14, 180)

    def teddy(cv):
        for ex in (4, 14):
            disc(cv, ex, 3.5, 2.6, (176, 120, 80))
        hs.ellipse(cv, 9, 8, 6, 5.5, [(210, 156, 106), (210, 156, 106), (176, 120, 80), (176, 120, 80)])
        hs.ellipse(cv, 9, 17, 7, 6, [(210, 156, 106), (210, 156, 106), (176, 120, 80), (176, 120, 80)])
        cv.set(7, 7, out), cv.set(11, 7, out), cv.set(9, 9, out)
    rm.paste(stamp(teddy, 19, 24, out), 10, 60)
    rm.paste(stamp(lambda cv: rug(cv, 26, 18, 25, 17, (250, 220, 240), (200, 170, 230)), 54, 38, out), 330, 150, flat=True)

    def wardrobe(cv):
        rect(cv, 0, 0, 39, 25, wood[2])
        rect(cv, 1, 1, 38, 21, wood[1])
        rect(cv, 2, 2, 37, 4, wood[0])
        cv.set(19, 12, wood[2]), cv.set(20, 12, wood[2])
    rm.paste(stamp(wardrobe, 40, 26, out), 360, 34)
    return rm


def room_garden():
    rm = Room()
    lawn = [(122, 180, 90), (110, 166, 80), (140, 196, 104)]
    path = [(222, 208, 172), (204, 188, 150)]                      # gravel paths between the hedges
    mx0, my0, mx1, my1 = MAZE_X - 2, MAZE_Y - 2, MAZE_X + MAZE_W * CELL + 2, MAZE_Y + MAZE_H * CELL + 2

    def ground(x, y):
        if mx0 <= x < mx1 and my0 <= y < my1:
            return path[1] if (x * 7 + y * 3) % 9 == 0 or (x * 2 + y * 5) % 13 == 0 else path[0]
        h = (x * 5 + y * 11) % 17
        return lawn[1] if h < 3 else lawn[2] if h == 8 else lawn[0]
    rm.fill(ground)
    sky = [(150, 206, 240), (186, 226, 248)]
    fence = [(236, 222, 196), (190, 170, 140)]

    def band(x, y):
        if y < 12:
            return sky[0] if y < 6 else sky[1]
        if x % 12 in (0, 1) or y in (14, 20):
            return fence[1] if x % 12 == 1 or y == 20 else fence[0]
        return fence[0] if x % 12 < 11 and y > 12 else fence[1]
    wall_band("garden", rm, band, [lawn[1], lawn[1]])
    out = (24, 44, 30)
    for cx in (20, 200, 300):                                       # little clouds
        for k in range(3):
            disc(rm.cv, rm.sx(cx + k * 5), 4 + (k % 2), 3, (250, 250, 250))
    sh = {lawn[0]: (96, 148, 72), lawn[1]: (90, 140, 68), lawn[2]: (110, 160, 84)}
    rm.sh = sh
    leaf = [(110, 176, 80), (66, 128, 58), (40, 94, 46)]

    def tree(cv):
        rr = random.Random(3)
        for k in range(12):
            a = k * 2 * math.pi / 12
            lx, ly = 22 + math.cos(a) * 13, 22 + math.sin(a) * 13
            hs.ellipse(cv, lx, ly, 8 + rr.uniform(0, 2), 7 + rr.uniform(0, 2), [leaf[0], leaf[0], leaf[1], leaf[2]])
        hs.ellipse(cv, 22, 22, 13, 13, [leaf[0], leaf[0], leaf[1], leaf[1]])
        for k in range(6):
            disc(cv, 12 + k * 4, 14 + (k % 3) * 6, 1.2, (230, 90, 80))
    rm.paste(stamp(tree, 46, 46, out), -94, 78)

    def flowers(cv):
        rect(cv, 0, 0, 29, 13, (140, 96, 64))
        rect(cv, 1, 1, 28, 11, (160, 112, 76))
        for k in range(7):
            c = [(250, 120, 140), (250, 220, 110), (200, 140, 250)][k % 3]
            disc(cv, 3 + k * 4, 4 + (k % 2) * 4, 1.6, c)
            cv.set(3 + k * 4, 4 + (k % 2) * 4, (250, 250, 250))
    rm.paste(stamp(flowers, 30, 14, out), 10, 60)
    rm.paste(stamp(flowers, 30, 14, out), 280, 190)
    rm.paste(stamp(flowers, 30, 14, out), 360, 60)

    def pond(cv):
        hs.ellipse(cv, 22, 14, 21, 13, [(150, 200, 236), (120, 180, 226), (96, 156, 210), (80, 136, 196)])
        for cx, cy in ((12, 10), (28, 18)):
            disc(cv, cx, cy, 3, (110, 176, 80))
            cv.set(cx, cy - 3, (150, 200, 236))
        disc(cv, 30, 8, 1.5, (250, 150, 170))
    rm.paste(stamp(pond, 46, 30, out), 330, 108)

    def stones(cv):
        for k in range(3):
            hs.ellipse(cv, 6 + k * 3, 5 + k * 9, 5, 3.5, [(214, 214, 214), (214, 214, 214), (170, 170, 176),
                                                           (170, 170, 176)])
    rm.paste(stamp(stones, 18, 30, out), 286, 70)
    rm.paste(stamp(stones, 18, 30, out), 18, 130)

    def birdbath(cv):
        disc(cv, 9, 9, 8.5, (196, 196, 206))
        disc(cv, 9, 9, 6, (150, 200, 236))
        hs.ellipse(cv, 7, 8, 2.4, 1.8, [(250, 120, 100), (250, 120, 100), (200, 80, 70), (200, 80, 70)])
    rm.paste(stamp(birdbath, 19, 19, out), 290, 150)
    rm.paste(stamp(birdbath, 19, 19, out), -40, 190)
    return rm


def room_attic():
    rm = Room()
    fl = [(132, 136, 150), (118, 122, 138), (96, 98, 116)]         # dusty blue-grey boards
    def boards(x, y):
        xx = x - MAZE_X
        col = xx // 12
        if xx % 12 == 11:
            return fl[2]
        if (y + col * 40) % 80 == 0:
            return fl[2]
        return fl[0] if col % 2 == 0 else fl[1]
    rm.fill(boards)
    beam = [(150, 110, 78), (116, 82, 58), (86, 60, 44)]

    def band(x, y):                                                 # the roof: slanted boards and a beam
        if (x + y * 2) % 16 < 2:
            return beam[2]
        return beam[1] if (x // 16) % 2 else beam[0]
    wall_band("attic", rm, band, [beam[2], (64, 44, 34)])
    out = (40, 30, 30)
    glass = [(170, 206, 236), (130, 176, 222)]

    def round_window(cv):
        disc(cv, 11, 11, 10.5, beam[2])
        disc(cv, 11, 11, 8.5, glass[0])
        for y in range(23):
            for x in range(23):
                if dist(x, y, 11, 11) < 8.5 and (x == 11 or y == 11):
                    cv.set(x, y, beam[2])
        cv.set(7, 7, (250, 250, 250))
    rm.paste(stamp(round_window, 23, 23, out), 18, 0)
    rm.paste(stamp(round_window, 23, 23, out), 260, 0)
    card = [(228, 192, 138), (198, 152, 100), (150, 108, 68)]
    tape = (236, 220, 176)
    sh = {fl[0]: (104, 106, 122), fl[1]: (94, 96, 112)}
    rm.sh = sh
    for x, y, w, h in ((-96, 60, 36, 28), (-70, 92, 30, 24), (-100, 120, 28, 26), (300, 60, 30, 26),
                       (290, 150, 26, 22), (360, 196, 34, 26), (6, 196, 30, 22)):
        rm.paste(stamp(lambda cv, w=w, h=h: box(cv, 0, 0, w, h, card, tape), w + 1, h + 1, out), x, y)

    def trunk(cv):
        rect(cv, 0, 0, 43, 25, (96, 66, 120))
        rect(cv, 1, 1, 42, 21, (126, 92, 156))
        for x in (6, 37):
            rect(cv, x, 0, x + 1, 23, (214, 184, 96))
        rect(cv, 20, 9, 23, 13, (214, 184, 96))
    rm.paste(stamp(trunk, 44, 26, out), 348, 104)

    def web(cv):
        for k in range(5):
            a = k * math.pi / 8
            for i in range(14):
                cv.set(int(i * math.cos(a)), int(i * math.sin(a)), (210, 214, 226))
        for r in (5, 9, 13):
            for k in range(13):
                a = k * math.pi / 26
                cv.set(int(r * math.cos(a)), int(r * math.sin(a)), (210, 214, 226))
    rm.paste(stamp(web, 15, 15, (210, 214, 226)), -108, 28, flat=True)

    def lamp(cv):
        disc(cv, 8, 8, 7.5, (250, 228, 150))
        disc(cv, 8, 8, 4.5, (252, 244, 210))
    rm.paste(stamp(lamp, 17, 17, out), 14, 64)
    rm.paste(stamp(lamp, 17, 17, out), 290, 110)
    return rm


ROOM_FNS = {"kitchen": room_kitchen, "living": room_living, "bedroom": room_bedroom, "garden": room_garden,
            "attic": room_attic}


# ---- the sheet --------------------------------------------------------------------------------------------------------
def sprite_frames():
    out = {}
    out["kit_side"] = [kitten_side(f) for f in range(3)]
    out["kit_down"] = [kitten_front(f) for f in range(3)]
    out["kit_up"] = [kitten_back(f) for f in range(2)]
    out["kit_dizzy"] = [kitten_dizzy(f) for f in range(2)]
    out["kit_icon"] = [kitten_icon()]
    out["yarn"] = [yarn(f) for f in range(2)]
    out["fish"] = [fish(f) for f in range(2)]
    out["eyes"] = [zip_eyes(d) for d in range(4)]
    out["dust"] = [dust(f) for f in range(2)]
    out["digit"] = [small_digit(n) for n in range(10)]
    out["sparkle"] = [sparkle(f) for f in range(2)]
    out["star"] = [star(f) for f in range(2)]
    for k in ("dash", "sneak", "patrol", "wobble"):
        out[k] = [vacuum(k, v, f) for v in ("side", "front", "back") for f in range(2)]
    out["scared"] = [vacuum("scared", "front", f) for f in range(2)]
    return out


def sheet():
    im = Image.new("RGB", (SHEET_W, SHEET_H), hs.MAG)
    frames = sprite_frames()
    for name, x, y, w, h, n, _g in SPRITES:
        fr = frames[name]
        assert len(fr) == n, name
        for i, cv in enumerate(fr):
            im.paste(cv.image(), (x + i * w, y))
    return im


def write_all(out_dir, preview=False):
    os.makedirs(os.path.join(out_dir, "tiles"), exist_ok=True)
    files = [("sprites.png", sheet()), ("tiles/walls.png", walls_sheet()), ("tiles/items.png", items_sheet())]
    for room in ROOMS:
        files.append(("tiles/%s.png" % room, ROOM_FNS[room]().cv.image()))
    for name, im in files:
        im.save(os.path.join(out_dir, name))
        print("wrote", os.path.join(out_dir, name))
    if preview:
        os.makedirs(os.path.join(out_dir, "preview"), exist_ok=True)
        big = files[0][1].resize((SHEET_W * 3, SHEET_H * 3), Image.NEAREST)
        big.save(os.path.join(out_dir, "preview", "sprites.png"))
        for i, room in enumerate(ROOMS):
            im = room_preview(i, files[3 + i][1], files[1][1], files[2][1], files[0][1])
            im.resize((im.width * 2, im.height * 2), Image.NEAREST).save(
                os.path.join(out_dir, "preview", "room-%s.png" % room))


def read_mazes():
    """The mazes from src/mazes.c (the game's source of truth): ROOMS lists of MAZE_H strings."""
    import re
    src = open(os.path.join(GAME, "src", "mazes.c")).read()
    rows = re.findall(r'"([#. oT\-D1234f]{%d})"' % MAZE_W, src)
    assert len(rows) == MAZE_H * len(ROOMS), len(rows)
    return [rows[i * MAZE_H:(i + 1) * MAZE_H] for i in range(len(ROOMS))]


def room_preview(i, pano, walls, items, sprites):
    """A 520x240 picture of room i as the game shows it (the art review: walls, kibble, a kitten, the vacuums)."""
    maze = read_mazes()[i]
    im = pano.crop((-100 - PANO_X0, 0, -100 - PANO_X0 + 520, 240))
    ox = 100

    def wall(x, y):
        if y < 0 or y >= MAZE_H:
            return False
        return maze[y][x % MAZE_W] == "#" if 0 <= x < MAZE_W else False
    for y in range(MAZE_H):
        for x in range(MAZE_W):
            ch = maze[y][x]
            sx, sy = ox + MAZE_X + x * 8, MAZE_Y + y * 8
            src = None
            if ch == "#":
                m = 0
                for b, (dx, dy) in NB.items():
                    if wall(x + dx, y + dy):
                        m |= b
                k = BLOB.index(canon(m))
                src = walls.crop((k * 8, i * 8, k * 8 + 8, i * 8 + 8))
            elif ch == ".":
                src = items.crop(((x + y) % 2 * 8, 0, (x + y) % 2 * 8 + 8, 8))
            elif ch == "-":
                src = items.crop((16, 0, 24, 8))
            elif ch == "D":
                src = items.crop((24, 0, 32, 8))
            if src is not None:
                for yy in range(8):
                    for xx in range(8):
                        c = src.getpixel((xx, yy))
                        if c != hs.MAG:
                            im.putpixel((sx + xx, sy + yy), c)

    def spr(sx, sy, x, y):
        for yy in range(16):
            for xx in range(16):
                c = sprites.getpixel((sx + xx, sy + yy))
                if c != hs.MAG:
                    im.putpixel((ox + x + xx, y + yy), c)
    for y in range(MAZE_H):
        for x in range(MAZE_W):
            if maze[y][x] == "o":
                spr(0, 16, MAZE_X + x * 8 - 4, MAZE_Y + y * 8 - 4)
            if maze[y][x] == "1":
                spr(0, 0, MAZE_X + x * 8 - 4, MAZE_Y + y * 8 - 4)
    for k, (sx, sy) in enumerate(((0, 48), (96 + 32, 48), (0 + 64, 64), (96, 64))):
        spr(sx, sy, MAZE_X + (13 + [0, -2, 0, 2][k]) * 8 - 4, MAZE_Y + [7, 10, 10, 10][k] * 8 - 4)
    return im


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(GAME, "art"))
    ap.add_argument("--preview", action="store_true", help="also write art/preview/ (x3 sheets)")
    a = ap.parse_args(argv)
    write_all(a.out, a.preview)


if __name__ == "__main__":
    main()

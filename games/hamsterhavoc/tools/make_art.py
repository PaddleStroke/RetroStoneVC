#!/usr/bin/env python3
"""Hamster Havoc: the code-drawn art, in the 8BCraft house style (docs/art-direction.md).

Code-drawn art only, no image generation: every sprite and panorama is drawn here with
games/common/tools/house_style.py (1-px outlines, 3-4-shade ramps lit from the top-left).
The sheet layout (SPRITES) is shared with build_assets.py.

Writes, in the art folder (default games/hamsterhavoc/art):
  sprites.png (256x96): the hamster's glass-dome ship (golden fur; players 2-4 recolour the fur: white, grey,
      ginger), its icon, the sunflower seed, the power-ups and the bubble shield (OBJ 0); the three
      kinds of space mice (two march frames each), their pops into sparkles and crumbs, small digits, the
      mothership's hit bar (OBJ 2); the crumbs, peas and jelly beans, the bonus saucer (OBJ 6); the mothership (OBJ 7)
  tiles/moon.png (576x32)     BG2: the big cheese moon below (its curve shows on wide screens), and
  tiles/shelter.png (24x16)   the cheese wedge of the four shelters (the game erodes it pixel by pixel)
  tiles/decor.png (576x240)   BG3: planets, a nebula and a comet, faint, no outline (mostly in the wide margins)
  tiles/stars.png (512x240)   BG4: the stars (two twinkle groups, lit by the game)

The moon and the decor are "stage panoramas": x 0 is stage x -128, so they cover every width up to 560.

    python3 games/hamsterhavoc/tools/make_art.py [--out DIR] [--preview]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/hamsterhavoc/LICENSE.
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
# name, x, y, w, h, frames, palette group (hero: OBJ 0, players 2-4 = OBJ 1, 4, 5 recoloured; mice: OBJ 2;
# props: OBJ 6; boss: OBJ 7)
SPRITES = [
    ("ship", 0, 0, 32, 24, 2, "hero"),          # the ship; frame 1: the hamster blinks
    ("ship_fire", 64, 0, 32, 24, 1, "hero"),    # a seed fired: the launcher flashes, cheeks puffed
    ("ship_hit", 96, 0, 32, 24, 2, "hero"),     # bonk: X eyes, the dome cracked
    ("bubble", 160, 0, 32, 32, 1, "hero"),      # the bubble shield
    ("icon", 192, 0, 16, 16, 1, "hero"),        # the title's player slots, the results
    ("seed", 216, 0, 8, 8, 1, "hero"),          # the sunflower seed
    ("badge", 224, 0, 8, 8, 2, "hero"),         # double seeds, rapid seeds (by the ship)
    ("power", 0, 32, 16, 16, 3, "hero"),        # the capsules: double seeds, rapid seeds, the bubble
    ("mouse_a", 48, 32, 16, 16, 2, "mice"),     # 10 points: the round grey-blue mouse with antennae
    ("mouse_b", 80, 32, 16, 16, 2, "mice"),     # 20: the pink mouse in a bubble helmet
    ("mouse_c", 112, 32, 16, 16, 2, "mice"),    # 30: the little mint mouse with star antennae
    ("pop", 144, 32, 16, 16, 3, "mice"),        # a mouse pops: a sparkle burst, sparkles and crumbs, crumbs
    ("splat", 192, 32, 8, 8, 2, "mice"),
    ("sparkle", 208, 32, 8, 8, 2, "mice"),
    ("hp", 224, 32, 8, 8, 3, "mice"),           # the mothership's hits left: full, half, empty
    ("digit", 0, 48, 8, 8, 10, "mice"),         # small digits (points)
    ("crumb", 80, 48, 8, 8, 2, "props"),        # the shots: a cheese crumb (aimed) ...
    ("pea", 96, 48, 8, 8, 2, "props"),          # ... a space pea (fast) ...
    ("jelly", 112, 48, 8, 8, 2, "props"),       # ... a wiggly jelly bean
    ("saucer", 128, 48, 32, 16, 2, "props"),    # the bonus saucer: a cheese wheel, a fat mouse at the wheel
    ("boss", 0, 64, 64, 32, 2, "boss"),         # the mouse mothership, its captain in a cheese crown
]
PANORAMAS = {"moon": ("tiles/moon.png", 576, 32), "decor": ("tiles/decor.png", 576, 240),
             "stars": ("tiles/stars.png", 512, 240)}
SHELTER = ("tiles/shelter.png", 24, 16)
PANO_X0 = -128                       # stage x of the panoramas' column 0
MOON_Y0 = 208                        # screen line of the moon panorama's row 0

# ---- colours (each group fits one 15-colour sprite palette) ---------------------------------------------------------
WHITE = (250, 250, 250)
INK = hs.HOUSE["ink"][0]
# the hero: the ship's hull, the glass dome, the hamster's fur (swapped for players 2-4), pink, the seed, the accent
OUT = (40, 22, 52)
HULL = [WHITE, (214, 226, 240), (150, 168, 196), (96, 108, 140)]
GLASS = [(196, 238, 255), (120, 190, 230)]
FUR = [(255, 226, 150), (240, 184, 84), (204, 132, 48), (150, 88, 30)]          # P1 golden
FUR_P = {1: [(255, 250, 244), (232, 224, 214), (196, 184, 176), (146, 132, 128)],  # P2 white
         2: [(212, 214, 226), (170, 172, 190), (126, 128, 148), (86, 88, 108)],    # P3 grey
         3: [(255, 198, 142), (242, 142, 66), (204, 94, 34), (142, 56, 24)]}       # P4 ginger
PINK = (255, 150, 170)
SEEDD = (60, 52, 64)
ACC = (240, 96, 72)
# the mice: three kinds, the helmet glass, yellow, crumbs
OUTM = (36, 20, 56)
MA = [(196, 206, 230), (146, 158, 196), (100, 108, 150)]
MB = [(255, 196, 214), (236, 140, 172), (186, 90, 130)]
MC = [(190, 240, 190), (120, 206, 140), (70, 150, 100)]
HELM = (180, 230, 255)
YEL = (255, 226, 100)
CRUMB = (240, 170, 60)
# the props: cheese, peas, jelly, the saucer's pilot
OUTP = (40, 24, 40)
CH = [(255, 232, 140), (246, 196, 72), (206, 144, 40)]
PEA = [(170, 232, 110), (96, 170, 60)]
JEL = [(255, 130, 160), (206, 70, 110)]
GLASSP = (180, 230, 255)
GREY = [(190, 190, 210), (130, 130, 156)]
PINKP = (255, 170, 190)
RED = (255, 90, 80)
# the mothership
OUTB = (30, 16, 46)
HULLB = [(200, 170, 240), (146, 110, 210), (96, 66, 160)]
CAP = [(220, 200, 190), (170, 146, 140), (120, 96, 96)]
PINKB = (255, 160, 180)
YELB = (255, 226, 100)
REDB = (255, 100, 90)
CHB = (246, 196, 72)
# the BG layers: the moon and the shelters, the decor, the stars
RIND = (130, 80, 34)
RIM = (255, 248, 210)
CHEESE = [(255, 236, 160), (246, 206, 98), (220, 170, 70), (176, 126, 50)]
HOLE = [(196, 146, 60), (150, 104, 40)]
# the moon's body below its lit horizon, in shadow (two bands: from line 224, the credits' row, and from 232) and the
# craters there (dark, lit edge)
DEEP = [(164, 108, 44), (136, 88, 38)]
HOLE_DEEP = [[(136, 88, 38), (186, 132, 56)], [(112, 72, 32), (164, 108, 44)]]
TEAL = [(96, 140, 160), (74, 112, 138), (56, 88, 116)]
RING = [(176, 160, 140), (136, 122, 112)]
PURP = [(120, 96, 160), (94, 74, 132)]
GREYM = [(116, 116, 140), (90, 90, 116)]
NEB = [(48, 40, 96), (40, 34, 82)]
COMET = [(190, 200, 230), (120, 140, 190)]
STARS = [(255, 255, 160), (160, 255, 255)]                # the twinkle keys (lit by the game)
STAR_FAINT = [(90, 96, 150), (150, 160, 210)]


def dist(x, y, cx, cy):
    return math.hypot(x + 0.5 - cx, y + 0.5 - cy)


def disc(cv, cx, cy, r, col):
    for y in range(int(cy - r) - 1, int(cy + r) + 2):
        for x in range(int(cx - r) - 1, int(cx + r) + 2):
            if dist(x, y, cx, cy) <= r:
                cv.set(x, y, col)


def shaded_disc(cv, cx, cy, r, r4):
    hs.ellipse(cv, cx, cy, r, r, r4)


def ring(cv, cx, cy, r, col, width=1.0):
    for y in range(int(cy - r) - 2, int(cy + r) + 3):
        for x in range(int(cx - r) - 2, int(cx + r) + 3):
            if r - width < dist(x, y, cx, cy) <= r:
                cv.set(x, y, col)


# ---- the hamster ---------------------------------------------------------------------------------------------------
def hamster_head(cv, cx, cy, pose="idle", look=1, scale=1.0):
    """The hamster's head (front, looking a little to the side): round, two ears, the house eyes, pink cheeks."""
    s = scale
    for ex in (-1, 1):                                     # the ears, behind
        disc(cv, cx + ex * 4.6 * s, cy - 4.2 * s, 2.3 * s, FUR[2])
        disc(cv, cx + ex * 4.6 * s, cy - 4.0 * s, 1.1 * s, PINK)
    hs.ellipse(cv, cx, cy, 6.2 * s, 5.4 * s, FUR)
    hs.ellipse(cv, cx, cy + 2.2 * s, 3.6 * s, 2.4 * s, None, flat=FUR[0])     # the light muzzle
    ex0, ey = int(round(cx - 3.3 * s)), int(round(cy - 1.6 * s))
    ex1 = int(round(cx + 1.3 * s))
    if pose == "hit":
        for ex in (ex0, ex1):
            for d in range(3):
                cv.set(ex + d, ey + d, INK), cv.set(ex + 2 - d, ey + d, INK)
    elif pose == "blink":
        for ex in (ex0, ex1):
            cv.set(ex, ey + 2, INK), cv.set(ex + 1, ey + 2, INK)
    elif pose == "fire":
        for ex in (ex0, ex1):                             # eyes squeezed: > <
            cv.set(ex, ey + 1, INK), cv.set(ex + 1, ey + 2, INK), cv.set(ex, ey + 3, INK)
    else:
        hs.eye(cv, ex0, ey, look=(look, 0), white=WHITE, pupil=INK)
        hs.eye(cv, ex1, ey, look=(look, 0), white=WHITE, pupil=INK)
    cheek = 1 if pose == "fire" else 0
    for cxx in (cx - 4.4 * s, cx + 4.4 * s):              # the cheeks (puffed when it fires)
        disc(cv, cxx, cy + 1.6 * s, 1.2 * s + cheek * 0.6, PINK)
    nx = int(round(cx - 0.5 + look * 0.5))
    cv.set(nx, int(round(cy + 1.0 * s)), PINK)            # the nose
    cv.set(nx, int(round(cy + 2.6 * s)), FUR[3])           # the mouth


def ship(pose="idle"):
    """The glass-dome ship, 32x24, the hamster inside looking right (flipped for left)."""
    cv = Canvas(32, 24)
    # the hull: a round saucer, lit from the top-left, three lights
    hs.ellipse(cv, 16.0, 18.0, 14.5, 4.6, HULL)
    for x in range(4, 28):
        if cv.get(x, 21) is not None:
            cv.set(x, 21, HULL[3])
    for lx in (8, 16, 24):
        cv.set(lx, 18, ACC if pose != "hit" else HULL[3])
        cv.set(lx - 1, 18, WHITE if pose == "fire" else HULL[2])
    cv.set(15, 22, ACC), cv.set(16, 22, ACC)               # the thruster's glow
    if pose != "hit":
        cv.set(15, 23, ACC if pose == "fire" else None)
    # the dome: glass, the hamster inside, a glint top-left
    disc(cv, 16.0, 10.5, 8.6, GLASS[1])
    disc(cv, 15.4, 9.8, 7.6, GLASS[0])
    hamster_head(cv, 16.5, 11.5, {"idle": "idle", "blink": "blink", "fire": "fire", "hit": "hit", "hit2": "hit"}[pose])
    for x, y in ((10, 6), (10, 5), (11, 4), (12, 3)):
        cv.set(x, y, WHITE)
    if pose in ("hit", "hit2"):                             # the dome cracked
        for x, y in ((19, 3), (20, 4), (20, 5), (21, 6), (22, 6), (23, 7), (21, 7)):
            cv.set(x, y, WHITE)
    # the seed launcher on top
    for y in (0, 1):
        cv.set(15, y, SEEDD), cv.set(16, y, SEEDD)
    cv.set(15, 2, HULL[2]), cv.set(16, 2, HULL[2])
    if pose == "fire":
        cv.set(15, 0, WHITE), cv.set(16, 0, WHITE)
    if pose == "hit2":
        cv = hs.squash(cv, 1.12)
    cv.outline(OUT)
    return cv


def icon():
    cv = Canvas(16, 16)
    hamster_head(cv, 8.0, 9.0, "idle", 1, 1.05)
    cv.outline(OUT)
    return cv


def seed_shape(cv, cx, top, h=6):
    """A sunflower seed pointing up: dark, two light stripes."""
    for i in range(h):
        half = 0 if i == 0 else 1 if i < h - 1 else 0.6
        for x in range(int(round(cx - half)), int(round(cx + half)) + 1):
            cv.set(x, top + i, SEEDD)
    for i in range(1, h - 1):
        cv.set(int(round(cx - 1)), top + i, WHITE if i % 3 else HULL[2])
    cv.set(int(round(cx + 1)), top + h - 2, HULL[2])


def seed():
    """The player's shot: a sunflower seed, tip up, striped white / ink / light grey so it reads on the night sky."""
    cv = Canvas(8, 8)
    rows = ("..W..", ".WWH.", ".WSH.", ".HSM.", ".HSM.", ".MMM.")     # pointed tip up, lit left, the dark stripe
    for y, row in enumerate(rows):
        for x, k in enumerate(row):
            if k != ".":
                cv.set(x + 2, y + 1, {"W": WHITE, "H": HULL[1], "M": HULL[2], "S": SEEDD}[k])
    cv.outline(OUT)
    return cv


def badge(i):
    cv = Canvas(8, 8)
    if i == 0:                                             # double: two little seeds
        for x in (2, 5):
            for y in range(1, 6):
                cv.set(x, y, SEEDD)
            cv.set(x, 2, WHITE)
    else:                                                  # rapid: a seed and speed lines
        for y in range(1, 6):
            cv.set(5, y, SEEDD)
        cv.set(5, 2, WHITE)
        for x, y in ((1, 2), (2, 2), (1, 4), (2, 4), (3, 6)):
            cv.set(x, y, ACC)
    cv.outline(OUT)
    return cv


def power(i):
    """A capsule: a glass orb, the power inside."""
    cv = Canvas(16, 16)
    disc(cv, 8.0, 8.0, 6.8, GLASS[1])
    disc(cv, 7.6, 7.6, 5.8, GLASS[0])
    if i == 0:
        seed_shape(cv, 5.5, 4, 7)
        seed_shape(cv, 10.5, 4, 7)
    elif i == 1:
        seed_shape(cv, 9.5, 4, 7)
        for x, y in ((4, 5), (5, 5), (3, 8), (4, 8), (5, 8), (4, 11), (5, 11)):
            cv.set(x, y, ACC)
    else:
        ring(cv, 8.0, 8.0, 3.6, GLASS[1])
        cv.set(6, 6, WHITE), cv.set(7, 5, WHITE)
    for x, y in ((4, 4), (4, 3), (5, 3)):
        cv.set(x, y, WHITE)
    cv.outline(OUT)
    return cv


def bubble():
    """The bubble shield: a thin glass ring with a glint (mostly transparent)."""
    cv = Canvas(32, 32)
    ring(cv, 16.0, 16.5, 15.0, GLASS[0], 1.2)
    ring(cv, 16.0, 16.5, 13.8, GLASS[1], 0.6)
    for a in range(200, 250, 6):
        r = math.radians(a)
        cv.set(int(16 + 12.0 * math.cos(r)), int(16.5 + 12.0 * math.sin(r)), WHITE)
    for y in range(32):                                    # it shows the ship through: keep only the rim
        for x in range(32):
            if dist(x, y, 16.0, 16.5) < 11.5:
                cv.set(x, y, None)
    return cv


# ---- the mice ----------------------------------------------------------------------------------------------------
def mouse(kind, frame):
    cv = Canvas(16, 16)
    r = {"a": MA, "b": MB, "c": MC}[kind]
    r4 = [r[0], r[0], r[1], r[2]]
    if kind == "a":                                        # round, big ears, two antennae
        for ax, tip in ((6, (5 - frame, 0)), (10, (11 - frame, 0))):
            for i in range(3):
                cv.set(ax + (tip[0] - ax) * i // 3, 3 - i, r[2])
            cv.set(tip[0], tip[1], YEL)
        for ex in (3.2, 12.8):
            disc(cv, ex, 5.5 + (frame if ex < 8 else 1 - frame) * 0.6, 2.8, r[1])
            disc(cv, ex, 5.6 + (frame if ex < 8 else 1 - frame) * 0.6, 1.3, MB[0])
        hs.ellipse(cv, 8.0, 9.6, 5.6, 4.6, r4)
        feet = ((5, 14), (10, 14)) if frame == 0 else ((4, 14), (11, 13))
        for fx, fy in feet:
            cv.set(fx, fy, r[2]), cv.set(fx + 1, fy, r[2])
    elif kind == "b":                                      # in a bubble helmet, waving
        hs.ellipse(cv, 8.0, 11.5, 4.6, 3.4, r4)
        arm = (2, 9) if frame == 0 else (2, 12)
        cv.set(arm[0], arm[1], r[1]), cv.set(arm[0] + 1, arm[1] + (1 if frame == 0 else -1), r[1])
        cv.set(15 - arm[0], arm[1], r[1]), cv.set(14 - arm[0], arm[1] + (1 if frame == 0 else -1), r[1])
        for ex in (5.0, 11.0):
            disc(cv, ex, 3.8, 1.8, r[1])
        hs.ellipse(cv, 8.0, 6.6, 3.8, 3.2, r4)
        ring(cv, 8.0, 6.2, 5.6, HELM, 1.0)
        cv.set(5, 2, WHITE), cv.set(4, 3, WHITE)
        feet = ((5, 15), (10, 15)) if frame == 0 else ((6, 15), (9, 15))
        for fx, fy in feet:
            cv.set(fx, fy, r[2])
    else:                                                  # small, tall antennae with star tips
        for ax, tx in ((6, 4 + frame), (10, 12 - frame)):
            for i in range(4):
                cv.set(ax + (tx - ax) * i // 4, 5 - i, r[2])
            cv.set(tx, 1, YEL), cv.set(tx - 1, 1, YEL if frame else None), cv.set(tx, 0, YEL)
        for ex in (4.4, 11.6):
            disc(cv, ex, 6.6, 1.9, r[1])
        hs.ellipse(cv, 8.0, 9.4, 4.4, 4.0, r4)
        legs = ((4, 13), (5, 14), (11, 13), (10, 14)) if frame == 0 else ((5, 13), (4, 14), (10, 13), (11, 14))
        for lx, ly in legs:
            cv.set(lx, ly, r[2])
    ey = {"a": 8, "b": 6, "c": 8}[kind]
    for ex in (5, 9):                                      # the eyes: white, an ink pupil, mischievous
        cv.set(ex, ey, WHITE), cv.set(ex + 1, ey, WHITE), cv.set(ex, ey + 1, WHITE), cv.set(ex + 1, ey + 1, INK)
    cv.set(7, ey + 3 if kind != "b" else ey + 2, INK), cv.set(8, ey + 3 if kind != "b" else ey + 2, INK)   # a grin
    cv.outline(OUTM)
    return cv


def pop(frame):
    cv = Canvas(16, 16)
    if frame == 0:                                         # a sparkle burst
        for d in range(1, 7):
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                cv.set(8 + dx * d - (dx < 0), 8 + dy * d - (dy < 0), YEL if d < 5 else WHITE)
        for d in range(1, 4):
            for dx, dy in ((1, 1), (-1, -1), (1, -1), (-1, 1)):
                cv.set(8 + dx * d - (dx < 0), 8 + dy * d - (dy < 0), WHITE)
        disc(cv, 7.5, 7.5, 1.6, WHITE)
    elif frame == 1:                                       # sparkles and crumbs flying
        for x, y, c in ((2, 3, YEL), (13, 2, WHITE), (1, 11, CRUMB), (14, 12, YEL), (7, 1, CRUMB), (8, 14, WHITE),
                        (5, 7, CRUMB), (11, 8, CRUMB)):
            cv.set(x, y, c)
            if c == YEL:
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    cv.set(x + dx, y + dy, WHITE)
            elif c == CRUMB:
                cv.set(x + 1, y, CRUMB), cv.set(x, y + 1, CH_DARK_M)
    else:                                                  # the last crumbs falling
        for x, y in ((1, 13), (14, 14), (6, 15), (10, 12), (3, 9)):
            cv.set(x, y, CRUMB)
        cv.set(12, 4, WHITE), cv.set(4, 3, YEL)
    return cv


CH_DARK_M = MA[2]          # a crumb's shadow (a mouse palette colour)


def splat(frame):
    cv = Canvas(8, 8)
    pts = ((3, 3), (4, 4), (2, 5), (5, 2), (4, 6)) if frame == 0 else ((1, 2), (6, 1), (0, 6), (7, 6))
    for x, y in pts:
        cv.set(x, y, CRUMB if (x + y) % 2 else YEL)
    if frame == 0:
        cv.set(3, 4, WHITE)
    return cv


def sparkle(frame):
    cv = Canvas(8, 8)
    arm = 3 if frame == 0 else 2
    for d in range(1, arm + 1):
        for x, y in ((3 + d, 3), (3 - d, 3), (3, 3 + d), (3, 3 - d)):
            cv.set(x, y, YEL)
    cv.set(3, 3, WHITE)
    return cv


def hp(frame):
    cv = Canvas(8, 8)
    for x in range(0, 7):
        for y in range(3, 6):
            full = frame == 0 or (frame == 1 and x < 3)
            cv.set(x, y, (YEL if y == 3 else CRUMB) if full else MA[2])
    cv.outline(OUTM)
    return cv


DIGITS = ["111101101101111", "010110010010111", "111001111100111", "111001011001111", "101101111001001",
          "111100111001111", "111100111101111", "111001010010010", "111101111101111", "111101111001111"]


def digit(n):
    cv = Canvas(8, 8)
    bits = DIGITS[n]
    for y in range(5):
        for x in range(3):
            if bits[y * 3 + x] == "1":
                cv.set(2 + x, 1 + y, WHITE)
    cv.outline(OUTM)
    return cv


# ---- the props -----------------------------------------------------------------------------------------------------
def crumb(frame):
    cv = Canvas(8, 8)
    pts = [(2, 1), (3, 1), (2, 2), (3, 2), (4, 2), (2, 3), (3, 3), (4, 3), (5, 3), (2, 4), (3, 4), (4, 4), (5, 4),
           (2, 5), (3, 5), (4, 5)]
    for x, y in pts:
        c = CH[0] if x + y < 5 else CH[1] if x + y < 8 else CH[2]
        cv.set(x if frame == 0 else 7 - x, y, c)
    cv.set(3 if frame == 0 else 4, 4, CH[2])               # a hole
    cv.outline(OUTP)
    return cv


def pea(frame):
    cv = Canvas(8, 8)
    hs.ellipse(cv, 4.0, 4.0, 2.6 + 0.3 * frame, 2.6 - 0.3 * frame, [PEA[0], PEA[0], PEA[1], PEA[1]])
    cv.set(3, 2, WHITE)
    cv.outline(OUTP)
    return cv


def jelly(frame):
    cv = Canvas(8, 8)
    path = [(3, 0), (4, 1), (3, 2), (4, 3), (3, 4), (4, 5)] if frame == 0 else [(4, 0), (3, 1), (4, 2), (3, 3),
                                                                                 (4, 4), (3, 5)]
    for x, y in path:
        cv.set(x, y, JEL[0]), cv.set(x + (1 if x == 3 else -1), y, JEL[1])
    cv.set(path[0][0], path[0][1], WHITE)
    cv.outline(OUTP)
    return cv


def saucer(frame):
    cv = Canvas(32, 16)
    disc(cv, 16.0, 6.5, 5.6, GLASSP)                       # the dome, a fat grey mouse at the wheel
    for ex in (12.0, 20.0):
        disc(cv, ex, 3.2, 2.0, GREY[1])
        disc(cv, ex, 3.3, 0.9, PINKP)
    hs.ellipse(cv, 16.0, 7.2, 4.2, 3.4, [GREY[0], GREY[0], GREY[1], GREY[1]])
    cv.set(14, 6, INK), cv.set(17, 6, INK), cv.set(15, 8, PINKP), cv.set(16, 8, PINKP)
    cv.set(12, 4, WHITE), cv.set(13, 3, WHITE)
    hs.ellipse(cv, 16.0, 10.5, 15.0, 4.2, [CH[0], CH[0], CH[1], CH[2]])     # the cheese wheel
    for hx, hy in ((6, 10), (11, 12), (21, 11), (26, 10), (16, 13)):
        cv.set(hx, hy, CH[2]), cv.set(hx + 1, hy, CH[2])
    for i, lx in enumerate((4, 10, 16, 22, 28)):           # the lights, blinking in turn
        cv.set(lx, 9, RED if (i + frame) % 2 else WHITE)
    cv.outline(OUTP)
    return cv


# ---- the mothership ------------------------------------------------------------------------------------------------
def boss(frame):
    cv = Canvas(64, 32)
    for cxx in (18, 32, 46):                               # three crumb cannons under it
        for y in range(25, 31):
            for x in range(cxx - 2, cxx + 2):
                cv.set(x, y, HULLB[2] if x > cxx - 1 else HULLB[1])
        if frame:
            cv.set(cxx - 1, 30, YELB), cv.set(cxx, 30, YELB)
    disc(cv, 32.0, 12.0, 11.4, GLASS[1])                   # the big dome
    disc(cv, 31.4, 11.4, 10.4, GLASS[0])
    for ex in (25.5, 38.5):                                # the captain: big ears, a cheese crown, a grin
        disc(cv, ex, 7.5, 3.4, CAP[1])
        disc(cv, ex, 7.7, 1.8, PINKB)
    hs.ellipse(cv, 32.0, 13.5, 6.4, 5.4, [CAP[0], CAP[0], CAP[1], CAP[2]])
    for x in range(28, 37):                                # the crown
        cv.set(x, 6, CHB)
        if x % 2 == 0:
            cv.set(x, 5, CHB)
    cv.set(28, 4, CHB), cv.set(32, 4, CHB), cv.set(36, 4, CHB)
    for ex in (28, 34):                                    # eyes under mischievous brows
        cv.set(ex, 12, WHITE), cv.set(ex + 1, 12, WHITE), cv.set(ex + 1, 13, INK), cv.set(ex, 13, WHITE)
    cv.set(27, 10, INK), cv.set(28, 11, INK), cv.set(36, 10, INK), cv.set(35, 11, INK)
    for x in range(30, 35):
        cv.set(x, 16, INK)
    cv.set(31, 17, WHITE), cv.set(33, 17, WHITE)           # two teeth
    cv.set(31, 15, PINKB), cv.set(32, 15, PINKB)
    for x, y in ((24, 5), (23, 6), (23, 7), (24, 4)):
        cv.set(x, y, WHITE)
    hs.ellipse(cv, 32.0, 21.0, 30.5, 6.8, [WHITE, HULLB[0], HULLB[1], HULLB[2]])     # the hull
    for i, lx in enumerate(range(8, 58, 6)):               # the lights, blinking in turn
        cv.set(lx, 22, YELB if (i + frame) % 2 else REDB)
        cv.set(lx + 1, 22, YELB if (i + frame) % 2 else REDB)
    for x in range(4, 61):
        if cv.get(x, 18) is not None:
            cv.set(x, 18, HULLB[0] if x < 32 else HULLB[1])
    cv.outline(OUTB)
    return cv


def sprite_frame(name, i):
    if name == "ship":
        return ship("blink" if i else "idle")
    if name == "ship_fire":
        return ship("fire")
    if name == "ship_hit":
        return ship("hit2" if i else "hit")
    if name.startswith("mouse_"):
        return mouse(name[6:], i)
    fn = {"bubble": bubble, "icon": icon, "seed": seed}.get(name)
    if fn:
        return fn()
    fn = {"badge": badge, "power": power, "pop": pop, "splat": splat, "sparkle": sparkle, "hp": hp, "digit": digit,
          "crumb": crumb, "pea": pea, "jelly": jelly, "saucer": saucer, "boss": boss}[name]
    return fn(i)


# ---- the BG layers -------------------------------------------------------------------------------------------------
def wedge(x, y):
    """The shelter's shape: the same formula as play.c wedge() (the game erodes these pixels)."""
    if y < 7 - x * 7 // 23:
        return False
    dx, dy = 2 * x + 1 - 24, 2 * y + 1 - 32
    return dx * dx + dy * dy >= 100


def shelter():
    """A cheese wedge: the sloping top lit, the face shaded, holes; its edge drawn inside its shape (rind)."""
    W, H = SHELTER[1], SHELTER[2]
    cv = Canvas(W, H)
    inside = {(x, y) for y in range(H) for x in range(W) if wedge(x, y)}
    holes = [(5, 11, 1.6), (17, 6, 1.8), (12, 6, 1.1), (20, 12, 1.4), (3, 9, 0.9)]
    for x, y in inside:
        top = 7 - x * 7 // 23
        c = CHEESE[0] if y - top < 2 else CHEESE[1] if y < 10 else CHEESE[2]
        for hx, hy, r in holes:
            d = dist(x, y, hx, hy)
            if d <= r:
                c = HOLE[1] if (x + 0.5 - hx) + (y + 0.5 - hy) > 0 else HOLE[0]
        edge = any((x + dx, y + dy) not in inside for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
        if edge:
            c = RIM if y == top and y - 1 < top else RIND
            if y == top:
                c = RIM
        cv.set(x, y, c)
    return cv


def moon_surface(sx):
    return 216 + (sx - 160) * (sx - 160) // 4000


def moon():
    W, H = PANORAMAS["moon"][1:]
    cv = Canvas(W, H)
    rng = random.Random(7)
    holes = []
    for k in range(70):
        X = rng.randint(0, W - 1)
        sx = X + PANO_X0
        top = moon_surface(sx) + 4
        y = rng.randint(min(top, 238), 238) - MOON_Y0
        r = rng.choice((1.4, 2.0, 2.6, 3.2))
        if top < 238:
            holes.append((X, y, r))
    for X in range(W):
        sx = X + PANO_X0
        top = moon_surface(sx) - MOON_Y0
        if top - 1 >= 0:
            cv.set(X, top - 1, RIND)
        for y in range(max(0, top), H):
            d = y - top
            c = RIM if d == 0 else CHEESE[0] if d < 3 else CHEESE[1] if y + MOON_Y0 < 221 else CHEESE[2] if \
                y + MOON_Y0 < 224 else DEEP[0] if y + MOON_Y0 < 232 else DEEP[1]
            cv.set(X, y, c)
    for hx, hy, r in holes:
        for y in range(int(hy - r) - 1, int(hy + r) + 2):
            for x in range(int(hx - r * 1.6) - 1, int(hx + r * 1.6) + 2):
                if cv.get(x, y) is None or cv.get(x, y) in (RIM, RIND):
                    continue
                dx, dy = (x + 0.5 - hx) / (r * 1.6), (y + 0.5 - hy) / r
                if dx * dx + dy * dy <= 1:
                    base = cv.get(x, y)
                    pair = HOLE_DEEP[base == DEEP[1]] if base in DEEP else (HOLE[1], HOLE[0])
                    cv.set(x, y, pair[0] if dy < 0 else pair[1])
    return cv


def decor():
    """Space, faint: in the stage only a far planet and a nebula wisp; the margins (wide screens) get a ringed planet,
    a purple planet with craters, a moonlet and a comet."""
    W, H = PANORAMAS["decor"][1:]
    cv = Canvas(W, H)
    X = lambda sx: sx - PANO_X0                            # noqa: E731

    def planet(sx, y, r, ramp):
        cx = X(sx)
        for yy in range(int(y - r) - 1, int(y + r) + 2):
            for xx in range(int(cx - r) - 1, int(cx + r) + 2):
                d = dist(xx, yy, cx, y)
                if d <= r:
                    nd = ((xx + 0.5 - cx) * -0.62 + (yy + 0.5 - y) * -0.78) / r
                    cv.set(xx, yy, ramp[0] if nd > 0.3 else ramp[1] if nd > -0.35 or len(ramp) < 3 else ramp[2])

    # the nebula: two soft bands of dithered dots across the top-left of the stage (and the left margin)
    rng = random.Random(3)
    for k in range(900):
        sx = rng.randint(-128, 200)
        base = 34 + int(14 * math.sin((sx + 128) / 60.0))
        y = base + int(rng.gauss(0, 7))
        if 0 <= y < H and hs.checker(X(sx), y):
            cv.set(X(sx), y, NEB[0] if abs(y - base) < 5 else NEB[1])
    planet(12, 150, 5, NEB)                                # a far moonlet at the stage's edge, very faint
    # the ringed planet (right margin: from 427 columns)
    cx, cy, r = 360, 74, 17
    for xx in range(X(cx) - 32, X(cx) + 33):               # the ring behind
        for yy in range(cy - 8, cy + 9):
            dx, dy = (xx + 0.5 - X(cx)) / 30.0, (yy + 0.5 - cy) / 6.5
            if 0.78 < dx * dx + dy * dy <= 1.0 and yy < cy:
                cv.set(xx, yy, RING[1])
    planet(cx, cy, r, TEAL)
    for yy in range(cy - 3, cy + 1):                       # a band on the planet
        for xx in range(X(cx) - r, X(cx) + r):
            if cv.get(xx, yy) in TEAL and (xx + yy) % 5 == 0:
                cv.set(xx, yy, TEAL[2])
    for xx in range(X(cx) - 32, X(cx) + 33):               # the ring in front
        for yy in range(cy - 8, cy + 9):
            dx, dy = (xx + 0.5 - X(cx)) / 30.0, (yy + 0.5 - cy) / 6.5
            if 0.78 < dx * dx + dy * dy <= 1.0 and yy >= cy:
                cv.set(xx, yy, RING[0])
    # the purple planet with craters (left margin)
    planet(-44, 140, 15, PURP + [PURP[1]])
    for hx, hy, rr in ((-48, 134, 2.5), (-38, 146, 2.0), (-50, 148, 1.5)):
        for yy in range(int(hy - rr) - 1, int(hy + rr) + 2):
            for xx in range(X(hx) - 3, X(hx) + 4):
                if dist(xx, yy, X(hx), hy) <= rr and cv.get(xx, yy) is not None:
                    cv.set(xx, yy, PURP[1] if yy < hy else PURP[0])
    planet(402, 160, 6, GREYM)                             # a moonlet (520+)
    # a comet (far left: 520+)
    hx, hy = X(-96), 40
    for i in range(26):
        x, y = hx + i, hy - i // 3
        if 0 <= x < W and (i < 8 or hs.checker(x, y)):
            cv.set(x, y, COMET[1])
    disc(cv, hx, hy, 1.8, COMET[0])
    return cv


def stars():
    W, H = PANORAMAS["stars"][1:]
    cv = Canvas(W, H)
    rng = random.Random(21)
    for k in range(190):
        x, y = rng.randint(1, W - 2), rng.randint(1, H - 2)
        if cv.get(x, y) is not None:
            continue
        col = STARS[k % 2] if k % 3 == 0 else STAR_FAINT[k % 2]
        cv.set(x, y, col)
        if k % 17 == 0:
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                cv.set(x + dx, y + dy, STAR_FAINT[1] if col in STARS else STAR_FAINT[0])
    return cv


def build_sheet():
    im = Image.new("RGB", (SHEET_W, SHEET_H), hs.MAGENTA)
    for name, x, y, w, h, frames, _g in SPRITES:
        for i in range(frames):
            fr = sprite_frame(name, i)
            assert (fr.w, fr.h) == (w, h), name
            im.paste(fr.image(), (x + i * w, y))
    return im


def write_all(out, preview=False):
    os.makedirs(os.path.join(out, "tiles"), exist_ok=True)
    files = {"sprites.png": build_sheet(), SHELTER[0]: shelter().image()}
    for key, fn in (("moon", moon), ("decor", decor), ("stars", stars)):
        files[PANORAMAS[key][0]] = fn().image()
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

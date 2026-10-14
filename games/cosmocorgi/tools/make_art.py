#!/usr/bin/env python3
"""Cosmo Corgi: the code-drawn art, in the 8BCraft house style (docs/art-direction.md).

Code-drawn art only, no image generation: every sprite, tile and panorama is drawn here with
games/common/tools/house_style.py (1-px outlines, 3-4-shade ramps lit from the top-left).
The sheet layout (SPRITES, BOSSES) is shared with build_assets.py.

Writes, in the art folder (default games/cosmocorgi/art):
  sprites.png (256x128): the corgi's rocket (red-tan corgi, red rocket; players 2-4 recolour the rocket's paint and
      the fur: blue and sable, green and black, purple and fawn), its icon, the puppy pods, the shield, the shots: the
      bark, the spread pellet, the star beam, the homing bones (OBJ 0); the robots: the wind-up ladybug, the propeller
      drone, the little UFO, the dasher fish, the gift carrier, the turret, an asteroid, a comet, the robo-turtle (OBJ 2,
      and gold for the capsule groups: OBJ 7); the capsules, the pops into sparkles and bolts, the robots' pellets and
      icicles, the warning arrow, the boss bar, small digits (OBJ 6)
  bosses.png (256x96): the four bosses, 64x48, two frames each (the robots' palette; the game loads one at a time)
  tiles/terrain.png (64x24): the walls, 8 tiles per theme (rock, station, ice): surface, corners, fill, sides
  tiles/clouds.png (512x240): BG3, the nebula clouds and the planets (recoloured for each section)
  tiles/stars.png (512x240): BG4, the stars (two twinkle groups, lit by the game)

    python3 games/cosmocorgi/tools/make_art.py [--out DIR] [--preview]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/cosmocorgi/LICENSE.
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
SHEET_W, SHEET_H = 256, 128
# name, x, y, w, h, frames, palette group (hero: OBJ 0 and the players' swaps; robots: OBJ 2 / gold OBJ 7; props: OBJ 6)
SPRITES = [
    ("rocket", 0, 0, 32, 24, 5, "hero"),        # level (flame A, flame B), nose up, nose down, bonk
    ("icon", 160, 0, 16, 16, 1, "hero"),        # the title's player slots, the results
    ("pod", 176, 0, 16, 16, 2, "hero"),         # a puppy pod (ears up, ears flopping)
    ("bark", 208, 0, 8, 8, 2, "hero"),          # the bark: the basic shot
    ("spread", 224, 0, 8, 8, 1, "hero"),        # the spread shot's pellet
    ("beam", 160, 16, 16, 8, 2, "hero"),        # the star beam
    ("bone", 192, 16, 8, 8, 4, "hero"),         # a homing bone: 0, 45, 90, 135 degrees
    ("shield", 0, 24, 48, 32, 1, "hero"),       # the shield bubble
    ("bug", 0, 56, 16, 16, 2, "robots"),        # the wind-up ladybug (sine waves, lines)
    ("drone", 32, 56, 16, 16, 2, "robots"),     # the propeller drone (loops, swoops, from behind)
    ("ufo", 64, 56, 16, 16, 2, "robots"),       # the little UFO (hovers, fires)
    ("dasher", 96, 56, 16, 16, 2, "robots"),    # the dasher fish: winding up, dashing
    ("carrier", 128, 56, 16, 16, 2, "robots"),  # the gift carrier (a capsule inside)
    ("turret", 160, 56, 16, 16, 3, "robots"),   # the turret: aiming left, up-left, up (V-flipped on a ceiling)
    ("rock", 208, 56, 16, 16, 1, "robots"),     # an asteroid
    ("comet", 224, 56, 16, 16, 1, "robots"),    # an icy comet
    ("tank", 0, 72, 32, 24, 2, "robots"),       # the robo-turtle (slow, tough)
    ("capsule", 0, 96, 16, 16, 6, "props"),     # spread, beam, homing, speed, shield, pod
    ("pop", 96, 96, 16, 16, 3, "props"),        # a robot pops: a sparkle burst, sparkles and bolts, bolts
    ("spark", 144, 96, 8, 8, 2, "props"),
    ("bolt", 144, 104, 8, 8, 2, "props"),
    ("pellet", 160, 96, 8, 8, 2, "props"),      # the robots' shots: a pink pellet (pulsing) ...
    ("big", 160, 104, 8, 8, 2, "props"),        # ... a big one (the walrus' snowball) ...
    ("icicle", 176, 96, 16, 8, 1, "props"),     # ... an icicle
    ("fizzle", 176, 104, 8, 8, 2, "props"),     # a shot ends (its reach, a wall)
    ("warn", 192, 96, 16, 16, 2, "props"),      # robots coming from behind
    ("hp", 224, 96, 8, 8, 3, "props"),          # the boss bar: full, half, empty
    ("digit", 0, 112, 8, 8, 10, "props"),       # small digits (points)
]
BOSSES = [("ladybird", 0, 0), ("crab", 128, 0), ("owl", 0, 48), ("walrus", 128, 48)]   # two 64x48 frames each
PANORAMAS = {"clouds": ("tiles/clouds.png", 512, 240), "stars": ("tiles/stars.png", 512, 240)}
TERRAIN = ("tiles/terrain.png", 64, 24)
TER_KINDS = ["top", "top_l", "top_lr", "fill", "fill2", "side_l", "side_lr", "top2"]

# ---- colours (each group fits one 15-colour sprite palette) ---------------------------------------------------------
WHITE = (250, 250, 250)
INK = hs.HOUSE["ink"][0]
# the hero: outline, ink, white, the hull's greys, the paint (swapped), the fur (swapped), glass, yellow, pink
OUT = (40, 20, 56)
HULL = [(206, 214, 232), (140, 150, 180)]
ACC = [(255, 132, 120), (230, 66, 74), (156, 30, 58)]                    # P1: a red rocket
ACC_P = {1: [(146, 200, 255), (72, 132, 236), (40, 72, 170)],           # P2 blue
         2: [(156, 236, 140), (72, 190, 92), (34, 118, 62)],            # P3 green
         3: [(228, 166, 255), (172, 98, 232), (108, 54, 168)]}          # P4 purple
FUR = [(255, 212, 146), (238, 150, 70), (178, 96, 40)]                   # P1: a red-tan corgi
FUR_P = {1: [(228, 192, 142), (172, 120, 72), (112, 70, 40)],           # P2 sable
         2: [(158, 156, 176), (88, 86, 108), (46, 44, 62)],             # P3 black (tricolour)
         3: [(255, 242, 210), (240, 214, 162), (198, 168, 118)]}        # P4 fawn
GLASS = [(200, 240, 255), (120, 188, 232)]
YEL = (255, 228, 96)
PINK = (255, 150, 172)
# the robots: outline, ink, white, metal, red, teal, yellow, pink, brown (the gold groups: metal, teal, red golden)
OUTR = (34, 20, 54)
MET = [(214, 222, 238), (152, 162, 192), (96, 104, 140)]
RED = [(255, 120, 110), (226, 58, 70), (150, 28, 54)]
TEAL = [(140, 232, 220), (52, 170, 170)]
YELR = (255, 222, 90)
PINKR = (255, 160, 190)
BRN = [(178, 136, 110), (118, 84, 72)]
GOLD = {MET[0]: (255, 244, 170), MET[1]: (246, 200, 70), MET[2]: (186, 128, 30),
        TEAL[0]: (255, 236, 140), TEAL[1]: (230, 170, 50),
        RED[0]: (255, 190, 110), RED[1]: (240, 140, 50), RED[2]: (170, 84, 30)}
# the props
OUTP = (36, 20, 52)
YELP = (255, 230, 100)
ORANGE = (255, 156, 64)
BPINK = (255, 112, 186)
BMAG = (196, 40, 140)
GLASSP = [(200, 240, 255), (110, 176, 228)]
GREEN = (110, 220, 110)
REDP = (240, 72, 72)
BLUEP = (90, 120, 240)
GREYP = [(190, 196, 214), (120, 126, 150)]
DARKP = (70, 52, 96)
# the walls: rock, station metal, ice (one BG palette each)
ROCK = [(176, 132, 128), (138, 98, 104), (102, 70, 86), (70, 48, 70), (52, 34, 56)]
ROCK_LIP = [(226, 186, 160), (200, 156, 136)]
METAL = [(200, 214, 230), (150, 166, 190), (104, 118, 148), (70, 80, 112), (46, 52, 82)]
STRIPE = [(255, 210, 80), (40, 36, 60)]
LAMP = (120, 255, 200)
ICE = [(246, 252, 255), (196, 232, 250), (140, 196, 236), (96, 150, 210), (64, 104, 170)]
ICE_DEEP = (46, 74, 140)
# the clouds (BG3): drawn in the nebula's colours; build_assets.py recolours them for the other sections
CLOUD = [(112, 72, 140), (88, 56, 120), (68, 44, 102)]
PLANET = [(150, 110, 170), (112, 80, 140), (84, 58, 118)]
RINGC = [(176, 150, 190), (132, 108, 156)]
MOON = [(132, 120, 162), (100, 90, 132)]
CLOUD_THEMES = {   # per section: the same entries, in its colours (rock: dusty amber; station: teal; ice: pale blue)
    1: {CLOUD[0]: (136, 98, 84), CLOUD[1]: (100, 72, 70), CLOUD[2]: (74, 52, 58), PLANET[0]: (170, 120, 90),
        PLANET[1]: (132, 88, 72), PLANET[2]: (98, 64, 60), RINGC[0]: (190, 160, 130), RINGC[1]: (146, 118, 100),
        MOON[0]: (140, 120, 120), MOON[1]: (104, 88, 96)},
    2: {CLOUD[0]: (56, 122, 136), CLOUD[1]: (40, 92, 110), CLOUD[2]: (30, 66, 88), PLANET[0]: (90, 150, 170),
        PLANET[1]: (64, 116, 140), PLANET[2]: (46, 88, 116), RINGC[0]: (150, 190, 200), RINGC[1]: (110, 150, 168),
        MOON[0]: (110, 140, 160), MOON[1]: (80, 106, 132)},
    3: {CLOUD[0]: (124, 166, 210), CLOUD[1]: (92, 132, 184), CLOUD[2]: (68, 102, 158), PLANET[0]: (190, 210, 240),
        PLANET[1]: (150, 176, 220), PLANET[2]: (112, 140, 196), RINGC[0]: (220, 230, 250), RINGC[1]: (170, 190, 230),
        MOON[0]: (170, 186, 220), MOON[1]: (130, 148, 196)},
}
STARS = [(255, 255, 160), (160, 255, 255)]                # the twinkle keys (lit by the game)
STAR_FAINT = [(110, 110, 170), (170, 176, 220)]


def dist(x, y, cx, cy):
    return math.hypot(x + 0.5 - cx, y + 0.5 - cy)


def disc(cv, cx, cy, r, col):
    for y in range(int(cy - r) - 1, int(cy + r) + 2):
        for x in range(int(cx - r) - 1, int(cx + r) + 2):
            if dist(x, y, cx, cy) <= r:
                cv.set(x, y, col)


def ring(cv, cx, cy, rx, ry, col, width=1.0):
    for y in range(int(cy - ry) - 2, int(cy + ry) + 3):
        for x in range(int(cx - rx) - 2, int(cx + rx) + 3):
            d = math.hypot((x + 0.5 - cx) / rx, (y + 0.5 - cy) / ry)
            if 1.0 - width / min(rx, ry) < d <= 1.0:
                cv.set(x, y, col)


def poly(cv, pts, col):
    """A filled polygon (even-odd, pixel centres)."""
    ys = [p[1] for p in pts]
    for y in range(int(min(ys)) - 1, int(max(ys)) + 2):
        yc = y + 0.5
        xs = []
        for i in range(len(pts)):
            (x0, y0), (x1, y1) = pts[i], pts[(i + 1) % len(pts)]
            if (y0 <= yc < y1) or (y1 <= yc < y0):
                xs.append(x0 + (yc - y0) * (x1 - x0) / (y1 - y0))
        xs.sort()
        for a, b in zip(xs[::2], xs[1::2]):
            for x in range(int(math.ceil(a - 0.5)), int(math.floor(b - 0.5)) + 1):
                cv.set(x, y, col)


def shear(cv, k, cx=16.0):
    """Tilt a drawing: each column moves up or down by k px per px from cx (the rocket's nose up / down)."""
    out = Canvas(cv.w, cv.h)
    for y in range(cv.h):
        for x in range(cv.w):
            c = cv.get(x, y)
            if c is not None:
                out.set(x, int(round(y + (x - cx) * k)), c)
    return out


def r4(r):
    return [r[0], r[0], r[1], r[2]] if len(r) == 3 else [r[0], r[0], r[1], r[1]]


# ---- the corgi and its rocket --------------------------------------------------------------------------------------
def corgi_head(cv, cx, cy, s=1.0, eyes="open", ears_back=0):
    """The corgi's head, looking right: big pointed ears, a white blaze and muzzle, the house eye, a black nose."""
    for ex, lean in ((-2.6, -1.0), (1.4, 0.4)):            # the ears (the far one first)
        bx = cx + ex * s
        tip = (bx + (lean - ears_back * 1.6) * s, cy - 7.2 * s + ears_back * 1.2)
        poly(cv, [(bx - 1.9 * s, cy - 2.0 * s), (bx + 1.9 * s, cy - 2.0 * s), tip], FUR[1])
        poly(cv, [(bx - 0.8 * s, cy - 2.4 * s), (bx + 0.9 * s, cy - 2.4 * s), (tip[0], tip[1] + 2.0 * s)], FUR[0])
    hs.ellipse(cv, cx, cy, 4.4 * s, 3.8 * s, [FUR[0], FUR[0], FUR[1], FUR[2]])
    for y in range(int(cy - 3 * s), int(cy + 1)):               # the blaze up the forehead
        cv.set(int(round(cx + 0.5 * s)), y, WHITE)
    hs.ellipse(cv, cx + 2.6 * s, cy + 1.3 * s, 2.7 * s, 1.9 * s, None, flat=WHITE)   # the muzzle
    cv.set(int(round(cx + 5.0 * s)), int(round(cy + 0.4 * s)), INK)                  # the nose
    cv.set(int(round(cx + 4.4 * s)), int(round(cy + 0.4 * s)), INK)
    ex, ey = int(round(cx + 0.9 * s)), int(round(cy - 1.6 * s))
    if eyes == "x":
        for d in range(3):
            cv.set(ex - 1 + d, ey + d, INK), cv.set(ex + 1 - d, ey + d, INK)
    elif eyes == "happy":
        cv.set(ex - 1, ey + 1, INK), cv.set(ex, ey, INK), cv.set(ex + 1, ey + 1, INK)
    else:
        hs.eye(cv, ex, ey, look=(1, 0), white=WHITE, pupil=INK)
    cv.set(int(round(cx - 1.6 * s)), int(round(cy + 1.4 * s)), PINK)                   # a pink cheek


def rocket(pose):
    """The rocket, 32x24, flying right: a white hull, the red paint (nose, stripe, fins), the corgi in its cockpit."""
    cv = Canvas(32, 24)
    hs.capsule(cv, 9.0, 15.5, 22.0, 15.5, 5.2, [WHITE, WHITE, HULL[0], HULL[1]])
    poly(cv, [(22.0, 10.4), (30.6, 15.5), (22.0, 20.6)], ACC[1])                       # the nose cone
    poly(cv, [(22.0, 10.4), (30.6, 15.5), (22.0, 13.0)], ACC[0])
    poly(cv, [(22.0, 18.0), (30.6, 15.5), (22.0, 20.6)], ACC[2])
    for y in range(11, 21):                                                            # the stripe
        if cv.get(12, y) is not None:
            cv.set(12, y, ACC[1] if y < 18 else ACC[2]), cv.set(13, y, ACC[1] if y < 18 else ACC[2])
    poly(cv, [(4.0, 11.5), (9.5, 11.5), (4.0, 6.0)], ACC[1])                           # the fins
    poly(cv, [(4.0, 19.5), (9.5, 19.5), (4.0, 24.0)], ACC[2])
    cv.set(17, 17, GLASS[1]), cv.set(18, 17, GLASS[1]), cv.set(17, 16, GLASS[0])        # a porthole
    cv.set(18, 16, GLASS[1])
    for x in range(14, 23):                                                            # the cockpit's rim
        cv.set(x, 11, HULL[1])
    corgi_head(cv, 18.0, 8.6, 1.0, "x" if pose == 4 else "open", ears_back=1 if pose in (1, 2) else 0)
    if pose in (0, 1, 2, 3):                                                           # the flame
        n = 5 if pose == 1 else 4
        for i in range(n):
            for y in range(14 - (1 if i < n - 1 else 0), 18 + (1 if i < n - 1 else 0)):
                cv.set(3 - i, y, YEL if i < n - 2 and 14 < y < 17 else ACC[0])
    if pose == 4:                                                                      # bonk: a dent, sparks
        cv.set(25, 13, HULL[1]), cv.set(26, 14, HULL[1])
    k = {2: -0.13, 3: 0.13, 4: 0.2}.get(pose, 0.0)
    if k:
        cv = shear(cv, k)
    cv.outline(OUT)
    return cv


def icon():
    cv = Canvas(16, 16)
    corgi_head(cv, 7.0, 10.0, 1.15)
    cv.outline(OUT)
    return cv


def pod(frame):
    """A puppy pod: a glass bubble, a corgi puppy inside."""
    cv = Canvas(16, 16)
    disc(cv, 8.0, 8.5, 6.8, GLASS[1])
    disc(cv, 7.6, 8.1, 5.9, GLASS[0])
    for ex in (5.2, 9.8):                                                              # ears (flopping in frame 1)
        tip = (ex - 0.8, 3.0) if frame == 0 else (ex - 1.8 if ex < 8 else ex + 1.8, 5.0)
        poly(cv, [(ex - 1.5, 7.0), (ex + 1.5, 7.0), tip], FUR[1])
    hs.ellipse(cv, 7.5, 9.0, 3.6, 3.0, [FUR[0], FUR[0], FUR[1], FUR[2]])
    hs.ellipse(cv, 8.6, 10.3, 2.0, 1.3, None, flat=WHITE)
    cv.set(6, 8, INK), cv.set(9, 8, INK), cv.set(9, 10, INK)
    cv.set(4, 4, WHITE), cv.set(5, 3, WHITE)
    cv.outline(OUT)
    return cv


def bark(frame):
    """The bark: a little ) of sound, the paint colour around a yellow core."""
    cv = Canvas(8, 8)
    pts = [(3, 1), (4, 2), (5, 3), (5, 4), (4, 5), (3, 6)] if frame == 0 else [(2, 1), (4, 2), (5, 3), (5, 4), (4, 5),
                                                                                 (2, 6)]
    for x, y in pts:
        cv.set(x, y, YEL)
        cv.set(x - 1, y, ACC[0])
    cv.outline(OUT)
    return cv


def spread():
    cv = Canvas(8, 8)
    disc(cv, 4.0, 4.0, 2.2, ACC[0])
    cv.set(3, 3, YEL), cv.set(4, 3, YEL), cv.set(3, 4, YEL), cv.set(4, 4, WHITE)
    cv.outline(OUT)
    return cv


def beam(frame):
    cv = Canvas(16, 8)
    for x in range(0, 16):
        cv.set(x, 3, WHITE), cv.set(x, 4, WHITE)
        if 1 <= x <= 14:
            cv.set(x, 2, ACC[0]), cv.set(x, 5, ACC[0])
    for x in (3, 9) if frame == 0 else (6, 12):
        cv.set(x, 1, YEL), cv.set(x, 6, YEL)
    cv.outline(OUT)
    return cv


def bone(frame):
    """A dog bone at 0, 45, 90 or 135 degrees."""
    cv = Canvas(8, 8)
    a = math.radians(frame * 45)
    dx, dy = math.cos(a) * 2.2, -math.sin(a) * 2.2
    hs.capsule(cv, 3.5 - dx, 3.5 - dy, 3.5 + dx, 3.5 + dy, 0.8, None, flat=WHITE)
    nx, ny = -dy / 2.2 * 1.1, dx / 2.2 * 1.1
    for sx, sy in ((3.5 - dx, 3.5 - dy), (3.5 + dx, 3.5 + dy)):
        disc(cv, sx + nx, sy + ny, 0.9, WHITE)
        disc(cv, sx - nx, sy - ny, 0.9, HULL[0])
    cv.outline(OUT)
    return cv


def shield():
    cv = Canvas(48, 32)
    ring(cv, 24.0, 15.5, 22.5, 14.5, GLASS[0], 1.2)
    ring(cv, 24.0, 15.5, 21.0, 13.2, GLASS[1], 0.7)
    for a in range(200, 250, 7):
        r = math.radians(a)
        cv.set(int(24 + 19.0 * math.cos(r)), int(15.5 + 11.6 * math.sin(r)), WHITE)
    for y in range(32):                                    # keep the rim only (the rocket shows through)
        for x in range(48):
            if math.hypot((x + 0.5 - 24) / 19.5, (y + 0.5 - 15.5) / 11.0) < 1.0:
                cv.set(x, y, None)
    return cv


# ---- the robots (they face left) ----------------------------------------------------------------------------------
def robot_eye(cv, x, y, look=-1):
    cv.rect(x, y, x + 1, y + 2, WHITE)
    cv.set(x if look < 0 else x + 1, y + 1, INK), cv.set(x if look < 0 else x + 1, y + 2, INK)


def bug(frame):
    cv = Canvas(16, 16)
    for lx, ly in ((5, 13), (8, 14), (11, 13)):                                        # little legs
        cv.set(lx + (frame if lx != 8 else -frame), ly, MET[2])
    hs.ellipse(cv, 9.0, 8.5, 6.0, 5.2, r4(RED))
    for sx, sy in ((10, 6), (12, 9), (8, 10)):                                         # spots
        cv.set(sx, sy, INK), cv.set(sx + 1, sy, INK)
    hs.ellipse(cv, 4.0, 9.5, 3.2, 3.0, r4(MET))                                        # the face
    robot_eye(cv, 2, 8)
    cv.set(5, 11, PINKR)
    kx = 11                                                                            # the wind-up key
    cv.set(kx, 3, MET[2]), cv.set(kx, 2, MET[2])
    if frame == 0:
        for y in (0, 1):
            cv.set(kx - 1, y, YELR), cv.set(kx + 1, y, YELR)
        cv.set(kx, 0, YELR)
    else:
        cv.set(kx, 1, YELR), cv.set(kx, 0, YELR)
    cv.outline(OUTR)
    return cv


def drone(frame):
    cv = Canvas(16, 16)
    hs.ellipse(cv, 8.0, 9.5, 5.6, 5.0, r4(TEAL))
    disc(cv, 6.3, 9.0, 2.6, WHITE)                                                     # one big eye
    cv.set(5, 9, INK), cv.set(5, 10, INK), cv.set(6, 9, INK), cv.set(6, 10, INK)
    cv.set(7, 8, WHITE)
    cv.set(8, 4, MET[1]), cv.set(8, 3, MET[1])                                         # the mast, the propeller
    span = 5 if frame == 0 else 2
    for x in range(8 - span, 9 + span):
        cv.set(x, 2, MET[0] if x < 8 else MET[1])
    cv.set(11, 13, YELR)
    cv.outline(OUTR)
    return cv


def ufo(frame):
    cv = Canvas(16, 16)
    disc(cv, 8.0, 7.5, 3.8, TEAL[0])                                                   # the dome, a robot inside
    cv.set(6, 7, INK), cv.set(9, 7, INK), cv.set(6, 4, WHITE)
    cv.set(8, 3, YELR)
    hs.ellipse(cv, 8.0, 10.5, 7.5, 2.8, r4(MET))
    for i, lx in enumerate((3, 6, 9, 12)):
        cv.set(lx, 11, YELR if (i + frame) % 2 else PINKR)
    cv.outline(OUTR)
    return cv


def dasher(frame):
    cv = Canvas(16, 16)
    poly(cv, [(11.0, 8.0), (15.0, 4.0), (15.0, 12.0)], RED[1])                         # the tail fin
    hs.ellipse(cv, 7.5, 8.0, 6.0, 3.8, r4(MET))
    poly(cv, [(6.0, 5.0), (10.0, 5.0), (9.0, 2.0)], RED[0])                            # the top fin
    robot_eye(cv, 3, 6)
    cv.set(2, 10, INK), cv.set(3, 10, INK)
    if frame == 1:                                                                     # dashing: a flame
        for x in (14, 15):
            cv.set(x, 7, YELR), cv.set(x, 8, YELR)
    else:                                                                              # winding up: the key
        cv.set(8, 12, MET[2]), cv.set(8, 13, YELR), cv.set(7, 14, YELR), cv.set(9, 14, YELR)
    cv.outline(OUTR)
    return cv


def carrier(frame):
    cv = Canvas(16, 16)
    hs.ellipse(cv, 8.0, 5.5, 4.5, 3.0, r4(TEAL))
    robot_eye(cv, 5, 4)
    span = 4 if frame == 0 else 2
    for x in range(8 - span, 9 + span):
        cv.set(x, 1, MET[1])
    cv.set(8, 2, MET[1])
    cv.rect(4, 9, 11, 14, RED[1])                                                      # the gift box
    cv.rect(4, 9, 11, 9, RED[0])
    for y in range(9, 15):
        cv.set(7, y, YELR), cv.set(8, y, YELR)
    cv.set(6, 8, YELR), cv.set(9, 8, YELR)
    cv.outline(OUTR)
    return cv


def turret(frame):
    cv = Canvas(16, 16)
    ang = [180, 135, 90][frame]                                                        # the barrel
    a = math.radians(ang)
    for y in range(9, 16):                                                             # the dome base
        for x in range(1, 15):
            if math.hypot(x + 0.5 - 8.0, (y + 0.5 - 16.0) * 1.0) <= 7.0:
                d = x + 0.5 - 8.0
                cv.set(x, y, MET[0] if d < -3 else MET[1] if d < 3 else MET[2])
    hs.capsule(cv, 8.0, 10.5, 8.0 + math.cos(a) * 7.0, 10.5 - math.sin(a) * 7.0, 1.6, None, flat=MET[2])
    cv.set(int(round(8.0 + math.cos(a) * 7.0)), int(round(10.5 - math.sin(a) * 7.0)), INK)
    cv.set(7, 12, RED[1]), cv.set(8, 12, RED[1]), cv.set(7, 11, RED[0])
    cv.outline(OUTR)
    return cv


def rock():
    cv = Canvas(16, 16)
    rng = random.Random(4)
    for y in range(16):
        for x in range(16):
            ang = math.atan2(y + 0.5 - 8, x + 0.5 - 8)
            r = 6.2 + 0.9 * math.sin(ang * 3 + 1) + 0.5 * math.cos(ang * 5)
            if dist(x, y, 8, 8) <= r:
                nd = ((x + 0.5 - 8) * -0.62 + (y + 0.5 - 8) * -0.78) / r
                cv.set(x, y, BRN[0] if nd > 0.2 else BRN[1] if nd > -0.5 else MET[2])
    for cx, cy, rr in ((6, 7, 1.4), (10, 10, 1.1), (9, 5, 0.8)):
        disc(cv, cx, cy, rr, BRN[1])
        cv.set(int(cx), int(cy - rr), MET[2])
    cv.outline(OUTR)
    return cv


def comet():
    cv = Canvas(16, 16)
    for i in range(8):                                                                 # the tail, up and right
        x, y = 9 + i, 6 - i // 2
        if i < 4 or hs.checker(x, y):
            cv.set(x, y, TEAL[0]), cv.set(x, y + 1, TEAL[1] if i < 5 else None)
    hs.ellipse(cv, 7.0, 9.0, 4.4, 4.0, [WHITE, WHITE, TEAL[0], MET[1]])
    cv.set(5, 7, WHITE), cv.set(6, 10, MET[0])
    cv.outline(OUTR)
    return cv


def tank(frame):
    cv = Canvas(32, 24)
    for lx in (9, 14, 20, 25):                                                         # the legs, walking
        up = (lx // 5 + frame) % 2
        cv.rect(lx, 18 - up, lx + 2, 21 - up, MET[2])
    hs.ellipse(cv, 18.0, 13.0, 11.0, 8.0, r4(TEAL))                                    # the shell
    for hx, hy in ((14, 10), (20, 9), (24, 13), (17, 15), (11, 14)):                   # its plates (rivets)
        cv.set(hx, hy, TEAL[0]), cv.set(hx + 1, hy, MET[0])
    cv.rect(8, 16, 28, 17, MET[1])                                                     # the rim
    hs.ellipse(cv, 6.0, 13.0, 4.4, 3.8, r4(MET))                                       # the head
    robot_eye(cv, 3, 11)
    cv.set(2, 15, INK), cv.set(3, 15, INK)
    cv.rect(11, 8, 13, 9, MET[2])                                                      # the cannon on its back
    cv.set(10, 8, MET[2]), cv.set(9, 8, MET[2])
    cv.set(22, 5, YELR), cv.set(22, 6, MET[2])                                         # the antenna
    cv.outline(OUTR)
    return cv


# ---- the bosses (64x48, the robots' palette) ------------------------------------------------------------------------
def boss_ladybird(frame):
    cv = Canvas(64, 48)
    for i, lx in enumerate((22, 30, 38, 46)):                                          # legs
        up = (i + frame) % 2
        cv.rect(lx, 38 - up, lx + 1, 43 - up, MET[2])
    if frame:                                                                          # wings peeking out
        poly(cv, [(40.0, 8.0), (62.0, 2.0), (60.0, 14.0)], TEAL[0])
    hs.ellipse(cv, 37.0, 22.0, 22.0, 17.0, r4(RED))
    for x in range(36, 38):                                                            # the shell's seam
        for y in range(6, 39):
            if cv.get(x, y) is not None:
                cv.set(x, y, RED[2])
    for sx, sy, rr in ((30, 15, 2.6), (45, 14, 2.4), (28, 28, 2.2), (46, 28, 2.6), (37, 34, 1.8)):
        disc(cv, sx, sy, rr, INK)
    hs.ellipse(cv, 14.0, 25.0, 11.0, 10.0, r4(MET))                                    # the face
    for ex in (8, 15):
        cv.rect(ex, 20, ex + 3, 24, WHITE)
        cv.rect(ex, 22, ex + 1, 24, INK)
        cv.set(ex + 2, 20, WHITE)
    disc(cv, 7.0, 29.0, 1.6, PINKR), disc(cv, 20.0, 29.0, 1.6, PINKR)
    cv.rect(11, 31, 14, 31, INK)
    for ax, tip in ((10, (4, 8)), (16, (14, 6))):                                      # antennae
        for i in range(8):
            cv.set(ax + (tip[0] - ax) * i // 8, 16 - (16 - tip[1]) * i // 8, MET[2])
        disc(cv, tip[0], tip[1], 1.5, YELR)
    kx, ky = 59, 22                                                                    # the wind-up key
    cv.rect(kx - 2, ky - 1, kx, ky + 1, MET[2])
    if frame == 0:
        cv.rect(kx + 1, ky - 6, kx + 3, ky + 6, YELR)
        cv.rect(kx + 1, ky - 1, kx + 3, ky + 1, MET[1])
    else:
        cv.rect(kx + 1, ky - 2, kx + 3, ky + 2, YELR)
    cv.outline(OUTR)
    return cv


def boss_crab(frame):
    cv = Canvas(64, 48)
    for i, lx in enumerate((28, 36, 44, 52)):                                          # legs
        up = (i + frame) % 2
        poly(cv, [(lx, 34.0), (lx + 3, 34.0), (lx + 5, 45.0 - up), (lx + 3, 45.0 - up)], BRN[1])
    hs.ellipse(cv, 40.0, 26.0, 21.0, 14.0, [BRN[0], BRN[0], BRN[1], RED[2]])           # the shell
    for sx, sy in ((34, 20), (46, 22), (40, 30)):
        disc(cv, sx, sy, 2.2, RED[1])
        cv.set(sx - 1, sy - 1, RED[0])
    for ex, ey in ((24, 9), (31, 7)):                                                   # eye stalks
        cv.rect(ex, ey + 3, ex + 1, 18, MET[2])
        disc(cv, ex + 0.5, ey + 1.5, 2.6, WHITE)
        cv.set(ex - 1, ey + 1, INK), cv.set(ex - 1, ey + 2, INK)
    for cy, flip in ((11.0, -1), (37.0, 1)):                                           # the claws
        hs.capsule(cv, 26.0, 26.0 + flip * 2, 14.0, cy, 2.4, r4(MET))
        hs.ellipse(cv, 10.0, cy, 7.0, 5.0, r4(RED))
        gap = 3.0 if frame else 1.0
        for x in range(1, 9):
            for dy in range(-int(gap), int(gap) + 1):
                cv.set(x, int(cy) + dy, None)
        cv.set(2, int(cy) - int(gap) - 1, WHITE)
    cv.rect(24, 28, 28, 29, INK)                                                       # a grin
    cv.outline(OUTR)
    return cv


def boss_owl(frame):
    cv = Canvas(64, 48)
    if frame:                                                                          # wings out
        poly(cv, [(18.0, 22.0), (2.0, 34.0), (16.0, 38.0)], MET[1])
        poly(cv, [(50.0, 22.0), (62.0, 34.0), (50.0, 38.0)], MET[2])
    hs.ellipse(cv, 34.0, 26.0, 18.0, 20.0, r4(TEAL))                                   # the body
    for ex in (20.0, 46.0):                                                            # ear tufts
        poly(cv, [(ex - 5, 12.0), (ex + 5, 12.0), (ex + (-4 if ex < 34 else 4), 1.0)], TEAL[1])
    hs.ellipse(cv, 34.0, 20.0, 14.0, 9.0, None, flat=MET[0])                          # the face disc
    for ex in (27.0, 41.0):                                                            # big round eyes
        disc(cv, ex, 20.0, 5.6, YELR)
        disc(cv, ex, 20.0, 3.6, WHITE)
        if frame:
            cv.rect(int(ex) - 4, 17, int(ex) + 4, 19, MET[1])
        disc(cv, ex - 1.0, 21.0, 1.8, INK)
    poly(cv, [(32.0, 24.0), (36.0, 24.0), (34.0, 29.0)], YELR)                         # the beak
    for y in range(34, 44, 3):                                                         # the chest's feathers
        for x in range(27, 42, 4):
            cv.set(x + (y // 3) % 2, y, MET[0])
    for fx_ in (29, 38):                                                               # feet
        cv.rect(fx_, 45, fx_ + 3, 46, YELR)
    cv.outline(OUTR)
    return cv


def boss_walrus(frame):
    cv = Canvas(64, 48)
    hs.ellipse(cv, 38.0, 28.0, 24.0, 16.0, [WHITE, MET[0], MET[1], MET[2]])            # the body
    poly(cv, [(46.0, 38.0), (60.0, 46.0), (40.0, 46.0)], MET[1])                       # flippers
    poly(cv, [(20.0, 38.0), (10.0, 46.0), (26.0, 46.0)], MET[1])
    hs.ellipse(cv, 16.0, 24.0, 12.0, 10.0, [WHITE, MET[0], MET[1], MET[2]])            # the head
    cv.rect(6, 15, 25, 17, TEAL[1])                                                    # the goggles
    for gx in (9, 18):
        disc(cv, gx + 1.5, 17.5, 3.4, TEAL[0])
        disc(cv, gx + 1.5, 17.5, 1.6, INK)
        cv.set(gx, 15, WHITE)
    hs.ellipse(cv, 12.0, 26.0, 7.0, 4.0, None, flat=MET[0])                           # the muzzle, whiskers
    for wx, wy in ((8, 25), (11, 26), (14, 25), (9, 27), (13, 27)):
        cv.set(wx, wy, MET[2])
    cv.rect(10, 22, 13, 23, INK)                                                       # the nose
    if frame:                                                                          # the mouth open
        hs.ellipse(cv, 12.0, 31.0, 3.0, 2.0, None, flat=RED[2])
    for tx in (8, 15):                                                                 # the tusks
        cv.rect(tx, 29, tx + 1, 39 + (1 if frame else 0), WHITE)
        cv.set(tx + 1, 39 + (1 if frame else 0), MET[1])
    for sx, sy in ((40, 20), (50, 26), (34, 32)):                                      # frost
        cv.set(sx, sy, TEAL[0]), cv.set(sx + 1, sy, WHITE)
    cv.outline(OUTR)
    return cv


BOSS_FN = {"ladybird": boss_ladybird, "crab": boss_crab, "owl": boss_owl, "walrus": boss_walrus}


# ---- the props ------------------------------------------------------------------------------------------------------
CAP_RIM = [ORANGE, BLUEP, GREYP[0], GREEN, GLASSP[1], BPINK]


def capsule(kind):
    """A capsule: a glass orb, its rim the power's colour, the power inside."""
    cv = Canvas(16, 16)
    disc(cv, 8.0, 8.0, 7.2, CAP_RIM[kind])
    disc(cv, 7.8, 7.8, 5.7, GLASSP[0])
    if kind == 0:                                                      # spread: three shots fanning out
        for tx, ty in ((11, 4), (12, 8), (11, 12)):
            for i in range(6):
                x, y = 4 + (tx - 4) * i / 5.0, 8 + (ty - 8) * i / 5.0
                cv.set(int(round(x)), int(round(y)), REDP if i < 4 else ORANGE)
    elif kind == 1:                                                    # the star beam: a bar
        for x in range(3, 13):
            cv.set(x, 7, WHITE), cv.set(x, 8, BLUEP)
        cv.set(12, 6, YELP)
    elif kind == 2:                                                    # homing: a bone
        bone_px = [(x, 8) for x in range(5, 11)] + [(x, 7) for x in range(6, 10)] + [(3, 6), (4, 6), (3, 10), (4, 10),
                   (12, 6), (11, 6), (12, 10), (11, 10), (3, 7), (4, 7), (4, 8), (3, 9), (4, 9), (11, 7), (12, 7),
                   (11, 8), (11, 9), (12, 9)]
        for x, y in bone_px:
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                if (x + dx, y + dy) not in bone_px:
                    cv.set(x + dx, y + dy, GREYP[1])
        for x, y in bone_px:
            cv.set(x, y, WHITE)
    elif kind == 3:                                                    # speed: chevrons
        for i in (0, 4):
            for d in range(3):
                cv.set(5 + i + d, 5 + d, GREEN), cv.set(5 + i + d, 11 - d, GREEN)
    elif kind == 4:                                                    # shield: a bubble
        for a in range(0, 360, 20):
            r = math.radians(a)
            cv.set(int(round(7.5 + 3.4 * math.cos(r))), int(round(7.5 + 3.4 * math.sin(r))), BLUEP)
    else:                                                              # a puppy pod: a little face
        disc(cv, 8.0, 8.5, 3.0, ORANGE)
        cv.set(6, 4, ORANGE), cv.set(10, 4, ORANGE), cv.set(6, 5, ORANGE), cv.set(10, 5, ORANGE)
        cv.set(7, 8, OUTP), cv.set(9, 8, OUTP), cv.set(8, 10, WHITE)
    cv.set(4, 4, WHITE), cv.set(5, 3, WHITE), cv.set(3, 5, WHITE)
    cv.outline(OUTP)
    return cv


def pop(frame):
    cv = Canvas(16, 16)
    if frame == 0:                                                     # a sparkle burst
        for d in range(1, 7):
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                cv.set(8 + dx * d - (dx < 0), 8 + dy * d - (dy < 0), YELP if d < 5 else WHITE)
        for d in range(1, 4):
            for dx, dy in ((1, 1), (-1, -1), (1, -1), (-1, 1)):
                cv.set(8 + dx * d - (dx < 0), 8 + dy * d - (dy < 0), WHITE)
        disc(cv, 7.5, 7.5, 1.8, WHITE)
    elif frame == 1:                                                   # sparkles and bolts flying
        for x, y in ((2, 3), (13, 2), (14, 12), (7, 1)):
            cv.set(x, y, YELP)
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                cv.set(x + dx, y + dy, WHITE)
        for x, y in ((1, 11), (8, 14), (5, 7), (11, 8)):
            cv.set(x, y, GREYP[0]), cv.set(x + 1, y, GREYP[1]), cv.set(x, y + 1, GREYP[1])
    else:                                                              # the last bolts
        for x, y in ((1, 13), (14, 14), (6, 15), (10, 12)):
            cv.set(x, y, GREYP[1]), cv.set(x + 1, y, GREYP[0])
        cv.set(12, 4, WHITE), cv.set(4, 3, YELP)
    return cv


def spark(frame):
    cv = Canvas(8, 8)
    arm = 3 if frame == 0 else 2
    for d in range(1, arm + 1):
        for x, y in ((3 + d, 3), (3 - d, 3), (3, 3 + d), (3, 3 - d)):
            cv.set(x, y, YELP)
    cv.set(3, 3, WHITE)
    return cv


def bolt(frame):
    cv = Canvas(8, 8)
    if frame == 0:                                                     # a nut
        disc(cv, 3.5, 3.5, 2.3, GREYP[0])
        cv.set(3, 3, OUTP), cv.set(4, 3, GREYP[1])
    else:                                                              # a bolt
        for i in range(5):
            cv.set(1 + i, 1 + i, GREYP[0] if i else GREYP[1])
        cv.set(1, 2, GREYP[1]), cv.set(2, 1, GREYP[1])
    cv.outline(OUTP)
    return cv


def pellet(frame):
    cv = Canvas(8, 8)
    disc(cv, 4.0, 4.0, 2.7 if frame == 0 else 2.3, BMAG)
    disc(cv, 4.0, 4.0, 1.9 if frame == 0 else 1.5, BPINK)
    cv.set(3, 3, WHITE)
    if frame == 0:
        cv.set(4, 3, WHITE)
    cv.outline(OUTP)
    return cv


def big(frame):
    cv = Canvas(8, 8)
    disc(cv, 4.0, 4.0, 3.6, GLASSP[1] if frame else GLASSP[0])
    disc(cv, 3.6, 3.6, 2.4, WHITE)
    cv.set(2, 2, WHITE)
    return cv


def icicle():
    cv = Canvas(16, 8)
    poly(cv, [(1.0, 4.0), (15.0, 1.5), (15.0, 6.5)], GLASSP[0])
    poly(cv, [(1.0, 4.0), (15.0, 4.2), (15.0, 6.5)], GLASSP[1])
    for x in range(6, 15, 3):
        cv.set(x, 3, WHITE)
    cv.outline(OUTP)
    return cv


def fizzle(frame):
    cv = Canvas(8, 8)
    pts = ((3, 3), (4, 4), (3, 4), (4, 3), (2, 2), (5, 5), (5, 2), (2, 5)) if frame == 0 else ((1, 1), (6, 1), (1, 6),
                                                                                               (6, 6))
    for x, y in pts:
        cv.set(x, y, WHITE if (x + y) % 2 else YELP)
    return cv


def warn(frame):
    """Robots from behind: a red arrow pointing in from the left edge, an exclamation mark."""
    cv = Canvas(16, 16)
    poly(cv, [(1.0, 4.0), (9.0, 4.0), (9.0, 1.0), (15.0, 8.0), (9.0, 15.0), (9.0, 12.0), (1.0, 12.0)],
         REDP if frame == 0 else ORANGE)
    for y in range(5, 9):
        cv.set(5, y, WHITE), cv.set(6, y, WHITE)
    cv.set(5, 10, WHITE), cv.set(6, 10, WHITE)
    cv.outline(OUTP)
    return cv


def hp(frame):
    cv = Canvas(8, 8)
    for x in range(0, 7):
        for y in range(2, 6):
            full = frame == 0 or (frame == 1 and x < 3)
            cv.set(x, y, (YELP if y == 2 else REDP) if full else DARKP)
    cv.outline(OUTP)
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
    cv.outline(OUTP)
    return cv


def sprite_frame(name, i):
    fn1 = {"icon": icon, "spread": spread, "shield": shield, "rock": rock, "comet": comet, "icicle": icicle}.get(name)
    if fn1:
        return fn1()
    fn = {"rocket": rocket, "pod": pod, "bark": bark, "beam": beam, "bone": bone, "bug": bug, "drone": drone,
          "ufo": ufo, "dasher": dasher, "carrier": carrier, "turret": turret, "tank": tank, "capsule": capsule,
          "pop": pop, "spark": spark, "bolt": bolt, "pellet": pellet, "big": big, "fizzle": fizzle, "warn": warn,
          "hp": hp, "digit": digit}[name]
    return fn(i)


# ---- the walls: 8 tiles per theme -----------------------------------------------------------------------------------
def ter_tile(theme, kind):
    """One 8x8 wall tile of a floor (the ceiling is the same tile V-flipped): the surface row on top, the open side(s)
    on the left (the right side is the tile H-flipped)."""
    cv = Canvas(8, 8)
    pal = {"rock": ROCK, "station": METAL, "ice": ICE}[theme]
    top = kind.startswith("top")
    lopen = kind in ("top_l", "top_lr", "side_l", "side_lr")
    ropen = kind in ("top_lr", "side_lr")
    for y in range(8):
        for x in range(8):
            c = pal[2] if (x + y * 3) % 7 else pal[3]
            if theme == "rock":
                c = pal[2] if (x * 5 + y * 3 + (kind == "fill2") * 4) % 9 > 2 else pal[3]
                if (x * 3 + y * 7) % 11 == 0:
                    c = pal[1]
                if kind == "fill2" and (x, y) in ((2, 3), (3, 3), (5, 6)):
                    c = pal[4]
            elif theme == "station":
                c = pal[2]
                if y == 0 or (kind == "fill2" and x == 0):
                    c = pal[3]                                     # the panels' seams
                if (x, y) in ((1, 2), (6, 2)) and kind != "fill2":
                    c = pal[1]                                     # rivets
                if kind == "fill2" and 2 <= x <= 5 and 3 <= y <= 4:
                    c = LAMP if (x + y) % 2 else pal[3]            # a vent light
            else:
                c = pal[2] if (x + 2 * y) % 6 else pal[3]
                if kind == "fill2" and (x - y) in (1, 2) and 2 <= x <= 6:
                    c = pal[1]                                     # a crack's glint
            cv.set(x, y, c)
    if top:                                                        # the surface: a lit lip on the top rows
        for x in range(8):
            if theme == "rock":
                cv.set(x, 0, ROCK_LIP[0] if x % 3 else ROCK_LIP[1]), cv.set(x, 1, pal[1] if x % 4 else pal[0])
                cv.set(x, 2, pal[1] if (x + 1) % 3 else pal[2])
            elif theme == "station":
                cv.set(x, 0, pal[0]), cv.set(x, 1, pal[1])
                cv.set(x, 2, STRIPE[0] if (x // 2) % 2 == (kind == "top2") else STRIPE[1])
                cv.set(x, 3, pal[3])
            else:
                cv.set(x, 0, pal[0]), cv.set(x, 1, pal[0]), cv.set(x, 2, pal[1])
                if x in (2, 6) or (kind == "top2" and x == 4):
                    cv.set(x, 3, pal[0]), cv.set(x, 4, pal[1])     # icicles (hanging, on a ceiling)
    if lopen:                                                      # the open side: a darker face, a lit edge
        for y in range(8):
            cv.set(0, y, pal[4] if not (top and y < 2) else pal[1])
            cv.set(1, y, pal[3] if not (top and y < 2) else cv.get(1, y))
    if ropen:
        for y in range(8):
            cv.set(7, y, pal[4] if not (top and y < 2) else pal[1])
            cv.set(6, y, pal[3] if not (top and y < 2) else cv.get(6, y))
    if top and lopen:                                              # rounded corners
        cv.set(0, 0, None)
        if theme == "ice":
            cv.set(1, 0, None)
    if top and ropen:
        cv.set(7, 0, None)
    return cv


def terrain():
    cv = Canvas(*TERRAIN[1:])
    for t, theme in enumerate(("rock", "station", "ice")):
        for k, kind in enumerate(TER_KINDS):
            cv.paste(ter_tile(theme, kind), k * 8, t * 8)
    return cv


# ---- the far layers -------------------------------------------------------------------------------------------------
def clouds():
    """The nebula: soft clouds of three tones (dithered at their edges), a ringed planet, a moon, a far planet. Low
    contrast, no outline; 512 px wide, it repeats."""
    W, H = PANORAMAS["clouds"][1:]
    cv = Canvas(W, H)
    rng = random.Random(11)
    blobs = []
    for k in range(9):
        cx, cy = rng.randint(0, W), rng.choice((30, 60, 170, 200, 120))
        for j in range(5):
            blobs.append((cx + rng.randint(-50, 50), cy + rng.randint(-14, 14), rng.randint(14, 30), rng.randint(8, 14)))
    for y in range(H):
        for x in range(W):
            v = 0.0
            for bx, by, rx, ry in blobs:
                for wx in (bx - W, bx, bx + W):                    # it repeats every 512 px
                    dx, dy = (x - wx) / rx, (y - by) / ry
                    d = dx * dx + dy * dy
                    if d < 1.0:
                        v = max(v, 1.0 - d)
            if v > 0.72:
                cv.set(x, y, CLOUD[0])
            elif v > 0.3:
                cv.set(x, y, CLOUD[1])
            elif v > 0.06 and hs.checker(x, y):
                cv.set(x, y, CLOUD[2])

    def planet(cx, cy, r, ramp):
        for yy in range(int(cy - r) - 1, int(cy + r) + 2):
            for xx in range(int(cx - r) - 1, int(cx + r) + 2):
                d = dist(xx, yy, cx, cy)
                if d <= r:
                    nd = ((xx + 0.5 - cx) * -0.62 + (yy + 0.5 - cy) * -0.78) / r
                    cv.set(xx % W, yy, ramp[0] if nd > 0.3 else ramp[1] if nd > -0.35 or len(ramp) < 3 else ramp[2])

    cx, cy, r = 392, 64, 17                                        # the ringed planet
    for xx in range(cx - 32, cx + 33):
        for yy in range(cy - 8, cy + 9):
            dx, dy = (xx + 0.5 - cx) / 30.0, (yy + 0.5 - cy) / 6.5
            if 0.78 < dx * dx + dy * dy <= 1.0 and yy < cy:
                cv.set(xx, yy, RINGC[1])
    planet(cx, cy, r, PLANET)
    for xx in range(cx - 32, cx + 33):
        for yy in range(cy - 8, cy + 9):
            dx, dy = (xx + 0.5 - cx) / 30.0, (yy + 0.5 - cy) / 6.5
            if 0.78 < dx * dx + dy * dy <= 1.0 and yy >= cy:
                cv.set(xx, yy, RINGC[0])
    planet(120, 186, 9, MOON)                                      # a moon with craters
    for hx, hy in ((117, 183), (123, 190)):
        cv.set(hx, hy, MOON[1]), cv.set(hx + 1, hy, MOON[1])
    planet(250, 104, 4, PLANET)                                    # a far planet
    return cv


def stars():
    W, H = PANORAMAS["stars"][1:]
    cv = Canvas(W, H)
    rng = random.Random(21)
    for k in range(210):
        x, y = rng.randint(1, W - 2), rng.randint(1, H - 2)
        if cv.get(x, y) is not None:
            continue
        col = STARS[k % 2] if k % 3 == 0 else STAR_FAINT[k % 2]
        cv.set(x, y, col)
        if k % 19 == 0:
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                cv.set(x + dx, y + dy, STAR_FAINT[1] if col in STARS else STAR_FAINT[0])
    return cv


# ---- the sheets -----------------------------------------------------------------------------------------------------
def build_sheet():
    im = Image.new("RGB", (SHEET_W, SHEET_H), hs.MAGENTA)
    for name, x, y, w, h, frames, _g in SPRITES:
        for i in range(frames):
            fr = sprite_frame(name, i)
            assert (fr.w, fr.h) == (w, h), name
            im.paste(fr.image(), (x + i * w, y))
    return im


def build_bosses():
    im = Image.new("RGB", (256, 96), hs.MAGENTA)
    for name, x, y in BOSSES:
        for i in range(2):
            im.paste(BOSS_FN[name](i).image(), (x + i * 64, y))
    return im


def write_all(out, preview=False):
    os.makedirs(os.path.join(out, "tiles"), exist_ok=True)
    files = {"sprites.png": build_sheet(), "bosses.png": build_bosses(), TERRAIN[0]: terrain().image(),
             PANORAMAS["clouds"][0]: clouds().image(), PANORAMAS["stars"][0]: stars().image()}
    for f, im in files.items():
        im.save(os.path.join(out, f))
        if preview:
            os.makedirs(os.path.join(out, "preview"), exist_ok=True)
            k = 2 if im.width > 256 else 4
            im.resize((im.width * k, im.height * k), Image.NEAREST).save(
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

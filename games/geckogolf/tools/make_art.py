#!/usr/bin/env python3
"""Gecko Golf: the code-drawn art, in the 8BCraft house style (docs/art-direction.md).

Code-drawn art only, no image generation: every sprite and panorama is drawn here with
games/common/tools/house_style.py (1-px outlines, 3-4-shade ramps lit from the top-left, big eyes). The course itself
(the ground, the grass, the sand, the ice, the mushrooms, the water, the cup) is drawn by src/draw.c pixel by pixel as
the camera moves, in the biome's palette (draw.c TERRAIN_PAL). The sheet layout (SPRITES) is shared with
build_assets.py.

  the golfer   a leopard gecko in a white visor, seen from the side (facing right; draw.c flips it), 32x32: address
               (2 frames), the backswing (3, the power gauge), the follow-through, the cheer (2), the sad one; its
               head (the title's player slots, the results); the ball and the strokes' pips (the player's colour)
  the props    the flag (2 frames), the aim dots, the power gauge, the arrow (the ball above the screen), the out of
               bounds stake, the splash, the sand puff, the slingshot's ring
  the sky      clouds, the sun, the moon, the stars: drawn in KEY colours, recoloured per biome by draw.c (OBJ 6)
  the decor    one or two things per biome: flowers, tufts, cacti, rocks, pines, a snowman, glowing mushrooms,
               fireflies, lollipops, candy canes (OBJ 7)
  the hills    the far hills, a 512x152 panorama in 5 KEY tones (no outline), recoloured per biome by draw.c

Writes, in the art folder (default games/geckogolf/art):
  sprites.png (256x128), tiles/hills.png (512x152)

    python3 games/geckogolf/tools/make_art.py [--out DIR] [--preview]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/geckogolf/LICENSE.
"""
import argparse
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
SHEET_W, SHEET_H = 256, 160
# name, x, y, w, h, frames, palette group (hero: OBJ 0, players 2-4 = OBJ 1, 4, 5 recoloured; props: OBJ 2;
# sky: OBJ 6, recoloured per biome; decor: OBJ 7)
SPRITES = [
    ("gecko_idle", 0, 0, 32, 32, 2, "hero"),
    ("gecko_back", 64, 0, 32, 32, 3, "hero"),
    ("gecko_swing", 160, 0, 32, 32, 1, "hero"),
    ("gecko_sad", 192, 0, 32, 32, 1, "hero"),
    ("gecko_cheer", 0, 32, 32, 32, 2, "hero"),
    ("gecko_icon", 64, 32, 16, 16, 1, "hero"),
    ("ball", 80, 32, 8, 8, 1, "hero"),
    ("pip", 88, 32, 8, 8, 1, "hero"),
    ("tag", 96, 32, 16, 8, 4, "hero"),
    ("flag", 160, 32, 16, 32, 2, "props"),
    ("dot", 192, 32, 8, 8, 2, "props"),
    ("gauge", 208, 32, 8, 32, 1, "props"),
    ("fill", 64, 48, 8, 8, 8, "props"),
    ("fill_y", 64, 56, 8, 8, 8, "props"),
    ("fill_r", 0, 88, 8, 8, 8, "props"),
    ("arrow", 216, 32, 8, 8, 1, "props"),
    ("stake", 224, 32, 8, 16, 1, "props"),
    ("splash", 0, 64, 16, 16, 3, "props"),
    ("puff", 48, 64, 8, 8, 3, "props"),
    ("ring", 72, 64, 16, 16, 1, "props"),
    ("cloud_big", 96, 64, 48, 24, 1, "sky"),
    ("cloud_small", 144, 64, 32, 16, 1, "sky"),
    ("sun", 176, 64, 32, 32, 1, "sky"),
    ("moon", 208, 64, 32, 32, 1, "sky"),
    ("star", 240, 64, 8, 8, 2, "sky"),
    ("flower", 0, 96, 8, 16, 2, "decor"),
    ("tuft", 16, 96, 16, 8, 1, "decor"),
    ("cactus", 32, 96, 16, 24, 2, "decor"),
    ("rock", 64, 96, 16, 8, 1, "decor"),
    ("pine", 80, 96, 16, 24, 2, "decor"),
    ("snowman", 112, 96, 16, 24, 1, "decor"),
    ("glowshroom", 128, 96, 16, 16, 1, "decor"),
    ("firefly", 144, 96, 8, 8, 2, "decor"),
    ("lollipop", 160, 96, 8, 24, 2, "decor"),
    ("candycane", 176, 96, 8, 24, 1, "decor"),
]
PANORAMAS = {"hills": ("tiles/hills.png", 512, 152)}

# ---- colours ------------------------------------------------------------------------------------------------------------
# the gecko: a warm orange (its main material: the hue players 2-4 turn, build_assets.py P2_HUE), a cream belly, spots,
# a white visor, a grey club; the outline the darkest shade pushed towards violet
GECKO = [(255, 200, 120), (250, 160, 58), (216, 108, 36), (150, 64, 28)]
SPOT = (124, 52, 30)
BELLY = [(252, 242, 206), (232, 214, 168)]
VISOR = [(250, 250, 250), (196, 204, 220)]
CLUB = [(226, 230, 238), (150, 156, 172), (90, 94, 110)]
OUT = (52, 22, 44)
WHITE, INK = hs.HOUSE["white"][0], hs.HOUSE["ink"][0]

# the props (OBJ 2)
P_OUT = (34, 24, 40)
P_WHITE = (250, 250, 250)
P_GREY = (190, 194, 206)
P_RED = (232, 72, 64)
P_DRED = (160, 36, 44)
P_YEL = (250, 214, 72)
P_ORA = (246, 150, 50)
P_GREEN = (110, 200, 90)
P_DGREEN = (50, 120, 60)
P_WATER = [(196, 238, 255), (112, 184, 226)]
P_SAND = [(248, 232, 190), (208, 176, 120)]

# the sky (OBJ 6): KEY colours, in this order the palette's indices 1..9 (draw.c SKY_PAL recolours them per biome)
SKY_KEYS = [(255, 255, 255), (232, 240, 250), (200, 218, 236), (255, 250, 210), (255, 232, 140), (255, 206, 110),
            (240, 240, 220), (190, 196, 200), (255, 250, 200)]
CL_L, CL_M, CL_S, SUN_C, SUN_M, SUN_R, MOON_L, MOON_S, STAR = SKY_KEYS

# the decor (OBJ 7): one fixed palette for every biome, in this order (indices 1..15)
DECOR_PAL = [(40, 30, 40), (250, 250, 250), (150, 214, 90), (84, 160, 60), (40, 100, 50), (250, 140, 180),
             (226, 64, 72), (255, 220, 80), (176, 120, 70), (110, 70, 40), (186, 190, 200), (110, 114, 130),
             (140, 250, 230), (170, 110, 220), (190, 214, 240)]
D_OUT, D_WHITE, D_GL, D_GM, D_GD, D_PINK, D_RED, D_YEL, D_BL, D_BD, D_GRL, D_GRD, D_CYAN, D_PURP, D_SNOW = DECOR_PAL

# the far hills: 5 KEY tones (far light, far shade, near light, near mid, near dark), recoloured by draw.c HILL_PAL
HILL_KEYS = [(170, 206, 214), (150, 190, 204), (126, 178, 160), (106, 160, 146), (88, 140, 130)]


# ---- helpers ------------------------------------------------------------------------------------------------------------
def line(cv, x0, y0, x1, y1, c, r=0.6):
    """A thick line (a capsule, flat colour)."""
    hs.capsule(cv, x0, y0, x1, y1, r, None, flat=c)


def disc(cv, cx, cy, r, c):
    for y in range(int(cy - r) - 1, int(cy + r) + 2):
        for x in range(int(cx - r) - 1, int(cx + r) + 2):
            if (x + 0.5 - cx) ** 2 + (y + 0.5 - cy) ** 2 <= r * r:
                cv.set(x, y, c)


# ---- the gecko ----------------------------------------------------------------------------------------------------------
# hands, club head, the body's lift (px up), the head's drop, arms up?, the eye ("open", "half", "happy")
POSES = {
    "idle0": dict(hands=(19.5, 21.5), head=(24.5, 29.0), lift=0, nod=0, eye="open"),
    "idle1": dict(hands=(19.5, 21.5), head=(24.5, 29.0), lift=0, nod=1, eye="open"),
    "back0": dict(hands=(17.0, 21.0), head=(9.0, 27.0), lift=0, nod=0, eye="open"),
    "back1": dict(hands=(15.5, 19.0), head=(3.5, 14.0), lift=0, nod=0, eye="open"),
    "back2": dict(hands=(14.5, 17.0), head=(7.0, 3.5), lift=0, nod=0, eye="open"),
    "swing0": dict(hands=(19.0, 15.0), head=(27.0, 4.0), lift=0, nod=0, eye="open"),
    "sad0": dict(hands=(18.0, 23.0), head=(25.5, 29.5), lift=0, nod=2, eye="half"),
    "cheer0": dict(hands=(23.0, 11.0), head=(29.5, 2.0), lift=0, nod=0, eye="happy", up=True),
    "cheer1": dict(hands=(23.0, 10.0), head=(30.0, 1.5), lift=2, nod=0, eye="happy", up=True),
}


def gecko(pose):
    P = POSES[pose]
    cv = Canvas(32, 32)
    lift, nod = P["lift"], P["nod"]
    hx, hy = P["hands"]
    hx2, hy2 = P["head"]
    hy -= lift
    hy2 -= lift
    G = GECKO
    # the club behind the body when it is back (the backswing), else in front: draw it first or last
    club_behind = pose.startswith("back")

    def club():
        line(cv, hx, hy, hx2, hy2, CLUB[1], 0.55)
        dx, dy = hx2 - hx, hy2 - hy
        n = math.hypot(dx, dy) or 1
        ux, uy = dx / n, dy / n
        # the club head: a little iron, across the shaft's end
        for t in (-0.5, 0.5, 1.5):
            cv.set(int(round(hx2 + ux * t)), int(round(hy2 + uy * t)), CLUB[2])
        for t in (0.0, 1.0):
            cv.set(int(round(hx2 + ux * t + 1)), int(round(hy2 + uy * t)), CLUB[0])
        cv.set(int(round(hx - ux * 0.5)), int(round(hy - uy * 0.5)), CLUB[2])        # the grip

    if club_behind:
        club()
    # the tail: curling up behind
    pts = [(10.0, 25.0 - lift), (6.5, 27.0 - lift), (3.5, 26.0 - lift), (2.2, 23.0 - lift), (3.8, 21.0 - lift)]
    for i in range(len(pts) - 1):
        r = 2.0 - i * 0.35
        hs.capsule(cv, pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1], r, G)
    # the far leg and arm (darker, behind)
    line(cv, 11.0, 25.0 - lift, 10.5, 29.0 - lift, G[2], 1.1)
    cv.rect(8, 29 - lift, 11, 29 - lift, G[3])
    if P.get("up"):
        line(cv, 13.0, 17.0 - lift, hx - 1.5, hy + 0.5, G[3], 0.9)
    else:
        line(cv, 13.5, 18.0 - lift, hx - 1.0, hy + 0.5, G[3], 0.9)
    # the torso: an upright egg, a cream belly in front
    hs.ellipse(cv, 13.5, 21.0 - lift, 4.6, 6.0, G)
    hs.ellipse(cv, 15.6, 22.0 - lift, 2.4, 4.3, [BELLY[0], BELLY[0], BELLY[1], BELLY[1]])
    # the near leg, the toes
    line(cv, 15.0, 25.5 - lift, 15.5, 29.0 - lift, G[1], 1.2)
    cv.rect(14, 29 - lift, 18, 29 - lift, G[2])
    cv.set(18, 28 - lift, G[2])
    # the head: big and round, a snout to the right
    by = 10.0 - lift + nod
    hs.ellipse(cv, 16.0, by, 6.2, 5.2, G)
    hs.ellipse(cv, 20.5, by + 1.2, 3.8, 3.0, G)
    cv.set(12, int(by - 3), G[0]), cv.set(13, int(by - 4), G[0])                    # the highlight, top-left
    # spots
    for sx, sy in ((12, by - 1), (14, by + 2), (11, 19 - lift), (12, 23 - lift), (10, 21 - lift), (5, 26 - lift)):
        cv.set(int(sx), int(sy), SPOT)
    # the eye: a gecko's big round eye
    ex, ey = 16, int(by - 3)
    if P["eye"] == "happy":
        for i, (dx, dy) in enumerate(((0, 2), (1, 1), (2, 1), (3, 2))):
            cv.set(ex + dx, ey + dy, INK)
    elif P["eye"] == "half":
        cv.rect(ex, ey + 1, ex + 3, ey + 1, G[3])
        cv.rect(ex, ey + 2, ex + 3, ey + 3, WHITE)
        cv.set(ex + 2, ey + 3, INK), cv.set(ex + 3, ey + 3, INK)
    else:
        cv.rect(ex, ey, ex + 3, ey + 3, WHITE)
        cv.rect(ex + 2, ey + 1, ex + 3, ey + 3, INK)
        cv.set(ex + 2, ey + 1, WHITE)                                                    # the glint
    # the smile, the nostril
    my = int(by + 3)
    if P["eye"] == "happy":
        for x in range(19, 24):
            cv.set(x, my, G[3])
        cv.rect(20, my + 1, 22, my + 1, (220, 90, 90))
    elif P["eye"] == "half":
        cv.rect(19, my + 1, 22, my + 1, G[3])
    else:
        cv.rect(19, my, 22, my, G[3])
        cv.set(23, my - 1, G[3])
    cv.set(23, int(by), G[3])
    # the white visor: a band round the head and a brim to the front
    vy = int(by - 5)
    cv.rect(12, vy, 19, vy, VISOR[0])
    cv.rect(11, vy + 1, 19, vy + 1, VISOR[1])
    cv.rect(19, vy + 1, 24, vy + 1, VISOR[0])
    cv.rect(20, vy + 2, 23, vy + 2, VISOR[1])
    # the near arm, the hands
    sx0, sy0 = (15.0, 17.0 - lift)
    line(cv, sx0, sy0, hx, hy, G[1], 1.0)
    disc(cv, hx, hy, 1.3, G[0])
    if not club_behind:
        club()
    cv.outline(OUT)
    return cv


def gecko_frame(name, i):
    key = {"gecko_idle": "idle%d", "gecko_back": "back%d", "gecko_swing": "swing%d", "gecko_sad": "sad%d",
           "gecko_cheer": "cheer%d"}[name] % i
    return gecko(key)


def gecko_icon():
    """The 16x16 head (the title's player slots, the results): the same colours, so the swaps of players 2-4 apply."""
    cv = Canvas(16, 16)
    hs.ellipse(cv, 7.0, 8.5, 5.6, 4.8, GECKO)
    hs.ellipse(cv, 11.0, 9.6, 3.6, 2.8, GECKO)
    cv.set(3, 6, GECKO[0])
    cv.rect(6, 5, 9, 8, WHITE)
    cv.rect(8, 6, 9, 8, INK)
    cv.set(8, 6, WHITE)
    cv.rect(10, 12, 13, 12, GECKO[3])
    cv.set(4, 9, SPOT), cv.set(5, 11, SPOT)
    cv.rect(3, 3, 10, 3, VISOR[0])
    cv.rect(2, 4, 10, 4, VISOR[1])
    cv.rect(10, 4, 14, 4, VISOR[0])
    cv.outline(OUT)
    return cv


def ball():
    """The ball: 4x4 in the player's colour (the gecko's main material), a white glint, the outline."""
    cv = Canvas(8, 8)
    for y in range(3, 7):
        for x in range(2, 6):
            if (x in (2, 5)) and (y in (3, 6)):
                continue
            cv.set(x, y, GECKO[1] if x + y < 8 else GECKO[2])
    cv.set(3, 4, WHITE)
    cv.set(4, 3, GECKO[0])
    cv.outline(OUT)
    return cv


def pip():
    cv = Canvas(8, 8)
    for y in range(2, 6):
        for x in range(2, 6):
            if (x in (2, 5)) and (y in (2, 5)):
                continue
            cv.set(x, y, GECKO[1])
    cv.set(3, 3, GECKO[0])
    cv.outline(OUT)
    return cv


# 3x5 digits for the player tags
TINY = {"P": ["111", "101", "111", "100", "100"], "1": ["010", "110", "010", "010", "111"],
        "2": ["111", "001", "111", "100", "111"], "3": ["111", "001", "111", "001", "111"],
        "4": ["101", "101", "111", "001", "001"]}


def tag(n):
    """P1..P4 on a little label in the player's colour (over the gecko whose turn it is, 2-4 players)."""
    cv = Canvas(16, 8)
    cv.rect(1, 0, 14, 7, GECKO[2])
    cv.rect(1, 0, 14, 0, GECKO[1])
    for i, ch in enumerate("P" + str(n)):
        for y, row in enumerate(TINY[ch]):
            for x, b in enumerate(row):
                if b == "1":
                    cv.set(4 + i * 4 + x, 1 + y, WHITE)
    cv.set(1, 0, None), cv.set(14, 0, None), cv.set(1, 7, None), cv.set(14, 7, None)
    cv.outline(OUT)
    return cv


# ---- props --------------------------------------------------------------------------------------------------------------
def flag(frame):
    """The pin: a white pole, a red pennant waving (2 frames)."""
    cv = Canvas(16, 32)
    cv.rect(1, 2, 1, 31, P_WHITE)
    cv.rect(2, 2, 2, 31, P_GREY)
    for x in range(3, 14):
        k = x - 3
        wave = int(round(math.sin((k + frame * 3) * 0.55) * 1.0))
        top = 2 + wave + k * 2 // 11
        bot = 10 + wave - k * 3 // 11
        for y in range(top, bot + 1):
            cv.set(x, y, P_RED if y < bot - 1 or k < 2 else P_DRED)
        cv.set(x, top, P_RED if k > 0 else P_RED)
    cv.set(1, 1, P_YEL), cv.set(2, 1, P_YEL)
    cv.outline(P_OUT)
    return cv


def dot(frame):
    cv = Canvas(8, 8)
    if frame == 0:
        cv.rect(3, 3, 4, 4, P_WHITE)
    else:
        cv.set(3, 3, P_WHITE)
    cv.outline(P_OUT)
    return cv


def gauge():
    """The power gauge's frame: a rounded bar, 6 px wide inside, 30 tall (draw.c fills it with `fill`)."""
    cv = Canvas(8, 32)
    cv.rect(1, 0, 6, 31, P_OUT)
    cv.rect(2, 1, 5, 30, (60, 50, 70))
    cv.set(1, 0, None), cv.set(6, 0, None), cv.set(1, 31, None), cv.set(6, 31, None)
    return cv


def fill(n, col=None):
    """n + 1 rows (from the bottom) of the gauge's fill: green at the bottom, yellow, orange, red at the top (draw.c
    picks the colour by the height: the frames are the rows of one 8x8 piece)."""
    cv = Canvas(8, 8)
    for y in range(7 - n, 8):
        for x in range(2, 6):
            cv.set(x, y, (col or P_GREEN) if x > 2 else P_WHITE)
    return cv


def arrow():
    cv = Canvas(8, 8)
    for y in range(1, 6):
        for x in range(4 - (y - 1) // 2 - 1, 4 + (y - 1) // 2 + 1):
            cv.set(x, y, P_WHITE)
    cv.outline(P_OUT)
    return cv


def stake():
    """An out of bounds stake: white with a red top."""
    cv = Canvas(8, 16)
    cv.rect(3, 1, 4, 15, P_WHITE)
    cv.rect(3, 1, 4, 4, P_RED)
    cv.set(4, 6, P_GREY), cv.set(4, 10, P_GREY), cv.set(4, 14, P_GREY)
    cv.outline(P_OUT)
    return cv


def splash(frame):
    cv = Canvas(16, 16)
    for i in range(7):
        a = math.pi * (0.15 + 0.7 * i / 6)
        r = 3 + frame * 2.5
        x = 8 + math.cos(a) * r * 1.2
        y = 14 - math.sin(a) * (r + 2) + frame * frame * 0.8
        if frame == 2 and i % 2:
            continue
        cv.rect(int(x), int(y), int(x) + (1 if frame < 2 else 0), int(y) + 1, P_WATER[0] if i % 2 else P_WATER[1])
    if frame == 0:
        cv.rect(5, 13, 10, 14, P_WATER[1])
        cv.rect(6, 12, 9, 12, P_WATER[0])
    return cv


def puff(frame):
    """Sand: a little cloud that grows and fades with the house checker dither (never on a body)."""
    cv = Canvas(8, 8)
    r = [1.8, 2.8, 3.5][frame]
    for y in range(8):
        for x in range(8):
            d = math.hypot(x + 0.5 - 4, y + 0.5 - 5)
            if d < r and (frame == 0 or hs.checker(x, y, frame)):
                cv.set(x, y, P_SAND[0] if d < r - 1 else P_SAND[1])
    return cv


def ring():
    """Where the slingshot's finger went down."""
    cv = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            d = math.hypot(x + 0.5 - 8, y + 0.5 - 8)
            if 4.5 <= d < 6.0:
                cv.set(x, y, P_WHITE)
    cv.outline(P_OUT)
    return cv


# ---- the sky (KEY colours) --------------------------------------------------------------------------------------------
def cloud(w, h, blobs):
    """A puffy cloud: overlapping discs, lit from the top, a flat shaded base; no outline (far layer)."""
    cv = Canvas(w, h)
    for cx, cy, r in blobs:
        for y in range(h):
            for x in range(w):
                d = math.hypot(x + 0.5 - cx, y + 0.5 - cy)
                if d <= r:
                    ny = (y + 0.5 - cy) / r
                    c = CL_L if ny < -0.25 else CL_M if ny < 0.45 else CL_S
                    old = cv.get(x, y)
                    if old is None or SKY_KEYS.index(c) < SKY_KEYS.index(old):
                        cv.set(x, y, c)
    base = h - 2
    for x in range(w):
        for y in range(base, h):
            if cv.get(x, y) is not None:
                cv.set(x, y, CL_S)
    return cv


def sun():
    cv = Canvas(32, 32)
    for y in range(32):
        for x in range(32):
            d = math.hypot(x + 0.5 - 16, y + 0.5 - 16)
            if d < 9:
                cv.set(x, y, SUN_C if d < 6.5 or (x < 14 and y < 14) else SUN_M)
            elif d < 11:
                cv.set(x, y, SUN_M if d < 10 else SUN_R)
            elif d < 15 and hs.dither_ok(x, y, int((15 - d) * 3)):
                cv.set(x, y, SUN_R)
    return cv


def moon():
    cv = Canvas(32, 32)
    for y in range(32):
        for x in range(32):
            d = math.hypot(x + 0.5 - 16, y + 0.5 - 16)
            if d < 10:
                cv.set(x, y, MOON_L)
                if math.hypot(x + 0.5 - 20, y + 0.5 - 13) < 2.2 or math.hypot(x + 0.5 - 13, y + 0.5 - 20) < 3.0 \
                        or math.hypot(x + 0.5 - 21, y + 0.5 - 21) < 1.5:
                    cv.set(x, y, MOON_S)
                if d > 8.6 and x > 17:
                    cv.set(x, y, MOON_S)
    return cv


def star(frame):
    cv = Canvas(8, 8)
    cv.set(3, 3, STAR)
    if frame == 1:
        for dx, dy in ((0, -1), (0, 1), (-1, 0), (1, 0)):
            cv.set(3 + dx, 3 + dy, STAR)
    return cv


# ---- the decor (DECOR_PAL) ----------------------------------------------------------------------------------------------
def flower(v):
    cv = Canvas(8, 16)
    line(cv, 3.5, 15.5, 3.5, 7.0, D_GM, 0.5)
    cv.set(4, 11, D_GL), cv.set(5, 10, D_GL), cv.set(2, 13, D_GL)
    petal = [D_PINK, D_WHITE][v]
    for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1), (-2, 0), (2, 0), (0, -2), (0, 2), (-1, -1), (1, -1), (-1, 1), (1, 1)):
        if abs(dx) + abs(dy) < 3:
            cv.set(3 + dx, 4 + dy, petal)
    cv.set(3, 4, D_YEL)
    cv.set(2, 3, D_WHITE if v == 0 else D_PINK)
    cv.outline(D_OUT)
    return cv


def tuft():
    cv = Canvas(16, 8)
    for x0, top, c in ((4, 2, D_GM), (7, 0, D_GL), (10, 1, D_GM), (12, 3, D_GD)):
        line(cv, x0 + 0.5, 7.5, x0 + 1.5, top + 0.5, c, 0.5)
    return cv


def cactus(v):
    cv = Canvas(16, 24)
    hs.capsule(cv, 7.5, 6.0, 7.5, 22.5, 2.6, [D_GL, D_GL, D_GM, D_GD])
    hs.capsule(cv, 3.5, 11.0, 3.5, 15.5, 1.5, [D_GL, D_GL, D_GM, D_GD])
    line(cv, 3.5, 15.5, 6.0, 15.5, D_GM, 1.0)
    if v:
        hs.capsule(cv, 11.5, 9.0, 11.5, 13.5, 1.5, [D_GL, D_GL, D_GM, D_GD])
        line(cv, 9.0, 13.5, 11.5, 13.5, D_GM, 1.0)
        cv.set(7, 4, D_PINK), cv.set(8, 4, D_PINK), cv.set(7, 3, D_YEL)
    for y in (9, 13, 17, 20):
        cv.set(6, y, D_GD)
    cv.outline(D_OUT)
    return cv


def rock():
    cv = Canvas(16, 8)
    hs.ellipse(cv, 7.5, 6.5, 6.0, 4.0, [D_GRL, D_GRL, D_GRD, D_GRD])
    cv.set(5, 4, D_WHITE)
    cv.outline(D_OUT)
    return cv


def pine(v):
    cv = Canvas(16, 24)
    cv.rect(7, 19, 8, 23, D_BD)
    for i, (y0, w) in enumerate(((3, 3), (8, 5), (13, 7))):
        for y in range(y0, y0 + 7):
            half = (y - y0 + 1) * w / 7.0
            for x in range(int(8 - half), int(8 + half) + 1):
                cv.set(x, y, D_GD if x > 8 else D_GM)
        if v == 0:
            for x in range(int(8 - w + 1), int(8 + w)):
                cv.set(x, y0 + 6, D_SNOW if x > 8 else D_WHITE)
    if v == 0:
        cv.set(8, 2, D_WHITE)
    cv.outline(D_OUT)
    return cv


def snowman():
    cv = Canvas(16, 24)
    snow = [D_WHITE, D_WHITE, D_SNOW, D_SNOW]
    hs.ellipse(cv, 8.0, 18.0, 5.5, 5.0, snow)
    hs.ellipse(cv, 8.0, 10.0, 4.0, 3.8, snow)
    cv.rect(5, 4, 10, 6, D_OUT)
    cv.rect(4, 6, 11, 6, D_OUT)
    cv.rect(5, 5, 10, 5, D_RED)
    cv.set(7, 9, D_OUT), cv.set(9, 9, D_OUT)
    cv.set(10, 11, (255, 150, 60)), cv.set(11, 11, (255, 150, 60))
    cv.set(8, 16, D_OUT), cv.set(8, 19, D_OUT)
    line(cv, 3.0, 14.0, 0.5, 11.5, D_BD, 0.4)
    line(cv, 13.0, 14.0, 15.5, 11.5, D_BD, 0.4)
    cv.outline(D_OUT)
    return cv


def glowshroom():
    cv = Canvas(16, 16)
    cv.rect(6, 9, 9, 15, D_SNOW)
    cv.rect(8, 9, 9, 15, D_GRL)
    hs.ellipse(cv, 7.5, 8.0, 6.5, 4.5, [D_CYAN, D_CYAN, D_PURP, D_PURP])
    for x in range(1, 15):
        if cv.get(x, 9) is not None and cv.get(x, 10) is None:
            pass
    cv.rect(1, 9, 14, 9, D_PURP)
    cv.set(5, 6, D_WHITE), cv.set(9, 5, D_WHITE), cv.set(11, 7, D_WHITE)
    cv.outline(D_OUT)
    return cv


def firefly(frame):
    cv = Canvas(8, 8)
    if frame == 0:
        cv.set(3, 3, D_YEL), cv.set(4, 3, D_YEL), cv.set(3, 4, D_YEL), cv.set(4, 4, D_WHITE)
        cv.set(2, 3, D_CYAN), cv.set(5, 4, D_CYAN)
    else:
        cv.set(3, 3, D_YEL), cv.set(4, 4, D_YEL)
    return cv


def lollipop(v):
    cv = Canvas(8, 24)
    cv.rect(3, 9, 4, 23, D_WHITE)
    cv.set(4, 12, D_GRL), cv.set(4, 16, D_GRL), cv.set(4, 20, D_GRL)
    cols = [(D_RED, D_WHITE), (D_PURP, D_YEL)][v]
    for y in range(0, 9):
        for x in range(8):
            d = math.hypot(x + 0.5 - 4, y + 0.5 - 4.5)
            if d < 3.8:
                a = math.atan2(y + 0.5 - 4.5, x + 0.5 - 4) + d * 0.9
                cv.set(x, y, cols[0] if math.sin(a * 2) > 0 else cols[1])
    cv.outline(D_OUT)
    return cv


def candycane():
    cv = Canvas(8, 24)
    pts = [(3.5, 23.0), (3.5, 6.0)]
    line(cv, pts[0][0], pts[0][1], pts[1][0], pts[1][1], D_WHITE, 1.1)
    for a in range(0, 181, 15):
        x = 5.5 - 2.0 * math.cos(math.radians(a))
        y = 5.5 - 2.6 * math.sin(math.radians(a))
        disc(cv, x, y, 1.1, D_WHITE)
    for y in range(0, 24):
        for x in range(8):
            if cv.get(x, y) == D_WHITE and (x + y) % 4 < 2:
                cv.set(x, y, D_RED)
    cv.outline(D_OUT)
    return cv


def sprite_frame(name, i):
    if name.startswith("gecko_") and name != "gecko_icon":
        return gecko_frame(name, i)
    table = {
        "gecko_icon": gecko_icon, "ball": ball, "pip": pip, "tag": lambda: tag(i + 1), "flag": lambda: flag(i),
        "dot": lambda: dot(i), "gauge": gauge, "fill": lambda: fill(i), "fill_y": lambda: fill(i, P_YEL), "fill_r": lambda: fill(i, P_RED), "arrow": arrow, "stake": stake,
        "splash": lambda: splash(i), "puff": lambda: puff(i), "ring": ring,
        "cloud_big": lambda: cloud(48, 24, [(12, 15, 7), (22, 11, 9.5), (33, 13, 8), (41, 17, 5.5), (26, 18, 6)]),
        "cloud_small": lambda: cloud(32, 16, [(9, 10, 5.5), (17, 8, 6.5), (24, 11, 5)]),
        "sun": sun, "moon": moon, "star": lambda: star(i),
        "flower": lambda: flower(i), "tuft": tuft, "cactus": lambda: cactus(i), "rock": rock, "pine": lambda: pine(i),
        "snowman": snowman, "glowshroom": glowshroom, "firefly": lambda: firefly(i), "lollipop": lambda: lollipop(i),
        "candycane": candycane,
    }
    return table[name]()


# ---- the far hills (KEY tones, no outline) ----------------------------------------------------------------------------
def hills():
    """Two soft ranges, seamless over 512 px: far, round mountains, and near hills with a fringe of round treetops
    (low contrast, no outline: the background never competes with the course)."""
    W, H = PANORAMAS["hills"][1], PANORAMAS["hills"][2]
    cv = Canvas(W, H)
    t = 2 * math.pi / W
    for x in range(W):
        far = 30 + 16 * math.sin(x * t * 2 + 0.6) + 9 * math.sin(x * t * 5 + 2.1) + 4 * math.sin(x * t * 11)
        near = 66 + 10 * math.sin(x * t * 3 + 1.4) + 6 * math.sin(x * t * 7 + 0.3)
        bump = 2.5 * abs(math.sin(x * t * 32))                          # treetops along the near ridge
        for y in range(int(far), H):
            cv.set(x, y, HILL_KEYS[0] if y - far < 3 or math.sin(x * t * 9 + y * 0.05) > 0.55 else HILL_KEYS[1])
        top = int(near - bump)
        for y in range(top, H):
            cv.set(x, y, HILL_KEYS[2] if y - top < 2 else HILL_KEYS[3] if y < 104 else HILL_KEYS[4])
    return cv


def build_sheet():
    im = Image.new("RGB", (SHEET_W, SHEET_H), hs.MAGENTA)
    for name, x, y, w, h, frames, _g in SPRITES:
        for i in range(frames):
            fr = sprite_frame(name, i)
            assert fr.w == w and fr.h == h, name
            im.paste(fr.image(), (x + i * w, y))
    return im


def write_all(out, preview=False):
    os.makedirs(os.path.join(out, "tiles"), exist_ok=True)
    files = {"sprites.png": build_sheet(), PANORAMAS["hills"][0]: hills().image()}
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

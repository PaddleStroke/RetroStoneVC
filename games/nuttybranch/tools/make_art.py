#!/usr/bin/env python3
"""Nutty Branch: the code-drawn art, in the 8BCraft house style (docs/art-direction.md).

Code-drawn art only, no image generation: every sprite, the branch texture and the background are drawn here
with games/common/tools/house_style.py (1-px outlines, 3-4-shade ramps lit from the top-left). Each sprite of the
world (nuts, traps, birds) is drawn natively at the 5 sizes of the perspective (LOD_SCALES, px per world unit):
never resampled.

Writes, in the art folder (default games/nuttybranch/art):
  sprites.png + sprites.json   every sprite frame (the sheet and its layout: anchor, palette group, mirror)
  branch.png                   the affine branch texture (512x256: bark rows, gap ends, fork rows)
  backdrop.png                 BG3: the canopy (top) and the far forest band (256x256)
  palettes.json                the fixed palettes (sprites) and the time-of-day / season bands (BG, props, sky)

    python3 games/nuttybranch/tools/make_art.py [--out DIR] [--preview]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE.
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
LOD_SCALES = [1.33, 0.9, 0.6, 0.38, 0.22]     # px per world unit (src/draw.c picks the nearest)
BRANCH_R = 64

# ---- palettes ---------------------------------------------------------------------------------------------------
# the squirrel (OBJ 0; players 2-4 = the fur hues rotated, build_assets.py)
HERO = dict(
    out=(46, 18, 34), f1=(244, 156, 86), f2=(216, 104, 50), f3=(164, 64, 32), f4=(108, 38, 24),
    tip=(255, 214, 160), cream=(250, 228, 192), cream2=(212, 176, 140), white=(250, 250, 250), ink=(18, 8, 26),
    pink=(236, 136, 148), claw=(74, 40, 30))
HERO_PAL = [HERO[k] for k in ("out", "f1", "f2", "f3", "f4", "tip", "cream", "cream2", "white", "ink", "pink",
                              "claw")]
FUR = [HERO["f1"], HERO["f2"], HERO["f3"], HERO["f4"]]
FUR_HUES = (0.0, 0.085)                         # the hues the player swaps turn (not the cream, not the tips)
P2_SHIFT = 0.55                                 # player 2: the red turned blue-grey

# props of wood (OBJ 2; entries 2-6 follow the bark of the time of day)
WOOD = dict(
    out=(34, 20, 12), b1=(176, 136, 96), b2=(136, 100, 66), b3=(100, 70, 46), b4=(66, 44, 30), moss=(112, 150, 64),
    a1=(218, 156, 84), a2=(178, 110, 52), a3=(126, 74, 36), c1=(152, 120, 72), c2=(104, 78, 46),
    hz=(226, 178, 112), grain=(234, 204, 156), dark=(26, 20, 32), hi=(250, 240, 220))
WOOD_PAL = [WOOD[k] for k in ("out", "b1", "b2", "b3", "b4", "moss", "a1", "a2", "a3", "c1", "c2", "hz", "grain",
                              "dark", "hi")]
# colourful props (OBJ 6; the leaves 14-15 follow the season)
COL = dict(
    out=(30, 20, 42), g1=(255, 240, 150), g2=(250, 202, 64), g3=(196, 140, 30), s1=(255, 196, 96), s2=(220, 128, 34),
    s3=(150, 72, 22), w1=(242, 246, 252), w2=(166, 178, 200), k1=(194, 232, 96), k2=(122, 186, 52),
    k3=(64, 122, 38), spot=(255, 222, 82), l1=(150, 200, 80), l2=(88, 140, 48))
COL_PAL = [COL[k] for k in ("out", "g1", "g2", "g3", "s1", "s2", "s3", "w1", "w2", "k1", "k2", "k3", "spot",
                            "l1", "l2")]
# birds and the balance meter (OBJ 7)
BIRD = dict(
    out=(26, 16, 32), white=(250, 250, 250), black=(36, 32, 46), red=(222, 54, 54), grey=(150, 152, 166),
    o1=(206, 166, 112), o2=(152, 110, 66), o3=(98, 66, 42), ocream=(240, 222, 182), eye=(255, 202, 44),
    beak=(240, 152, 44), mg=(116, 204, 92), my=(250, 212, 82), mr=(232, 84, 62), mrim=(44, 32, 54))
BIRD_PAL = [BIRD[k] for k in ("out", "white", "black", "red", "grey", "o1", "o2", "o3", "ocream", "eye", "beak",
                              "mg", "my", "mr", "mrim")]

# the branch texture's palette (BG 1): index -> colour (band 0); see BANDS for the others
BARK_IDX = dict(rim=1, b1=2, b2=3, b3=4, b4=5, f1=6, f2=7, m1=8, m2=9, g1=10, g2=11, knot=12, lichen=13, b5=14,
                hi=15)
BARK0 = {1: (40, 24, 16), 2: (178, 138, 98), 3: (142, 104, 70), 4: (108, 76, 50), 5: (78, 54, 36), 6: (64, 44, 30),
         7: (48, 32, 22), 8: (126, 166, 72), 9: (86, 124, 52), 10: (236, 206, 158), 11: (196, 156, 104),
         12: (56, 36, 26), 13: (168, 182, 150), 14: (124, 90, 60), 15: (214, 182, 140)}
# the backdrop (BG 2 on layer BG3): canopy 1-4, far forest 5-7, the crowns below 8-10, trunks 11, mist 12-13
SCENE0 = {1: (150, 200, 84), 2: (100, 160, 60), 3: (62, 118, 44), 4: (36, 78, 34), 5: (140, 178, 168),
          6: (116, 156, 150), 7: (96, 136, 134), 8: (112, 150, 110), 9: (88, 128, 94), 10: (66, 104, 80),
          11: (70, 60, 52), 12: (176, 200, 188), 13: (150, 180, 170), 14: (122, 168, 72), 15: (52, 96, 40)}


def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def mul(c, k, add=(0, 0, 0)):
    return tuple(max(0, min(255, int(round(c[i] * k[i] + add[i])))) for i in range(3))


# ---- the time of day and the seasons (every 400 m: src/draw.c blends them) ----------------------------------------
# sky: top, horizon, below-horizon top, bottom (the misty depths); leaves: canopy ramp; far: forest tones;
# bark: a multiply tint and an added light; leaf: the wind's leaves
BANDS = [
    dict(name="spring morning", sky=[(104, 168, 228), (232, 228, 204), (172, 200, 168), (84, 120, 96)],
         leaves=[(170, 214, 96), (118, 176, 66), (72, 132, 50), (40, 86, 38)],
         far=[(150, 188, 176), (124, 164, 156), (100, 140, 136)], crowns=[(120, 160, 112), (94, 136, 96), (70, 110, 82)],
         mist=[(184, 206, 190), (156, 184, 172)], bark=(1.0, 1.0, 1.0), light=(0, 0, 0),
         leaf=[(150, 204, 80), (88, 140, 48)]),
    dict(name="summer noon", sky=[(64, 146, 232), (196, 228, 250), (150, 196, 150), (60, 104, 70)],
         leaves=[(140, 200, 70), (86, 156, 50), (48, 112, 40), (26, 70, 30)],
         far=[(130, 176, 170), (104, 152, 146), (80, 128, 124)], crowns=[(96, 150, 90), (70, 124, 74), (50, 98, 62)],
         mist=[(170, 206, 196), (140, 184, 170)], bark=(1.06, 1.04, 1.0), light=(6, 4, 0),
         leaf=[(120, 190, 64), (64, 128, 40)]),
    dict(name="autumn afternoon", sky=[(96, 146, 210), (250, 214, 160), (206, 172, 120), (110, 80, 60)],
         leaves=[(250, 190, 70), (230, 120, 40), (180, 66, 30), (110, 40, 26)],
         far=[(204, 168, 130), (176, 136, 104), (146, 108, 84)], crowns=[(200, 130, 60), (164, 96, 46), (120, 70, 40)],
         mist=[(226, 196, 156), (196, 164, 128)], bark=(1.04, 0.96, 0.9), light=(8, 0, 0),
         leaf=[(240, 150, 50), (190, 80, 30)]),
    dict(name="dusk", sky=[(56, 58, 120), (250, 150, 110), (150, 96, 110), (52, 40, 70)],
         leaves=[(196, 120, 96), (144, 76, 80), (96, 48, 70), (58, 30, 54)],
         far=[(150, 104, 130), (120, 80, 112), (94, 62, 96)], crowns=[(116, 72, 96), (90, 56, 84), (66, 42, 70)],
         mist=[(200, 140, 140), (160, 110, 124)], bark=(0.86, 0.76, 0.84), light=(0, 0, 10),
         leaf=[(220, 120, 80), (150, 70, 70)]),
    dict(name="night", sky=[(12, 18, 46), (44, 58, 104), (36, 50, 84), (10, 14, 30)],
         leaves=[(70, 110, 120), (46, 80, 96), (30, 56, 76), (18, 34, 54)],
         far=[(52, 72, 104), (40, 58, 90), (30, 46, 76)], crowns=[(40, 64, 84), (30, 50, 72), (22, 38, 60)],
         mist=[(60, 80, 116), (44, 62, 98)], bark=(0.56, 0.62, 0.86), light=(0, 4, 18),
         leaf=[(100, 140, 150), (60, 90, 110)]),
    dict(name="winter dawn", sky=[(150, 170, 212), (252, 220, 214), (210, 214, 226), (120, 130, 156)],
         leaves=[(246, 248, 252), (206, 216, 232), (150, 160, 186), (92, 98, 124)],
         far=[(196, 204, 220), (172, 182, 204), (150, 160, 186)], crowns=[(214, 220, 234), (180, 188, 210), (140, 150, 176)],
         mist=[(232, 230, 238), (206, 208, 222)], bark=(0.9, 0.9, 0.98), light=(10, 10, 16),
         leaf=[(230, 220, 200), (180, 160, 140)]),
]


def band_bark(b):
    """The bark palette (BG 1, 16 entries) of band b."""
    t = BANDS[b]
    out = [(0, 0, 0)] * 16
    for i, c in BARK0.items():
        if i in (8, 9):                                      # the moss follows the leaves
            c = lerp(c, t["leaves"][1 if i == 8 else 2], 0.5)
        if b == 5 and i in (2, 15):                          # snow on the bark's lit top
            c = lerp(c, (240, 242, 250), 0.55)
        out[i] = mul(c, t["bark"], t["light"])
    return out


def band_scene(b):
    t = BANDS[b]
    out = [(0, 0, 0)] * 16
    for i in range(4):
        out[1 + i] = t["leaves"][i]
    for i in range(3):
        out[5 + i] = t["far"][i]
        out[8 + i] = t["crowns"][i]
    out[11] = mul(SCENE0[11], t["bark"], t["light"])
    out[12], out[13] = t["mist"]
    out[14] = lerp(t["leaves"][0], t["leaves"][1], 0.5)
    out[15] = lerp(t["leaves"][2], t["leaves"][3], 0.5)
    return out


def band_wood(b):
    """The wood props (OBJ 2): the bark entries follow the band."""
    bk = band_bark(b)
    out = [(0, 0, 0)] + list(WOOD_PAL)
    out[2], out[3], out[4], out[5] = bk[2], bk[3], bk[4], bk[5]
    out[6] = lerp(BANDS[b]["leaves"][1], WOOD["moss"], 0.4)
    return out


def band_col(b):
    out = [(0, 0, 0)] + list(COL_PAL)
    out[14], out[15] = BANDS[b]["leaf"]
    return out


# ---- drawing helpers -----------------------------------------------------------------------------------------------
def fit(n):
    return max(8, (int(math.ceil(n)) + 7) // 8 * 8)


def rot(x, y, a):
    c, s = math.cos(a), math.sin(a)
    return x * c - y * s, x * s + y * c


class Shape:
    """A shaded ellipse in a local frame, rotated by `ang` (radians, clockwise on screen) about `pivot`."""

    def __init__(self, cx, cy, rx, ry, col, flat=False, tilt=0.0):
        self.cx, self.cy, self.rx, self.ry, self.col, self.flat, self.tilt = cx, cy, rx, ry, col, flat, tilt
        self.casts = True


def draw_shapes(cv, shapes, ang=0.0, pivot=(0, 0), light=True):
    """Paint the shapes in order (later ones on top); colour = a 4-ramp shaded by the house light (in screen space),
    or a flat colour."""
    px, py = pivot
    for y in range(cv.h):
        for x in range(cv.w):
            lx, ly = rot(x + 0.5 - px, y + 0.5 - py, -ang)
            lx += px
            ly += py
            for sh in shapes:
                dx, dy = lx - sh.cx, ly - sh.cy
                if sh.tilt:
                    dx, dy = rot(dx, dy, -sh.tilt)
                u, v = dx / sh.rx, dy / sh.ry
                if u * u + v * v <= 1.0:
                    if sh.flat or not isinstance(sh.col, list):
                        cv.set(x, y, sh.col if not isinstance(sh.col, list) else sh.col[1])
                    else:
                        nx, ny = rot(u, v, ang + sh.tilt)
                        nd = nx * hs.LIGHT_DIR[0] + ny * hs.LIGHT_DIR[1]
                        cv.set(x, y, hs.shade_nd(nd, sh.col))


def flip(cv):
    return hs.flip_h(cv)


# ---- the squirrel (seen from behind) ---------------------------------------------------------------------------------
def paint_ids(W, H, shapes, ang, pivot):
    """Rasterise shapes (later on top) rotated by ang about pivot: an id map and the local normal of each pixel."""
    ids = [[-1] * W for _ in range(H)]
    nrm = [[None] * W for _ in range(H)]
    px, py = pivot
    for y in range(H):
        for x in range(W):
            lx, ly = rot(x + 0.5 - px, y + 0.5 - py, -ang)
            lx += px
            ly += py
            for i, sh in enumerate(shapes):
                dx, dy = lx - sh.cx, ly - sh.cy
                if sh.tilt:
                    dx, dy = rot(dx, dy, -sh.tilt)
                u, v = dx / sh.rx, dy / sh.ry
                if u * u + v * v <= 1.0:
                    ids[y][x] = i
                    nrm[y][x] = rot(u, v, ang + sh.tilt)
    return ids, nrm


def shade_ids(cv, shapes, ids, nrm, crease=True):
    """Colour the pixels by their shape's ramp and the house light; a shape drawn over another casts a 1-px crease
    (the darkest shade) on the pixels of the one behind that touch it: the head on the shoulders, an arm on the back."""
    H, W = len(ids), len(ids[0])
    for y in range(H):
        for x in range(W):
            i = ids[y][x]
            if i < 0:
                continue
            sh = shapes[i]
            if not isinstance(sh.col, list):
                cv.set(x, y, sh.col)
                continue
            nx, ny = nrm[y][x]
            c = hs.shade_nd(nx * hs.LIGHT_DIR[0] + ny * hs.LIGHT_DIR[1], sh.col)
            if crease:
                for dx, dy in ((0, -1), (-1, 0), (1, 0)):
                    j = ids[y + dy][x + dx] if 0 <= y + dy < H and 0 <= x + dx < W else -1
                    if j > i and shapes[j].casts:
                        c = sh.col[3]
            cv.set(x, y, c)


def crop8(cv, ax, ay):
    """Crop a drawing to its bounding box grown to multiples of 8: returns (canvas, ax, ay)."""
    xs = [x for y in range(cv.h) for x in range(cv.w) if cv.p[y][x] is not None]
    ys = [y for y in range(cv.h) for x in range(cv.w) if cv.p[y][x] is not None]
    if not xs:
        return Canvas(8, 8), 4, 4
    x0, x1, y0, y1 = min(xs), max(xs) + 1, min(ys), max(ys) + 1
    w, h = fit(x1 - x0), fit(y1 - y0)
    x0 -= (w - (x1 - x0)) // 2
    y0 = y1 - h
    out = Canvas(w, h)
    for y in range(h):
        for x in range(w):
            out.p[y][x] = cv.get(x0 + x, y0 + y)
    return out, ax - x0, ay - y0


def body(lean_deg, pose="run", frame=0):
    """The squirrel from behind, standing on the bark's normal (tilted by `lean`, clockwise = to the right).
    Drawn on a 48x48 board with the feet at (24, 40), then cropped (crop8): returns (canvas, ax, ay).
    pose: run (2 frames), jump (0 rising, 1 falling), fall (tumbling, pivoting round its middle)."""
    W = H = 48
    cv = Canvas(W, H)
    F = FUR
    BACK = [HERO["f2"], HERO["f2"], HERO["f3"], HERO["f4"]]       # the back: a shade darker than the head
    up = 0
    if pose == "run":
        lf, rf = (-3, 0) if frame == 0 else (0, -3)          # one hind foot pushes off (its sole shows)
        la, ra = (1, -1) if frame == 0 else (-1, 1)
    elif pose == "jump":
        lf = rf = -3 if frame == 0 else 1
        la = ra = -3 if frame == 0 else 1
        up = -2 if frame == 0 else 0
    else:
        lf, rf, la, ra = -2, 2, -4, 3
    cx, fy = 24, 40
    sh = [Shape(cx - 6.5, fy - 1.2 + lf + up, 3.2, 2.0, F), Shape(cx + 6.5, fy - 1.2 + rf + up, 3.2, 2.0, F),
          Shape(cx, fy - 7.0 + up, 8.4, 6.2, BACK),
          Shape(cx - 7.6, fy - 14.0 + la + up, 2.3, 3.6, F, tilt=0.45),
          Shape(cx + 7.6, fy - 14.0 + ra + up, 2.3, 3.6, F, tilt=-0.45),
          Shape(cx, fy - 14.5 + up, 6.4, 6.4, BACK),
          Shape(cx, fy - 22.5 + up, 5.8, 5.2, F),
          Shape(cx - 4.2, fy - 28.6 + up, 1.9, 3.4, F, tilt=-0.22), Shape(cx + 4.2, fy - 28.6 + up, 1.9, 3.4, F, tilt=0.22)]
    sh[0].casts = sh[1].casts = False
    a = math.radians(lean_deg)
    pivot = (cx, fy - 14) if pose == "fall" else (cx, fy)
    ids, nrm = paint_ids(W, H, sh, a, pivot)
    shade_ids(cv, sh, ids, nrm)

    def put(lx, ly, c):
        x, y = rot(lx - pivot[0], ly - pivot[1], a)
        cv.set(int(round(pivot[0] + x - 0.5)), int(round(pivot[1] + y - 0.5)), c)
    for ex, t in ((-4.2, -0.22), (4.2, 0.22)):                # ear tufts: dark tips
        put(cx + ex + math.sin(t) * 3.2, fy - 31.6 + up, HERO["f4"])
        put(cx + ex + math.sin(t) * 3.6, fy - 32.4 + up, HERO["f4"])
    for fx, lift in ((-6.5, lf), (6.5, rf)):                  # the soles of a lifted foot, the claws
        if lift < -1:
            put(cx + fx - 0.6, fy - 1.0 + lift + up, HERO["cream2"])
            put(cx + fx + 0.6, fy - 1.0 + lift + up, HERO["cream2"])
        put(cx + fx, fy + 0.6 + lift + up, HERO["claw"])
    if pose == "fall":                                         # tumbling: the cream belly shows
        for dy in range(-3, 3):
            for dx in range(-3, 4):
                if dx * dx / 9.0 + dy * dy / 7.0 <= 1:
                    put(cx + dx, fy - 12 + dy, HERO["cream"])
    put(cx - 2.6, fy - 25.0 + up, HERO["tip"])                 # the light catching the back of the head
    cv.outline(HERO["out"])
    return crop8(cv, 24, 40)


def tail(ang_deg, frame=0):
    """The bushy tail, rising from the rump and curling at the top, swung by ang (clockwise = to the right); drawn
    on a 40x40 board with the rump at (20, 34), cropped: returns (canvas, ax, ay)."""
    W = H = 40
    cv = Canvas(W, H)
    a = math.radians(ang_deg)
    pts = []
    n = 12
    for i in range(n):
        t = i / (n - 1.0)
        # up from the rump, bulging a little left, the tip curling back to the right
        x = 20 - 3.2 * math.sin(t * math.pi * 0.9) + 4.0 * max(0.0, t - 0.75) * 4
        y = 34 - 20.0 * t + 3.0 * max(0.0, t - 0.8) * 5
        r = 1.9 + 5.0 * math.sin(min(1.0, t * 1.1) * math.pi * 0.82)
        s = Shape(x, y, r * 0.92, r, FUR)
        s.casts = False
        pts.append(s)
    ids, nrm = paint_ids(W, H, pts, a, (20, 34))
    shade_ids(cv, pts, ids, nrm, crease=False)
    rng = random.Random(7 + frame)
    tufts = []                                                 # fluff: tufts poking out of the edge, away from the rump
    for y in range(H):
        for x in range(W):
            if cv.get(x, y) is None and y < 30 and rng.random() < 0.22:
                if any(cv.get(x + dx, y + dy) is not None for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                    tufts.append((x, y))
    for x, y in tufts:
        cv.set(x, y, HERO["f2"])
    for y in range(H):                                         # fur: light tips along the outer edge, a dark spine
        for x in range(W):
            if cv.get(x, y) is None:
                continue
            edge = cv.get(x - 1, y) is None or cv.get(x, y - 1) is None or cv.get(x + 1, y) is None
            if edge and (x + 2 * y) % 3 != 0 and rng.random() < 0.85:
                cv.set(x, y, HERO["tip"] if cv.get(x, y) in (HERO["f1"], HERO["f2"]) else HERO["f2"])
    for i in range(2, n - 2):
        s = pts[i]
        x, y = rot(s.cx - 20 + 0.8, s.cy - 34 + 0.6, a)
        if cv.get(int(20 + x), int(34 + y)) is not None:
            cv.set(int(20 + x), int(34 + y), HERO["f3"])
    cv.outline(HERO["out"])
    return crop8(cv, 20, 34)


def icon():
    """16x16: the squirrel's face (the title's player slots, the results), in its palette."""
    cv = Canvas(16, 16)
    hs.ellipse(cv, 4.0, 3.5, 1.8, 3.0, FUR)
    hs.ellipse(cv, 12.0, 3.5, 1.8, 3.0, FUR)
    hs.ellipse(cv, 8.0, 9.0, 6.4, 5.6, FUR)
    hs.ellipse(cv, 8.0, 11.6, 3.6, 2.6, None, flat=HERO["cream"])
    hs.eye(cv, 4, 7, look=(1, 0))
    hs.eye(cv, 10, 7, look=(-1, 0))
    cv.set(7, 10, HERO["pink"]), cv.set(8, 10, HERO["pink"])
    cv.set(4, 4, HERO["f4"]), cv.set(12, 4, HERO["f4"])
    cv.outline(HERO["out"])
    return cv


# ---- the props, at each size k (px per unit). Each returns (canvas, ax, ay): the anchor is the point on the bark ----
def framed(wpx, hpx, pad=1):
    W, H = fit(wpx + 2 * pad), fit(hpx + 2 * pad)
    return Canvas(W, H), W / 2.0, H - pad - 0.5


def acorn(k):
    cv, ax, ay = framed(7 * k, 9 * k)
    A, Cp = [WOOD["a1"], WOOD["a1"], WOOD["a2"], WOOD["a3"]], [WOOD["c1"], WOOD["c1"], WOOD["c2"], WOOD["c2"]]
    rx, ry = max(1.0, 3.2 * k), max(1.0, 3.4 * k)
    hs.ellipse(cv, ax, ay - ry, rx, ry, A)
    hs.ellipse(cv, ax, ay - 2 * ry + 0.4 * k, rx * 1.12, max(1.0, 1.8 * k), Cp)
    if k > 0.5:
        cv.set(int(ax), int(ay - 2 * ry - 1.6 * k), WOOD["c2"])
        cv.set(int(ax - rx * 0.5), int(ay - ry - 0.5), WOOD["hi"])
    cv.outline(WOOD["out"])
    return cv, ax, ay


def hazel(k):
    cv, ax, ay = framed(8 * k, 8 * k)
    H = [WOOD["hz"], WOOD["a1"], WOOD["a2"], WOOD["a3"]]
    r = max(1.0, 3.6 * k)
    hs.ellipse(cv, ax, ay - r, r, r, H)
    if k > 0.5:
        hs.ellipse(cv, ax, ay - 0.6 * r, r * 0.9, r * 0.45, None, flat=WOOD["c1"])
        cv.set(int(ax - r * 0.4), int(ay - r * 1.4), WOOD["hi"])
    cv.outline(WOOD["out"])
    return cv, ax, ay


def walnut(k):
    cv, ax, ay = framed(10 * k, 10 * k)
    G = [COL["g1"], COL["g2"], COL["g2"], COL["g3"]]
    r = max(1.2, 4.5 * k)
    hs.ellipse(cv, ax, ay - r, r, r * 0.95, G)
    if k > 0.5:                                                  # the wrinkles and a glint
        for y in range(cv.h):
            for x in range(cv.w):
                if cv.get(x, y) == COL["g2"] and (x * 3 + y * 2) % 7 == 0:
                    cv.set(x, y, COL["g3"])
        cv.set(int(ax - r * 0.4), int(ay - r * 1.5), COL["w1"])
    cv.outline(COL["out"])
    return cv, ax, ay


def bark_ramp():
    return [WOOD["b1"], WOOD["b2"], WOOD["b3"], WOOD["b4"]]


def twig(k):
    """A short side shoot sticking up from the bark, a leaf on it."""
    cv, ax, ay = framed(16 * k, 12 * k)
    B = [WOOD["grain"], WOOD["hz"], WOOD["a2"], WOOD["a3"]]     # pale, freshly broken wood: it stands out of the bark
    w = max(1.0, 1.8 * k)
    hs.capsule(cv, ax - 2 * k, ay, ax + 3 * k, ay - 9 * k, w, B)
    hs.capsule(cv, ax + 1 * k, ay - 5 * k, ax - 5 * k, ay - 8 * k, max(0.8, w * 0.7), B)
    if k > 0.3:
        hs.ellipse(cv, ax + 5 * k, ay - 10 * k, max(1.0, 3.2 * k), max(1.0, 1.8 * k), None, flat=WOOD["moss"])
    hs.ellipse(cv, ax, ay - 0.2, max(1.0, 3.5 * k), max(0.6, 1.0 * k), B)
    cv.outline(WOOD["out"])
    return cv, ax, ay


def knot(k):
    """A burl: a big rounded lump of bark, half of it (drawn twice, mirrored)."""
    full_w, h = 30 * k, 24 * k
    W, H = fit(full_w / 2 + 1), fit(h + 2)
    cv = Canvas(W, H)
    ax, ay = float(W), H - 1.5
    B = bark_ramp()
    hs.ellipse(cv, ax, ay - h * 0.48, full_w / 2, h * 0.5, B)
    if k > 0.5:                                                    # rings of the burl
        for y in range(H):
            for x in range(W):
                if cv.get(x, y) is not None:
                    d = math.hypot((x + 0.5 - ax) / (full_w / 2), (y + 0.5 - (ay - h * 0.42)) / (h * 0.45))
                    if abs(d - 0.55) < 0.06 or abs(d - 0.8) < 0.05:
                        cv.set(x, y, WOOD["b4"])
    cv.outline(WOOD["out"])
    for y in range(H):                                             # no outline on the seam
        if cv.get(W - 1, y) == WOOD["out"] and cv.get(W - 2, y) not in (None, WOOD["out"]):
            cv.set(W - 1, y, cv.get(W - 2, y))
    return cv, ax, ay


def web(k):
    """Half a web strung between two thin twigs (mirrored)."""
    full_w, h = 44 * k, 38 * k
    W, H = fit(full_w / 2 + 1), fit(h + 2)
    cv = Canvas(W, H)
    ax, ay = float(W), H - 1.5
    cx, cy = ax, ay - h * 0.5
    tx = ax - full_w / 2 + 1.5 * k
    hs.capsule(cv, tx, ay, tx + 1.5 * k, ay - h, max(0.7, 1.1 * k), None, flat=COL["s3"])    # the twig at the edge
    rings = [0.25, 0.5, 0.75, 1.0]
    for y in range(H):
        for x in range(W):
            dx, dy = (x + 0.5 - cx) / (full_w / 2 - 2 * k), (y + 0.5 - cy) / (h * 0.5)
            r = math.hypot(dx, dy)
            if r > 1.0:
                continue
            ang = math.atan2(dy, dx)
            spoke = any(abs(math.sin(ang - s)) * r * h * 0.5 < 0.55 for s in (0, math.pi / 4, math.pi / 2, 3 * math.pi / 4))
            ring = any(abs(r - q) * h * 0.5 < 0.5 for q in rings)
            if spoke or ring:
                cv.set(x, y, COL["w1"] if (x + y) % 3 else COL["w2"])
    return cv, ax, ay


def sap(k):
    """Half an amber patch, flat on the bark (seen at a glance: squashed)."""
    full_w, h = 48 * k, 14 * k
    W, H = fit(full_w / 2 + 1), fit(h + 2)
    cv = Canvas(W, H)
    ax, ay = float(W), H / 2.0
    S = [COL["s1"], COL["s1"], COL["s2"], COL["s3"]]
    for y in range(H):
        for x in range(W):
            dx, dy = (x + 0.5 - ax) / (full_w / 2), (y + 0.5 - ay) / (h / 2)
            wob = 1.0 + 0.12 * math.sin(x * 1.3) * (1 if k > 0.5 else 0)
            if dx * dx + dy * dy <= wob:
                cv.set(x, y, hs.shade_nd(-dx * 0.62 - dy * 0.78, S) if k > 0.4 else COL["s2"])
    if k > 0.5:
        for i in range(3):                                         # glossy highlights
            cv.set(int(ax - full_w * (0.15 + 0.1 * i)), int(ay - h * 0.2), COL["w1"])
    return cv, ax, ay


def hole(k):
    """The woodpecker's hole: a dark oval with splinters, flat on the bark."""
    cv, ax, ay = framed(26 * k, 10 * k)
    ay = cv.h / 2.0
    rx, ry = max(1.5, 12 * k), max(1.0, 4.5 * k)
    hs.ellipse(cv, ax, ay, rx + max(1, k), ry + max(1, k * 0.8), None, flat=WOOD["grain"])
    hs.ellipse(cv, ax, ay, rx, ry, None, flat=WOOD["dark"])
    if k > 0.5:
        for i in range(5):
            x = int(ax - rx + (2 * rx) * (i + 0.5) / 5)
            cv.set(x, int(ay - ry - 1), WOOD["grain"])
    return cv, ax, ay


def caterpillar(k, frame):
    """Side view, crawling to the right (mirrored to crawl left): green segments, a yellow spot each."""
    cv, ax, ay = framed(22 * k, 9 * k)
    K = [COL["k1"], COL["k1"], COL["k2"], COL["k3"]]
    n = 6
    r = max(1.0, 2.2 * k)
    for i in range(n):
        x = ax - 9 * k + i * 3.4 * k
        hump = (1.6 * k if (i + frame) % 3 == 0 else 0)
        hs.ellipse(cv, x, ay - r - hump, r, r, K)
        if k > 0.5:
            cv.set(int(x), int(ay - r - hump - r * 0.3), COL["spot"])
    if k > 0.5:
        hx = ax - 9 * k + (n - 1) * 3.4 * k
        cv.set(int(hx + r * 0.4), int(ay - r * 1.3), COL["out"])           # the eye
    cv.outline(COL["out"])
    return cv, ax, ay


def pinecone(k):
    cv, ax, ay = framed(10 * k, 14 * k)
    P = [WOOD["a2"], WOOD["a2"], WOOD["a3"], WOOD["b4"]]
    hs.ellipse(cv, ax, ay - 7 * k, max(1.0, 4.6 * k), max(1.5, 6.6 * k), P)
    if k > 0.5:                                                   # the scales
        for y in range(cv.h):
            for x in range(cv.w):
                if cv.get(x, y) is not None and (x + 2 * y) % 4 == 0:
                    cv.set(x, y, WOOD["c1"] if (y // 2) % 2 else WOOD["a3"])
    cv.outline(WOOD["out"])
    return cv, ax, ay


def shadow(k):
    cv, ax, ay = framed(14 * k, 5 * k)
    ay = cv.h / 2.0
    for y in range(cv.h):
        for x in range(cv.w):
            if ((x + 0.5 - ax) / max(1.0, 7 * k)) ** 2 + ((y + 0.5 - ay) / max(0.8, 2.5 * k)) ** 2 <= 1 and hs.checker(x, y):
                cv.set(x, y, WOOD["dark"])
    return cv, ax, ay


def splinters(k):
    """The broken end of a dead branch: jagged shards of pale wood."""
    cv, ax, ay = framed(14 * k, 16 * k)
    B = [WOOD["grain"], WOOD["grain"], WOOD["b1"], WOOD["b2"]]
    for dx, hgt, wd in ((-4, 14, 2.2), (0, 9, 2.0), (4, 12, 1.8)):
        x0 = ax + dx * k
        for y in range(cv.h):
            for x in range(cv.w):
                yy = ay - (y + 0.5)
                if 0 <= yy <= hgt * k:
                    half = wd * k * (1 - yy / (hgt * k + 0.01)) + 0.5
                    if abs(x + 0.5 - x0) <= half:
                        cv.set(x, y, hs.shade3(hs.ramp_dict(B), x, y, int(2 * half) + 1, int(x0 - half)))
    cv.outline(WOOD["out"])
    return cv, ax, ay


def woodpecker(k, frame):
    """Clinging to the side of the branch, seen from behind: black back with white bars, a red cap; frame 1 pecks."""
    cv, ax, ay = framed(16 * k, 24 * k)
    peck = frame * 2 * k
    sh = [Shape(ax, ay - 9 * k, max(1.2, 4.5 * k), max(2.0, 9 * k), BIRD["black"]),
          Shape(ax - 0.5 * k, ay - 19 * k + peck * 0.3, max(1.2, 3.6 * k), max(1.2, 3.4 * k), BIRD["black"])]
    draw_shapes(cv, sh)
    if k > 0.5:
        for y in range(cv.h):
            for x in range(cv.w):
                if cv.get(x, y) == BIRD["black"] and y > ay - 15 * k and (y // max(1, int(2 * k))) % 2 == 0 and x % 2 == 0:
                    cv.set(x, y, BIRD["white"])
        hs.ellipse(cv, ax - 0.5 * k, ay - 21.5 * k + peck * 0.3, max(1.0, 2.6 * k), max(1.0, 1.4 * k), None, flat=BIRD["red"])
        bx = int(ax + 3.5 * k + peck)
        for i in range(max(1, int(3 * k))):
            cv.set(bx + i, int(ay - 19 * k + peck * 0.3), BIRD["grey"])
    cv.outline(BIRD["out"])
    return cv, ax, ay


def owl(k, frame):
    """Half an owl seen from above and behind, wings spread (mirrored); frame 1: wings down."""
    full_w, h = 52 * k, 22 * k
    W, H = fit(full_w / 2 + 1), fit(h + 6 * k + 2)
    cv = Canvas(W, H)
    ax, ay = float(W), H - 1.5
    O = [BIRD["o1"], BIRD["o1"], BIRD["o2"], BIRD["o3"]]
    wy = ay - h * (0.62 if frame == 0 else 0.38)
    sh = [Shape(ax - full_w * 0.25, wy, full_w * 0.26, h * 0.22, O, tilt=0.25 if frame == 0 else -0.2),
          Shape(ax, ay - h * 0.5, max(1.5, 6 * k), h * 0.48, O),
          Shape(ax, ay - h * 0.95, max(1.5, 5.4 * k), max(1.5, 5 * k), O)]
    draw_shapes(cv, sh)
    if k > 0.5:
        for y in range(H):
            for x in range(W):
                c = cv.get(x, y)
                if c in (BIRD["o2"], BIRD["o1"]) and (x * 2 + y) % 5 == 0:
                    cv.set(x, y, BIRD["ocream"])
        cv.set(int(ax - 3 * k), int(ay - h * 1.15), BIRD["o3"])       # the ear tuft
    cv.outline(BIRD["out"])
    for y in range(H):
        if cv.get(W - 1, y) == BIRD["out"] and cv.get(W - 2, y) not in (None, BIRD["out"]):
            cv.set(W - 1, y, cv.get(W - 2, y))
    return cv, ax, ay


def leaf(frame):
    cv = Canvas(8, 8)
    L = [COL["l1"], COL["l1"], COL["l2"], COL["l2"]]
    if frame == 0:
        hs.ellipse(cv, 4, 4, 3.2, 1.6, L)
    else:
        hs.ellipse(cv, 4, 4, 1.6, 3.0, L)
    cv.set(4, 4, COL["l2"])
    return cv, 4, 4


def meter():
    """32x8: the balance arc, green in the middle, yellow, red at the ends (+-62 degrees)."""
    cv = Canvas(32, 8)
    for x in range(1, 31):
        t = abs(x + 0.5 - 16) / 15.0
        y = int(round(1 + 4.5 * t * t))
        c = BIRD["mg"] if t < 0.4 else BIRD["my"] if t < 0.7 else BIRD["mr"]
        cv.set(x, y, c), cv.set(x, y + 1, c)
    cv.outline(BIRD["mrim"])
    return cv, 16, 0


def needle():
    cv = Canvas(8, 8)
    hs.ellipse(cv, 4, 4, 2.0, 2.0, None, flat=BIRD["white"])
    cv.outline(BIRD["mrim"])
    return cv, 4, 4


def crumb(frame):
    cv = Canvas(8, 8)
    for x, y in (((2, 3), (5, 2), (4, 5)) if frame == 0 else ((1, 2), (6, 1), (5, 6), (2, 6))):
        cv.set(x, y, WOOD["a2"]), cv.set(x + 1, y, WOOD["c1"])
    return cv, 4, 4


def puff(frame):
    cv = Canvas(8, 8)
    r = [1.6, 2.6, 3.4][frame]
    for y in range(8):
        for x in range(8):
            d = math.hypot(x + 0.5 - 4, y + 0.5 - 4)
            if d < r and (frame == 0 or hs.checker(x, y, frame)):
                cv.set(x, y, WOOD["grain"] if d < r - 1 else WOOD["b1"])
    return cv, 4, 4


# ---- the list of sprites: name, palette group, frames [(canvas, ax, ay)], mirror (drawn as two halves) --------------
LEANS = [0, 20, 40, 60]
TAILS = [0, 18, 36, 54, 72, 90]


def lods(fn, *args):
    return [fn(k, *args) for k in LOD_SCALES]


def sprite_defs():
    d = []
    d.append(("sq_run", "hero", [body(l, "run", f) for l in LEANS for f in (0, 1)], 0))
    d.append(("sq_jump", "hero", [body(0, "jump", f) for f in (0, 1)], 0))
    d.append(("sq_fall", "hero", [body(l, "fall", 0) for l in (110, 175)], 0))
    d.append(("sq_tail", "hero", [tail(a) for a in TAILS], 0))
    d.append(("sq_icon", "hero", [(icon(), 8, 8)], 0))
    d.append(("acorn", "wood", lods(acorn)[:4], 0))
    d.append(("hazel", "wood", lods(hazel)[:4], 0))
    d.append(("walnut", "col", lods(walnut)[:4], 0))
    d.append(("twig", "wood", lods(twig), 0))
    d.append(("knot", "wood", lods(knot), 1))
    d.append(("web", "col", lods(web), 1))
    d.append(("sap", "col", lods(sap), 1))
    d.append(("hole", "wood", lods(hole), 0))
    d.append(("cater", "col", lods(caterpillar, 0) + lods(caterpillar, 1), 0))
    d.append(("cone", "wood", lods(pinecone), 0))
    d.append(("shadow", "wood", lods(shadow), 0))
    d.append(("splinter", "wood", lods(splinters), 0))
    d.append(("pecker", "bird", lods(woodpecker, 0)[:4] + lods(woodpecker, 1)[:4], 0))
    d.append(("owl", "bird", lods(owl, 0)[:4] + lods(owl, 1)[:4], 1))
    d.append(("leaf", "col", [leaf(0), leaf(1)], 0))
    d.append(("meter", "bird", [meter()], 0))
    d.append(("needle", "bird", [needle()], 0))
    d.append(("crumb", "wood", [crumb(0), crumb(1)], 0))
    d.append(("puff", "wood", [puff(0), puff(1), puff(2)], 0))
    return d


# ---- the branch texture (BG 1 on the affine layer) --------------------------------------------------------------------
TEX_W, TEX_H, TEX_CX = 512, 256, 256


def bark_texel(u, v, plain=True, rng=None):
    """The colour index of the bark at u (-1..1 across the cylinder) and v (0..31 along it)."""
    I = BARK_IDX
    if abs(u) >= 1.0:
        return 0
    if abs(u) > 0.965:
        return I["rim"]
    a = math.asin(u)                                         # the angle round the branch
    nd = -0.62 * math.sin(a) + 0.78 * math.cos(a)            # the house light, top-left
    base = I["b1"] if nd > 0.86 else I["b2"] if nd > 0.55 else I["b3"] if nd > 0.05 else I["b4"]
    # furrows along the length, every 12 degrees, wandering a little with v
    deg = math.degrees(a) + (2.2 * math.sin(v * 2 * math.pi / 32 + round(math.degrees(a) / 12)) if plain else 0)
    fr = (deg / 12.0) % 1.0
    if fr < 0.14 or fr > 0.94:
        return I["f2"] if nd < 0.4 else I["f1"]
    if plain:
        # short cross cracks, a patch of moss, a knot, lichen
        if v in (6, 7) and 0.35 < fr < 0.75 and int(deg // 12) % 3 == 1:
            return I["f1"]
        if 9 <= v <= 18 and -40 < math.degrees(a) < -16 and (int(deg * 3) + v) % 5 < 3:
            return I["m1"] if nd > 0.6 else I["m2"]
        kd = math.hypot((math.degrees(a) - 24) / 7.0, (v - 25) / 3.2)
        if kd < 1.0:
            return I["knot"] if kd < 0.55 else I["b5"]
        if 20 <= v <= 23 and 30 < math.degrees(a) < 44 and (v + int(deg)) % 3 == 0:
            return I["lichen"]
    if base == I["b1"] and (int(deg * 2) + v * 3) % 11 == 0:
        return I["hi"]
    return base


def branch_texture():
    """Indices (0 = transparent) of the 512x256 texture: rows 0-31 bark, 32-39 end grain, 40-47 jagged edge, then
    the fork rows (two sections, centre to centre 16 * (j + 1), 8 identical rows each)."""
    T = [[0] * TEX_W for _ in range(TEX_H)]
    I = BARK_IDX
    R = BRANCH_R
    for v in range(32):
        for x in range(TEX_CX - R, TEX_CX + R):
            T[v][x] = bark_texel((x + 0.5 - TEX_CX) / R, v)
    rng = random.Random(11)
    for r in range(8):                                       # end grain: the broken face of the far side of a gap
        for x in range(TEX_CX - R, TEX_CX + R):
            u = (x + 0.5 - TEX_CX) / R
            if abs(u) > 0.965:
                T[32 + r][x] = I["rim"]
                continue
            ring = int((abs(u) * 6 + r * 0.35)) % 2
            T[32 + r][x] = I["g1"] if ring == 0 else I["g2"]
            if r == 0:
                T[32][x] = I["b4"]
    jag = [rng.randint(0, 7) for _ in range(2 * R)]          # the jagged edge (the near side of a gap)
    for i in range(1, len(jag) - 1):
        jag[i] = max(jag[i], (jag[i - 1] + jag[i + 1]) // 2 - 1)
    for r in range(8):
        for i, x in enumerate(range(TEX_CX - R, TEX_CX + R)):
            d = jag[i]
            if r < 7 - d:
                T[40 + r][x] = 0
            elif r == 7 - d:
                T[40 + r][x] = I["g1"]
            else:
                T[40 + r][x] = bark_texel((x + 0.5 - TEX_CX) / R, 0, plain=False)
    for j in range(24):                                      # the fork rows
        S = 16 * (j + 1)
        row = [0] * TEX_W
        for x in range(TEX_W):
            ul = (x + 0.5 - (TEX_CX - S / 2.0)) / R
            ur = (x + 0.5 - (TEX_CX + S / 2.0)) / R
            inl, inr = abs(ul) < 1.0, abs(ur) < 1.0
            if not inl and not inr:
                continue
            if inl and inr:                                   # the junction: the nearer surface, a crease between
                if abs(abs(ul) - abs(ur)) < 0.06:
                    row[x] = I["f2"]
                    continue
                u = ul if abs(ul) < abs(ur) else ur
            else:
                u = ul if inl else ur
            row[x] = bark_texel(u, 0, plain=False)
        for r in range(8):
            T[48 + j * 8 + r] = list(row)
    return T


def texture_image(T, pal):
    im = Image.new("RGB", (len(T[0]), len(T)), hs.MAGENTA)
    px = im.load()
    for y, row in enumerate(T):
        for x, i in enumerate(row):
            if i:
                px[x, y] = pal[i]
    return im


# ---- the backdrop (BG 2 on layer BG3): the canopy above, the far forest at the horizon, the crowns below --------------
SCENE_W, SCENE_H = 256, 256
CANOPY_H = 48
FOREST_Y0, HORIZON_ROW = 64, 96


def backdrop():
    T = [[0] * SCENE_W for _ in range(SCENE_H)]
    rng = random.Random(5)
    # the canopy: leaf clusters hanging from the top, a 128-px pattern
    blobs = []
    for i in range(14):
        bx = rng.uniform(0, 128)
        by = rng.uniform(4, 30)
        r = rng.uniform(7, 14)
        blobs.append((bx, by, r))
    for y in range(CANOPY_H):
        for x in range(SCENE_W):
            xx = x % 128
            if y < 6:
                T[y][x] = 4 if y < 3 else 3
                continue
            best = None
            for bx, by, r in blobs:
                for off in (-128, 0, 128):
                    dx, dy = (xx - (bx + off)) / r, (y - by) / (r * 0.8)
                    d = dx * dx + dy * dy
                    if d <= 1.0 and (best is None or d < best[0]):
                        best = (d, dx, dy)
            if best:
                d, dx, dy = best
                nd = dx * hs.LIGHT_DIR[0] + dy * hs.LIGHT_DIR[1]
                c = 1 if nd > 0.55 else 2 if nd > 0.0 else 3 if nd > -0.5 else 4
                if (x * 7 + y * 3) % 13 == 0 and c < 4:
                    c += 1                                      # leaf texture
                T[y][x] = c
    for x in range(SCENE_W):                                    # a thin twig crossing the canopy
        y = int(24 + 6 * math.sin(x * 2 * math.pi / 128))
        if T[y][x] == 0 or (x % 128) > 70:
            T[y][x] = 11
    # the far forest: crowns on the horizon (5-7), then the crowns far below (8-10) fading into the mist (12-13)
    for x in range(SCENE_W):
        h1 = HORIZON_ROW - 14 - int(8 * abs(math.sin(x * math.pi / 32)) + 4 * math.sin(x * 2 * math.pi / 64 + 1))
        for y in range(max(FOREST_Y0, h1), HORIZON_ROW + 8):
            T[y][x] = 5 if y - h1 < 3 else 6 if y < HORIZON_ROW else 7
        for y in range(HORIZON_ROW + 6, HORIZON_ROW + 60):           # the shade between the crowns below
            T[y][x] = 10 if y < HORIZON_ROW + 50 or hs.dither_ok(x, y, 16 - (y - HORIZON_ROW - 50) * 2) else 0
        if x % 32 in (5, 6) and h1 < HORIZON_ROW:                 # a far trunk now and then
            for y in range(h1 + 4, HORIZON_ROW + 8):
                T[y][x] = 7
    # the crowns of the forest far below: rows of round crowns, bigger as they come nearer, lit from the top-left
    # (a 128-px pattern, far rows first), the lowest fading into the mist
    for j in range(6):
        r = 3.5 + 1.4 * j
        yc = HORIZON_ROW + 9 + 7 * j + 0.5 * j * j
        n = max(3, int(round(128 / (1.9 * r))))
        for i in range(n):
            bx = (i + 0.5 * (j % 2)) * 128.0 / n + (rng.random() - 0.5) * r * 0.6
            by = yc + (rng.random() - 0.5) * 2
            rr = r * (0.85 + 0.3 * rng.random())
            for y in range(int(by - rr) - 1, int(by + rr) + 2):
                if y < HORIZON_ROW + 4 or y >= HORIZON_ROW + 60:
                    continue
                for xx in range(int(bx - rr) - 1, int(bx + rr) + 2):
                    dx, dy = (xx + 0.5 - bx) / rr, (y + 0.5 - by) / (rr * 0.8)
                    if dx * dx + dy * dy > 1:
                        continue
                    if y >= HORIZON_ROW + 50 and not hs.dither_ok(xx, y, 16 - (y - HORIZON_ROW - 50) * 2):
                        continue
                    nd = dx * hs.LIGHT_DIR[0] + dy * hs.LIGHT_DIR[1]
                    c = 8 if nd > 0.35 else 9 if nd > -0.3 else 10
                    if j < 2:
                        c = 13 if nd > 0.35 else 12 if c == 9 else 9         # the far rows in the haze
                    for k in range(0, SCENE_W, 128):
                        T[y][(xx % 128) + k] = c
    return T


# ---- the sheet ----------------------------------------------------------------------------------------------------
GROUP_PAL = {"hero": HERO_PAL, "wood": WOOD_PAL, "col": COL_PAL, "bird": BIRD_PAL}


def layout(defs, width=512):
    """Shelf packing: returns [(name, group, mirror, [(x, y, w, h, ax, ay)])] and the sheet height."""
    out, x, y, shelf = [], 0, 0, 0
    for name, group, frames, mirror in defs:
        fr = []
        for cv, ax, ay in frames:
            if x + cv.w > width:
                x, y, shelf = 0, y + shelf, 0
            fr.append((x, y, cv.w, cv.h, ax, ay))
            x += cv.w
            shelf = max(shelf, cv.h)
        out.append((name, group, mirror, fr))
    return out, y + shelf


def write_all(out, preview=False):
    os.makedirs(out, exist_ok=True)
    defs = sprite_defs()
    lay, H = layout(defs)
    sheet = Image.new("RGB", (512, H), hs.MAGENTA)
    manifest = []
    for (name, group, frames, mirror), (_n, _g, _m, fr) in zip(defs, lay):
        for (cv, ax, ay), (x, y, w, h, _ax, _ay) in zip(frames, fr):
            sheet.paste(cv.image(), (x, y))
        manifest.append(dict(name=name, group=group, mirror=mirror,
                             frames=[dict(x=x, y=y, w=w, h=h, ax=int(math.floor(ax)), ay=int(math.floor(ay)))
                                     for (x, y, w, h, ax, ay) in fr]))
    files = {"sprites.png": sheet}
    files["branch.png"] = texture_image(branch_texture(), band_bark(0))
    files["backdrop.png"] = texture_image(backdrop(), band_scene(0))
    for f, im in files.items():
        im.save(os.path.join(out, f))
        if preview:
            os.makedirs(os.path.join(out, "preview"), exist_ok=True)
            im.resize((im.width * 3, im.height * 3), Image.NEAREST).save(os.path.join(out, "preview", f))
    with open(os.path.join(out, "sprites.json"), "w") as f:
        json.dump(manifest, f, indent=0)
    pals = dict(groups={g: [list(c) for c in p] for g, p in GROUP_PAL.items()},
                fur_hues=list(FUR_HUES), p2_shift=P2_SHIFT,
                bands=[dict(name=b["name"], sky=[list(c) for c in b["sky"]],
                            bark=[list(c) for c in band_bark(i)], scene=[list(c) for c in band_scene(i)],
                            wood=[list(c) for c in band_wood(i)], col=[list(c) for c in band_col(i)])
                       for i, b in enumerate(BANDS)])
    with open(os.path.join(out, "palettes.json"), "w") as f:
        json.dump(pals, f, indent=0)
    return sorted(files) + ["sprites.json", "palettes.json"]


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", default=os.path.join(GAME, "art"))
    ap.add_argument("--preview", action="store_true", help="also write 3x previews in <out>/preview")
    a = ap.parse_args(argv)
    for f in write_all(a.out, a.preview):
        print("wrote %s/%s" % (a.out, f))


if __name__ == "__main__":
    main()

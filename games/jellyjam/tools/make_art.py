#!/usr/bin/env python3
"""Jelly Jam: the code-drawn art, in the 8BCraft house style (docs/art-direction.md).

Code-drawn art only, no image generation: every sprite and tile is drawn here with games/common/tools/house_style.py
(1-px outlines never black, 3 shades lit from the top-left, white glints, big eyes). build_assets.py cuts the sheets
with the same layout (the functions below) and turns them into the jelly bitmaps, tiles, palettes and sprites.

What is drawn (DESIGN.md "Art"):
  the jellies (draw.c paints them into the fields by software): round shiny blobs, 16 and 12 px, in four colours
    (strawberry, lime, blueberry, lemon) and the grey rain, each in the 16 shapes of its joins to the same colour round
    it (up, right, down, left); and their faces on top, one look per colour (round eyes, happy arcs, sleepy lids, wide
    eyes; the rain dozes), in three moods (normal, blink, surprised: melting)
  the hero, Jam: a strawberry-jam jelly in a chef's toque (players 2-4: its hue turned), idle, happy and sad
  the computer chefs, our own animal cooks in toques and neckerchiefs (32x32 busts: idle, happy, dizzy): Pip the mouse,
    Dot the duckling, Hopsy the bunny, Mochi the cat, Truffle the piglet, Basil the fox, Bao the panda, Bruno the bear
  the props: the jellies as sprites (the next pairs), jam droplets, the rain queue's icons, the round stars
  the kitchen (BG3 tiles): a dusty-teal tiled wall, the fields' jar frames and their dark plum insides, a wooden board
    and shelves with jam jars

Writes, in the art folder (default games/jellyjam/art):
  jellies.png (the jelly shapes and faces, 16 and 12 px), sprites.png (the hero and the props), chefs.png (the chefs, one
  row each), tiles/kitchen.png (the kitchen's tiles)

    python3 games/jellyjam/tools/make_art.py [--out DIR] [--preview]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/jellyjam/LICENSE.
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
LX, LY = hs.LIGHT_DIR

# ---- the jelly palette: one BG palette for the fields' bitmaps (and OBJ 6 for the jelly sprites) ---------------------
# index 1 outline, 2 white, 3-5 strawberry (light, mid, dark), 6-8 lime, 9-11 blueberry, 12-14 lemon, 15 rain grey
OUTLINE = (40, 16, 48)
WHITE = (252, 250, 246)
RAMPS = [
    [(255, 150, 164), (234, 62, 96), (156, 22, 64)],        # 1 strawberry
    [(196, 242, 120), (108, 198, 66), (38, 124, 56)],        # 2 lime
    [(156, 204, 255), (72, 122, 236), (40, 52, 150)],        # 3 blueberry
    [(255, 244, 156), (250, 202, 52), (196, 126, 22)],       # 4 lemon
]
GREY = (170, 170, 196)
JELLY_PAL = [OUTLINE, WHITE] + [c for r in RAMPS for c in r] + [GREY]
NAMES = ["strawberry", "lime", "blueberry", "lemon", "rain"]
SIZES = (16, 12)
MOODS = ("normal", "blink", "oops")


def ramp_of(k):
    """colour k (0..4: the four jellies, then the rain): light, mid, dark"""
    if k < 4:
        return RAMPS[k]
    return [WHITE, GREY, RAMPS[2][2]]                         # the rain: grey, a navy shade


def body(S, k, mask):
    """A jelly's body (no face): a round blob with bridges to the neighbours it joins (mask: 1 up, 2 right, 4 down,
    8 left), the house light from the top-left, a white glint, a 1-px outline except where it joins."""
    cv = Canvas(S, S)
    c = (S - 1) / 2.0
    R = S / 2.0 - 1.0
    hw = R * 0.72                                             # a bridge's half width
    lt, md, dk = ramp_of(k)

    def inside(x, y):
        dx, dy = x - c, y - c
        if dx * dx + dy * dy <= R * R + 0.3:
            return True
        if (mask & 1) and y <= c and abs(dx) <= hw:
            return True
        if (mask & 4) and y >= c and abs(dx) <= hw:
            return True
        if (mask & 8) and x <= c and abs(dy) <= hw:
            return True
        if (mask & 2) and x >= c and abs(dy) <= hw:
            return True
        return False
    for y in range(S):
        for x in range(S):
            if not inside(x, y):
                continue
            dx, dy = (x - c) / R, (y - c) / R
            dx, dy = max(-1.0, min(1.0, dx)), max(-1.0, min(1.0, dy))
            nd = dx * LX + dy * LY - 0.28 * (dx * dx + dy * dy) + 0.12
            if k < 4:
                cv.set(x, y, lt if nd > 0.42 else md if nd > -0.42 else dk)
            else:                                             # the rain: mostly grey, its navy shade a thin rim
                cv.set(x, y, lt if nd > 0.42 else md if nd > -0.70 else dk)
    # the outline: outside pixels touching the body, but not on a joined edge of the cell
    add = []
    for y in range(S):
        for x in range(S):
            if cv.p[y][x] is not None:
                continue
            if (y == 0 and mask & 1) or (y == S - 1 and mask & 4) or (x == 0 and mask & 8) or (x == S - 1 and mask & 2):
                if inside(x, y) or any(cv.get(x + a, y + b) is not None for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                    continue
            if any(cv.get(x + a, y + b) not in (None, OUTLINE) for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                add.append((x, y))
    for x, y in add:
        cv.set(x, y, OUTLINE)
    # the glint, top-left
    gx, gy = int(round(c - R * 0.48)), int(round(c - R * 0.52))
    cv.set(gx, gy, WHITE)
    if S >= 16:
        cv.set(gx + 1, gy, WHITE)
        cv.set(gx, gy + 1, lt)
    return cv


def face(S, k, mood):
    """A jelly's face, drawn over its body: eyes and a mouth in the outline colour and white. One look per colour."""
    cv = Canvas(S, S)
    big = S >= 16
    cy = S // 2 - (1 if big else 1)
    lx, rx = (S // 2 - 4, S // 2 + 1) if big else (S // 2 - 3, S // 2 + 1)
    O, W = OUTLINE, WHITE

    def closed(x, y):                                         # a closed eye: a short line
        cv.set(x, y + 1, O)
        cv.set(x + 1, y + 1, O)
        if big:
            cv.set(x + 2, y + 1, O)

    def round_eye(x, y, look_down=True):                      # the house eye: white with a pupil
        w, h = (3, 3) if big else (2, 2)
        cv.rect(x, y, x + w - 1, y + h - 1, W)
        cv.set(x + w - 1, y + h - 1, O)
        if big:
            cv.set(x + w - 2, y + h - 1, O)
            cv.set(x + w - 1, y + h - 2, O)

    def mouth(kind):
        mx, my = S // 2 - 1, cy + (4 if big else 3)
        if kind == "smile":
            cv.set(mx - (1 if big else 0), my, O)
            cv.set(mx + 2 if big else mx + 1, my, O)
            cv.set(mx, my + 1, O)
            cv.set(mx + 1, my + 1, O)
        elif kind == "o":
            cv.set(mx, my, O)
            cv.set(mx + 1, my, O)
            if big:
                cv.set(mx, my + 1, O)
                cv.set(mx + 1, my + 1, O)
        elif kind == "flat":
            cv.set(mx, my, O)
            cv.set(mx + 1, my, O)
        elif kind == "open":
            cv.rect(mx, my, mx + 1, my + (1 if big else 0), O)
            if big:
                cv.set(mx, my + 2, O)
                cv.set(mx + 1, my + 2, O)
    if mood == "blink":
        closed(lx, cy)
        closed(rx, cy)
        mouth("flat" if k == 4 else "smile")
        return cv
    if mood == "oops":                                        # melting: eyes wide, mouth open
        for x in (lx, rx):
            cv.rect(x, cy - 1, x + (2 if big else 1), cy + (1 if big else 0), W)
            cv.set(x + (1 if big else 0), cy, O)
        mouth("open")
        return cv
    if k == 0:                                                # strawberry: round eyes looking down, a smile
        round_eye(lx, cy)
        round_eye(rx, cy)
        mouth("smile")
    elif k == 1:                                              # lime: happy arcs
        for x in (lx, rx):
            cv.set(x, cy + 1, O)
            cv.set(x + 1, cy, O)
            if big:
                cv.set(x + 2, cy + 1, O)
            else:
                cv.set(x + 2, cy + 1, O) if x + 2 < S else None
        mouth("smile")
    elif k == 2:                                              # blueberry: sleepy lids
        for x in (lx, rx):
            w = 3 if big else 2
            cv.rect(x, cy, x + w - 1, cy + 1, W)
            for i in range(w):
                cv.set(x + i, cy, O)
            cv.set(x + w - 1, cy + 1, O)
        mouth("flat")
    elif k == 3:                                              # lemon: wide open eyes, an "o"
        for x in (lx, rx):
            w = 3 if big else 2
            cv.rect(x, cy - 1, x + w - 1, cy + 1, W)
            cv.set(x + w - 2, cy, O)
            cv.set(x + w - 1, cy, O) if big else None
            cv.set(x + w - 2, cy + 1, O) if big else None
        mouth("o")
    else:                                                     # the rain dozes
        closed(lx, cy)
        closed(rx, cy)
        mouth("flat")
    return cv


def jelly_sheet():
    """{(size, colour, mask): canvas} bodies and {(size, colour, mood): canvas} faces."""
    bodies, faces = {}, {}
    for S in SIZES:
        for k in range(5):
            for m in range(16 if k < 4 else 1):
                bodies[(S, k, m)] = body(S, k, m)
            for d, mood in enumerate(MOODS):
                faces[(S, k, d)] = face(S, k, mood)
    return bodies, faces


def jelly_layout():
    """Where each body and face goes on jellies.png: rows by size and colour, 16 bodies then 3 faces."""
    pos = {}
    y = 0
    for S in SIZES:
        for k in range(5):
            for m in range(16 if k < 4 else 1):
                pos[("b", S, k, m)] = (m * S, y)
            for d in range(3):
                pos[("f", S, k, d)] = ((16 + d) * S, y)
            y += S
    return pos, 19 * 16, y


# ---- the hero: Jam, a strawberry-jam jelly chef (OBJ 0; players 2-4: hue turned) -----------------------------------
HERO_RAMP = [(255, 206, 222), (248, 134, 172), (218, 74, 128), (150, 36, 88)]
HAT = [(252, 250, 246), (210, 210, 228)]
INK = (18, 8, 26)
CHEEK = (255, 156, 176)
HERO_OUT = (52, 14, 44)
HERO_PAL = [HERO_OUT, HAT[0], HAT[1], INK, CHEEK] + HERO_RAMP


def toque(cv, cx, top, w):
    """A chef's toque: a puffy white top (three puffs) on a band."""
    r = w / 4.0
    for i, (ox, oy) in enumerate(((-r * 1.1, r * 0.4), (r * 1.1, r * 0.4), (0, -r * 0.2))):
        hs.ellipse(cv, cx + ox, top + r + oy, r * 1.15, r, [HAT[0], HAT[0], HAT[0], HAT[1]])
    band_y = int(top + 2 * r + 1)
    for y in range(band_y, band_y + 3):
        for x in range(int(cx - w / 2.0 + 1), int(cx + w / 2.0)):
            cv.set(x, y, HAT[1] if y == band_y + 2 else HAT[0])
    return band_y + 3


def hero(frame):
    """frame: idle0, idle1, happy, sad (32x32)."""
    cv = Canvas(32, 32)
    sq = 1.0 if frame != "idle1" else 1.08
    bw, bh = 11.5 * sq, 10.0 / sq
    cx, cy = 16.0, 31 - bh
    hs.ellipse(cv, cx, cy, bw, bh, HERO_RAMP)
    for y in range(32):                                       # the jelly's wobbly bottom edge, a shade darker
        for x in range(32):
            if cv.get(x, y) is not None and cv.get(x, y + 1) is None and y > cy + bh * 0.6:
                cv.set(x, y, HERO_RAMP[3])
    cv.set(int(cx - bw * 0.55), int(cy - bh * 0.45), HAT[0])
    cv.set(int(cx - bw * 0.55) + 1, int(cy - bh * 0.45), HAT[0])
    toque(cv, cx + 1, cy - bh - 9, 14)
    ey = int(cy - 1)
    if frame == "happy":
        for x in (12, 18):
            cv.set(x, ey, INK), cv.set(x + 1, ey - 1, INK), cv.set(x + 2, ey, INK)
        cv.rect(15, ey + 3, 17, ey + 4, INK)
        cv.set(16, ey + 4, CHEEK)
    elif frame == "sad":
        for x in (12, 18):
            cv.rect(x, ey, x + 2, ey + 2, HAT[0])
            cv.set(x + 1, ey + 2, INK), cv.set(x + 1, ey + 1, INK)
        cv.set(11, ey - 2, INK), cv.set(12, ey - 3, INK)
        cv.set(21, ey - 2, INK), cv.set(20, ey - 3, INK)
        cv.set(15, ey + 5, INK), cv.set(16, ey + 4, INK), cv.set(17, ey + 5, INK)
        cv.set(22, ey + 3, HAT[0]), cv.set(22, ey + 4, HAT[1])       # a tear
    else:
        for x in (12, 18):
            hs.eye(cv, x, ey - 1, look=(1, 1), white=HAT[0], pupil=INK, big=True)
        cv.set(15, ey + 4, INK), cv.set(16, ey + 5, INK), cv.set(17, ey + 4, INK)
    cv.rect(9, ey + 3, 10, ey + 3, CHEEK)
    cv.rect(22, ey + 3, 23, ey + 3, CHEEK)
    cv.outline(HERO_OUT)
    return cv


def hero_icon():
    cv = Canvas(16, 16)
    hs.ellipse(cv, 8.0, 11.0, 6.5, 4.6, HERO_RAMP)
    toque(cv, 8.5, 0.5, 9)
    cv.set(6, 10, INK), cv.set(10, 10, INK), cv.set(6, 9, HAT[0]), cv.set(10, 9, HAT[0])
    cv.set(5, 7, HAT[0])
    cv.outline(HERO_OUT)
    return cv


# ---- the props (OBJ 6, the jelly palette): next-pair jellies, droplets, rain icons, stars --------------------------
def sprite_jelly(S, k):
    cv = Canvas(16, 16)
    o = (16 - S) // 2
    b, f = body(S, k, 0), face(S, k, 0)
    cv.paste(b, o, o)
    cv.paste(f, o, o)
    return cv


def droplet(k, big):
    cv = Canvas(8, 8)
    lt, md, dk = ramp_of(k)
    if big:
        hs.ellipse(cv, 4.0, 4.5, 2.6, 2.6, [lt, lt, md, dk])
        cv.set(3, 3, WHITE)
    else:
        hs.ellipse(cv, 4.0, 4.0, 1.6, 1.6, None, flat=md)
        cv.set(3, 3, lt)
    cv.outline(OUTLINE)
    return cv


def rain_icon(kind):
    """The rain queue: small (1), big (6), a lump with a crest (30), a jam jar (180), a crown (720)."""
    if kind == "small":
        cv = Canvas(8, 8)
        hs.ellipse(cv, 4.0, 4.5, 2.7, 2.5, [WHITE, GREY, GREY, RAMPS[2][2]])
        cv.outline(OUTLINE)
        return cv
    cv = Canvas(16, 16)
    if kind == "big":
        cv.paste(body(12, 4, 0), 2, 2)
        cv.paste(face(12, 4, 0), 2, 2)
    elif kind == "lump":                                      # a pile of three
        for bx, by in ((4.5, 11.0), (11.5, 11.0), (8.0, 5.5)):
            hs.ellipse(cv, bx, by, 3.6, 3.4, [WHITE, GREY, GREY, RAMPS[2][2]])
        for bx, by in ((4.5, 11.0), (11.5, 11.0), (8.0, 5.5)):
            cv.set(int(bx) - 1, int(by), OUTLINE), cv.set(int(bx) + 1, int(by), OUTLINE)
        cv.outline(OUTLINE)
    elif kind == "jar":
        for y in range(5, 15):                                # the glass, jam inside
            for x in range(3, 13):
                cv.set(x, y, RAMPS[0][1] if y > 7 else WHITE)
        for y in range(8, 14):
            cv.set(4, y, RAMPS[0][0])
        for x in range(2, 14):                                # the lid
            cv.set(x, 3, RAMPS[0][2]), cv.set(x, 4, RAMPS[0][1] if x % 2 else WHITE)
        cv.outline(OUTLINE)
    elif kind == "crown":
        y0 = 4
        for x in range(2, 14):
            for y in range(y0 + 4, y0 + 10):
                cv.set(x, y, RAMPS[3][1] if y < y0 + 8 else RAMPS[3][2])
        for px in (2, 7, 13):
            for y in range(y0, y0 + 4):
                cv.set(px, y, RAMPS[3][1]), cv.set(px + (1 if px < 13 else -1), y + 1, RAMPS[3][0])
        cv.set(5, y0 + 6, RAMPS[0][1]), cv.set(10, y0 + 6, RAMPS[2][1]), cv.set(4, y0 + 5, WHITE)
        cv.outline(OUTLINE)
    return cv


def star(on):
    cv = Canvas(8, 8)
    pts = [(3, 0), (4, 0), (3, 1), (4, 1), (0, 2), (1, 2), (2, 2), (3, 2), (4, 2), (5, 2), (6, 2), (7, 2), (1, 3), (2, 3),
           (3, 3), (4, 3), (5, 3), (6, 3), (2, 4), (3, 4), (4, 4), (5, 4), (1, 5), (2, 5), (5, 5), (6, 5), (1, 6), (6, 6)]
    for x, y in pts:
        cv.set(x, y, (RAMPS[3][1] if y > 2 else RAMPS[3][0]) if on else GREY)
    if on:
        cv.set(3, 2, WHITE)
    cv.outline(OUTLINE) if not on else None
    return cv


def props():
    """name -> canvas (OBJ 6, the jelly palette)."""
    out = {}
    for S in SIZES:
        for k in range(5):
            out["jelly%d_%d" % (S, k)] = sprite_jelly(S, k)
    for k in range(5):
        out["drop_%d" % k] = droplet(k, True)
        out["dropsm_%d" % k] = droplet(k, False)
    for kind in ("small", "big", "lump", "jar", "crown"):
        out["rain_%s" % kind] = rain_icon(kind)
    out["star_on"] = star(True)
    out["star_off"] = star(False)
    return out


def hero_sprites():
    out = {"hero_%s" % f: hero(f) for f in ("idle0", "idle1", "happy", "sad")}
    out["hero_icon"] = hero_icon()
    return out


# ---- the computer chefs (OBJ 7, one palette each, loaded with the chef) ----------------------------------------------
# (name, animal, fur ramp, muzzle (light, dark), inner ear, nose, collar (light, dark), extra)
CHEFS = [
    ("PIP", "mouse", [(214, 210, 222), (164, 158, 178), (112, 104, 130)], [(240, 232, 236), (200, 190, 200)],
     (246, 164, 184), (234, 110, 140), [(236, 84, 84), (170, 40, 52)], (60, 52, 70)),
    ("DOT", "duck", [(255, 240, 132), (246, 206, 64), (200, 150, 30)], [(255, 250, 200), (240, 220, 120)],
     (255, 196, 120), (244, 140, 40), [(96, 160, 236), (52, 96, 176)], (204, 96, 24)),
    ("HOPSY", "bunny", [(252, 248, 242), (224, 216, 212), (176, 166, 172)], [(255, 255, 255), (236, 230, 228)],
     (250, 172, 196), (236, 120, 150), [(120, 200, 110), (60, 140, 70)], (150, 140, 150)),
    ("MOCHI", "cat", [(255, 204, 132), (238, 152, 70), (186, 100, 40)], [(255, 240, 216), (240, 214, 180)],
     (250, 170, 180), (236, 120, 140), [(130, 110, 220), (80, 60, 160)], (160, 76, 30)),
    ("TRUFFLE", "pig", [(255, 206, 214), (242, 156, 176), (204, 108, 136)], [(255, 180, 196), (226, 120, 150)],
     (232, 120, 150), (190, 70, 110), [(250, 200, 70), (200, 140, 30)], (150, 60, 90)),
    ("BASIL", "fox", [(252, 156, 84), (226, 106, 46), (164, 66, 30)], [(255, 248, 238), (232, 222, 214)],
     (90, 50, 50), (50, 30, 36), [(90, 190, 170), (40, 130, 120)], (60, 36, 40)),
    ("BAO", "panda", [(252, 252, 252), (222, 222, 232), (176, 176, 192)], [(255, 255, 255), (230, 230, 238)],
     (70, 64, 80), (40, 34, 48), [(236, 90, 110), (170, 40, 70)], (52, 46, 60)),
    ("BRUNO", "bear", [(206, 150, 100), (160, 106, 64), (110, 70, 42)], [(236, 200, 150), (206, 162, 112)],
     (180, 120, 90), (56, 34, 30), [(250, 216, 80), (206, 150, 30)], (80, 52, 36)),
]
CHEF_OUT = (36, 18, 34)


def chef_palette(i):
    _n, _a, fur, muz, ear, nose, col, extra = CHEFS[i]
    return [CHEF_OUT, HAT[0], HAT[1], INK, CHEEK] + fur + muz + [ear, nose] + col + [extra]


def chef(i, mood):
    """A chef's 32x32 bust: the head (ears by animal), the muzzle, the face by mood, the toque, the neckerchief."""
    _n, animal, fur, muz, ear, nose, col, extra = CHEFS[i]
    cv = Canvas(32, 32)
    cx, cy, R = 16.0, 19.0, 9.0
    fr = list(fur) + [fur[2]]
    # ears behind the head
    if animal == "mouse":
        for ex in (-8.5, 8.5):
            hs.ellipse(cv, cx + ex, cy - 7, 5.2, 5.0, fr)
            hs.ellipse(cv, cx + ex, cy - 7, 3.0, 2.8, None, flat=ear)
    elif animal in ("cat", "fox"):                            # pointed ears, up and out beside the toque
        tall = animal == "fox"
        for s in (-1, 1):
            tipx, tipy = cx + s * (10.5 if tall else 9.5), cy - (17 if tall else 15)
            ins, outs, by = cx + s * 2.5, cx + s * 10.0, cy - 5      # the base: from the brow out to the head's side
            for y in range(int(tipy), int(by) + 1):
                t = (y - tipy) / float(by - tipy)
                a, b = tipx + (ins - tipx) * t, tipx + (outs - tipx) * t
                lo, hi = (a, b) if a < b else (b, a)
                for x in range(int(round(lo)), int(round(hi)) + 1):
                    edge = abs(x - (tipx + (outs - tipx) * t)) < 1.0  # the outer edge, in shade
                    cv.set(x, y, extra if tall and t < 0.3 else fur[2] if edge else fur[1])
                    if t > 0.3 and lo + 1.5 < x < hi - 1.5:
                        cv.set(x, y, ear if not tall else muz[0])
    elif animal in ("bear", "panda"):
        for ex in (-7.5, 7.5):
            hs.ellipse(cv, cx + ex, cy - 7.5, 3.8, 3.6, None, flat=(extra if animal == "panda" else fur[1]))
            if animal == "bear":
                hs.ellipse(cv, cx + ex, cy - 7.5, 1.8, 1.7, None, flat=ear)
    # the head
    hs.ellipse(cv, cx, cy, R + (0.8 if animal in ("bear", "pig") else 0), R - 0.5, fr)
    if animal == "pig":                                       # ears folded forward over the head's top corners
        for s in (-1, 1):
            for k in range(5):
                x0, x1 = cx + s * (5.5 + k * 0.5), cx + s * (5.5 + 1.0 + k * 1.1)
                lo, hi = (x0, x1) if x0 < x1 else (x1, x0)
                for x in range(int(round(lo)), int(round(hi)) + 1):
                    cv.set(x, int(cy - 11 + k), fur[2] if k == 4 or abs(x - x1) < 0.8 else fur[0] if k < 2 else ear)
    if animal == "panda":                                     # eye patches
        for s in (-1, 1):
            hs.ellipse(cv, cx + s * 4.0, cy - 0.5, 2.6, 3.0, None, flat=extra)
    if animal == "cat":                                       # tabby stripes
        for s in (-1, 0, 1):
            cv.set(int(cx + s * 3), int(cy - 8), fur[2]), cv.set(int(cx + s * 3), int(cy - 7), fur[2])
    # the muzzle
    if animal == "duck":
        hs.ellipse(cv, cx, cy + 4.0, 5.0, 2.4, None, flat=nose)
        cv.rect(int(cx - 4), int(cy + 4), int(cx + 4), int(cy + 4), extra)
    elif animal == "pig":
        hs.ellipse(cv, cx, cy + 3.5, 3.8, 2.6, None, flat=muz[0])
        cv.set(int(cx - 1), int(cy + 3), muz[1]), cv.set(int(cx + 1), int(cy + 3), muz[1])
    else:
        hs.ellipse(cv, cx, cy + 4.0, 5.2 if animal in ("fox", "bear", "panda") else 4.4, 3.4, None, flat=muz[0])
        for x in range(int(cx - 4), int(cx + 5)):
            if cv.get(x, int(cy + 7)) == muz[0]:
                cv.set(x, int(cy + 7), muz[1])
        cv.rect(int(cx - 1), int(cy + 2), int(cx), int(cy + 2), nose)
        cv.set(int(cx - 1), int(cy + 3), nose) if animal != "bear" else cv.rect(int(cx - 1), int(cy + 3), int(cx), int(cy + 3), nose)
        if animal in ("mouse", "cat"):                        # whiskers
            for s in (-1, 1):
                cv.set(int(cx + s * 7), int(cy + 3), extra), cv.set(int(cx + s * 8), int(cy + 2), extra)
                cv.set(int(cx + s * 7), int(cy + 5), extra), cv.set(int(cx + s * 8), int(cy + 6), extra)
    # the face
    ey = int(cy - 1)
    lx, rx = int(cx - 5), int(cx + 3)
    if mood == "happy":
        for x in (lx, rx):
            cv.set(x, ey + 1, INK), cv.set(x + 1, ey, INK), cv.set(x + 2, ey + 1, INK)
        if animal != "duck":
            cv.rect(int(cx - 1), int(cy + 5), int(cx), int(cy + 6), INK)
    elif mood == "dizzy":
        for x in (lx, rx):
            cv.set(x, ey - 1, INK), cv.set(x + 2, ey - 1, INK), cv.set(x + 1, ey, INK)
            cv.set(x, ey + 1, INK), cv.set(x + 2, ey + 1, INK)
        if animal != "duck":
            cv.set(int(cx - 1), int(cy + 6), INK), cv.set(int(cx), int(cy + 5), INK), cv.set(int(cx + 1), int(cy + 6), INK)
    else:
        for x in (lx, rx):
            white = HAT[0]
            cv.rect(x, ey - 1, x + 2, ey + 1, white)
            cv.set(x + 1, ey, INK), cv.set(x + 1, ey + 1, INK), cv.set(x + 2, ey + 1, INK)
        if animal != "duck":
            cv.set(int(cx - 2), int(cy + 5), INK), cv.set(int(cx - 1), int(cy + 6), INK)
            cv.set(int(cx), int(cy + 6), INK), cv.set(int(cx + 1), int(cy + 5), INK)
    for s in (-1, 1):
        cv.set(int(cx + s * 6), int(cy + 3), CHEEK)
    # the toque (Bruno, the head chef, has the tallest), the neckerchief
    w = 16 if animal == "bear" else 12
    toque(cv, cx, 0 if animal == "bear" else 2, w)
    if animal == "bunny":                                     # the ears poke out beside the toque
        for s in (-1, 1):
            hs.capsule(cv, cx + s * 6, cy - 8, cx + s * 11, cy - 17, 2.4, fr)
            hs.capsule(cv, cx + s * 6.5, cy - 9, cx + s * 10.5, cy - 16, 0.9, None, flat=ear)
    for y in range(28, 32):
        for x in range(int(cx - 7 + (y - 28)), int(cx + 8 - (y - 28))):
            cv.set(x, y, col[0] if (x + y) % 5 else col[1])
    cv.set(int(cx), 31, col[1])
    cv.outline(CHEF_OUT)
    return cv


def chef_sheet():
    """{(i, mood): canvas}"""
    return {(i, m): chef(i, m) for i in range(len(CHEFS)) for m in ("idle", "happy", "dizzy")}


# ---- the kitchen (BG3 tiles) -------------------------------------------------------------------------------------
WALL = [(102, 140, 152), (84, 122, 138), (68, 104, 122), (46, 70, 88), (128, 166, 176), (92, 116, 140)]
FIELD = [(26, 16, 40), (220, 200, 244), (152, 126, 204), (36, 22, 52), (54, 34, 74), (66, 44, 90), (98, 74, 130)]
WOOD = [(214, 164, 104), (176, 124, 72), (132, 88, 48), (88, 54, 30)]
GLASS = [(232, 242, 250), (178, 202, 222)]
JAM = [(200, 40, 72), (238, 100, 124), (240, 150, 40), (255, 204, 104), (122, 52, 152), (176, 104, 206)]
LID = [(222, 64, 72), (250, 246, 236)]
DARK = (44, 24, 30)
JARS_PAL = WOOD + GLASS + JAM + LID + [DARK]                 # 15
BACKDROP = (24, 14, 34)                                      # behind the frames' rounded corners


def wall_meta(variant):
    """A 16x16 kitchen tile with its grout (two tints: a gentle checker)."""
    cv = Canvas(16, 16)
    face, light, shade, grout, hil, alt = WALL
    base = face if variant == 0 else alt
    for y in range(16):
        for x in range(16):
            if x == 15 or y == 15:
                c = grout
            elif x == 0 or y == 0:
                c = hil
            elif x == 14 or y == 14:
                c = shade
            else:
                c = base
            cv.set(x, y, c)
    cv.set(3, 3, hil), cv.set(4, 3, hil), cv.set(3, 4, hil)
    return cv


def ring_tiles():
    """The fields' jar frames (4 px: outline, light rim, mid rim, inner shadow) round the plum inside: 9 tiles of 8x8
    (corners, edges, the inside), the frame in the tile's outer half."""
    O, L, M, S, I, D, H = FIELD
    ring = [O, L, M, S]
    out = {}
    for name, (fx, fy) in {"tl": (-1, -1), "t": (0, -1), "tr": (1, -1), "l": (-1, 0), "in": (0, 0), "r": (1, 0),
                           "bl": (-1, 1), "b": (0, 1), "br": (1, 1)}.items():
        cv = Canvas(8, 8)
        for y in range(8):
            for x in range(8):
                dx = x if fx < 0 else (7 - x) if fx > 0 else 99      # distance to the frame's outer edge
                dy = y if fy < 0 else (7 - y) if fy > 0 else 99
                d = min(dx, dy)
                if fx and fy and dx < 4 and dy < 4:                 # a rounded corner
                    if dx + dy < 2:
                        continue
                    d = min(d, dx + dy - 2) if dx + dy < 5 else d
                c = ring[d] if d < 4 else (D if (x + y * 3) % 16 == 5 and not (fx or fy) else I)
                cv.set(x, y, c)
        out["ring_" + name] = cv
    return out


def wood_tile(kind):
    cv = Canvas(8, 8)
    L, M, D, DD = WOOD
    for y in range(8):
        for x in range(8):
            c = M
            if kind == "board":
                c = L if y == 2 and x % 5 else D if y == 6 and (x + 2) % 6 == 0 else M
            elif kind == "board2":
                c = D if y == 3 and x % 7 == 2 else L if y == 5 and x % 4 == 1 else M
            elif kind == "lip":                             # a shelf's top: a light edge, the front darker
                c = L if y < 2 else M if y < 5 else D if y < 7 else DD
            elif kind == "edge_l":
                c = DD if x == 0 else D if x == 1 else M
            elif kind == "edge_r":
                c = DD if x == 7 else D if x == 6 else M
            elif kind == "under":                           # the shadow under a shelf
                c = DD if y == 0 else None
            if c:
                cv.set(x, y, c)
    return cv


def jar(kind):
    """A 16x24 jam jar (kind 0 strawberry, 1 apricot, 2 plum) with a gingham lid."""
    cv = Canvas(16, 24)
    jam, jaml = JAM[kind * 2], JAM[kind * 2 + 1]
    for y in range(6, 24):
        for x in range(2, 14):
            if (y == 6 or y == 23) and (x == 2 or x == 13):
                continue
            c = GLASS[1] if x in (2, 13) else jam if y > 9 else GLASS[0]
            if y > 9 and x == 4:
                c = jaml
            if y > 9 and x == 11:
                c = DARK if y > 20 else jam
            cv.set(x, y, c)
    for y in range(1, 6):                                     # the lid and its cloth
        for x in range(1, 15):
            if y == 1 and x in (1, 14):
                continue
            cv.set(x, y, LID[0] if (x // 2 + y // 2) % 2 else LID[1])
    cv.rect(4, 13, 11, 18, LID[1])                            # the label
    cv.rect(5, 15, 10, 15, jam)
    cv.outline(DARK)
    return cv


def kitchen_tiles():
    """name -> (canvas 8x8, palette name). Wall (2 metatiles of 4), frames (9), wood (7), jars (3 x 6)."""
    out = {}
    for v in (0, 1):
        m = wall_meta(v)
        for ty in (0, 1):
            for tx in (0, 1):
                t = Canvas(8, 8)
                for y in range(8):
                    for x in range(8):
                        t.set(x, y, m.get(tx * 8 + x, ty * 8 + y))
                out["wall%d_%d%d" % (v, ty, tx)] = (t, "wall")
    for n, cv in ring_tiles().items():
        out[n] = (cv, "field")
    for k in ("board", "board2", "lip", "edge_l", "edge_r", "under"):
        out["wood_" + k] = (wood_tile(k), "jars")
    for j in range(3):
        cv = jar(j)
        for ty in range(3):
            for tx in range(2):
                t = Canvas(8, 8)
                for y in range(8):
                    for x in range(8):
                        t.set(x, y, cv.get(tx * 8 + x, ty * 8 + y))
                out["jar%d_%d%d" % (j, ty, tx)] = (t, "jars")
    return out


KITCHEN_PALS = {"wall": WALL, "field": FIELD, "jars": JARS_PAL}


# ---- sheets ------------------------------------------------------------------------------------------------------------
def pack(sprites, width=256):
    """Shelf-pack named canvases into a sheet: {name: (x, y, w, h)}, height."""
    pos, x, y, row = {}, 0, 0, 0
    for name in sorted(sprites, key=lambda n: (-sprites[n].h, n)):
        cv = sprites[name]
        if x + cv.w > width:
            x, y, row = 0, y + row, 0
        pos[name] = (x, y, cv.w, cv.h)
        x += cv.w
        row = max(row, cv.h)
    return pos, y + row


def sheet(sprites, width=256):
    pos, h = pack(sprites, width)
    im = Image.new("RGB", (width, h), hs.MAGENTA)
    for name, (x, y, w, hh) in pos.items():
        im.paste(sprites[name].image(), (x, y))
    return im, pos


def jellies_image():
    bodies, faces = jelly_sheet()
    pos, W, H = jelly_layout()
    im = Image.new("RGB", (W, H), hs.MAGENTA)
    for (S, k, m), cv in bodies.items():
        im.paste(cv.image(), pos[("b", S, k, m)])
    for (S, k, d), cv in faces.items():
        im.paste(cv.image(), pos[("f", S, k, d)])
    return im


def chef_layout():
    return {(i, m): (j * 32, i * 32) for i in range(len(CHEFS)) for j, m in enumerate(("idle", "happy", "dizzy"))}


def chefs_image():
    im = Image.new("RGB", (96, 32 * len(CHEFS)), hs.MAGENTA)
    for key, cv in chef_sheet().items():
        im.paste(cv.image(), chef_layout()[key])
    return im


def kitchen_layout():
    names = sorted(kitchen_tiles())
    return {n: ((i % 16) * 8, (i // 16) * 8) for i, n in enumerate(names)}, 128, ((len(names) + 15) // 16) * 8


def kitchen_image():
    tiles = kitchen_tiles()
    pos, W, H = kitchen_layout()
    im = Image.new("RGB", (W, H), hs.MAGENTA)
    for n, (cv, _p) in tiles.items():
        im.paste(cv.image(), pos[n])
    return im


def sprite_sets():
    """the sprite groups: (group, canvases, palette)"""
    return [("hero", hero_sprites(), HERO_PAL), ("props", props(), JELLY_PAL)]


def write_all(out, preview=False):
    os.makedirs(os.path.join(out, "tiles"), exist_ok=True)
    spr = {}
    for _g, cvs, _pal in sprite_sets():
        spr.update(cvs)
    files = {"jellies.png": jellies_image(), "sprites.png": sheet(spr)[0], "chefs.png": chefs_image(),
             "tiles/kitchen.png": kitchen_image()}
    for f, im in files.items():
        im.save(os.path.join(out, f))
        if preview:
            os.makedirs(os.path.join(out, "preview"), exist_ok=True)
            im.resize((im.width * 4, im.height * 4), Image.NEAREST).save(os.path.join(out, "preview", os.path.basename(f)))
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

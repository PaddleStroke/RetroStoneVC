#!/usr/bin/env python3
"""Fishy Feast: the code-drawn art, in the 8BCraft house style (docs/art-direction.md).

Code-drawn art only, no image generation: every sprite and tile is drawn here with games/common/tools/house_style.py
(1-px outlines, 3-4-shade ramps lit from the top, big eyes). build_assets.py cuts the sheets with the same layout
(fish_parts(), PROPS) and turns them into tiles, palettes and the parts tables of draw.c.

The fish (DESIGN.md "Art"): round and cute, seen from the side (facing right; draw.c flips them), pale
fins, a big eye. The screen size of a fish follows the zoom, and sprites do not scale: each fish is drawn in size
TIERS (its radius on screen). The small tiers (3 to 24 px) are one sprite each; the two big tiers (30 and 40 px) are
built from parts: the body's four quarters (a top and a bottom one, each mirrored left-right: lit from above, the
small fish's bands level), the face (eye, cheek, mouth), the tail and a highlight (the glint top-left). Every fish exists in two body ramps (variants A and B, colour indices
6-9 and 10-13 of the same palette): the palettes then give 4 players and 8 bot colours. Players' fish have their
outline in index 1 (the player's colour ring), the B variant in index 15 (navy).

Writes, in the art folder (default games/fishyfeast/art):
  fish.png (the fish tiers, in player 1's palette), props.png (urchins, bubbles, the touch controls),
  tiles/floor.png (512x512, the pond floor), tiles/rays.png (256x256, the light rays, as added light)

    python3 games/fishyfeast/tools/make_art.py [--out DIR] [--preview]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/fishyfeast/LICENSE.
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


def ceil8(v):
    return (int(math.ceil(v)) + 7) // 8 * 8


# ---- the fish palette (OBJ 0, 1, 4, 5: the players, 6, 7: the bots) -------------------------------------------------
# index: 1 outline of variant A (players: the colour ring), 2 white, 3 ink, 4 fin light, 5 fin dark,
# 6-9 ramp A (highlight, light, mid, dark), 10-13 ramp B, 14 cheek, 15 outline of variant B (navy)
NAVY = (40, 24, 64)
WHITE = (250, 250, 250)
INK = (18, 8, 26)
FIN = [(236, 246, 250), (170, 204, 224)]
CHEEK = (255, 150, 170)
P1_RAMP = [(255, 228, 160), (252, 170, 70), (232, 112, 40), (164, 62, 28)]       # a goldfish
P1_RING = (255, 244, 140)
# the bots' eight colours (draw.c: colour c -> palette and variant)
BOT_RAMPS = [
    [(255, 200, 220), (246, 140, 178), (214, 86, 138), (150, 46, 98)],      # 0 pink
    [(206, 250, 214), (132, 226, 170), (64, 184, 130), (30, 120, 90)],      # 1 mint
    [(228, 212, 255), (176, 150, 246), (124, 96, 212), (78, 54, 150)],      # 2 lavender
    [(204, 238, 255), (120, 196, 246), (64, 146, 214), (36, 92, 160)],      # 3 sky
    [(255, 250, 190), (250, 226, 100), (222, 180, 40), (160, 118, 24)],     # 4 lemon
    [(255, 214, 190), (250, 156, 120), (222, 98, 82), (156, 52, 52)],       # 5 coral
    [(192, 246, 236), (96, 214, 200), (36, 160, 156), (20, 104, 110)],      # 6 teal
    [(250, 204, 250), (226, 132, 226), (176, 72, 186), (118, 40, 130)],     # 7 orchid
]
# the sheets are drawn in player 1's palette, its B ramp the lemon: all 15 colours differ (build_assets maps back)
SHEET_B = BOT_RAMPS[4]


def fish_palette(ring, ramp_a, ramp_b, outline_a=None):
    return [outline_a or ring, WHITE, INK, FIN[0], FIN[1]] + list(ramp_a) + list(ramp_b) + [CHEEK, NAVY]


SHEET_PAL = fish_palette(P1_RING, P1_RAMP, SHEET_B)


def col(i):
    return SHEET_PAL[i - 1]


def ramp_of(variant):
    return [col(6 + k) for k in range(4)] if variant == 0 else [col(10 + k) for k in range(4)]


def outline_of(variant):
    return col(1) if variant == 0 else col(15)


# the tiers: the fish's radius on screen (px); the last BIG_TIERS are built from parts
SMALL_TIERS = [3, 5, 7, 9, 12, 15, 19, 24]
BIG_TIERS = [30, 40]
WAG_TIERS = [5, 7]              # a second frame: the tail flicks
TIERS = SMALL_TIERS + BIG_TIERS


def tail_len(R):
    return 0 if R < 5 else max(2, int(round(R * 0.4)))


def shade_body(dx, dy, r4, top=False):
    """dx, dy: the point on the body, -1..1. The house light from the top-left (small fish), or from straight above
    (the big fish's halves, each mirrored left-right: the same bands as the small fish, level; the highlight part puts
    the glint top-left), with a darker rim."""
    d2 = dx * dx + dy * dy
    if top:
        nd = -dy - 0.25 * d2 + 0.1
    else:
        nd = dx * hs.LIGHT_DIR[0] + dy * hs.LIGHT_DIR[1] - 0.25 * d2 + 0.1
    return hs.shade_nd(nd, r4)


def draw_tail(cv, cx, cy, R, T, wag, ox=0.0):
    """A fan behind the body (pale fins with darker rays), from x = cx - 0.7R back to cx - R - T."""
    x_att = cx - R * 0.7
    for y in range(cv.h):
        for x in range(cv.w):
            u = x_att - (x + 0.5) + ox
            if u < 0 or u > R * 0.3 + T:
                continue
            k = u / (R * 0.3 + T)
            half = R * 0.22 + u * 0.62
            mid = cy + wag * k * k * max(1.0, R * 0.12)
            dy = (y + 0.5) - mid
            if abs(dy) > half:
                continue
            if k > 0.62 and abs(dy) < (k - 0.62) * (R * 0.3 + T) * 0.75:     # the notch
                continue
            ray = int((dy / max(1.0, half)) * 2.4 + 9) % 2 == 0 and R >= 9
            cv.set(x, y, col(5) if (ray or k > 0.9) else col(4))


def draw_eye(cv, ex, ey, R):
    """The eye at (ex, ey) (pixel corners): the house eye for small fish, a round one with a glint for big ones."""
    if R <= 3:
        cv.set(int(ex), int(ey), col(3))
        return
    if R <= 5:
        cv.rect(int(ex) - 1, int(ey) - 1, int(ex), int(ey), col(2))
        cv.set(int(ex), int(ey), col(3))
        return
    if R <= 9:
        hs.eye(cv, int(ex) - 1, int(ey) - 2, look=(1, 0), white=col(2), pupil=col(3), big=R >= 9)
        return
    er = max(2.2, R * 0.21)
    hs.ellipse(cv, ex, ey, er, er * 1.08, None, flat=col(2))
    hs.ellipse(cv, ex + er * 0.35, ey + er * 0.1, er * 0.62, er * 0.75, None, flat=col(3))
    cv.set(int(ex + er * 0.15), int(ey - er * 0.35), col(2))
    if R >= 15:
        cv.set(int(ex + er * 0.15) + 1, int(ey - er * 0.35), col(2))


def draw_face(cv, cx, cy, R):
    draw_eye(cv, cx + R * 0.38, cy - R * 0.22, R)
    if R >= 9:                                              # the cheek and the mouth
        chx, chy = cx + R * 0.30, cy + R * 0.18
        cv.set(int(chx), int(chy), col(14))
        if R >= 15:
            cv.set(int(chx) + 1, int(chy), col(14))
            cv.set(int(chx), int(chy) + 1, col(14))
            cv.set(int(chx) + 1, int(chy) + 1, col(14))
        mx, my = cx + R * 0.86, cy + R * 0.2
        cv.set(int(mx), int(my), col(3))
        if R >= 12:
            cv.set(int(mx) - 1, int(my) + 1, col(3))


def fish_small(R, variant, wag=0):
    T = tail_len(R)
    W, H = ceil8(2 * R + 2 + T), ceil8(2 * R + 2)
    cv = Canvas(W, H)
    cx, cy = W - 1 - R, H // 2
    if T:
        draw_tail(cv, cx, cy, R, T, wag)
    r4 = ramp_of(variant)
    for y in range(H):
        for x in range(W):
            dx, dy = (x + 0.5 - cx) / R, (y + 0.5 - cy) / R
            if dx * dx + dy * dy <= 1.0:
                cv.set(x, y, shade_body(dx, dy, r4))
    if R >= 9:                                              # the side fin
        hs.ellipse(cv, cx - R * 0.15, cy + R * 0.38, R * 0.2, R * 0.1, None, flat=col(4))
        cv.set(int(cx - R * 0.15), int(cy + R * 0.38 + R * 0.08), col(5))
    if R >= 7:                                              # the highlight
        cv.set(int(cx - R * 0.45), int(cy - R * 0.55), col(2))
    if R >= 12:
        cv.set(int(cx - R * 0.45) + 1, int(cy - R * 0.55), col(2))
        cv.set(int(cx - R * 0.45), int(cy - R * 0.55) + 1, col(6 if variant == 0 else 10))
    draw_face(cv, cx, cy, R)
    cv.outline(outline_of(variant))
    return cv, (-cx, -cy)


def quarter(R, variant, bottom=0):
    """The top-left (or bottom-left) quarter of a big body (draw.c mirrors each left-right), lit from above."""
    Q = ceil8(R + 1)
    cv = Canvas(Q, Q)
    cy = 0 if bottom else Q
    r4 = ramp_of(variant)
    for y in range(Q):
        for x in range(Q):
            dx, dy = (x + 0.5 - Q) / R, (y + 0.5 - cy) / R
            if dx * dx + dy * dy <= 1.0:
                cv.set(x, y, shade_body(dx, dy, r4, top=True))
    cv.outline(outline_of(variant))
    return cv, Q


def crop(cv, x0, y0, w, h):
    out = Canvas(w, h)
    for y in range(h):
        for x in range(w):
            out.p[y][x] = cv.get(x0 + x, y0 + y)
    return out


def bbox(cv):
    xs = [x for y in range(cv.h) for x in range(cv.w) if cv.p[y][x] is not None]
    ys = [y for y in range(cv.h) for x in range(cv.w) if cv.p[y][x] is not None]
    return min(xs), min(ys), max(xs), max(ys)


def feature(draw, R, pad=2):
    """Draw a feature on a big canvas around the body centre, crop it to multiples of 8: (canvas, (ox, oy))."""
    S = 4 * R
    cv = Canvas(S, S)
    c = S // 2
    draw(cv, c, c)
    x0, y0, x1, y1 = bbox(cv)
    x0, y0 = x0 - pad + 1, y0 - pad + 1
    w, h = ceil8(x1 - x0 + pad), ceil8(y1 - y0 + pad)
    return crop(cv, x0, y0, w, h), (x0 - c, y0 - c)


def big_parts(R, variant):
    """The parts of a big fish facing right, front to back: (name, canvas, ox, oy, hflip, vflip); ox, oy: the top-left
    corner from the body's centre (draw.c mirrors them for a fish facing left)."""
    T = tail_len(R)
    parts = []

    def hi(cv, cx, cy):
        hs.ellipse(cv, cx - R * 0.42, cy - R * 0.56, R * 0.2, R * 0.1, None, flat=col(4))
        hs.ellipse(cv, cx - R * 0.47, cy - R * 0.58, R * 0.1, R * 0.05, None, flat=col(2))
    cv, (ox, oy) = feature(hi, R, pad=1)
    parts.append(("big_hi_%d" % R, cv, ox, oy, 0, 0))

    def face(cv, cx, cy):
        # the side fin as on the small fish (the face part carries it: no extra sprite), a rounder cheek, a wider mouth
        hs.ellipse(cv, cx - R * 0.15, cy + R * 0.38, R * 0.2, R * 0.1, None, flat=col(4))
        hs.ellipse(cv, cx - R * 0.15, cy + R * 0.42, R * 0.14, R * 0.05, None, flat=col(5))
        draw_face(cv, cx, cy, R)
        hs.ellipse(cv, cx + R * 0.30, cy + R * 0.18, R * 0.08, R * 0.05, None, flat=col(14))
        mx, my = cx + R * 0.86, cy + R * 0.2
        cv.set(int(mx) - 2, int(my) + 1, col(3))
    cv, (ox, oy) = feature(face, R, pad=1)
    parts.append(("big_face_%d" % R, cv, ox, oy, 0, 0))
    q, Q = quarter(R, variant)
    name = "big_q_%s%d" % ("ab"[variant], R)
    parts.append((name, q, -Q, -Q, 0, 0))
    parts.append((name, q, 0, -Q, 1, 0))
    q, Q = quarter(R, variant, bottom=1)
    name = "big_qb_%s%d" % ("ab"[variant], R)
    parts.append((name, q, -Q, 0, 0, 0))
    parts.append((name, q, 0, 0, 1, 0))

    def tail(cv, cx, cy):
        draw_tail(cv, cx, cy, R, T, 0)
        cv.outline(NAVY)
    cv, (ox, oy) = feature(tail, R, pad=1)
    parts.append(("big_tail_%d" % R, cv, ox, oy, 0, 0))
    return parts


def fish_parts():
    """Every fish frame: {(variant, tier, wag): [(sprite name, canvas, ox, oy, hflip, vflip), ...]} and the sprite
    canvases by name (shared parts once)."""
    table, sprites = {}, {}
    for variant in (0, 1):
        v = "ab"[variant]
        for R in SMALL_TIERS:
            for wag in ([0, 1] if R in WAG_TIERS else [0]):
                cv, (ox, oy) = fish_small(R, variant, -1 if wag else 0)
                name = "fish_%s%d%s" % (v, R, "w" if wag else "")
                sprites[name] = cv
                table[(variant, R, wag)] = [(name, cv, ox, oy, 0, 0)]
        for R in BIG_TIERS:
            parts = big_parts(R, variant)
            for p in parts:
                sprites.setdefault(p[0], p[1])
            table[(variant, R, 0)] = parts
    for variant in (0, 1):                                  # pellets: ejected mass, a small round blob
        v = "ab"[variant]
        for r in (2, 3, 6):
            S = ceil8(2 * r + 2)
            cv = Canvas(S, S)
            c = S / 2.0
            r4 = ramp_of(variant)
            for y in range(S):
                for x in range(S):
                    dx, dy = (x + 0.5 - c) / r, (y + 0.5 - c) / r
                    if dx * dx + dy * dy <= 1.0:
                        cv.set(x, y, shade_body(dx, dy, r4))
            cv.outline(outline_of(variant))
            sprites["pellet_%s%d" % (v, r)] = cv
    cv = Canvas(8, 8)                                       # the player's marker: a chevron in its ring colour
    for i in range(4):
        for x in range(1 + i, 7 - i):
            cv.set(x, 2 + i, col(1))
    cv.outline(NAVY)
    sprites["marker"] = cv
    return table, sprites


# ---- props (OBJ 2): urchins, bubbles, the touch controls ----------------------------------------------------------------
PROPS_PAL = [NAVY, WHITE, (255, 246, 220), (96, 120, 184), (150, 172, 226),
             (222, 186, 244), (170, 118, 220), (116, 70, 176), (68, 38, 116), (255, 226, 250),
             (196, 238, 255), (112, 184, 226), INK, (232, 88, 120), (60, 80, 140)]


def pc(i):
    return PROPS_PAL[i - 1]


URCHIN_TIERS = [5, 9, 13, 18, 23]


def urchin(R):
    S = ceil8(2 * R + 2)
    cv = Canvas(S, S)
    c = S / 2.0
    n = 8 if R < 9 else 12 if R < 15 else 16
    body = R * 0.55
    for k in range(n):                                      # the spikes: thin, light tips
        a = 2 * math.pi * (k + 0.5) / n
        for s in range(int(R * 4)):
            t = body * 0.8 + (R - body * 0.8) * s / (R * 4.0)
            x, y = c + math.cos(a) * t, c + math.sin(a) * t
            cv.set(int(x), int(y), pc(10) if t > R * 0.82 else pc(8) if t > body * 1.15 else pc(9))
    r4 = [pc(6), pc(7), pc(8), pc(9)]
    for y in range(S):
        for x in range(S):
            dx, dy = (x + 0.5 - c) / body, (y + 0.5 - c) / body
            if dx * dx + dy * dy <= 1.0:
                cv.set(x, y, shade_body(dx, dy, r4))
    if R >= 13:                                             # a little cute face: two dot eyes
        cv.set(int(c - body * 0.35), int(c - body * 0.05), pc(13))
        cv.set(int(c + body * 0.25), int(c - body * 0.05), pc(13))
        cv.set(int(c - body * 0.05), int(c + body * 0.3), pc(14))
    cv.outline(NAVY)
    return cv


def bubble(r):
    cv = Canvas(8, 8)
    c = 4.0
    for y in range(8):
        for x in range(8):
            d = math.hypot(x + 0.5 - c, y + 0.5 - c)
            if r - 1.0 < d <= r:
                cv.set(x, y, pc(12))
    cv.set(int(c - r * 0.45), int(c - r * 0.45), pc(2))
    return cv


def disc_quarter(R, Q, pressed):
    """A quarter of a round touch button: a see-through mid-blue disc (the house checker; pressed: solid, lighter) with
    a 2-px cream ring, like the apps' drawn buttons."""
    cv = Canvas(Q, Q)
    for y in range(Q):
        for x in range(Q):
            d = math.hypot(x + 0.5 - Q, y + 0.5 - Q)
            if d <= R - 2:
                if pressed:
                    cv.set(x, y, pc(5))
                elif hs.checker(x, y):
                    cv.set(x, y, pc(4))
            elif d <= R:
                cv.set(x, y, pc(3))
    for y in range(Q):                                      # the navy outline round the ring
        for x in range(Q):
            d = math.hypot(x + 0.5 - Q, y + 0.5 - Q)
            if R < d <= R + 1:
                cv.set(x, y, pc(1))
    return cv


def icon_split():
    """Two little round fish pieces flying apart, an arrow between."""
    cv = Canvas(16, 16)
    hs.ellipse(cv, 4.5, 9.5, 3.4, 3.4, None, flat=pc(3))
    hs.ellipse(cv, 11.5, 5.5, 2.6, 2.6, None, flat=pc(3))
    for i in range(3):
        cv.set(8 + i, 11 - i, pc(3))
    cv.outline(pc(1))
    return cv


def icon_eject():
    """A round fish spitting a pellet: a big dot and a small one."""
    cv = Canvas(16, 16)
    hs.ellipse(cv, 5.5, 8.0, 4.4, 4.4, None, flat=pc(3))
    hs.ellipse(cv, 13.0, 8.0, 1.7, 1.7, None, flat=pc(3))
    cv.outline(pc(1))
    return cv


def stick_quarter(R=22):
    Q = ceil8(R + 1)
    cv = Canvas(Q, Q)
    for y in range(Q):
        for x in range(Q):
            d = math.hypot(x + 0.5 - Q, y + 0.5 - Q)
            if R - 2 < d <= R:
                cv.set(x, y, pc(3))
            elif d <= R - 2 and hs.checker(x, y) and (x + y) % 4 == 0:
                cv.set(x, y, pc(4))
    for y in range(Q):
        for x in range(Q):
            d = math.hypot(x + 0.5 - Q, y + 0.5 - Q)
            if R < d <= R + 1:
                cv.set(x, y, pc(1))
    return cv


def knob():
    cv = Canvas(16, 16)
    hs.ellipse(cv, 8.0, 8.0, 6.5, 6.5, [pc(2), pc(3), pc(5), pc(4)])
    cv.outline(pc(1))
    return cv


def props():
    """name -> canvas (OBJ 2)."""
    out = {}
    for R in URCHIN_TIERS:
        out["urchin_%d" % R] = urchin(R)
    out["bubble_s"] = bubble(1.6)
    out["bubble_l"] = bubble(3.2)
    for name, R in (("split", 20), ("eject", 16)):
        Q = ceil8(R + 1)
        out["btn_%s" % name] = disc_quarter(R, Q, False)
        out["btn_%s_on" % name] = disc_quarter(R, Q, True)
    out["icon_split"] = icon_split()
    out["icon_eject"] = icon_eject()
    out["stick"] = stick_quarter()
    out["knob"] = knob()
    return out


# ---- the pond floor (BG4, the affine layer: it scales with the zoom) ----------------------------------------------------
# seen from above through the water: muted teal sand, pebbles, weed tufts, shells; darker and greyer than the fish
FLOOR = dict(s0=(92, 128, 114), s1=(84, 118, 106), s2=(102, 140, 122), s3=(76, 108, 100),
             p0=(150, 162, 152), p1=(118, 130, 126), p2=(72, 86, 94),
             w0=(126, 178, 98), w1=(86, 142, 76), w2=(54, 102, 64),
             h0=(226, 206, 186), h1=(176, 146, 136), sh=(66, 96, 92))


def floor_tile(rng, v):
    cv = Canvas(8, 8)
    for y in range(8):
        for x in range(8):
            cv.set(x, y, FLOOR["s0"])
    spots = [(rng.randrange(8), rng.randrange(8)) for _ in range(3 + v)]
    for i, (x, y) in enumerate(spots):
        cv.set(x, y, FLOOR["s2"] if i % 2 else FLOOR["s1"])
        if v == 3 and i == 0:
            cv.set((x + 1) % 8, y, FLOOR["s3"])
    return cv


def pebbles(kind):
    cv = Canvas(16 if kind < 2 else 24, 16)
    rng = random.Random(40 + kind)
    for _ in range(3 + kind):
        cx, cy = rng.uniform(4, cv.w - 4), rng.uniform(4, 12)
        rx, ry = rng.uniform(2.0, 3.6), rng.uniform(1.6, 2.8)
        for y in range(16):                                 # a soft shadow below-right
            for x in range(cv.w):
                dx, dy = (x + 0.5 - cx - 1) / rx, (y + 0.5 - cy - 1) / ry
                if dx * dx + dy * dy <= 1.0 and cv.get(x, y) is None:
                    cv.set(x, y, FLOOR["sh"])
        hs.ellipse(cv, cx, cy, rx, ry, [FLOOR["p0"], FLOOR["p0"], FLOOR["p1"], FLOOR["p2"]])
    return cv


def weed(kind):
    S = 24 if kind == 0 else 16
    cv = Canvas(S, S)
    c = S / 2.0
    n = 7 if kind == 0 else 5
    rng = random.Random(70 + kind)
    for k in range(n):                                      # leaves from the centre, seen from above
        a = 2 * math.pi * k / n + rng.uniform(-0.2, 0.2)
        L = c - 1.5 - rng.uniform(0, 2)
        for s in range(int(L * 3)):
            t = s / 3.0
            w = 1.6 * math.sin(math.pi * min(1.0, t / L)) + 0.5
            for q in range(-2, 3):
                x = c + math.cos(a) * t - math.sin(a) * q * w / 2
                y = c + math.sin(a) * t + math.cos(a) * q * w / 2
                cv.set(int(x), int(y), FLOOR["w0"] if q < 0 and t < L * 0.7 else FLOOR["w1"] if q < 1 else FLOOR["w2"])
    hs.ellipse(cv, c, c, 1.6, 1.6, None, flat=FLOOR["w2"])
    return cv


def shell():
    cv = Canvas(8, 8)
    for y in range(8):
        for x in range(8):
            d = math.hypot(x + 0.5 - 4, y + 0.5 - 5)
            if d < 3.2 and y < 6:
                cv.set(x, y, FLOOR["h0"] if (x + y) % 3 else FLOOR["h1"])
    cv.set(4, 6, FLOOR["h1"])
    return cv


def floor():
    W = 512
    rng = random.Random(2026)
    base = [floor_tile(rng, v) for v in range(4)]
    cv = Canvas(W, W)
    grid = [[0] * 64 for _ in range(64)]
    decos = []
    kinds = [(pebbles(0), 2, 2), (pebbles(1), 2, 2), (pebbles(2), 3, 2), (weed(0), 3, 3), (weed(1), 2, 2),
             (shell(), 1, 1)]
    weights = [6, 5, 4, 5, 6, 5]
    taken = [[False] * 64 for _ in range(64)]
    for _ in range(140):
        k = rng.choices(range(len(kinds)), weights)[0]
        img, tw, th = kinds[k]
        tx, ty = rng.randrange(64 - tw), rng.randrange(64 - th)
        if any(taken[ty + j][tx + i] for j in range(-1, th + 1) for i in range(-1, tw + 1)
               if 0 <= tx + i < 64 and 0 <= ty + j < 64):
            continue
        for j in range(th):
            for i in range(tw):
                taken[ty + j][tx + i] = True
        decos.append((img, tx, ty))
    for ty in range(64):
        for tx in range(64):
            v = 0 if taken[ty][tx] else rng.choice([0, 0, 1, 1, 2, 3])
            grid[ty][tx] = v
            src = base[v]
            fl = rng.randrange(4) if not taken[ty][tx] else 0
            for y in range(8):
                for x in range(8):
                    sx, sy = (7 - x if fl & 1 else x), (7 - y if fl & 2 else y)
                    cv.set(tx * 8 + x, ty * 8 + y, src.get(sx, sy))
    for img, tx, ty in decos:
        cv.paste(img, tx * 8, ty * 8)
    return cv


def rays():
    """The light rays (BG2, added to what is below): soft bands at the house slope 1/2, three shades fading out,
    the house checker at their edges; 128 px apart along the slope, so the tiles repeat."""
    W = 256
    shades = [(12, 14, 10), (8, 10, 6), (4, 6, 4)]
    bands = [(10, 40), (84, 94)]                            # (start, end) along x + 2y, modulo 128
    cv = Canvas(W, W)
    for y in range(W):
        for x in range(W):
            s = (x + 2 * y) % 128
            for a, b in bands:
                if a <= s < b:
                    m = (s - a + 0.5) / (b - a)
                    e = min(m, 1 - m) * 2                   # 0 at the edges, 1 in the middle
                    k = 0 if e > 0.6 else 1 if e > 0.3 else 2
                    if k < 2 or hs.checker(x, y):
                        cv.set(x, y, shades[k] if b - a > 8 else shades[min(2, k + 1)])
    return cv


def specks_preview():
    """A preview of the plankton specks (the tiles are made in build_assets.py: dots on a 2-px grid)."""
    cv = Canvas(64, 16)
    cols = [(220, 255, 120), (255, 170, 210), (190, 250, 255)]
    for i, c in enumerate(cols):
        hs.ellipse(cv, 8 + i * 20, 8, 1.6, 1.6, None, flat=c)
    return cv


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


def write_all(out, preview=False):
    os.makedirs(os.path.join(out, "tiles"), exist_ok=True)
    _table, fs = fish_parts()
    files = {"fish.png": sheet(fs)[0], "props.png": sheet(props())[0], "tiles/floor.png": floor().image(),
             "tiles/rays.png": rays().image()}
    for f, im in files.items():
        im.save(os.path.join(out, f))
        if preview:
            os.makedirs(os.path.join(out, "preview"), exist_ok=True)
            k = 2 if im.width >= 512 else 4
            im.resize((im.width * k, im.height * k), Image.NEAREST).save(
                os.path.join(out, "preview", os.path.basename(f)))
    if preview:
        specks_preview().image().resize((256, 64), Image.NEAREST).save(os.path.join(out, "preview", "specks.png"))
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

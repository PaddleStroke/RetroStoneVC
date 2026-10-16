#!/usr/bin/env python3
"""Snowball Siege: the code-drawn art, in the 8BCraft house style (docs/art-direction.md).

Code-drawn art only, no image generation: every sprite and the far panorama are drawn here with
games/common/tools/house_style.py (1-px outlines, 3-4-shade ramps lit from the top-left, squash and stretch).
The snowy battlefield itself is drawn at run time from the terrain bitmap (src/draw.c), with the palette TERRAIN.

The four teams (one per player colour; species, not palette swaps): polar bear cubs (P1, a red scarf), penguins (P2, a
blue one), foxes (P3, green) and hares (P4, purple). Each is 16x16 in nine poses; each has its own sprite palette.

Writes, in the art folder (default games/snowballsiege/art):
  critters.png (the four teams, a row each), props.png (OBJ 2: throws, splats, hearts, HUD), scenery.png (OBJ 6:
  trees, the map's dots), tiles/far.png (256x128: the far mountains and pines, BG3)

    python3 games/snowballsiege/tools/make_art.py [--out DIR] [--preview]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/snowballsiege/LICENSE.
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
WHITE = (250, 250, 250)
INK = hs.HOUSE["ink"][0]

# ---- the teams: (outline, fur ramp 4, second ramp, scarf ramp 3, extra colours) ---------------------------------
SPECIES = ["bear", "peng", "fox", "hare"]
POSES = ["idle0", "idle1", "walk0", "walk1", "throw", "hurt", "air", "mound", "icon"]
TEAMS = {
    "bear": dict(out=(44, 30, 62), fur=[(255, 255, 252), (236, 238, 236), (206, 210, 220), (160, 166, 188)],
                 sec=[(252, 236, 204), (222, 196, 160)], scarf=[(255, 128, 116), (230, 62, 66), (160, 34, 58)],
                 pink=(255, 168, 180)),
    "peng": dict(out=(22, 22, 52), fur=[(98, 110, 156), (64, 74, 120), (42, 48, 88), (28, 32, 62)],
                 sec=[(255, 255, 255), (214, 224, 242)], scarf=[(130, 214, 255), (52, 148, 234), (30, 88, 170)],
                 pink=(255, 168, 180), beak=[(255, 200, 84), (232, 128, 40)]),
    "fox": dict(out=(56, 26, 46), fur=[(255, 186, 112), (244, 134, 58), (204, 92, 38), (146, 58, 30)],
                sec=[(255, 255, 255), (226, 224, 236), (188, 184, 204)], scarf=[(168, 236, 120), (92, 186, 72),
                                                                                 (44, 124, 52)],
                pink=(255, 168, 180)),
    "hare": dict(out=(52, 36, 74), fur=[(255, 255, 255), (232, 232, 242), (198, 198, 216), (152, 150, 178)],
                 sec=[(255, 180, 198), (226, 128, 160)], scarf=[(216, 164, 252), (164, 98, 224), (106, 56, 162)],
                 pink=(255, 168, 180)),
}


def team_palette(sp):
    """The 15 colours of a team's sprite palette (index 1.. in this order)."""
    T = TEAMS[sp]
    cols = [T["out"]] + T["fur"] + T["sec"] + T["scarf"] + [WHITE, INK, T["pink"]] + T.get("beak", [])
    seen = []
    for c in cols:
        if c not in seen:
            seen.append(c)
    assert len(seen) <= 15, (sp, len(seen))
    return seen


# ---- a critter, facing right, 16x16 (feet on the bottom row) ------------------------------------------------------
def ellipse_flat(cv, cx, cy, rx, ry, col):
    hs.ellipse(cv, cx, cy, rx, ry, None, flat=col)


def head_and_ears(cv, sp, T, hx, hy, pose):
    fur, sec = T["fur"], T["sec"]
    hr = 4.1
    if sp == "bear":
        for ex, ey in ((hx - 2.6, hy - 3.2), (hx + 1.6, hy - 3.6)):
            hs.ellipse(cv, ex, ey, 1.7, 1.7, fur)
            cv.set(int(ex), int(ey), T["pink"])
    elif sp == "fox":
        for ex in (hx - 2.8, hx + 0.8):
            for k in range(4):
                for x in range(int(ex) - (3 - k) // 2, int(ex) + 1 + (3 - k) // 2 + 1):
                    cv.set(x, int(hy - 2 - k), fur[1] if k < 3 else fur[0])
            cv.set(int(ex), int(hy - 3), fur[3])
    elif sp == "hare":
        lean = 1 if pose == "air" else 0
        for ex, tall in ((hx - 2.2, 5.0), (hx + 0.6, 4.6)):
            hs.ellipse(cv, ex - lean, hy - 5.0, 1.3, tall / 1.6, fur)
            for y in range(int(hy - 7), int(hy - 3)):
                cv.set(int(ex - lean), y, sec[0])
    hs.ellipse(cv, hx, hy, hr, hr * 0.92, fur)
    if sp == "peng":
        ellipse_flat(cv, hx + 1.4, hy + 0.7, 2.4, 2.4, sec[0])              # the white face
    if sp in ("fox", "hare"):
        ellipse_flat(cv, hx + 1.6, hy + 1.8, 2.2, 1.5, sec[0] if sp == "fox" else fur[0])
    if sp == "bear":
        ellipse_flat(cv, hx + 2.4, hy + 1.3, 1.9, 1.4, sec[0])              # the muzzle


def face(cv, sp, T, hx, hy, pose):
    ix, iy = int(hx), int(hy)
    if pose == "hurt":
        for ex in (ix, ix + 3):
            cv.set(ex - 1, iy - 1, INK), cv.set(ex, iy, INK), cv.set(ex + 1, iy - 1, INK)    # squeezed eyes ^^
    elif pose == "air":
        for ex in (ix, ix + 3):
            cv.rect(ex - 1, iy - 2, ex, iy, WHITE)
            cv.set(ex, iy - 1, INK)
    else:
        # bead eyes (white fur would swallow the house eye's white): 2x2 ink with a glint, looking ahead
        for ex in (ix - 1, ix + 2):
            cv.rect(ex, iy - 2, ex + 1, iy - 1, INK)
            cv.set(ex, iy - 2, WHITE)
    # nose, beak, cheeks
    if sp == "bear":
        cv.set(ix + 4, iy + 1, INK), cv.set(ix + 3, iy + 1, INK)
    elif sp == "peng":
        B = T["beak"]
        for y, xs in ((iy + 1, range(ix + 3, ix + 7)), (iy + 2, range(ix + 3, ix + 6))):
            for x in xs:
                cv.set(x, y, B[0] if y == iy + 1 else B[1])
    elif sp == "fox":
        cv.set(ix + 5, iy + 1, INK)
    elif sp == "hare":
        cv.set(ix + 4, iy + 1, T["pink"])
    if pose not in ("hurt",):
        cv.set(ix - 1, iy + 1, T["pink"])                                     # a pink cheek


def scarf(cv, T, bx, by, pose, flutter=0):
    S = T["scarf"]
    for x in range(int(bx) - 3, int(bx) + 5):
        cv.set(x, int(by), S[0])
        cv.set(x, int(by) + 1, S[1])
    cv.set(int(bx) + 4, int(by) + 1, S[2])
    # the scarf's end hangs at the back (flies back in the air and the throw)
    tx = int(bx) - 4
    if pose in ("air", "throw") or flutter:
        for i in range(3):
            cv.set(tx - i, int(by) + i // 2, S[1])
            cv.set(tx - i, int(by) + 1 + i // 2, S[2])
    else:
        for i in range(3):
            cv.set(tx, int(by) + 1 + i, S[1] if i < 2 else S[2])
            cv.set(tx + 1, int(by) + 1 + i, S[2])


def tail(cv, sp, T, bx, by):
    fur, sec = T["fur"], T["sec"]
    if sp == "fox":
        hs.capsule(cv, bx - 6.0, by + 1.0, bx - 3.5, by - 1.5, 1.8, fur)
        ellipse_flat(cv, bx - 6.6, by + 1.6, 1.2, 1.2, sec[0])
    elif sp == "hare":
        hs.ellipse(cv, bx - 4.8, by, 1.6, 1.6, fur)
    elif sp == "bear":
        hs.ellipse(cv, bx - 4.6, by, 1.2, 1.2, fur)


def critter(sp, pose):
    T = TEAMS[sp]
    fur, sec = T["fur"], T["sec"]
    cv = Canvas(16, 16)
    if pose == "mound":
        snow = fur if sp in ("bear", "hare") else [sec[0], sec[0], sec[-1], sec[-1]]
        head_and_ears(cv, sp, T, 8.0, 10.0, pose)           # the ears and the top of the head poke out
        for y in range(16):
            for x in range(16):
                cv.p[y][x] = cv.p[y][x] if y < 9 else None
        hs.ellipse(cv, 8.0, 13.2, 7.0, 4.0, snow)
        for x in (5, 9, 12):
            cv.set(x, 10, WHITE)
        S = T["scarf"]
        cv.set(3, 14, S[1]), cv.set(2, 15, S[1]), cv.set(3, 15, S[2])
        cv.outline(T["out"])
        return cv
    if pose == "icon":
        head_and_ears(cv, sp, T, 7.6, 9.0, "idle")
        face(cv, sp, T, 7.6, 9.0, "idle")
        scarf(cv, T, 6.0, 13.2, "idle")
        cv.outline(T["out"])
        return cv
    bob = {"idle1": 1, "walk1": 1, "hurt": 1}.get(pose, 0)
    bx, by = 7.0, 11.0 + bob * 0.5
    hx, hy = 9.0, 6.6 + bob
    if sp == "hare":
        hy += 0.6
    if pose == "throw":
        hx += 0.6
    tail(cv, sp, T, bx, by)
    # feet
    fl = {"walk0": (-1, 1), "walk1": (1, -1)}.get(pose, (0, 0))
    for fx, d in ((bx - 2.0, fl[0]), (bx + 2.4, fl[1])):
        foot = T["beak"] if sp == "peng" else [fur[2], fur[3]]
        ellipse_flat(cv, fx + d * 0.6, 14.6, 1.7, 1.1, foot[1])
        cv.set(int(fx + d * 0.6), 14, foot[0])
    # the body
    if sp == "peng":
        hs.ellipse(cv, bx + 0.5, by - 0.5, 4.4, 4.6, fur)
        ellipse_flat(cv, bx + 2.0, by, 2.6, 3.4, sec[0])
        cv.set(int(bx + 3), int(by + 2), sec[1])
    else:
        hs.ellipse(cv, bx, by, 4.8, 3.9, fur)
        if sp == "fox":
            ellipse_flat(cv, bx + 2.6, by - 0.4, 1.6, 2.0, sec[0])
    # the arm: a paw (or a flipper) forward; raised with a snowball for the throw, up in the air
    arm = fur[2] if sp != "hare" else fur[1]
    if pose == "throw":
        hs.capsule(cv, bx + 2.0, by - 1.5, bx + 5.5, by - 6.5, 1.1, None, flat=arm)
        ellipse_flat(cv, bx + 6.0, by - 7.6, 1.6, 1.6, WHITE)
    elif pose == "air":
        hs.capsule(cv, bx + 1.0, by - 1.0, bx + 4.6, by - 4.8, 1.0, None, flat=arm)
    else:
        hs.capsule(cv, bx + 1.6, by + 0.2, bx + 3.4, by + 1.8, 1.0, None, flat=arm)
    head_and_ears(cv, sp, T, hx, hy, pose)
    face(cv, sp, T, hx, hy, pose)
    scarf(cv, T, hx - 1.0, hy + 3.3, pose)
    if pose == "hurt":                                       # a clump of snow on the head
        ellipse_flat(cv, hx - 1.0, hy - 3.6, 2.4, 1.4, WHITE)
    cv.outline(T["out"])
    if pose == "air":
        cv = hs.squash(cv, 0.86)
        cv.outline(T["out"])
    return cv


# ---- props (OBJ 2) ------------------------------------------------------------------------------------------------
PROPS_PAL = [(28, 30, 66), (255, 255, 255), (222, 236, 250), (178, 202, 236), (124, 152, 204), (196, 240, 255),
             (110, 196, 236), (236, 70, 84), (255, 168, 176), (128, 36, 64), (255, 222, 92), (248, 140, 48),
             (255, 246, 220), (96, 120, 184), (150, 172, 226)]
NAVY, SW, SL, SM, SS, IL, IM, RED, PINK, DRED, YEL, ORA, CREAM, BB, BL = PROPS_PAL
SNOW4 = [SW, SL, SM, SS]


def snowball(r, size):
    cv = Canvas(size, size)
    c = size / 2.0
    hs.ellipse(cv, c, c, r, r, SNOW4)
    cv.outline(NAVY)
    return cv


def ice_cube():
    cv = Canvas(8, 8)
    cv.rect(1, 1, 6, 6, IM)
    cv.rect(1, 1, 5, 2, IL)
    cv.rect(1, 1, 2, 5, IL)
    cv.set(2, 2, SW)
    cv.rect(5, 5, 6, 6, SS)
    cv.outline(NAVY)
    return cv


def chunk(frame):
    cv = Canvas(8, 8)
    if frame == 0:
        cv.rect(3, 3, 5, 5, SL), cv.set(3, 3, SW), cv.set(5, 5, SM)
    elif frame == 1:
        cv.rect(3, 3, 4, 4, SL), cv.set(3, 3, SW)
    else:
        cv.set(4, 4, SL)
    cv.outline(SS)
    return cv


def puff(frame):
    """A splat's ring of powder: grows and fades with the house checker (never inside a body)."""
    cv = Canvas(16, 16)
    r = [3.5, 5.5, 7.2][frame]
    for y in range(16):
        for x in range(16):
            d = math.hypot(x + 0.5 - 8, y + 0.5 - 8)
            if r - 2.2 < d < r and (frame == 0 or hs.checker(x, y, frame)):
                cv.set(x, y, SW if d < r - 1 else SM)
    return cv


def splash(frame):
    """Water thrown up by the pond (a plop or a critter falling in)."""
    cv = Canvas(16, 16)
    rng = random.Random(7 + frame)
    h = [6, 11, 7][frame]
    for i in range(7):
        x = 2 + i * 2
        top = 15 - int(h * (1 - abs(i - 3) / 4.5))
        for y in range(top, 16):
            cv.set(x, y, IL if y < top + 2 else IM)
    for _ in range(5 + frame * 2):
        cv.set(rng.randint(1, 14), rng.randint(15 - h - 3, 14 - h // 2), SW)
    if frame == 2:
        for y in range(16):
            for x in range(16):
                if cv.get(x, y) and not hs.checker(x, y):
                    cv.set(x, y, None)
    cv.outline(NAVY) if frame < 2 else None
    return cv


def dot():
    cv = Canvas(8, 8)
    cv.rect(3, 3, 4, 4, CREAM)
    cv.outline(NAVY)
    return cv


def gauge(k):
    """One 8-px piece of the power gauge, k of its 8 columns full (yellow, orange underneath)."""
    cv = Canvas(8, 8)
    for x in range(8):
        cv.set(x, 2, NAVY), cv.set(x, 6, NAVY)
        for y in (3, 4, 5):
            cv.set(x, y, (YEL if y < 5 else ORA) if x < k else BB)
    return cv


def marker():
    cv = Canvas(8, 8)
    for i in range(4):
        for x in range(1 + i, 7 - i):
            cv.set(x, 2 + i, YEL if i < 2 else ORA)
    cv.outline(NAVY)
    return cv


def sock(calm):
    """The windsock: a pole and a red and white cone, blowing right (or drooping when calm)."""
    cv = Canvas(16, 16)
    for y in range(2, 15):
        cv.set(2, y, SS)
    cv.set(2, 1, YEL)
    if calm:
        for i in range(6):
            for x in range(3, 6 - i // 3):
                cv.set(x, 3 + i, RED if (i // 2) % 2 == 0 else SW)
    else:
        for x in range(3, 14):
            half = 3 - (x - 3) * 2 // 11
            for y in range(6 - half, 6 + half + 1):
                cv.set(x, y, RED if ((x - 3) // 3) % 2 == 0 else SW)
    cv.outline(NAVY)
    return cv


def chevron():
    cv = Canvas(8, 8)
    for i in range(3):
        cv.set(2 + i, 1 + i, CREAM), cv.set(2 + i, 6 - i, CREAM)
        cv.set(3 + i, 1 + i, CREAM), cv.set(3 + i, 6 - i, CREAM)
    cv.outline(NAVY)
    return cv


def weapon_icon(k):
    cv = Canvas(16, 16)
    if k == 0:
        hs.ellipse(cv, 8, 8.5, 3.6, 3.6, SNOW4)
    elif k == 1:
        hs.ellipse(cv, 8, 8.5, 6.0, 6.0, SNOW4)
    else:
        cv.rect(4, 4, 11, 11, IM)
        cv.rect(4, 4, 10, 6, IL)
        cv.rect(4, 4, 6, 10, IL)
        cv.set(5, 5, SW), cv.set(6, 5, SW), cv.set(5, 6, SW)
        cv.rect(9, 9, 11, 11, SS)
    cv.outline(NAVY)
    return cv


def heart(cv, x0, state):
    """A 5x5 heart at x0 (state 2 full, 1 half, 0 empty: a frosty one)."""
    shape = ["01010", "11111", "11111", "01110", "00100"]
    for y, row in enumerate(shape):
        for x, ch in enumerate(row):
            if ch == "1":
                full = state == 2 or (state == 1 and x < 3)
                col = (PINK if (x, y) in ((1, 1), (0, 1)) else RED) if full else (SM if (x, y) == (1, 1) else SS)
                cv.set(x0 + x, 1 + y, col)


def hearts(n):
    cv = Canvas(24, 8)
    for i in range(3):
        heart(cv, 1 + i * 7, 2 if n >= 2 * (i + 1) else 1 if n == 2 * i + 1 else 0)
    cv.outline(NAVY)
    return cv


def flake(k):
    cv = Canvas(8, 8)
    if k == 0:
        cv.set(4, 4, SW), cv.set(3, 4, SL), cv.set(5, 4, SL), cv.set(4, 3, SL), cv.set(4, 5, SL)
    else:
        cv.rect(3, 3, 4, 4, SW)
    return cv


def up_arrow():
    cv = Canvas(8, 8)
    for i in range(3):
        for x in range(3 - i, 5 + i):
            cv.set(x, 1 + i, CREAM)
    cv.rect(3, 4, 4, 6, CREAM)
    cv.outline(NAVY)
    return cv


def disc_quarter(R, pressed):
    """The top-left quarter of the round touch button (mirrored 4 ways): a see-through mid-blue disc (the house checker;
    pressed: solid, lighter) with a 2-px cream ring and a navy outline, like the apps' drawn buttons."""
    Q = 16
    cv = Canvas(Q, Q)
    for y in range(Q):
        for x in range(Q):
            d = math.hypot(x + 0.5 - Q, y + 0.5 - Q)
            if d <= R - 2:
                if pressed:
                    cv.set(x, y, BL)
                elif hs.checker(x, y):
                    cv.set(x, y, BB)
            elif d <= R:
                cv.set(x, y, CREAM)
            elif d <= R + 1:
                cv.set(x, y, NAVY)
    return cv


def props():
    out = {"ball": snowball(2.6, 8), "bigball": snowball(4.2, 16), "ice": ice_cube(), "dot": dot(),
           "marker": marker(), "sock": sock(False), "sock_calm": sock(True), "chevron": chevron(),
           "uparrow": up_arrow(), "btn_q": disc_quarter(15, False), "btn_qp": disc_quarter(15, True)}
    for i in range(3):
        out["chunk%d" % i] = chunk(i)
        out["puff%d" % i] = puff(i)
        out["splash%d" % i] = splash(i)
        out["wpn%d" % i] = weapon_icon(i)
    for k in range(9):
        out["gauge%d" % k] = gauge(k)
    for n in range(7):
        out["hearts%d" % n] = hearts(n)
    for k in range(2):
        out["flake%d" % k] = flake(k)
    return out


# ---- scenery (OBJ 6): pine trees, the map's dots -----------------------------------------------------------------
SCENE_PAL = [(22, 52, 58), (36, 92, 82), (60, 130, 104), (110, 172, 130), (122, 84, 58), (78, 50, 38),
             (255, 255, 255), (218, 232, 250), (160, 188, 228), (232, 72, 72), (52, 148, 234), (96, 200, 80),
             (176, 104, 232), (255, 246, 220), (16, 24, 56)]
TEAM_DOT = [SCENE_PAL[9], SCENE_PAL[10], SCENE_PAL[11], SCENE_PAL[12]]


def pine(kind):
    cv = Canvas(16, 32)
    PO, PD, PM, PL, TR, TD, WH, WL, WS = SCENE_PAL[:9]
    cv.rect(7, 26, 8, 31, TR)
    cv.set(8, 27, TD), cv.set(8, 29, TD)
    tiers = [(6, 4.5), (13, 6.0), (20, 7.3)] if kind == 0 else [(5, 4.0), (11, 5.2), (17, 6.4), (22, 7.0)]
    for top, half in tiers:
        for y in range(top, top + 8):
            w = (y - top + 1) / 8.0 * half
            for x in range(int(7.5 - w), int(8.5 + w)):
                f = (x - (7.5 - w)) / max(1.0, 2 * w)
                cv.set(x, y, PL if f < 0.25 else PM if f < 0.7 else PD)
        for x in range(int(7.5 - half * 0.8), int(8.5 + half * 0.5)):         # snow on each tier
            cv.set(x, top + 6, WH if x < 8 else WL)
        cv.set(int(7.5 - half * 0.8), top + 7, WL)
    cv.rect(7, 3, 8, 5, WH)
    cv.set(8, 2, WL)
    cv.outline(PO)
    return cv


def map_dot(col):
    cv = Canvas(8, 8)
    cv.rect(3, 3, 5, 5, col)
    cv.set(3, 3, SCENE_PAL[13])
    cv.outline(SCENE_PAL[14])
    return cv


def map_view():
    """A side of the map's view frame: a cream bracket 8x24 (its left side; flipped for the right)."""
    cv = Canvas(8, 24)
    for y in range(24):
        cv.set(3, y, SCENE_PAL[13])
    for x in range(3, 6):
        cv.set(x, 0, SCENE_PAL[13]), cv.set(x, 23, SCENE_PAL[13])
    return cv


def scenery():
    out = {"tree0": pine(0), "tree1": pine(1), "mapview": map_view(), "mapball": map_dot(SCENE_PAL[6])}
    for i, c in enumerate(TEAM_DOT):
        out["mapdot%d" % i] = map_dot(c)
    return out


# ---- the far panorama (BG3): pale mountains and far pines, no outline, low contrast -------------------------------
FAR = [(176, 192, 226), (150, 168, 208), (226, 234, 248), (126, 150, 186), (104, 130, 170)]


def far():
    """256 px that repeat: a range of pale mountains (the slopes facing right in shade, snow on the peaks) and a row of
    far pines in front, 2-3 tones each, no outline."""
    W, H = 256, 128

    def mount(x):
        x %= W
        return 44 + 18 * math.sin(x * 2 * math.pi / 256 + 0.6) + 9 * math.sin(x * 2 * math.pi / 64 + 2.0) \
            + 4 * math.sin(x * 2 * math.pi / 32)
    cv = Canvas(W, H)
    def right_face(x):
        return mount(x + 1.5) > mount(x - 1.5)
    for x in range(W):
        m = int(mount(x))
        for y in range(m, H):
            shade = right_face(x - (y - m) // 2)              # the shade's edge runs down-left from each ridge
            snow = y < m + 3 + (2 if not shade else 0) and m < 46
            cv.set(x, y, FAR[2] if snow else FAR[1] if shade else FAR[0])
    rng = random.Random(5)
    x = 0
    while x < W:                                              # far pines: little triangles of 9-16 px
        h = rng.randint(9, 16)
        base = 96 + int(4 * math.sin(x * 2 * math.pi / 128 + 1.0))
        for y in range(base - h, H):
            half = (y - (base - h)) / 2.4 if y < base else 99
            for dx in range(-int(half), int(half) + 1):
                xx = (x + dx) % W
                if y >= base or abs(dx) <= half:
                    cv.set(xx, y, FAR[4] if (y >= base + 8 or dx > 0) else FAR[3])
        x += rng.randint(5, 9)
    return cv


# ---- the terrain's palette (BG2: the snowy field, drawn at run time by draw.c) -------------------------------------
TERRAIN = [(250, 252, 255), (226, 238, 250), (196, 216, 240), (160, 186, 224), (124, 148, 196), (252, 254, 255),
           (220, 234, 252), (112, 140, 200), (128, 116, 136), (88, 76, 102), (214, 240, 252), (150, 206, 236),
           (88, 150, 206), (60, 108, 170), (120, 170, 214)]


def pack(sprites, width=256):
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
    im = Image.new("RGB", (width, max(8, h)), hs.MAGENTA)
    for name, (x, y, w, hh) in pos.items():
        im.paste(sprites[name].image(), (x, y))
    return im, pos


def critter_sheet():
    im = Image.new("RGB", (16 * len(POSES), 16 * len(SPECIES)), hs.MAGENTA)
    for r, sp in enumerate(SPECIES):
        for c, pose in enumerate(POSES):
            im.paste(critter(sp, pose).image(), (c * 16, r * 16))
    return im


def write_all(out, preview=False):
    os.makedirs(os.path.join(out, "tiles"), exist_ok=True)
    files = {"critters.png": critter_sheet(), "props.png": sheet(props())[0], "scenery.png": sheet(scenery())[0],
             "tiles/far.png": far().image()}
    for f, im in files.items():
        im.save(os.path.join(out, f))
        if preview:
            os.makedirs(os.path.join(out, "preview"), exist_ok=True)
            k = 2 if im.width >= 256 else 4
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

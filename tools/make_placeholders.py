#!/usr/bin/env python3
"""Generate the PLACEHOLDER art of Bomber Mole in the final sheet format.

Writes characters.png, tiles.png and items_fx.png, laid out exactly as in
tools/sheets.py (docs/art-sheets.md), on a magenta background. Simple but
readable shapes, one distinct colour scheme per role, one terrain palette per
season. The real art replaces these files one for one.

    python3 tools/make_placeholders.py --out games/bombermole/art [--preview]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import argparse
import os
import random
import sys

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sheets  # noqa: E402
import placeholder_props  # noqa: E402
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "sdk", "tools"))
from gen_font import G as FONT  # noqa: E402

MAG = sheets.MAGENTA + (255,)


def canvas(w=16, h=16):
    im = Image.new("RGBA", (w, h), MAG)
    return im, ImageDraw.Draw(im)


def px(d, x, y, c):
    d.point((x, y), fill=c)


def rect(d, x0, y0, x1, y1, c):
    d.rectangle((x0, y0, x1, y1), fill=c)


def ell(d, x0, y0, x1, y1, c, o=None):
    d.ellipse((x0, y0, x1, y1), fill=c, outline=o)


# ---------------------------------------------------------------------------
# characters
MOLE = dict(out=(42, 26, 16), body=(107, 74, 47), light=(156, 115, 80), pink=(232, 160, 160),
            nose=(255, 111, 143), white=(255, 255, 255), eye=(10, 10, 10), yellow=(255, 210, 63),
            claw=(240, 230, 210), dirt=(176, 124, 72))


def mole(view, frame=0, pose="walk"):
    P = MOLE
    im, d = canvas()
    bob = 0
    if pose == "victory":
        bob = -1 if frame else 0
    if pose == "death":
        return mole_death(frame)
    y0 = 3 + bob
    # feet (walk cycle)
    feet = {0: (0, 0), 1: (-1, 1), 2: (1, -1)}[frame % 3] if pose == "walk" else (0, 0)
    if view in ("down", "up"):
        rect(d, 4, 13 + min(0, feet[0]) + bob, 5, 15 + bob, P["pink"])
        rect(d, 10, 13 + min(0, feet[1]) + bob, 11, 15 + bob, P["pink"])
        ell(d, 3, y0, 12, 14 + bob, P["body"], P["out"])
        if view == "down":
            ell(d, 5, y0 + 6, 10, 13 + bob, P["light"])
            face = pose != "hurt"
            if face:
                px(d, 6, y0 + 4, P["eye"]); px(d, 9, y0 + 4, P["eye"])
            else:
                for (x, y) in ((5, y0 + 3), (7, y0 + 5), (5, y0 + 5), (7, y0 + 3), (6, y0 + 4)):
                    px(d, x, y, P["eye"])
                for (x, y) in ((8, y0 + 3), (10, y0 + 5), (8, y0 + 5), (10, y0 + 3), (9, y0 + 4)):
                    px(d, x, y, P["eye"])
            rect(d, 7, y0 + 6, 8, y0 + 7, P["nose"])
        else:
            rect(d, 7, y0 + 2, 8, 12 + bob, P["light"])
        # paws
        if pose == "victory":
            rect(d, 1, y0 - 2, 2, y0 + 2, P["pink"]); rect(d, 13, y0 - 2, 14, y0 + 2, P["pink"])
            if frame:
                px(d, 2, 1, P["yellow"]); px(d, 13, 0, P["yellow"]); px(d, 7, 0, P["yellow"])
        elif pose == "bomb":
            rect(d, 5, 12, 6, 13, P["pink"]); rect(d, 9, 12, 10, 13, P["pink"])
            px(d, 7, 1, P["yellow"]); px(d, 8, 0, P["yellow"])
        elif pose == "dig":
            cy = 15 if view == "down" else 1
            off = frame
            for x in (4 + off, 7 + off, 10 - off):
                rect(d, x, cy - 1 if view == "down" else cy, x, cy if view == "down" else cy + 1, P["claw"])
            px(d, 2 + off * 2, cy, P["dirt"]); px(d, 13 - off * 2, cy, P["dirt"])
        else:
            rect(d, 2, 9 + bob, 2, 11 + bob, P["pink"]); rect(d, 13, 9 + bob, 13, 11 + bob, P["pink"])
        if pose == "hurt":
            d.ellipse((3, y0, 12, 14 + bob), outline=P["white"])
    else:
        right = view == "right"
        im2, d2 = canvas()
        # draw facing left, mirror for right
        rect(d2, 5 + feet[0], 14, 6 + feet[0], 15, P["pink"])
        rect(d2, 10 + feet[1], 14, 11 + feet[1], 15, P["pink"])
        ell(d2, 3, 5, 13, 14, P["body"], P["out"])
        ell(d2, 5, 9, 11, 13, P["light"])
        rect(d2, 1, 9, 3, 10, P["nose"])
        px(d2, 5, 8, P["eye"])
        if pose == "dig":
            for i, y in enumerate((7, 10, 13)):
                rect(d2, 0 + (frame if i != 1 else 1 - frame), y, 1 + (frame if i != 1 else 1 - frame), y, P["claw"])
            px(d2, 0, 4 + frame * 2, P["dirt"]); px(d2, 2, 3 + frame, P["dirt"])
        else:
            rect(d2, 4, 12, 5, 13, P["pink"])
        if right:
            im2 = im2.transpose(Image.FLIP_LEFT_RIGHT)
        im = im2
    return im


def mole_death(frame):
    P = MOLE
    im, d = canvas()
    if frame == 0:
        ell(d, 3, 3, 12, 14, P["body"], P["out"])
        for (x, y) in ((5, 6), (7, 8), (5, 8), (7, 6), (6, 7), (8, 6), (10, 8), (8, 8), (10, 6), (9, 7)):
            px(d, x, y, P["eye"])
        rect(d, 7, 10, 8, 11, P["nose"])
        px(d, 2, 1, P["yellow"]); px(d, 13, 2, P["yellow"])
    elif frame == 1:
        ell(d, 2, 6, 13, 14, P["body"], P["out"])
        rect(d, 5, 9, 6, 9, P["eye"]); rect(d, 9, 9, 10, 9, P["eye"])
        rect(d, 7, 11, 8, 11, P["nose"])
        px(d, 4, 3, P["yellow"]); px(d, 7, 2, P["yellow"]); px(d, 10, 3, P["yellow"])
    elif frame == 2:
        ell(d, 1, 10, 14, 15, P["body"], P["out"])
        rect(d, 4, 12, 5, 12, P["eye"]); rect(d, 10, 12, 11, 12, P["eye"])
        rect(d, 13, 12, 14, 13, P["pink"]); rect(d, 1, 12, 2, 13, P["pink"])
    else:
        ell(d, 4, 4, 11, 12, P["claw"], P["white"])
        px(d, 6, 7, P["eye"]); px(d, 9, 7, P["eye"])
        d.ellipse((5, 0, 10, 3), outline=P["yellow"])
    return im


FERRET = dict(out=(43, 33, 24), body=(240, 224, 176), dark=(74, 58, 42), tail=(176, 144, 96),
              nose=(255, 143, 160), eye=(10, 10, 10), star=(255, 224, 64))


def ferret(view, frame=0, stunned=False):
    P = FERRET
    im, d = canvas()
    if view in ("down", "up"):
        sway = 1 if frame else 0
        rect(d, 5 - sway, 13, 6 - sway, 15, P["dark"]); rect(d, 9 + sway, 13, 10 + sway, 15, P["dark"])
        if view == "up":
            rect(d, 7, 12, 8, 15, P["tail"])
        ell(d, 4, 2, 11, 14, P["body"], P["out"])
        if view == "down":
            rect(d, 5, 5, 10, 6, P["dark"])
            if stunned:
                px(d, 6, 5, P["star"]); px(d, 9, 5, P["star"])
                for (x, y) in ((3, 0), (8, 0), (12, 1)):
                    px(d, x, y, P["star"])
                    px(d, x - 1, y + 1, P["star"]); px(d, x + 1, y + 1, P["star"])
            else:
                px(d, 6, 5, P["eye"]); px(d, 9, 5, P["eye"])
            rect(d, 7, 8, 8, 8, P["nose"])
        else:
            rect(d, 5, 4, 10, 4, P["dark"])
    else:
        im2, d2 = canvas()
        step = frame % 2
        for x in (4, 10):
            rect(d2, x + (step if x == 4 else -step), 12, x + 1 + (step if x == 4 else -step), 15, P["dark"])
        ell(d2, 3, 7, 14, 13, P["body"], P["out"])
        rect(d2, 13, 6 + step, 15, 8 + step, P["tail"])
        ell(d2, 0, 5, 6, 11, P["body"], P["out"])
        rect(d2, 1, 7, 4, 8, P["dark"])
        px(d2, 2, 7, P["eye"])
        px(d2, 0, 9, P["nose"])
        if view == "right":
            im2 = im2.transpose(Image.FLIP_LEFT_RIGHT)
        im = im2
    return im


CAT = dict(out=(32, 32, 40), body=(138, 138, 154), stripe=(74, 74, 90), eye=(96, 255, 96),
           nose=(255, 143, 160), muzzle=(230, 230, 240))


def cat(view, frame=0, pounce=False):
    P = CAT
    im, d = canvas()
    if pounce:
        if frame == 0:   # crouch, facing right
            ell(d, 1, 9, 12, 15, P["body"], P["out"])
            rect(d, 4, 10, 4, 14, P["stripe"]); rect(d, 7, 10, 7, 14, P["stripe"])
            ell(d, 9, 7, 15, 13, P["body"], P["out"])
            d.polygon([(10, 8), (11, 5), (12, 8)], fill=P["body"], outline=P["out"])
            px(d, 13, 9, P["eye"]); px(d, 15, 11, P["nose"])
            rect(d, 0, 12, 1, 12, P["stripe"])
        else:            # leap
            ell(d, 2, 4, 13, 10, P["body"], P["out"])
            rect(d, 5, 5, 5, 9, P["stripe"]); rect(d, 8, 5, 8, 9, P["stripe"])
            ell(d, 10, 2, 15, 8, P["body"], P["out"])
            d.polygon([(11, 3), (12, 0), (13, 3)], fill=P["body"], outline=P["out"])
            px(d, 13, 4, P["eye"])
            rect(d, 12, 9, 13, 12, P["out"]); rect(d, 2, 10, 3, 13, P["out"])
            rect(d, 0, 4, 2, 5, P["stripe"])
        return im
    if view in ("down", "up"):
        s = 1 if frame else 0
        rect(d, 4 + s, 13, 5 + s, 15, P["out"]); rect(d, 10 - s, 13, 11 - s, 15, P["out"])
        ell(d, 4, 7, 11, 15, P["body"], P["out"])
        ell(d, 3, 1, 12, 9, P["body"], P["out"])
        d.polygon([(3, 4), (4, 0), (6, 2)], fill=P["body"], outline=P["out"])
        d.polygon([(12, 4), (11, 0), (9, 2)], fill=P["body"], outline=P["out"])
        if view == "down":
            ell(d, 5, 5, 10, 9, P["muzzle"])
            px(d, 5, 4, P["eye"]); px(d, 10, 4, P["eye"])
            px(d, 7, 6, P["nose"]); px(d, 8, 6, P["nose"])
        else:
            rect(d, 7, 2, 8, 12, P["stripe"])
            rect(d, 12, 10, 14, 11, P["body"])
    else:
        im2, d2 = canvas()
        s = frame % 2
        rect(d2, 4 + s, 12, 5 + s, 15, P["out"]); rect(d2, 10 - s, 12, 11 - s, 15, P["out"])
        ell(d2, 3, 7, 14, 14, P["body"], P["out"])
        rect(d2, 7, 8, 7, 13, P["stripe"]); rect(d2, 10, 8, 10, 13, P["stripe"])
        rect(d2, 13, 3, 14, 8, P["body"])
        ell(d2, 0, 2, 7, 9, P["body"], P["out"])
        d2.polygon([(1, 3), (2, 0), (4, 2)], fill=P["body"], outline=P["out"])
        px(d2, 2, 5, P["eye"]); px(d2, 0, 7, P["nose"])
        if view == "right":
            im2 = im2.transpose(Image.FLIP_LEFT_RIGHT)
        im = im2
    return im


BOSS = dict(out=(48, 16, 8), body=(224, 112, 32), stripe=(160, 64, 16), eye=(255, 224, 0),
            teeth=(255, 255, 255), nose=(255, 120, 150), muzzle=(255, 220, 180), dark=(20, 10, 5))


def boss(frame):
    P = BOSS
    im, d = canvas(32, 32)
    low = 3 if frame == 2 else 0
    s = 2 if frame == 1 else 0
    # legs
    rect(d, 7 + s, 26, 10 + s, 31, P["out"]); rect(d, 21 - s, 26, 24 - s, 31, P["out"])
    ell(d, 5, 14 + low, 26, 31, P["body"], P["out"])
    for x in (10, 15, 20):
        rect(d, x, 17 + low, x + 1, 28, P["stripe"])
    # head
    ell(d, 4, 2 + low, 27, 22 + low, P["body"], P["out"])
    d.polygon([(5, 8 + low), (7, 0 + low), (12, 5 + low)], fill=P["body"], outline=P["out"])
    d.polygon([(26, 8 + low), (24, 0 + low), (19, 5 + low)], fill=P["body"], outline=P["out"])
    rect(d, 14, 3 + low, 17, 6 + low, P["stripe"])
    ell(d, 9, 12 + low, 22, 21 + low, P["muzzle"])
    ey = 9 + low
    if frame == 2:
        rect(d, 9, ey, 12, ey, P["eye"]); rect(d, 19, ey, 22, ey, P["eye"])
    else:
        ell(d, 8, ey - 2, 12, ey + 2, P["eye"], P["out"]); ell(d, 19, ey - 2, 23, ey + 2, P["eye"], P["out"])
        rect(d, 10, ey - 1, 10, ey + 1, P["dark"]); rect(d, 21, ey - 1, 21, ey + 1, P["dark"])
    rect(d, 15, 13 + low, 16, 14 + low, P["nose"])
    if frame == 3:
        ell(d, 11, 15, 20, 22, P["dark"])
        for x in (12, 18):
            d.polygon([(x, 15), (x + 1, 18), (x + 2, 15)], fill=P["teeth"])
    else:
        rect(d, 13, 17 + low, 18, 17 + low, P["out"])
    return im


# ---------------------------------------------------------------------------
# items and effects
BOMB = dict(black=(32, 32, 48), hl=(96, 96, 128), fuse=(128, 96, 64), yellow=(255, 224, 64),
            red=(255, 64, 32), white=(255, 255, 255), dust1=(200, 192, 176), dust2=(232, 224, 208),
            out=(16, 16, 24))


def bomb(frame):
    P = BOMB
    im, d = canvas()
    g = 1 if frame == 1 else 0
    ell(d, 2 - g, 4 - g, 13 + g, 15, P["black"], P["out"])
    rect(d, 5, 7, 6, 8, P["hl"])
    rect(d, 9, 3, 10, 4, P["hl"])
    d.line((10, 3, 12, 1), fill=P["fuse"])
    spark = [(P["yellow"], 0), (P["red"], 1), (P["white"], 1)][frame]
    c, r = spark
    px(d, 12, 1, c)
    if r:
        for (x, y) in ((13, 0), (11, 0), (13, 2), (14, 1)):
            px(d, x, y, P["yellow"] if frame == 1 else P["red"])
    return im


def dust(frame):
    P = BOMB
    im, d = canvas()
    r = [3, 5, 7][frame]
    c = [P["dust2"], P["dust1"], P["dust1"]][frame]
    for (cx, cy) in ((8, 10), (5, 11), (11, 11)):
        ell(d, cx - r // 2, cy - r // 2, cx + r // 2, cy + r // 2 - (1 if frame == 2 else 0), c)
    if frame == 2:
        for (x, y) in ((6, 9), (9, 11), (4, 12), (12, 10)):
            px(d, x, y, MAG)
    return im


PICK = dict(gold=(255, 192, 32), dgold=(192, 128, 16), hl=(255, 240, 128), out=(96, 48, 0),
            blue=(48, 80, 192), red=(192, 48, 32), yellow=(255, 224, 64), green=(48, 160, 64),
            white=(255, 255, 255), purple=(128, 48, 176), black=(24, 24, 32), pink=(255, 128, 160))


def grub(frame):
    P = PICK
    im, d = canvas()
    segs = [(3, 10), (5, 8), (8, 7), (11, 8), (13, 10)] if frame == 0 else \
           [(3, 8), (5, 10), (8, 11), (11, 10), (13, 8)]
    for i, (x, y) in enumerate(segs):
        ell(d, x - 2, y - 2, x + 2, y + 2, P["gold"] if i % 2 == 0 else P["dgold"], P["out"])
    x, y = segs[-1]
    px(d, x, y - 1, P["black"])
    px(d, segs[2][0] - 1, segs[2][1] - 1, P["hl"])
    return im


def powerup(kind):
    P = PICK
    im, d = canvas()
    bg = {"pu_bomb": P["blue"], "pu_fire": P["red"], "pu_speed": P["green"], "pu_remote": P["purple"],
          "pu_heart": P["white"]}[kind]
    d.rounded_rectangle((1, 1, 14, 14), 3, fill=bg, outline=P["out"])
    d.rounded_rectangle((2, 2, 13, 13), 2, outline=P["white"] if kind != "pu_heart" else P["pink"])
    if kind == "pu_bomb":
        ell(d, 4, 5, 11, 12, P["black"]); d.line((9, 5, 11, 3), fill=P["yellow"])
    elif kind == "pu_fire":
        d.polygon([(8, 3), (12, 9), (10, 12), (6, 12), (4, 9)], fill=P["yellow"])
        d.polygon([(8, 7), (10, 10), (8, 12), (6, 10)], fill=P["white"])
    elif kind == "pu_speed":
        for x in (5, 8, 11):
            d.line((x - 1, 4, x, 11), fill=P["white"])
            px(d, x, 12, P["white"])
    elif kind == "pu_remote":
        rect(d, 5, 6, 10, 12, P["black"]); ell(d, 6, 8, 9, 11, P["red"])
        d.line((10, 6, 12, 3), fill=P["black"]); px(d, 12, 3, P["yellow"])
    elif kind == "pu_heart":
        heart(d, 3, 4, P["red"], P["pink"])
    return im


def heart(d, x, y, c, hl):
    d.polygon([(x, y + 2), (x + 2, y), (x + 4, y + 1), (x + 5, y + 2), (x + 6, y + 1), (x + 8, y),
               (x + 10, y + 2), (x + 10, y + 4), (x + 5, y + 9), (x, y + 4)], fill=c)
    px(d, x + 2, y + 2, hl)


FX = dict(white=(255, 255, 255), yellow=(255, 232, 64), orange=(255, 128, 32), red=(208, 32, 16),
          dred=(128, 16, 8))


def fx_layers(frame):
    P = FX
    if frame == 0:
        return [(10, P["yellow"]), (6, P["white"])]
    if frame == 1:
        return [(14, P["orange"]), (10, P["yellow"]), (4, P["white"])]
    return [(8, P["dred"]), (4, P["red"])]


def explosion(kind, frame):
    im, d = canvas()
    for size, c in fx_layers(frame):
        a, b = 8 - size // 2, 8 + size // 2 - 1
        if kind in ("h", "center"):
            rect(d, 0, a, 15, b, c)
        if kind in ("v", "center"):
            rect(d, a, 0, b, 15, c)
        if kind == "center":
            r = size // 2 + 2
            ell(d, 8 - r, 8 - r, 7 + r, 7 + r, c)
        if kind.startswith("end_"):
            # arm connecting toward the centre, rounded tip on the far side
            if kind == "end_left":
                rect(d, 7, a, 15, b, c); ell(d, 8 - size // 2 - 3, a, 8 + size // 2, b, c)
            elif kind == "end_right":
                rect(d, 0, a, 8, b, c); ell(d, 7 - size // 2, a, 7 + size // 2 + 3, b, c)
            elif kind == "end_up":
                rect(d, a, 7, b, 15, c); ell(d, a, 8 - size // 2 - 3, b, 8 + size // 2, c)
            else:
                rect(d, a, 0, b, 8, c); ell(d, a, 7 - size // 2, b, 7 + size // 2 + 3, c)
    return im


HUD = dict(panel=(24, 28, 48), border=(60, 70, 100), white=(255, 255, 255), dark=(8, 8, 16),
           red=(224, 48, 48), bomb=(40, 40, 56), grey=(120, 120, 150), orange=(255, 140, 32),
           gold=(255, 200, 40), green=(64, 200, 80), brown=(140, 90, 50), dbrown=(80, 50, 30),
           yellow=(255, 240, 80), sky=(100, 160, 255))


def outline_glyph(d, rows, ox, oy, scale, c, o):
    pts = set()
    for y, r in enumerate(rows):
        for x, ch in enumerate(r):
            if ch == '#':
                for sy in range(scale):
                    for sx in range(scale):
                        pts.add((ox + x * scale + sx, oy + y * scale + sy))
    for (x, y) in pts:
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                if (x + dx, y + dy) not in pts:
                    px(d, x + dx, y + dy, o)
    for (x, y) in pts:
        px(d, x, y, c)


def hud(kind, frame=0):
    P = HUD
    im, d = canvas()
    if kind == "hud_digit":
        outline_glyph(d, FONT[str(frame)], 3, 1, 2, P["white"], P["dark"])
    elif kind == "hud_slash":
        outline_glyph(d, FONT["/"], 3, 1, 2, P["white"], P["dark"])
    elif kind == "hud_heart":
        heart(d, 3, 3, P["red"], P["white"])
    elif kind == "hud_bomb":
        ell(d, 3, 4, 12, 13, P["bomb"], P["dark"]); rect(d, 5, 6, 6, 7, P["grey"])
        d.line((10, 4, 12, 2), fill=P["orange"]); px(d, 13, 1, P["yellow"])
    elif kind == "hud_fire":
        d.polygon([(8, 1), (13, 8), (11, 14), (5, 14), (3, 8)], fill=P["orange"], outline=P["red"])
        d.polygon([(8, 6), (10, 10), (9, 13), (7, 13), (6, 10)], fill=P["yellow"])
    elif kind == "hud_grub":
        for i, x in enumerate((3, 6, 9, 12)):
            ell(d, x - 2, 6, x + 2, 11, P["gold"], P["dbrown"])
        px(d, 13, 7, P["dark"])
    elif kind == "hud_speed":
        for x in (4, 8, 12):
            d.line((x - 2, 3, x, 12), fill=P["white"]); px(d, x, 13, P["grey"])
    elif kind in ("hud_depth_surface", "hud_depth_under1", "hud_depth_under2"):
        d.rectangle((1, 2, 14, 13), fill=P["dbrown"], outline=P["border"])
        if kind == "hud_depth_surface":
            rect(d, 2, 3, 13, 6, P["sky"]); rect(d, 2, 7, 13, 8, P["green"]); rect(d, 2, 9, 13, 12, P["brown"])
        elif kind == "hud_depth_under1":
            rect(d, 2, 3, 13, 12, P["brown"]); rect(d, 4, 7, 11, 8, P["dark"])
        else:
            rect(d, 2, 3, 13, 12, P["dbrown"]); rect(d, 4, 9, 11, 10, P["dark"]); rect(d, 4, 5, 5, 6, P["brown"])
    elif kind == "hud_danger":
        d.polygon([(8, 1), (15, 14), (1, 14)], fill=P["red"], outline=P["dark"])
        rect(d, 7, 5, 8, 10, P["white"]); rect(d, 7, 12, 8, 12, P["white"])
    elif kind == "hud_lock":
        d.arc((4, 1, 11, 9), 180, 360, fill=P["grey"], width=2)
        rect(d, 3, 7, 12, 14, P["grey"]); rect(d, 7, 9, 8, 12, P["dark"])
    elif kind == "hud_check":
        d.line((3, 8, 6, 12), fill=P["green"], width=2); d.line((6, 12, 13, 3), fill=P["green"], width=2)
    elif kind == "hud_cursor":
        d.polygon([(4, 2), (12, 8), (4, 14)], fill=P["yellow"], outline=P["dark"])
    elif kind == "hud_panel":
        rect(d, 0, 0, 15, 15, P["panel"]); rect(d, 0, 15, 15, 15, P["border"])
    return im


# ---------------------------------------------------------------------------
# terrain (one row per season)
BASE = dict(
    grass=(88, 192, 64), grass_d=(56, 144, 48), grass_l=(136, 224, 96),
    dirt=(138, 90, 48), dirt_d=(100, 62, 32), dirt_l=(176, 124, 72),
    rock=(136, 136, 144), rock_d=(88, 88, 100), rock_l=(184, 184, 192),
    stone=(72, 80, 104), stone_d=(40, 44, 64), stone_l=(112, 120, 148),
    root=(120, 72, 40), root_l=(170, 120, 70),
    tunnel=(70, 44, 26), tunnel_d=(50, 30, 18), tunnel_l=(92, 60, 36),
    black=(8, 6, 4), light=(255, 240, 180), wood=(180, 130, 70), wood_d=(120, 80, 40),
    water=(64, 128, 224), water_l=(128, 192, 255), water_d=(32, 72, 160),
    snow=(240, 244, 255), snow_s=(184, 200, 232), ice=(160, 200, 232), frozen=(104, 112, 140),
    leaf_r=(208, 64, 32), leaf_o=(240, 144, 32), leaf_y=(248, 208, 64), gold=(255, 208, 64),
)
SEASON_TWEAKS = {
    "spring": {},
    "summer": dict(grass=(160, 200, 64), grass_d=(120, 160, 40), grass_l=(208, 232, 112),
                   dirt=(192, 152, 96), dirt_d=(150, 112, 64), dirt_l=(224, 192, 136),
                   tunnel=(110, 80, 48), tunnel_d=(84, 60, 36), tunnel_l=(136, 104, 64)),
    "autumn": dict(grass=(168, 144, 64), grass_d=(128, 104, 40), grass_l=(200, 176, 88),
                   dirt=(112, 64, 32), dirt_d=(80, 44, 20), dirt_l=(150, 96, 56)),
    "winter": dict(grass=(216, 228, 248), grass_d=(160, 180, 216), grass_l=(248, 252, 255),
                   dirt=(112, 100, 110), dirt_d=(80, 72, 84), dirt_l=(150, 140, 156),
                   tunnel=(60, 52, 64), tunnel_d=(40, 34, 44), tunnel_l=(84, 76, 92),
                   water=(120, 170, 220), water_l=(200, 230, 255), water_d=(80, 120, 180)),
}


def speckle(d, rng, colors, n, box=(0, 0, 15, 15)):
    for _ in range(n):
        px(d, rng.randint(box[0], box[2]), rng.randint(box[1], box[3]), rng.choice(colors))


def terrain(name, season):
    P = dict(BASE)
    P.update(SEASON_TWEAKS[sheets.SEASONS[season]])
    rng = random.Random("%s-%d" % (name, season))
    im = Image.new("RGBA", (16, 16), P["grass"] + (255,))
    d = ImageDraw.Draw(im)

    def grass_bg():
        rect(d, 0, 0, 15, 15, P["grass"])
        for _ in range(6):
            x, y = rng.randint(1, 14), rng.randint(2, 14)
            px(d, x, y, P["grass_d"]); px(d, x - 1, y - 1, P["grass_d"]); px(d, x + 1, y - 1, P["grass_l"])

    def tunnel_bg():
        rect(d, 0, 0, 15, 15, P["tunnel"])
        speckle(d, rng, [P["tunnel_d"], P["tunnel_l"]], 14)

    def block(fill, dark, light, n=10):
        rect(d, 0, 0, 15, 15, fill)
        speckle(d, rng, [dark, light], n, (1, 1, 14, 14))
        rect(d, 0, 0, 15, 0, light); rect(d, 0, 0, 0, 15, light)
        rect(d, 0, 15, 15, 15, dark); rect(d, 15, 0, 15, 15, dark)

    if name == "grass":
        grass_bg()
    elif name == "grass_edge":
        grass_bg()
        rect(d, 0, 0, 15, 2, P["grass_d"])
        for x in range(0, 16, 2):
            px(d, x, 3, P["grass_d"])
    elif name == "soft_dirt":
        block(P["dirt"], P["dirt_d"], P["dirt_l"], 16)
    elif name == "dirt_crack":
        block(P["dirt"], P["dirt_d"], P["dirt_l"], 16)
        d.line((3, 2, 7, 7, 5, 11, 9, 14), fill=P["black"])
        d.line((7, 7, 12, 5), fill=P["black"])
    elif name == "hard_rock":
        tunnel_bg()
        ell(d, 0, 1, 15, 15, P["rock"], P["rock_d"])
        rect(d, 3, 4, 6, 5, P["rock_l"]); d.line((9, 9, 12, 12), fill=P["rock_d"])
        d.line((5, 10, 7, 12), fill=P["rock_d"])
    elif name == "stone":
        rect(d, 0, 0, 15, 15, P["stone"])
        for y in (0, 8):
            rect(d, 0, y + 7, 15, y + 7, P["stone_d"])
            off = 0 if y == 0 else 4
            for x in (off, off + 8):
                rect(d, x % 16, y, x % 16, y + 6, P["stone_d"])
            rect(d, 0, y, 15, y, P["stone_l"])
    elif name == "roots":
        rect(d, 0, 0, 15, 15, P["dirt_d"])
        d.line((0, 3, 5, 6, 10, 4, 15, 7), fill=P["root"], width=2)
        d.line((0, 11, 6, 9, 11, 12, 15, 10), fill=P["root"], width=2)
        d.line((7, 0, 6, 15), fill=P["root_l"])
    elif name == "tunnel":
        tunnel_bg()
    elif name == "hole_down":
        tunnel_bg()
        ell(d, 1, 1, 14, 14, P["dirt_l"]); ell(d, 3, 3, 12, 12, P["black"])
    elif name == "hole_up":
        tunnel_bg()
        ell(d, 2, 2, 13, 13, P["light"])
        for (x, y) in ((1, 1), (14, 1), (1, 14), (14, 14)):
            d.line((x, y, 8, 8), fill=P["light"])
        ell(d, 5, 5, 10, 10, P["dirt_l"])
    elif name == "ladder":
        tunnel_bg()
        rect(d, 3, 0, 4, 15, P["wood_d"]); rect(d, 11, 0, 12, 15, P["wood_d"])
        for y in (2, 7, 12):
            rect(d, 3, y, 12, y + 1, P["wood"])
    elif name == "thin_floor":
        rect(d, 0, 0, 15, 15, P["wood"])
        for y in (4, 9, 14):
            rect(d, 0, y, 15, y, P["wood_d"])
        d.line((4, 1, 7, 6, 5, 10), fill=P["black"]); d.line((11, 5, 9, 12), fill=P["black"])
    elif name in ("exit_closed", "exit_open"):
        grass_bg()
        ell(d, 1, 3, 14, 15, P["dirt"], P["dirt_d"]); ell(d, 4, 4, 11, 9, P["dirt_l"])
        if name == "exit_closed":
            d.line((5, 7, 10, 12), fill=P["dirt_d"], width=2); d.line((10, 7, 5, 12), fill=P["dirt_d"], width=2)
        else:
            ell(d, 4, 6, 11, 13, P["black"])
            for (x, y) in ((2, 1), (13, 2), (8, 1), (14, 12), (1, 11)):
                px(d, x, y, P["gold"])
            rect(d, 7, 0, 8, 1, P["gold"])
    elif name == "puddle":
        grass_bg()
        ell(d, 1, 4, 14, 13, P["water"], P["water_d"]); rect(d, 4, 6, 7, 6, P["water_l"])
    elif name == "snow":
        rect(d, 0, 0, 15, 15, P["snow"])
        speckle(d, rng, [P["snow_s"]], 10)
    elif name == "frozen_dirt":
        block(P["frozen"], P["stone_d"], P["ice"], 8)
        d.line((3, 3, 6, 3), fill=P["ice"]); d.line((9, 10, 12, 10), fill=P["ice"])
    elif name == "leaves":
        grass_bg()
        for _ in range(14):
            x, y = rng.randint(1, 12), rng.randint(2, 12)
            c = rng.choice([P["leaf_r"], P["leaf_o"], P["leaf_y"]])
            rect(d, x, y, x + 2, y + 1, c)
    elif name == "water":
        rect(d, 0, 0, 15, 15, P["water"])
        for y in (3, 9):
            d.line((1, y, 3, y - 1, 5, y, 7, y - 1), fill=P["water_l"])
            d.line((9, y + 3, 11, y + 2, 13, y + 3), fill=P["water_d"])
    return im


# ---------------------------------------------------------------------------
def draw_entry(e, frame, season):
    if e.sheet == "props":
        return placeholder_props.draw(e, frame)
    n = e.name
    if n.startswith("mole_"):
        parts = n.split("_")
        if n == "mole_place_bomb":
            return mole("down", 0, "bomb")
        if n == "mole_hurt":
            return mole("down", 0, "hurt")
        if n == "mole_victory":
            return mole("down", frame, "victory")
        if n == "mole_death":
            return mole_death(frame)
        pose = parts[1]
        return mole(parts[2], frame, "walk" if pose == "walk" else "dig")
    if n.startswith("ferret_"):
        if n == "ferret_stunned":
            return ferret("down", 0, stunned=True)
        return ferret(n.split("_")[2], frame)
    if n.startswith("cat_"):
        if n == "cat_pounce":
            return cat("right", frame, pounce=True)
        return cat(n.split("_")[2], frame)
    if n == "boss":
        return boss(frame)
    if n == "bomb":
        return bomb(frame)
    if n == "dust":
        return dust(frame)
    if n == "grub":
        return grub(frame)
    if n.startswith("pu_"):
        return powerup(n)
    if n.startswith("expl_"):
        return explosion(n[5:], frame)
    if n.startswith("hud_"):
        return hud(n, frame)
    if e.sheet == "tiles":
        return terrain(n, season)
    raise ValueError(n)


def build_sheet(sheet):
    W, H = sheets.sheet_size(sheet)
    im = Image.new("RGBA", (W, H), MAG)
    for e, f, s, (x, y, w, h) in sheets.reading_order(sheet):
        cell = draw_entry(e, f, s)
        assert cell.size == (w, h), (e.name, cell.size)
        im.paste(cell, (x, y))
    return im.convert("RGB")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", default="games/bombermole/art")
    ap.add_argument("--preview", action="store_true", help="also write 4x previews")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    for sheet in sheets.SHEETS:
        im = build_sheet(sheet)
        path = os.path.join(a.out, sheets.SHEETS[sheet]["file"])
        im.save(path)
        print("wrote", path, im.size)
        if a.preview:
            im.resize((im.width * 4, im.height * 4), Image.NEAREST).save(path.replace(".png", "_4x.png"))


if __name__ == "__main__":
    main()

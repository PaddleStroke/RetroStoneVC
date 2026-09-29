"""Placeholder art for props.png (level gimmicks and the extra bosses).
Used by tools/make_placeholders.py. MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft)."""
import math
import random

from PIL import Image, ImageDraw

import sheets

MAG = sheets.MAGENTA + (255,)

PB = dict(wood=(180, 130, 70), wood_d=(120, 80, 40), water=(64, 128, 224), water_l=(128, 192, 255),
          ice=(190, 225, 250), ice_d=(130, 180, 220), white=(255, 255, 255), mud=(90, 60, 40),
          mud_l=(120, 85, 55), green=(60, 170, 50), green_d=(30, 110, 30), corn=(230, 200, 60),
          dark=(20, 15, 10), grey=(140, 140, 150), red=(200, 50, 40))
PR = dict(grey=(150, 150, 160), dark=(40, 40, 50), blue=(60, 110, 220), white=(255, 255, 255),
          water=(128, 192, 255), wood=(180, 130, 70), wood_d=(120, 80, 40), pale=(220, 225, 240),
          orange=(240, 130, 30), green=(60, 160, 50), ice=(170, 210, 245), red=(210, 40, 40), roof=(170, 60, 50))
CR = dict(brown=(150, 100, 60), light=(210, 170, 120), dark=(50, 30, 20), pink=(250, 120, 140),
          white=(255, 255, 255), yellow=(250, 210, 40), black=(20, 20, 20))


def canvas(w=16, h=16):
    im = Image.new("RGBA", (w, h), MAG)
    return im, ImageDraw.Draw(im)


def px(d, x, y, c):
    d.point((x, y), fill=c)


def rect(d, x0, y0, x1, y1, c):
    d.rectangle((x0, y0, x1, y1), fill=c)


def ell(d, x0, y0, x1, y1, c, o=None):
    d.ellipse((x0, y0, x1, y1), fill=c, outline=o)


def speckle(d, rng, colors, n):
    for _ in range(n):
        px(d, rng.randint(0, 15), rng.randint(0, 15), rng.choice(colors))


def prop_bg(name, frame):
    P = PB
    im, d = canvas()
    if name in ("bridge", "bridge_v"):
        # drawn over the water (magenta around): planks across the way, rails along it
        rect(d, 0, 2, 15, 13, P["wood"])
        for x in (3, 7, 11, 15):
            rect(d, x, 2, x, 13, P["wood_d"])
        rect(d, 0, 2, 15, 2, P["wood_d"])
        rect(d, 0, 13, 15, 13, P["wood_d"])
        if name == "bridge_v":
            im = im.transpose(Image.Transpose.TRANSPOSE)
    elif name == "ice":
        rect(d, 0, 0, 15, 15, P["ice"])
        d.line((2, 12, 6, 8), fill=P["white"])
        d.line((9, 5, 12, 2), fill=P["white"])
        px(d, 13, 12, P["ice_d"])
        px(d, 4, 3, P["ice_d"])
    elif name == "thin_ice":
        rect(d, 0, 0, 15, 15, P["ice_d"])
        speckle(d, random.Random(3), [P["water_l"]], 12)
        if frame:
            d.line((1, 3, 6, 8, 4, 14), fill=P["water"])
            d.line((6, 8, 14, 6), fill=P["water"])
    elif name == "mud":
        rect(d, 0, 0, 15, 15, P["mud"])
        for (x, y) in ((3, 4), (10, 3), (6, 10), (12, 12)):
            ell(d, x - 2, y - 1, x + 2, y + 1, P["mud_l"])
    elif name in ("tall_grass", "corn"):
        c = P["green"] if name == "tall_grass" else P["corn"]
        for x in range(0, 16, 3):
            d.line((x, 15, x + 1, 1), fill=c)
            px(d, x + 1, 0, P["corn"] if name == "corn" else c)
    elif name == "burnt":
        rect(d, 0, 0, 15, 15, P["mud"])
        speckle(d, random.Random(4), [P["dark"], P["grey"]], 20)
    elif name == "gate":     # drawn over the ground: magenta around
        if frame == 0:
            for x in (1, 5, 9, 13):
                rect(d, x, 1, x + 1, 14, P["grey"])
            rect(d, 0, 4, 15, 5, P["dark"])
            rect(d, 0, 10, 15, 11, P["dark"])
        else:
            rect(d, 0, 1, 1, 14, P["grey"])
            rect(d, 14, 1, 15, 14, P["grey"])
    elif name == "plate":     # drawn over the ground: magenta around
        d.rectangle((2, 2, 13, 13), fill=P["grey"] if frame == 0 else P["dark"], outline=P["dark"])
        if frame == 0:
            rect(d, 3, 3, 12, 3, P["white"])
    elif name == "lever":     # drawn over the ground: magenta around
        rect(d, 4, 11, 11, 14, P["grey"])
        if frame == 0:
            d.line((8, 11, 4, 3), fill=P["dark"], width=2)
            ell(d, 2, 1, 5, 4, P["red"])
        else:
            d.line((8, 11, 12, 3), fill=P["dark"], width=2)
            ell(d, 11, 1, 14, 4, P["green"])
    elif name == "steam_vent":     # drawn over the ground: magenta around
        ell(d, 2, 2, 13, 13, P["grey"], P["dark"])
        ell(d, 5, 5, 10, 10, P["dark"])
        if frame:
            ell(d, 4, 1, 11, 8, P["white"])
            ell(d, 6, 4, 9, 9, P["ice"])
    elif name == "pipe":     # drawn over the ground: magenta around
        ell(d, 1, 1, 14, 14, P["grey"], P["dark"])
        ell(d, 4, 4, 11, 11, P["dark"])
    elif name == "crate":
        rect(d, 1, 1, 14, 13, P["wood"])
        d.rectangle((1, 1, 14, 13), outline=P["wood_d"])
        d.line((1, 1, 14, 13), fill=P["wood_d"])
        d.line((14, 1, 1, 13), fill=P["wood_d"])
        for x in range(1, 15, 2):
            px(d, x, 14, P["dark"])                          # contact shadow
        ell(d, 5, 5, 10, 10, P["red"])
    elif name == "splat":     # drawn over the ground: magenta around
        d.polygon([(8, 2), (10, 6), (14, 5), (11, 9), (13, 13), (8, 11), (3, 14), (5, 9), (1, 6), (6, 6)],
                  fill=P["red"])
    elif name == "beehive":     # drawn over the ground: magenta around
        ell(d, 3, 2, 12, 14, P["corn"], P["dark"])
        for y in (5, 8, 11):
            rect(d, 4, y, 11, y, P["wood_d"])
        rect(d, 7, 11, 8, 12, P["dark"])
    elif name == "well":     # drawn over the ground: magenta around
        ell(d, 1, 3, 14, 15, P["grey"], P["dark"])
        ell(d, 4, 6, 11, 12, P["water"])
        rect(d, 1, 0, 2, 8, P["wood_d"])
        rect(d, 13, 0, 14, 8, P["wood_d"])
        rect(d, 1, 0, 14, 1, P["wood"])
    elif name == "mushroom":     # drawn over the ground: magenta around
        rect(d, 6, 8, 9, 14, P["white"])
        ell(d, 1, 2, 14, 10, P["red"], P["dark"])
        for (x, y) in ((4, 5), (9, 4), (11, 7)):
            px(d, x, y, P["white"])
    elif name in ("rails_h", "rails_v"):     # drawn over the ground: magenta around
        for k in (1, 5, 9, 13):
            if name == "rails_h":
                rect(d, k, 3, k + 1, 12, P["wood_d"])
            else:
                rect(d, 3, k, 12, k + 1, P["wood_d"])
        if name == "rails_h":
            rect(d, 0, 4, 15, 4, P["grey"])
            rect(d, 0, 11, 15, 11, P["grey"])
        else:
            rect(d, 4, 0, 4, 15, P["grey"])
            rect(d, 11, 0, 11, 15, P["grey"])
    elif name == "apple_tree":     # drawn over the ground: magenta around
        rect(d, 7, 10, 8, 15, P["wood_d"])
        ell(d, 1, 0, 14, 11, P["green_d"], P["dark"])
        for (x, y) in ((4, 4), (10, 3), (7, 7), (11, 8)):
            rect(d, x, y, x + 1, y + 1, P["red"])
    return im


def prop_obj(name, frame):
    P = PR
    im, d = canvas()
    if name == "sprinkler":
        ell(d, 4, 4, 11, 11, P["grey"], P["dark"])
        ell(d, 6, 6, 9, 9, P["blue"])
        dx, dy = [(0, -1), (1, 0), (0, 1), (-1, 0)][frame]
        d.line((8, 8, 8 + dx * 7, 8 + dy * 7), fill=P["dark"], width=2)
        px(d, min(15, 8 + dx * 7), min(15, 8 + dy * 7), P["water"])
    elif name == "spray":
        for (x, y) in ((3, 4), (8, 2), (12, 6), (5, 10), (10, 12), (2, 13)):
            px(d, (x + frame * 2) % 16, y, P["water"])
            px(d, (x + frame * 2 + 1) % 16, y, P["white"])
    elif name == "log":
        # a short round log: bark with grain lines, both ends cut (light, with rings), no dark end
        rect(d, 3, 4, 12, 12, P["wood"])
        d.line((3, 4, 12, 4), fill=P["wood_d"])
        d.line((3, 12, 12, 12), fill=P["wood_d"])
        for y, x0, x1 in ((6, 4, 8), (8, 6, 11), (10, 4, 9)):
            d.line((x0, y, x1, y), fill=P["wood_d"])
        for cx in (2, 13):
            ell(d, cx - 2, 4, cx + 2, 12, (226, 190, 130), P["wood_d"])
            ell(d, cx - 1, 6, cx + 1, 10, (226, 190, 130), (170, 125, 70))
            px(d, cx, 8, (170, 125, 70))
    elif name == "wind":
        for i, y in enumerate((4, 8, 12)):
            x0 = (frame * 4 + i * 5) % 10
            d.line((x0, y, x0 + 5, y), fill=P["pale"] if i != 1 else P["white"])
            px(d, x0 + 6, y - 1, P["pale"])
    elif name == "pumpkin":
        ell(d, 1, 4, 14, 15, P["orange"], P["wood_d"])
        d.line((5, 5, 5, 14), fill=P["wood_d"])
        d.line((10, 5, 10, 14), fill=P["wood_d"])
        rect(d, 7, 1, 8, 4, P["green"])
    elif name == "snowball":
        r = 4 if frame == 0 else 7
        ell(d, 8 - r, 15 - 2 * r, 8 + r, 15, P["white"], P["ice"])
        px(d, 8 - r // 2, 15 - r - 1, P["ice"])
    elif name == "icicle":
        d.polygon([(3, 0), (12, 0), (8, 15)], fill=P["ice"], outline=P["white"])
    elif name == "mine_cart":
        d.polygon([(1, 4), (14, 4), (12, 12), (3, 12)], fill=P["grey"], outline=P["dark"])
        ell(d, 2, 11, 6, 15, P["dark"])
        ell(d, 9, 11, 13, 15, P["dark"])
        rect(d, 2, 5, 13, 6, P["wood_d"])
    elif name == "bucket":
        d.polygon([(3, 5), (12, 5), (11, 15), (4, 15)], fill=P["wood"], outline=P["wood_d"])
        d.arc((3, 0, 12, 10), 180, 360, fill=P["grey"])
        rect(d, 4, 6, 11, 7, P["water"])
    elif name == "tomato":
        ell(d, 3, 4, 12, 13, P["red"], P["dark"])
        if frame == 0:
            rect(d, 7, 3, 8, 5, P["green"])
            px(d, 5, 6, P["white"])
        else:
            rect(d, 10, 7, 12, 8, P["green"])
            px(d, 9, 11, P["white"])
    elif name == "tomato_shadow":
        ell(d, 3, 10, 12, 14, P["dark"])
    elif name == "apple":
        ell(d, 4, 5, 11, 12, P["red"], P["dark"])
        rect(d, 7, 2, 8, 5, P["green"])
    elif name == "zzz":
        for i, (x, y) in enumerate(((3, 9), (8, 4))):
            s = 3 + i
            x += frame
            d.line((x, y, x + s, y), fill=P["white"])
            d.line((x + s, y, x, y + s), fill=P["white"])
            d.line((x, y + s, x + s, y + s), fill=P["white"])
    elif name == "steam":
        ell(d, 2, 5, 9, 12, P["white"])
        ell(d, 6, 1, 13, 8, P["pale"])
        ell(d, 7, 8, 14, 14, P["pale"])
    return im


def windmill(frame):
    P = PR
    im, d = canvas(32, 32)
    d.polygon([(10, 31), (22, 31), (19, 12), (13, 12)], fill=P["pale"], outline=P["dark"])
    d.polygon([(11, 13), (21, 13), (16, 6)], fill=P["roof"], outline=P["dark"])
    rect(d, 14, 24, 17, 31, P["wood_d"])
    cx, cy = 16, 11
    for k in range(4):
        a = math.radians(frame * 22.5 + k * 90)
        x1, y1 = cx + 14 * math.cos(a), cy + 14 * math.sin(a)
        d.line((cx, cy, x1, y1), fill=P["wood_d"])
        b = a + 0.35
        d.polygon([(cx + 4 * math.cos(a), cy + 4 * math.sin(a)), (x1, y1),
                   (cx + 12 * math.cos(b), cy + 12 * math.sin(b))], fill=P["white"], outline=P["grey"])
    ell(d, 14, 9, 18, 13, P["wood"], P["dark"])
    return im


def windmill_side(frame):
    """Side view: the tower, and the sails on its right side seen edge-on (they blow to the right)."""
    P = PR
    im, d = canvas(32, 32)
    d.polygon([(9, 31), (21, 31), (19, 12), (11, 12)], fill=P["pale"], outline=P["dark"])
    d.polygon([(9, 13), (21, 13), (16, 5)], fill=P["roof"], outline=P["dark"])
    rect(d, 13, 24, 16, 31, P["wood_d"])
    rect(d, 20, 9, 23, 12, P["wood_d"])                      # the shaft out of the roof, to the right
    h = [13, 8, 3, 8][frame]                                 # the sails turn: edge-on, their height changes
    rect(d, 24, 11 - h, 26, 11 + h, P["white"])
    d.rectangle((24, 11 - h, 26, 11 + h), outline=P["grey"])
    rect(d, 23, 10, 25, 12, P["wood"])
    return im


def critter(name, frame):
    P = CR
    im, d = canvas()
    if name == "dog_sleep":
        ell(d, 1, 8, 14, 15, P["brown"], P["dark"])
        ell(d, 1, 7, 6, 12, P["light"], P["dark"])
        rect(d, 2, 8, 3, 9, P["dark"])
    elif name.startswith("dog_walk"):
        s = frame % 2
        rect(d, 4 + s, 12, 5 + s, 15, P["dark"])
        rect(d, 10 - s, 12, 11 - s, 15, P["dark"])
        ell(d, 3, 6, 14, 13, P["brown"], P["dark"])
        ell(d, 0, 3, 6, 10, P["light"], P["dark"])
        rect(d, 1, 3, 2, 6, P["dark"])
        px(d, 2, 6, P["black"])
        rect(d, 0, 8, 1, 8, P["black"])
        rect(d, 1, 9, 2, 10, P["pink"])
        rect(d, 13, 4, 15, 5 + s, P["brown"])
        if name == "dog_walk_right":
            im = im.transpose(Image.FLIP_LEFT_RIGHT)
    elif name == "bees":
        for (x, y) in ((3, 5), (9, 3), (6, 10), (12, 9)):
            x += frame if y < 8 else -frame
            ell(d, x - 2, y - 1, x + 2, y + 1, P["yellow"], P["black"])
            px(d, x, y - 2, P["white"])
    return im


def boss32(name, frame):
    im, d = canvas(32, 32)
    if name == "fox":
        O, B, W, K = (60, 20, 5), (235, 120, 30), (255, 245, 235), (20, 15, 10)
        s = 2 if frame in (1, 2) else 0
        up = -4 if frame == 2 else 0
        rect(d, 8 + s, 24 + up, 10 + s, 31 + up, K)
        rect(d, 20 - s, 24 + up, 22 - s, 31 + up, K)
        ell(d, 5, 13 + up, 27, 27 + up, B, O)
        d.polygon([(26, 18 + up), (31, 10 + up), (31, 22 + up)], fill=B, outline=O)
        rect(d, 29, 10 + up, 31, 12 + up, W)
        ell(d, 0, 6 + up, 14, 20 + up, B, O)
        d.polygon([(2, 8 + up), (3, max(0, up)), (7, 6 + up)], fill=B, outline=O)
        d.polygon([(8, 6 + up), (11, max(0, up)), (13, 8 + up)], fill=B, outline=O)
        ell(d, 0, 12 + up, 8, 20 + up, W)
        px(d, 0, 15 + up, K)
        rect(d, 5, 10 + up, 6, 11 + up, K)
        if frame == 3:
            d.line((3, 9, 8, 12), fill=K)
            d.line((8, 9, 3, 12), fill=K)
    elif name == "owl":
        O, B, G, Y, K = (90, 90, 110), (245, 245, 250), (190, 195, 210), (250, 210, 40), (20, 20, 30)
        spread = [0, 3, 3, 1][frame]
        d.polygon([(16, 8), (4 - spread, 18), (16, 26)], fill=G, outline=O)
        d.polygon([(16, 8), (28 + spread, 18), (16, 26)], fill=G, outline=O)
        ell(d, 7, 4, 25, 30, B, O)
        for (x, y) in ((12, 18), (19, 20), (14, 24), (20, 25)):
            px(d, x, y, O)
        ell(d, 9, 8, 15, 14, Y, O)
        ell(d, 17, 8, 23, 14, Y, O)
        rect(d, 11, 10, 13, 12, K)
        rect(d, 19, 10, 21, 12, K)
        d.polygon([(15, 13), (17, 13), (16, 17)], fill=(200, 140, 20))
        if frame == 3:
            d.line((10, 9, 14, 13), fill=K)
            d.line((18, 9, 22, 13), fill=K)
    elif name == "badger":
        O, G, K, W = (30, 30, 35), (130, 130, 140), (25, 25, 30), (245, 245, 245)
        s = 2 if frame == 1 else 0
        rect(d, 6 + s, 25, 9 + s, 31, K)
        rect(d, 22 - s, 25, 25 - s, 31, K)
        ell(d, 3, 12, 29, 29, G, O)
        ell(d, 8, 3, 24, 21, W, O)
        rect(d, 11, 4, 13, 20, K)
        rect(d, 19, 4, 21, 20, K)
        px(d, 12, 12, W)
        px(d, 20, 12, W)
        rect(d, 15, 17, 17, 19, K)
        if frame == 2:
            for (x, y) in ((2, 28), (28, 27), (5, 31), (26, 31)):
                rect(d, x, y - 1, x + 1, y, (120, 80, 40))
        if frame == 3:
            d.line((10, 10, 14, 14), fill=(250, 60, 60))
            d.line((18, 10, 22, 14), fill=(250, 60, 60))
    elif name == "farmer":
        SK, HAT, OV, SH, K, W, R, G = ((240, 190, 150), (220, 190, 90), (60, 90, 190), (200, 60, 50),
                                      (30, 20, 20), (255, 255, 255), (220, 40, 30), (60, 150, 50))
        kind, f = frame // 2, frame % 2
        bob = f
        rect(d, 10, 26, 13, 31, K)
        rect(d, 19, 26, 22, 31, K)
        rect(d, 9, 15 + bob, 23, 27, OV)
        rect(d, 9, 13 + bob, 23, 17 + bob, SH)
        ell(d, 10, 3 + bob, 22, 15 + bob, SK, K)
        rect(d, 6, 4 + bob, 26, 5 + bob, HAT)
        ell(d, 10, bob, 22, 6 + bob, HAT)
        fy = 9 + bob
        if kind == 2:       # angry
            d.line((12, fy - 2, 15, fy), fill=K)
            d.line((20, fy - 2, 17, fy), fill=K)
            rect(d, 13, fy + 3, 19, fy + 3, K)
            ell(d, 11, fy + 1, 13, fy + 2, R)
            ell(d, 19, fy + 1, 21, fy + 2, R)
        elif kind == 3:     # hurt
            for x in (12, 17):
                d.line((x, fy - 1, x + 3, fy + 1), fill=K)
                d.line((x + 3, fy - 1, x, fy + 1), fill=K)
            ell(d, 14, fy + 3, 18, fy + 5, K)
        else:
            px(d, 13, fy, K)
            px(d, 19, fy, K)
            rect(d, 14, fy + 3, 18, fy + 3, K)
        if kind == 1:       # throwing: arm up with a tomato, then forward
            ay = 6 if f == 0 else 12
            d.line((23, 15, 28, ay), fill=SH, width=3)
            if f == 0:
                ell(d, 26, ay - 4, 31, ay + 1, R, K)
        else:
            rect(d, 5, 15 + bob, 8, 23 + bob, SH)
            rect(d, 24, 15 + bob, 27, 23 + bob, SH)
            rect(d, 5, 23 + bob, 8, 25 + bob, SK)
            rect(d, 24, 23 + bob, 27, 25 + bob, SK)
        rect(d, 12, 18 + bob, 14, 19 + bob, W)
        rect(d, 18, 18 + bob, 20, 19 + bob, W)
        if kind == 0 and f == 1:
            rect(d, 27, 20, 29, 22, G)
    return im


def draw(e, frame):
    if e.group == "propbg":
        return prop_bg(e.name, frame)
    if e.group == "critter":
        return critter(e.name, frame)
    if e.name == "windmill":
        return windmill(frame)
    if e.name == "windmill_side":
        return windmill_side(frame)
    if e.w == 32 or e.group.startswith("boss_"):      # 48 or 64 px bosses are resized by the caller
        return boss32(e.name, frame)
    return prop_obj(e.name, frame)

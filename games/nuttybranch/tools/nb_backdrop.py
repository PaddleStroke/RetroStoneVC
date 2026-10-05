#!/usr/bin/env python3
"""Nutty Branch: the forest behind the branch, in four styles to choose from (BG_STYLE in src/tuning.h, or
--opt bg=a..d). Each style is two parallax layers of 256x256 (BG4 far, scrolled at 1/4 of the curves' drift; BG3
near, at 1/2), drawn in the scene palette (BG 2: the canopy ramp 1-4, far tones 5-7, the crowns far below 8-10,
trunks 11, mist 12-13, leaf mids 14-15), so every style follows the seasons and the time of day.
  a  a deep forest: tall trunks receding in two depths, dappled light, vines
  b  a sunlit canopy: a thick roof of leaves, hanging leaves, shafts of light
  c  a misty pine forest: layered silhouettes fading into the mist
  d  birches and beeches: white trunks, round crowns, a bright sky
The horizon is row 96 (src/tuning.h HORIZON_Y); below it the crowns of the forest far below fade into the mist.
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE.
"""
import math
import random

import house_style as hs

W = H = 256
HZ = 96
LIGHT = hs.LIGHT_DIR


def blank():
    return [[0] * W for _ in range(H)]


PERIOD = 128                     # every layer repeats every 128 px (half the tiles)


def put(T, x, y, c):
    if 0 <= y < H:
        T[y][x % PERIOD] = c
        T[y][x % PERIOD + PERIOD] = c


def blob(T, cx, cy, rx, ry, ramp, period=PERIOD, speck=True):
    """a shaded leafy blob (lit from the top-left), repeated every `period` pixels"""
    for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
        for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
            dx, dy = (x + 0.5 - cx) / rx, (y + 0.5 - cy) / ry
            if dx * dx + dy * dy > 1:
                continue
            nd = dx * LIGHT[0] + dy * LIGHT[1]
            c = ramp[0] if nd > 0.55 else ramp[1] if nd > 0.0 else ramp[2] if nd > -0.5 else ramp[3]
            if speck and (x * 7 + y * 3) % 13 == 0:
                c = ramp[min(3, ramp.index(c) + 1)]
            for k in range(0, W, period):
                put(T, x % period + k, y, c)


def canopy(T, depth, rng, period=128, n=14, ramp=(1, 2, 3, 4)):
    for y in range(min(6, depth)):
        for x in range(W):
            T[y][x] = 4 if y < 3 else 3
    for _ in range(n):
        blob(T, rng.uniform(0, PERIOD), rng.uniform(4, depth - 8), rng.uniform(7, 15), rng.uniform(6, 11), list(ramp))


def crowns_below(T, rng, y0=HZ + 6, rows=6, period=128):
    for y in range(y0, HZ + 60):
        for x in range(W):
            if y < HZ + 50 or hs.dither_ok(x, y, 16 - (y - HZ - 50) * 2):
                T[y][x] = 10
    for j in range(rows):
        r = 3.5 + 1.4 * j
        yc = y0 + 3 + 7 * j + 0.5 * j * j
        n = max(3, int(round(period / (1.9 * r))))
        for i in range(n):
            bx = (i + 0.5 * (j % 2)) * period / n + (rng.random() - 0.5) * r * 0.6
            by = yc + (rng.random() - 0.5) * 2
            rr = r * (0.85 + 0.3 * rng.random())
            for y in range(int(by - rr) - 1, int(by + rr) + 2):
                if y < HZ + 4 or y >= HZ + 60:
                    continue
                for xx in range(int(bx - rr) - 1, int(bx + rr) + 2):
                    dx, dy = (xx + 0.5 - bx) / rr, (y + 0.5 - by) / (rr * 0.8)
                    if dx * dx + dy * dy > 1:
                        continue
                    if y >= HZ + 50 and not hs.dither_ok(xx, y, 16 - (y - HZ - 50) * 2):
                        continue
                    nd = dx * LIGHT[0] + dy * LIGHT[1]
                    c = 8 if nd > 0.35 else 9 if nd > -0.3 else 10
                    if j < 2:                               # the farthest rows: flat in the mist
                        c = 12 if nd > -0.3 else 9
                    for k in range(0, W, period):
                        T[y][(xx % period) + k] = c


def trunk(T, x0, w, y0, y1, dark=11, mid=10, lit=9, flecks=None, rng=None, marks=None):
    """a vertical trunk, lit on its left edge, bark lines, optional dapples of light or birch marks"""
    for y in range(y0, y1):
        wob = int(round(1.2 * math.sin(y * 2 * math.pi / 64 + x0)))     # (periodic: the tiles repeat)
        for i in range(w):
            x = x0 + i + wob
            f = i / float(max(1, w - 1))
            c = lit if f < 0.18 else dark if f > 0.55 else mid
            if marks is None and ((y % 32) * 3 + i * 7) % 17 == 0 and 0.2 < f < 0.9:
                c = dark
            if marks is not None and marks(x, y, i):
                c = 11
            put(T, x, y, c)
    if flecks and rng:                                     # dapples of light, repeating every 32 rows
        spots = [(rng.randint(0, 31), rng.randint(1, max(1, w - 2))) for _ in range(max(1, w // 4))]
        for y in range(y0 - y0 % 32, y1, 32):
            for fy, fi in spots:
                if y0 <= y + fy < y1 - 1:
                    put(T, x0 + fi, y + fy, flecks), put(T, x0 + fi + 1, y + fy, flecks), put(T, x0 + fi, y + fy + 1, flecks)


def style_a():
    """a deep forest: tall trunks receding, dappled light, vines"""
    rng = random.Random(101)
    far, near = blank(), blank()
    for x0 in (6, 45, 70, 104):                             # the far trunks, misty, thin
        w = rng.randint(3, 7)
        trunk(far, x0 + rng.randint(0, 8), w, 10, HZ + 30, dark=7, mid=6, lit=5)
    for _ in range(5):
        blob(far, rng.uniform(0, PERIOD), rng.uniform(6, 30), rng.uniform(14, 26), rng.uniform(8, 14), [5, 5, 6, 7], PERIOD, False)
    crowns_below(far, rng)
    canopy(near, 40, rng)
    for x0 in (14, 84):                                     # the near trunks, dark, dappled
        trunk(near, x0, rng.randint(14, 22), 30, H, flecks=14, rng=rng)
    for x in (30, 58, 121):                                 # a few vines from the canopy
        if True:
            L = rng.randint(24, 56)
            for y in range(30, 30 + L):
                put(near, x + int(2 * math.sin(y / 7.0)), y, 15 if y % 5 else 2)
    return near, far


def style_b():
    """a sunlit canopy: a roof of leaves, leafy strands hanging from it, shafts of light"""
    rng = random.Random(202)
    far, near = blank(), blank()
    for x in range(W):                                      # the far forest's crowns on the horizon
        h1 = HZ - 14 - int(8 * abs(math.sin(x * math.pi / 32)) + 4 * math.sin(x * 2 * math.pi / 64 + 1))
        for y in range(h1, HZ + 8):
            far[y][x] = 5 if y - h1 < 3 else 6 if y < HZ else 7
    crowns_below(far, rng)
    canopy(near, 56, rng, n=22)
    for x in range(W):                                      # shafts of light from the top-left, dithered, in front
        for y in range(36, HZ + 44):
            band = (x - y // 2) % 64
            dens = 1.0 - abs(y - (HZ - 10)) / 70.0          # strongest mid-height, fading up and down
            if band < 9 and near[y][x] == 0 and hs.checker(x, y) and dens > 0.25 and (band > 1 or dens > 0.6):
                near[y][x] = 13 if band < 4 else 12
    for x0 in (9, 36, 62, 89, 114):                         # leafy strands hanging from the canopy
        L = rng.randint(16, 40)
        for y in range(44, 44 + L):
            xx = x0 + int(round(1.4 * math.sin(y / 6.0 + x0)))
            put(near, xx, y, 4)
            if y % 5 == 0 or y == 43 + L:
                blob(near, xx + (2.2 if (y // 5) % 2 else -2.2), y + 0.5, 2.6, 1.7, [1, 14, 2, 3], PERIOD, False)
    return near, far


def pine(T, cx, base, h, w, c, period=PERIOD):
    for y in range(int(base - h), int(base)):
        t = (y - (base - h)) / float(h)
        half = w * t * (0.7 + 0.3 * ((y // 3) % 2))         # layered boughs
        for x in range(int(cx - half), int(cx + half) + 1):
            put(T, x, y, c)


def style_c():
    """a misty pine forest: layered silhouettes fading into the mist"""
    rng = random.Random(303)
    far, near = blank(), blank()
    for x in range(W):                                      # the mist below the horizon
        for y in range(HZ + 4, HZ + 60):
            if hs.dither_ok(x, y, 16 - max(0, (y - HZ - 30))):
                far[y][x] = 12 if y < HZ + 30 else 13
    for i in range(8):                                      # the far rows of pines, in the mist
        pine(far, i * 16 + rng.uniform(0, 8), HZ + 10, rng.uniform(40, 60), rng.uniform(7, 10), 13)
    for i in range(6):
        pine(far, i * 22 + rng.uniform(0, 10), HZ + 22, rng.uniform(50, 75), rng.uniform(9, 13), 12)
    for i in range(4):                                      # the near pines, dark
        pine(near, i * 32 + rng.uniform(0, 12), HZ + 70, rng.uniform(90, 130), rng.uniform(12, 18), 7)
    for i in range(3):
        pine(near, i * 43 + rng.uniform(0, 20), H, rng.uniform(110, 150), rng.uniform(16, 24), 10)
    for x in range(W):                                      # a fringe of needles at the top
        for y in range(0, 10 + int(4 * math.sin(x / 9.0))):
            near[y][x] = 4 if (x + y) % 3 else 3
    return near, far


def style_d():
    """birches and beeches: white trunks, round crowns, a bright sky"""
    rng = random.Random(404)
    far, near = blank(), blank()
    for _ in range(9):                                      # beech crowns on the horizon
        blob(far, rng.uniform(0, PERIOD), HZ - rng.uniform(6, 26), rng.uniform(12, 22), rng.uniform(9, 15), [1, 14, 2, 3],
             W, True)
    for x in range(W):
        for y in range(HZ - 4, HZ + 8):
            if far[y][x] == 0:
                far[y][x] = 6
    crowns_below(far, rng)
    canopy(near, 26, rng, n=10, ramp=(1, 14, 2, 3))

    dash = {}                                               # birch bark: short dark dashes across, irregular
    for y in range(64):
        if rng.random() < 0.2:
            dash[y] = (rng.uniform(-0.1, 0.6), rng.uniform(0.25, 0.6))

    def marks(x, y, i):
        for yy in (y % 64, (y - 1) % 64):                  # 2 px tall
            d = dash.get(yy)
            if d and d[0] <= i / 9.0 <= d[0] + d[1]:
                return True
        return False
    for x0 in (20, 66, 104):                                # white birch trunks
        trunk(near, x0, rng.randint(6, 11), 18, H, dark=13, mid=12, lit=12, marks=marks)
    return near, far


STYLES = {"a": style_a, "b": style_b, "c": style_c, "d": style_d}

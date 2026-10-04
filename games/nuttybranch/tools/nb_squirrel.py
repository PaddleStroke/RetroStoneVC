#!/usr/bin/env python3
"""Nutty Branch: the squirrel, seen from behind (code-drawn, house style): the body in its lean angles, the run,
the jump and the fall, the bushy tail in its swing angles, the face icon.
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE.
"""
import math
import random

import house_style as hs
from nb_pal import FUR, HERO

Canvas = hs.Canvas


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

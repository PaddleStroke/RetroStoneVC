#!/usr/bin/env python3
"""Leady Squid: the code-drawn PLACEHOLDER art, in the final sheet format.

Clean NES/SNES style (the one of Bomber Mole's code-drawn art): flat shades,
2-4 per material, a 1-px dark outline, light from the top-left, readable at
1x. The squid is drawn in its own body frame and rasterised at each of its six
angles (SNES games pre-render their rotations too), so every angle stays crisp.

Writes, in the art folder (default games/leadysquid/art):
  sprites.png, obstacles.png, props.png, title_logo.png   (tools/ls_sheets.py)
  tiles/seabed.png, tiles/backdrop.png, tiles/midground.png (code-drawn BG panoramas)

    python3 tools/make_placeholders.py --game leadysquid [--out DIR] [--preview]
    python3 games/leadysquid/tools/make_art.py [--out DIR] [--preview]

All rights reserved, 8BCraft.
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
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "sdk", "tools"))
import ls_sheets  # noqa: E402
from gen_font import G as FONT  # noqa: E402

MAG = ls_sheets.MAGENTA


# ---- a tiny pixel canvas ------------------------------------------------------------------
class Canvas:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.p = [[None] * w for _ in range(h)]

    def set(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.p[y][x] = c

    def get(self, x, y):
        return self.p[y][x] if 0 <= x < self.w and 0 <= y < self.h else None

    def rect(self, x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.set(x, y, c)

    def outline(self, c, diag=False):
        """1-px outline outside the drawing (transparent pixels touching a filled one)."""
        add = []
        for y in range(self.h):
            for x in range(self.w):
                if self.p[y][x] is not None:
                    continue
                nb = [(1, 0), (-1, 0), (0, 1), (0, -1)] + ([(1, 1), (-1, -1), (1, -1), (-1, 1)] if diag else [])
                if any(self.get(x + dx, y + dy) not in (None, c) for dx, dy in nb):
                    add.append((x, y))
        for x, y in add:
            self.p[y][x] = c

    def image(self):
        im = Image.new("RGB", (self.w, self.h), MAG)
        px = im.load()
        for y in range(self.h):
            for x in range(self.w):
                if self.p[y][x] is not None:
                    px[x, y] = self.p[y][x]
        return im

    def paste(self, other, ox, oy):
        for y in range(other.h):
            for x in range(other.w):
                if other.p[y][x] is not None:
                    self.set(ox + x, oy + y, other.p[y][x])


# ---- the squid --------------------------------------------------------------------------------
SQ = dict(out=(40, 16, 58), dark=(100, 46, 138), mid=(150, 82, 194), light=(194, 134, 230),
          hi=(234, 200, 250), spot=(222, 110, 176), white=(250, 250, 250), pupil=(18, 8, 26),
          lid=(118, 58, 152), ldark=(66, 70, 84), lmid=(118, 124, 140), llight=(178, 184, 198),
          buckle=(236, 198, 76))


def seg_dist(px, py, ax, ay, bx, by):
    vx, vy = bx - ax, by - ay
    L = vx * vx + vy * vy
    t = 0 if L == 0 else max(0.0, min(1.0, ((px - ax) * vx + (py - ay) * vy) / L))
    dx, dy = px - (ax + t * vx), py - (ay + t * vy)
    return math.hypot(dx, dy), t


def body_hv(u, squeeze=1.0):
    """Half-height of the body (head + mantle) at u (local x, nose = +u)."""
    u0, a = -0.5, 8.6
    t = (u - u0) / a
    if abs(t) >= 1:
        return -1
    return 6.4 * squeeze * math.sqrt(1 - t * t) * (1 - 0.33 * max(t, 0) ** 1.5)


def tentacles(pose, frame):
    """Polylines (list of points in local coords) and thickness of the tentacles."""
    out = []
    roots = [-3.9, -1.3, 1.3, 3.9]
    for i, rv in enumerate(roots):
        if pose == "squeeze":
            spread, length, amp = 0.55, 6.0, 0.5
        elif pose == "jet":
            spread, length, amp = 2.0, 8.0, 0.4
        elif pose == "recover":
            spread, length, amp = 1.4, 7.5, 0.8
        elif pose in ("hit", "dead"):
            spread, length, amp = 1.7, 7.0, 1.3
        else:
            spread, length, amp = 1.25, 8.0, 0.9
        pts = []
        n = 6
        for k in range(n + 1):
            s = k / float(n)
            u = -7.4 - s * length
            ph = frame * 1.7 + i * 1.3
            v = rv * (1 + (spread - 1) * s) + amp * math.sin(ph + s * 3.0) * s
            if pose == "idle":
                v += 0.9 * math.sin(frame * math.pi + i) * s * s
            pts.append((u, v))
        # curled tip
        lu, lv = pts[-1]
        pts.append((lu - 0.6, lv + (0.9 if rv > 0 else -0.9) * (1 if i % 2 == 0 else -0.6)))
        out.append(pts)
    return out


def squid_frame(angle_deg, pose="swim", frame=0, eyes="open"):
    """One 32x32 frame; the hitbox centre is the cell centre (16, 16)."""
    C = SQ
    cv = Canvas(32, 32)
    th = math.radians(angle_deg)
    ca, sa = math.cos(th), math.sin(th)
    squeeze = {"squeeze": 0.8, "jet": 0.92, "recover": 0.97}.get(pose, 1.0)
    stretch = {"squeeze": 1.1, "jet": 1.04}.get(pose, 1.0)
    tents = tentacles(pose, frame)
    light_dir = (-0.62, -0.78)      # top-left, screen space

    def local(px, py):
        x, y = px + 0.5 - 16, py + 0.5 - 16
        # screen = R * local with R = [[c, s], [-s, c]] (y down, + angle = nose up)
        u = x * ca - y * sa
        v = x * sa + y * ca
        return u / stretch if u > 0 else u, v

    for py in range(32):
        for px in range(32):
            u, v = local(px, py)
            col = None
            sx, sy = px + 0.5 - 16, py + 0.5 - 16
            # tentacles (behind the body): thin, alternating shades, a dark line between them
            best, near = None, 9.0
            for ti, pts in enumerate(tents):
                for a, b in zip(pts, pts[1:]):
                    d, t = seg_dist(u, v, a[0], a[1], b[0], b[1])
                    if d < near:
                        near, best = d, ti
            if near < 0.95:
                col = [C["light"], C["mid"], C["mid"], C["dark"]][best] if angle_deg > -30 else \
                    [C["mid"], C["light"], C["mid"], C["dark"]][best]
            elif near < 1.6:
                col = C["out"]
            hv = body_hv(u, squeeze)
            fin_hit = False
            # fins at the tip of the mantle (top and bottom)
            for sgn in (-1, 1):
                # triangle (3.6, sgn*4.2) (8.8, sgn*7.4) (8.4, sgn*1.0)
                ax, ay, bx, by, cx2, cy2 = 3.4, sgn * 4.0 * squeeze, 8.6, sgn * 7.3 * squeeze, 8.3, sgn * 0.8
                d1 = (u - bx) * (ay - by) - (ax - bx) * (v - by)
                d2 = (u - cx2) * (by - cy2) - (bx - cx2) * (v - cy2)
                d3 = (u - ax) * (cy2 - ay) - (cx2 - ax) * (v - ay)
                neg = (d1 < 0) or (d2 < 0) or (d3 < 0)
                pos = (d1 > 0) or (d2 > 0) or (d3 > 0)
                if not (neg and pos):
                    fin_hit = True
            if fin_hit and not (hv > 0 and abs(v) <= hv):
                col = C["dark"] if (sx * light_dir[0] + sy * light_dir[1]) < -2 else C["mid"]
            if hv > 0 and abs(v) <= hv:
                # shade in screen space: light from the top-left
                nd = (sx * light_dir[0] + sy * light_dir[1]) / 7.0
                col = C["hi"] if nd > 0.62 else C["light"] if nd > 0.18 else C["mid"] if nd > -0.45 else C["dark"]
                # spots on the mantle
                for su, sv in ((4.2, -2.6), (6.3, -0.4), (2.4, 2.9)):
                    if (u - su) ** 2 + (v - sv) ** 2 < 0.75:
                        col = C["spot"]
                # the weight belt
                if 0.7 <= u <= 2.5:
                    col = C["ldark"]
                    if -1.0 <= v <= 1.0 and 1.0 <= u <= 2.2:
                        col = C["buckle"]
                # eye and grumpy lid
                eu, ev, er = -4.4, -1.4, 2.35
                if (u - eu) ** 2 + (v - ev) ** 2 <= er * er:
                    if eyes == "x":
                        du, dv = u - eu, v - ev
                        col = C["pupil"] if abs(abs(du) - abs(dv)) < 0.7 else C["white"]
                    else:
                        lid = -2.1 + (u - eu) * 0.5
                        if v < lid:
                            col = C["lid"]
                        elif v < lid + 0.7:
                            col = C["out"]
                        else:
                            col = C["pupil"] if (u + 3.7) ** 2 + (v + 0.3) ** 2 < 1.3 else C["white"]
                # the pout
                if abs(u + 7.2) < 0.6 and 1.2 < v < 2.6:
                    col = C["out"]
            # lead weights on the belt: blocks sticking out above and below the mantle
            for sgn in (-1, 1):
                edge = body_hv(1.6, squeeze)
                if 0.1 <= u <= 3.1 and edge - 1.2 <= v * sgn <= edge + 2.2:
                    shade = (sx * light_dir[0] + sy * light_dir[1])
                    col = C["llight"] if (v * sgn < edge + 0.2 and shade > 0) else C["lmid"]
                    if abs(u - 3.1) < 0.5 or v * sgn > edge + 1.7:
                        col = C["ldark"]
            if col:
                cv.set(px, py, col)
    cv.outline(C["out"])
    return cv


# ---- effects, UI ------------------------------------------------------------------------------
FX = dict(ring=(196, 238, 255), fill=(112, 184, 226), white=(255, 255, 255), ink=(42, 26, 62),
          inkm=(84, 58, 114), inkl=(134, 104, 164), ldark=(60, 64, 76), lmid=(114, 120, 134),
          llight=(172, 178, 192), spark=(255, 238, 150))


def bubble(frame):
    cv = Canvas(8, 8)
    r = [1.3, 2.2, 3.2, 3.2][frame]
    for y in range(8):
        for x in range(8):
            d = math.hypot(x + 0.5 - 4, y + 0.5 - 4)
            if frame == 3:
                if abs(d - 3.2) < 0.6 and (x + y) % 2 == 0:
                    cv.set(x, y, FX["ring"])
            elif abs(d - r) < 0.65 or (r < 1.5 and d < r):
                cv.set(x, y, FX["ring"])
    if frame < 3:
        hx = int(4 - r * 0.5)
        cv.set(hx, hx, FX["white"])
    return cv


def ink(frame):
    rng = random.Random(40 + frame)
    cv = Canvas(16, 16)
    n = [26, 22, 15, 8][frame]
    blobs = [(8 + rng.uniform(-3, 3), 8 + rng.uniform(-3, 3), rng.uniform(1.2, 2.8)) for _ in range(6)]
    for y in range(16):
        for x in range(16):
            for bx, by, br in blobs:
                rr = br * (1 + frame * 0.35)
                if (x - bx) ** 2 + (y - by) ** 2 < rr * rr:
                    if frame >= 2 and (x + y + frame) % 2:
                        continue
                    c = FX["ink"] if frame == 0 else FX["inkm"] if frame < 3 else FX["inkl"]
                    if (x - bx) + (y - by) < -rr * 0.6:
                        c = FX["inkl"] if frame < 2 else c
                    cv.set(x, y, c)
    for _ in range(max(1, n // 8)):
        bx, by = rng.randint(2, 13), rng.randint(2, 13)
        cv.set(bx, by, FX["ring"])
    return cv


def weight(frame):
    cv = Canvas(8, 8)
    if frame == 0:
        cv.rect(1, 3, 6, 6, FX["lmid"])
        cv.rect(1, 3, 6, 3, FX["llight"])
        cv.rect(1, 6, 6, 6, FX["ldark"])
        cv.set(3, 4, FX["ldark"]), cv.set(4, 4, FX["ldark"])
    else:
        for y in range(8):
            for x in range(8):
                u, v = (x - 3.5) * 0.8 + (y - 3.5) * 0.6, -(x - 3.5) * 0.6 + (y - 3.5) * 0.8
                if abs(u) < 2.7 and abs(v) < 1.8:
                    cv.set(x, y, FX["llight"] if v < -0.8 else FX["lmid"] if v < 0.9 else FX["ldark"])
    cv.outline((24, 22, 30))
    return cv


def sparkle(frame):
    cv = Canvas(8, 8)
    r = 3 if frame == 0 else 2
    for i in range(-r, r + 1):
        cv.set(4 + i, 4, FX["spark"])
        cv.set(4, 4 + i, FX["spark"])
    cv.set(4, 4, FX["white"])
    if frame == 1:
        for d in (-1, 1):
            cv.set(4 + d, 4 + d, FX["spark"]), cv.set(4 + d, 4 - d, FX["spark"])
    return cv


UI = dict(out=(22, 18, 40), white=(250, 250, 250), shade=(168, 200, 232), bd=(138, 76, 38), bl=(210, 142, 82),
          sd=(128, 138, 156), sl=(222, 230, 238), gd=(186, 128, 22), gl=(252, 216, 72), pd=(200, 138, 170),
          pl=(242, 202, 214), pearl=(255, 250, 238))


def digit(n):
    cv = Canvas(16, 16)
    g = FONT[str(n)]
    for y, row in enumerate(g[:7]):
        for x, c in enumerate(row):
            if c == "#":
                for sy in range(2):
                    for sx in range(2):
                        cv.set(2 + x * 2 + sx, 1 + y * 2 + sy, UI["shade"] if y >= 5 else UI["white"])
    cv.outline(UI["out"], diag=True)
    return cv


def shell(kind):
    cv = Canvas(24, 24)
    dark, light = [(UI["bd"], UI["bl"]), (UI["sd"], UI["sl"]), (UI["gd"], UI["gl"]), (UI["pd"], UI["pl"])][kind]
    cx, cy = 12, 17
    for y in range(24):
        for x in range(24):
            dx, dy = x + 0.5 - cx, y + 0.5 - cy
            r = math.hypot(dx, dy)
            ang = math.atan2(-dy, dx)
            if 0.08 < ang < math.pi - 0.08 and r < 10.5 - 0.8 * abs(math.cos(ang * 7)):
                rib = int((ang / math.pi) * 7 + 0.5)
                edge = abs((ang / math.pi) * 7 - rib) < 0.16
                col = dark if edge or r > 9.3 else light
                if dx + dy < -6 and not edge:
                    col = UI["white"] if r < 7 else light
                cv.set(x, y, col)
            # the hinge
            if 16 <= y <= 19 and abs(dx) < 3.2:
                cv.set(x, y, dark)
    if kind == 3:                            # the pearl
        for y in range(24):
            for x in range(24):
                if math.hypot(x + 0.5 - 12, y + 0.5 - 13) < 3.3:
                    cv.set(x, y, UI["pearl"] if (x + y) > 22 else UI["white"])
        cv.set(11, 12, UI["white"])
    cv.outline(UI["out"])
    return cv


def hint(frame):
    cv = Canvas(16, 16)
    oy = 1 if frame else 0
    for y in range(16):
        for x in range(16):
            d = math.hypot(x + 0.5 - 8, y + 0.5 - 7.5 - oy)
            if d < 6.2:
                cv.set(x, y, UI["sl"] if (x + y) < 14 else UI["sd"])
            elif d < 7.2 and y > 8 and not frame:
                cv.set(x, y, UI["sd"])
    g = FONT["A"]
    for y, row in enumerate(g[:7]):
        for x, c in enumerate(row):
            if c == "#":
                cv.set(6 + x, 4 + y + oy, UI["out"])
    cv.outline(UI["out"])
    return cv


# ---- obstacles (bodies are tiles, caps are sprites; one palette per theme) ----------------------
TH = {
    "kelp": dict(out=(22, 40, 18), d=(52, 86, 30), m=(92, 134, 46), l=(146, 182, 70), h=(198, 220, 108),
                 b=(176, 150, 52), bl=(228, 204, 104)),
    "coral": dict(out=(74, 18, 34), d=(160, 50, 74), m=(222, 96, 106), l=(248, 150, 136), h=(255, 206, 176),
                  b=(236, 136, 56), bl=(255, 190, 108)),
    "masts": dict(out=(34, 20, 12), d=(86, 52, 28), m=(130, 86, 46), l=(174, 122, 70), h=(212, 162, 102),
                  b=(62, 66, 76), bl=(132, 138, 152), cl=(204, 192, 160), cd=(156, 144, 116)),
    "chains": dict(out=(34, 18, 16), d=(100, 52, 36), m=(152, 84, 52), l=(198, 124, 74), h=(232, 172, 112),
                   b=(58, 62, 72), bl=(104, 110, 124), il=(160, 166, 180)),
}


def shade3(P, x, y, w, x0=0):
    """Cylinder shading across a column: light on the left, dark on the right."""
    f = (x - x0) / float(max(1, w - 1))
    return P["h"] if f < 0.12 else P["l"] if f < 0.4 else P["m"] if f < 0.78 else P["d"]


def kelp_body(v):
    P = TH["kelp"]
    cv = Canvas(24, 16)
    for y in range(16):
        for k, (x0, ph) in enumerate(((2, 0.0), (9, 2.1), (16, 4.0))):
            off = int(round(1.2 * math.sin((y + v * 8) * math.pi / 8 + ph)))
            for x in range(x0 + off, x0 + off + 6):
                cv.set(x, y, shade3(P, x, y, 6, x0 + off))
    # leaf blades filling the sides
    for (lx, ly, dirx) in ((0, 3 + v * 6, 1), (18, 9 - v * 5, -1), (7, 12 - v * 7, 1)):
        for i in range(6):
            yy = (ly + i) % 16
            for j in range(3 - abs(i - 3) // 2):
                cv.set(lx + (j if dirx > 0 else -j) + (i // 2) * dirx, yy, P["l"] if j == 0 else P["m"])
    cv.set(12, (5 + v * 8) % 16, P["b"]), cv.set(12, (6 + v * 8) % 16, P["bl"])
    return cv


def kelp_cap(top):
    """A crown of kelp blades with air bladders, drawn pointing up (flipped for the top part):
    the lower half continues the stalks of the body, the blades fill the 24-px column."""
    P = TH["kelp"]
    up = Canvas(24, 16)
    body = kelp_body(0)
    for y in range(9, 16):                               # the stalks, as in the body
        for x in range(24):
            c = body.get(x, y)
            if c is not None:
                up.set(x, y, c)
    blades = [(4, 10, 0, 1), (8, 9, 5, 0), (12, 9, 12, 0), (16, 9, 19, 0), (20, 10, 23, 2)]
    for bx, by, tx, ty in blades:                        # lens-shaped blades, a dark midrib
        n = 24
        for i in range(n + 1):
            s = i / float(n)
            x = bx + (tx - bx) * s
            y = by + (ty - by) * s
            w = 2.2 * math.sin(math.pi * min(1.0, s * 1.1))
            for k in range(-3, 4):
                if abs(k) <= w:
                    xx = int(round(x + k))
                    col = P["h"] if k < -0.8 * w + 0.5 else P["l"] if k < 0 else P["m"]
                    if abs(k) < 0.5 and 0.2 < s < 0.85:
                        col = P["d"]
                    up.set(xx, int(round(y)), col)
    for bx, by in ((6, 10), (12, 10), (18, 10)):        # air bladders at the blade bases
        up.rect(bx, by, bx + 2, by + 2, P["b"])
        up.set(bx, by, P["bl"])
        up.set(bx + 2, by + 2, P["d"])
    up.outline(P["out"])
    if not top:
        return up
    cv = Canvas(24, 16)
    for y in range(16):
        for x in range(24):
            cv.set(x, y, up.get(x, 15 - y))
    return cv


def coral_body(v):
    """A pillar of branching coral: rounded knobs on the edges, grooves, polyps."""
    P = TH["coral"]
    cv = Canvas(24, 16)
    for y in range(16):
        # the edge wobbles with knobs every 8 lines (the tile repeats every 16)
        k = abs(((y + v * 4) % 8) - 4) / 4.0
        x0, x1 = 1 + int(round(k * 1.5)), 22 - int(round((1 - k) * 1.5))
        for x in range(x0, x1 + 1):
            col = shade3(P, x, y, x1 - x0 + 1, x0)
            if x in (8, 15) and (y + x) % 5 != 0:        # grooves between the branches
                col = P["d"] if x == 15 else P["m"]
            cv.set(x, y, col)
        cv.set(x0 - 1, y, P["out"]), cv.set(x1 + 1, y, P["out"])
    rng = random.Random(7 + v)
    for _ in range(7):                                   # polyps
        x, y = rng.choice((4, 11, 18)) + rng.randint(-1, 1), rng.randint(0, 15)
        cv.set(x, y, P["b"]), cv.set(x + 1, y, P["bl"]), cv.set(x, (y + 1) % 16, P["d"])
    return cv


def coral_cap(top):
    P = TH["coral"]
    cv = Canvas(24, 16)
    for y in range(16):
        for x in range(24):
            yy = y if top else 15 - y
            dx = x + 0.5 - 12
            r = 12 - max(0, yy - 6) * 1.15
            if abs(dx) < r:
                col = shade3(P, x, y, 24)
                if yy > 4 and (int(dx * 1.4 + yy * 0.8) % 4 == 0):   # brain-coral grooves
                    col = P["d"]
                cv.set(x, y, col)
    cv.outline(P["out"])
    return crop_to(cv, 24, 16)


def mast_body(v):
    P = TH["masts"]
    cv = Canvas(24, 16)
    for y in range(16):
        for x in range(6, 18):
            cv.set(x, y, shade3(P, x, y, 12, 6))
        for x in (1, 22):                    # shrouds (ropes) on both sides
            cv.set(x, y, P["cd"])
        if (y + v * 4) % 4 == 0:             # ratlines between the shrouds
            for x in range(1, 23):
                if cv.get(x, y) is None:
                    cv.set(x, y, P["cl"] if x < 6 else P["cd"])
    for y in (4 + v * 6, 5 + v * 6):         # an iron hoop
        for x in range(6, 18):
            cv.set(x, y % 16, P["b"] if y % 2 else P["bl"])
    return cv


def mast_cap(top):
    P = TH["masts"]
    cv = Canvas(24, 16)
    if top:        # the yard with a torn sail hanging from it
        cv.rect(0, 0, 23, 3, P["m"])
        cv.rect(0, 0, 23, 0, P["h"])
        cv.rect(0, 3, 23, 3, P["d"])
        for x in range(2, 22):
            hang = 13 - (3 if x % 7 < 2 else 0) - (2 if x % 5 == 0 else 0)
            for y in range(4, hang):
                cv.set(x, y, P["cl"] if x < 12 else P["cd"])
        cv.rect(6, 0, 17, 1, P["l"])
    else:          # the crow's nest
        cv.rect(1, 3, 22, 14, P["m"])
        for y in range(3, 15):
            for x in range(1, 23):
                cv.set(x, y, shade3(P, x, y, 22, 1))
        cv.rect(0, 2, 23, 3, P["b"])
        cv.rect(0, 2, 23, 2, P["bl"])
        cv.rect(1, 9, 22, 9, P["b"])
        cv.rect(6, 15, 17, 15, P["d"])
    cv.outline(P["out"])
    return crop_to(cv, 24, 16)


def link(cv, P, x0, y0, face):
    if face:     # a ring seen from the front: 10 wide, 8 tall, hollow
        for y in range(8):
            for x in range(10):
                dx, dy = x + 0.5 - 5, y + 0.5 - 4
                d = (dx / 5.0) ** 2 + (dy / 4.0) ** 2
                if 0.35 < d <= 1.0:
                    cv.set(x0 + x, y0 + y, P["l"] if dx + dy < -1 else P["m"] if dx + dy < 2 else P["d"])
    else:        # seen from the side: a bar
        for y in range(8):
            for x in range(3, 7):
                cv.set(x0 + x, y0 + y, P["l"] if x == 3 else P["m"] if x < 6 else P["d"])


def chain_body(v):
    P = TH["chains"]
    cv = Canvas(24, 16)
    for k, x0 in enumerate((1, 13)):
        for i in range(2):
            link(cv, P, x0, i * 8, (i + k + v) % 2 == 0)
    # rust flakes and a middle strut, so the column reads as one
    for y in range(0, 16, 4):
        cv.set(11, y + (v % 2), P["d"]), cv.set(12, y + 1, P["m"])
    return cv


def chain_cap(top):
    P = TH["chains"]
    cv = Canvas(24, 16)
    if top:        # the hanging anchor
        cv.rect(10, 0, 13, 11, P["bl"])
        cv.rect(10, 0, 10, 11, P["il"])
        cv.rect(4, 2, 19, 3, P["bl"])            # the stock
        cv.rect(4, 2, 19, 2, P["il"])
        for x in range(24):                       # the arms and flukes, curving up
            dx = x + 0.5 - 12
            y = int(round(14.5 - 0.022 * dx * dx * 1.6))
            for yy in range(y - 1, min(16, y + 2)):
                cv.set(x, yy, P["b"] if yy > y else P["bl"])
            if abs(dx) > 9:
                for yy in range(y - 4, y):
                    cv.set(x, yy, P["bl"] if dx < 0 else P["b"])
        cv.set(11, 12, P["b"]), cv.set(12, 13, P["b"])
    else:          # the mooring buoy
        for y in range(16):
            for x in range(24):
                d = math.hypot((x + 0.5 - 12) / 11.5, (y + 0.5 - 9) / 7.5)
                if d <= 1.0:
                    col = P["h"] if d < 0.45 and x < 11 and y < 8 else shade3(P, x, y, 24)
                    cv.set(x, y, col)
        for x in (4, 12, 20):
            cv.set(x, 9, P["d"])                  # rivets
        cv.rect(0, 9, 23, 9, P["d"])
        cv.rect(9, 14, 14, 15, P["b"])            # the shackle into the chain
    cv.outline(P["out"])
    return crop_to(cv, 24, 16)


def crop_to(cv, w, h):
    return cv       # the outline stays inside the cell (drawings keep a 1-px margin where needed)


OBST = {"kelp": (kelp_body, kelp_cap), "coral": (coral_body, coral_cap), "masts": (mast_body, mast_cap),
        "chains": (chain_body, chain_cap)}


# ---- mid-ground props (BG3, muted, one palette) -------------------------------------------------------
MIDP = dict(out=(18, 36, 58), d=(30, 58, 86), m=(44, 80, 110), l=(62, 104, 132), h=(86, 130, 154),
            wd=(78, 60, 56), wl=(112, 88, 70), gold=(206, 174, 78), cd=(140, 88, 116), cl=(176, 118, 140))


def prop(name):
    P = MIDP
    e = ls_sheets.entry(name)
    cv = Canvas(e.w, e.h)
    W, H = e.w, e.h
    rng = random.Random(name)
    if name == "shipwreck":
        # a hull half sunk in the sand, bow raised on the right, stern cut square on the left
        def deck(x):
            return 30 - 10 * (x / 128.0) ** 2.2 - (4 if x > 104 else 0) * (x - 104) / 24.0
        def keel(x):
            return 60 - 8 * ((x - 70) / 70.0) ** 2
        for x in range(4, 126):
            d, k = deck(x), keel(x)
            for y in range(int(d), min(H, int(k) + 1)):
                rel = (y - d) / max(1.0, k - d)
                col = P["wl"] if rel < 0.12 else P["wd"]
                if int(y - d) % 5 == 4:
                    col = P["d"]                       # plank seams follow the deck
                if 26 <= x <= 44 and 0.35 < rel < 0.8 and (x - 26) + (y - d) * 0.5 < 22:
                    col = None                         # a broken hole
                if col:
                    cv.set(x, y, col)
            cv.set(x, int(d) - 1, P["wl"])             # the rail
            if x % 9 == 0:
                for y in range(int(d) - 4, int(d)):
                    cv.set(x, y, P["wd"])              # rail posts
        for i in range(44):                            # the broken mast, leaning
            x, y = 72 + i * 0.45, 22 - i
            for k in range(3):
                cv.set(int(x) + k, int(y), P["wl"] if k == 0 else P["wd"])
        for x in range(74, 100):                       # a yard, hanging
            cv.set(x, 6 + (x - 74) // 4, P["wd"])
            cv.set(x, 7 + (x - 74) // 4, P["d"])
        for (px, py) in ((58, 38), (74, 37), (90, 36)):   # portholes
            cv.rect(px, py, px + 3, py + 3, P["d"])
            cv.set(px + 1, py + 1, P["h"])
    elif name == "chest":
        cv.rect(2, 10, 29, 23, P["wd"])
        cv.rect(2, 10, 29, 11, P["wl"])
        for y in range(2, 10):                       # lid ajar
            for x in range(2 + (10 - y) // 3, 30):
                cv.set(x, y, P["wl"] if y < 5 else P["wd"])
        cv.rect(4, 9, 27, 10, P["gold"])             # the glint of gold
        cv.rect(14, 12, 17, 16, P["gold"])
        for x in (8, 22):
            cv.rect(x, 10, x + 1, 23, P["d"])
    elif name in ("rock_small", "rock_big"):
        for y in range(H):
            for x in range(W):
                dx, dy = (x + 0.5 - W / 2) / (W / 2.0), (y + 0.5 - H) / float(H)
                if dx * dx + dy * dy * 1.05 <= 1:
                    nd = -dx * 0.6 - (dy + 0.5) * 0.8
                    cv.set(x, y, P["h"] if nd > 0.55 else P["l"] if nd > 0.1 else P["m"] if nd > -0.4 else P["d"])
        for _ in range(W // 6):                      # barnacles
            x, y = rng.randint(3, W - 4), rng.randint(H // 3, H - 3)
            if cv.get(x, y):
                cv.set(x, y, P["h"])
    elif name == "coral_clump":
        def branch(x, y, ang, n):
            for i in range(n):
                x2, y2 = x + math.cos(ang) * 1.0, y - math.sin(ang) * 1.0
                cv.set(int(x2), int(y2), P["cl"] if i < n // 2 else P["cd"])
                cv.set(int(x2) + 1, int(y2), P["cd"])
                x, y = x2, y2
                ang += rng.uniform(-0.25, 0.25)
            if n > 6:
                branch(x, y, ang + 0.6, n // 2)
                branch(x, y, ang - 0.6, n // 2)
        branch(16, 31, math.pi / 2, 16)
        branch(10, 31, math.pi / 2 + 0.5, 11)
        branch(22, 31, math.pi / 2 - 0.5, 11)
    elif name == "kelp_clump":
        for y in range(H):
            off = int(round(2 * math.sin(y / 6.0)))
            for x in range(6 + off, 10 + off):
                cv.set(x, y, P["l"] if x == 6 + off else P["m"])
            if y % 7 == 3:
                for x in range(1 + off, 6 + off):
                    cv.set(x, y, P["m"])
            if y % 9 == 5:
                for x in range(10 + off, 15 + off):
                    cv.set(x, y, P["d"])
    elif name == "amphora":
        for y in range(H):
            for x in range(W):
                dx = x + 0.5 - 8
                half = 2 if y < 4 else 6 * math.sin(math.pi * (y - 2) / 22.0) + 1
                if abs(dx) < half:
                    cv.set(x, y, P["wl"] if dx < -1 else P["wd"])
        cv.rect(4, 3, 11, 4, P["wd"])
    cv.outline(P["out"])
    return cv


def midground(props):
    pano = ls_sheets.PANORAMAS["midground"]
    cv = Canvas(pano["w"], pano["h"])
    for name, x, bottom in ls_sheets.MID_LAYOUT:
        p = props[name]
        cv.paste(p, x, bottom - p.h)
    return cv


# ---- seabed (BG2 front) and the backdrop (BG4: rays + reef) --------------------------------------------
SEA = dict(out=(92, 70, 42), d=(168, 134, 86), m=(208, 176, 120), l=(232, 208, 152), h=(248, 232, 190),
           pd=(96, 100, 108), pm=(150, 154, 162), shell=(238, 170, 162), star=(232, 120, 64),
           gd=(40, 92, 62), gm=(74, 142, 88), gl=(122, 186, 112))


def seabed():
    P = SEA
    W, H = 512, 48
    cv = Canvas(W, H)
    rng = random.Random(3)
    top = [int(round(2 + 1.5 * math.sin(x * 2 * math.pi / 128) + 0.8 * math.sin(x * 2 * math.pi / 32))) for x in range(W)]
    for x in range(W):
        for y in range(top[x], H):
            d = y - top[x]
            col = P["h"] if d == 0 else P["l"] if d < 3 else P["m"]
            # ripples: dark wavy lines
            if d > 4 and (y + int(3 * math.sin((x + y * 3) / 11.0))) % 9 == 0:
                col = P["d"]
            if d > 4 and (y + int(3 * math.sin((x + y * 3) / 11.0))) % 9 == 1:
                col = P["l"]
            cv.set(x, y, col)
        cv.set(x, top[x] - 1, P["out"])
    for _ in range(26):                                 # pebbles
        x, y = rng.randint(2, W - 6), rng.randint(10, H - 5)
        cv.rect(x, y, x + 2, y + 1, P["pm"])
        cv.set(x + 2, y + 1, P["pd"]), cv.set(x, y + 2, P["d"]), cv.set(x + 1, y + 2, P["d"])
    for _ in range(9):                                  # shells
        x, y = rng.randint(4, W - 8), rng.randint(12, H - 6)
        for i in range(4):
            cv.set(x + i, y + (1 if i in (0, 3) else 0), P["shell"])
        cv.set(x + 1, y + 1, P["shell"]), cv.set(x + 2, y + 1, P["out"])
    for x0 in (60, 300, 450):                            # starfish
        y0 = 24 + x0 % 11
        for i in range(-2, 3):
            cv.set(x0 + i, y0, P["star"]), cv.set(x0, y0 + i, P["star"])
        cv.set(x0 - 2, y0 + 2, P["star"]), cv.set(x0 + 2, y0 + 2, P["star"])
    for x0 in range(10, W, 37):                          # sea-grass tufts along the top
        x0 += rng.randint(0, 12)
        for b in range(3):
            hgt = rng.randint(4, 8)
            for i in range(hgt):
                x = x0 + b * 2 + (i // 3) * (1 if b == 2 else -1 if b == 0 else 0)
                cv.set(x % W, top[x0 % W] - 1 - i, P["gm"] if i < hgt - 2 else P["gl"])
                cv.set((x + 1) % W, top[x0 % W] - 1 - i, P["gd"])
    return cv


BACK = dict(r1=(12, 22, 26), r2=(20, 34, 40), r3=(30, 50, 56), s1=(52, 88, 96), s2=(96, 146, 156),
            f1=(10, 30, 56), f2=(16, 42, 72), f3=(24, 56, 88), f4=(34, 70, 102))


def backdrop():
    """Rays (lines 0..RAYS_END-1, drawn in ADDED colours) and the far reef (opaque)."""
    P = BACK
    W, H = 512, 192
    re = ls_sheets.RAYS_END
    cv = Canvas(W, H)
    rays = [(40, 18), (120, 10), (190, 24), (290, 14), (360, 20), (450, 12)]
    for y in range(re):
        band = y // 16                                   # widths and shades change every 16 lines,
        fade = 1 - (band * 16) / float(re)               # the slope is 1/2: the tiles repeat
        for cx, w in rays:
            x0 = cx + y // 2
            ww = w + band * 2
            for x in range(x0 - ww // 2, x0 + ww // 2):
                edge = min(x - (x0 - ww // 2), (x0 + ww // 2) - x)
                lvl = (3 if edge > ww // 4 else 2) if fade > 0.55 else (2 if edge > ww // 4 else 1) if fade > 0.25 else 1
                if fade < 0.2 and (x + y) % 2:
                    continue
                cv.set(x % W, y, [None, P["r1"], P["r2"], P["r3"]][lvl])
    for x in range(W):                                   # the surface shimmer
        for y in range(0, 5):
            v = math.sin(x * 2 * math.pi / 24 + y * 1.3) + 0.6 * math.sin(x * 2 * math.pi / 58)
            if y == 0 or (y < 3 and v > 0.4) or (y < 5 and v > 1.2):
                cv.set(x, y, P["s2"] if y == 0 or v > 1.0 else P["s1"])
    # the far reef: soft humps and spires, three tones, darkest at the bottom
    for x in range(W):
        h1 = 150 + int(12 * math.sin(x * 2 * math.pi / 170) + 8 * math.sin(x * 2 * math.pi / 61 + 1))
        spire = 0
        for sx, sh in ((70, 26), (230, 34), (330, 18), (420, 30)):
            d = abs(x - sx)
            if d < 10:
                spire = max(spire, int(sh * (1 - d / 10.0) ** 1.5))
        top = max(re + 4, h1 - spire)
        for y in range(top, H):
            d = y - top
            col = P["f4"] if d < 2 else P["f3"] if d < 10 else P["f2"] if y < 176 else P["f1"]
            cv.set(x, y, col)
    return cv


# ---- title logo (256x64): "LEADY" in lead, "SQUID" in purple ------------------------------------------------
def logo():
    im = Image.new("RGB", (256, 64), MAG)
    px = im.load()
    word1, word2 = "LEADY", "SQUID"
    sc = 4
    lead = [(208, 214, 226), (160, 166, 180), (116, 122, 138)]
    purp = [(236, 196, 252), (190, 120, 228), (138, 70, 180)]
    outc, shadow = (26, 16, 40), (20, 34, 70)
    adv = 5 * sc + 2
    total = (len(word1) + len(word2) + 1) * adv - 2
    x0 = (256 - total) // 2
    y0 = 14
    mask = {}
    for i, ch in enumerate(word1 + " " + word2):
        g = FONT[ch]
        pal = lead if i < len(word1) else purp
        for y, row in enumerate(g[:7]):
            for x, c in enumerate(row):
                if c != "#":
                    continue
                for sy in range(sc):
                    for sx in range(sc):
                        X, Y = x0 + i * adv + x * sc + sx, y0 + y * sc + sy
                        k = 0 if y * sc + sy < 8 else 1 if y * sc + sy < 18 else 2
                        mask[(X, Y)] = pal[k]
    for (X, Y) in mask:                                  # drop shadow
        for d in (3, 4):
            if (X + d // 2, Y + d) not in mask:
                px[X + d // 2, Y + d] = shadow
    for (X, Y), c in mask.items():
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                if (X + dx, Y + dy) not in mask:
                    px[X + dx, Y + dy] = outc
    for (X, Y), c in mask.items():
        px[X, Y] = c
    # rivets on LEADY
    for i in range(len(word1)):
        cx = x0 + i * adv + 2 * sc
        if (cx, y0 + 2) in mask:
            px[cx, y0 + 2] = (250, 250, 250)
    # bubbles
    for bx, by, r in ((20, 50, 3), (232, 12, 4), (240, 44, 2), (12, 18, 2)):
        for y in range(by - r - 1, by + r + 2):
            for x in range(bx - r - 1, bx + r + 2):
                if abs(math.hypot(x - bx, y - by) - r) < 0.6:
                    px[x, y] = (196, 238, 255)
    return im


# ---- sheets ------------------------------------------------------------------------------------------
TILT_ANGLES = [20, 0, -22, -45, -67, -90]


def sprite_frame(e, i):
    n = e.name
    if n == "squid_tilt":
        return squid_frame(TILT_ANGLES[i], "swim", i)
    if n == "squid_idle":
        return squid_frame(0, "idle", i)
    if n == "squid_flap":
        return squid_frame(20, ["squeeze", "jet", "recover"][i], i)
    if n == "squid_hit":
        return squid_frame(0, "hit", 0, eyes="x")
    if n == "squid_sink":
        return squid_frame(-90, "dead", i, eyes="x")
    if n == "squid_rest":
        return squid_frame(-90, "dead", 2, eyes="x")
    if n == "digits":
        return digit(i)
    if n == "bubble":
        return bubble(i)
    if n == "weight":
        return weight(i)
    if n == "sparkle":
        return sparkle(i)
    if n == "ink":
        return ink(i)
    if n == "hint":
        return hint(i)
    if n == "medal":
        return shell(i)
    for th, (body, cap) in OBST.items():
        if n == th + "_body":
            return body(i)
        if n == th + "_cap_top":
            return cap(True)
        if n == th + "_cap_bottom":
            return cap(False)
    raise KeyError(n)


def build_sheet(sheet):
    s = ls_sheets.SHEETS[sheet]
    im = Image.new("RGB", (s["w"], s["h"]), MAG)
    props = {}
    for e in ls_sheets.entries(sheet):
        for i, (x, y, w, h) in enumerate(ls_sheets.frame_rects(e)):
            cv = prop(e.name) if sheet == "props" else sprite_frame(e, i)
            if sheet == "props":
                props[e.name] = cv
            im.paste(cv.image(), (x, y))
    return im


def props_from_sheet(im):
    """The props as canvases, cut from a props sheet (placeholder or imported art)."""
    out = {}
    px = im.convert("RGB").load()
    for e in ls_sheets.entries("props"):
        cv = Canvas(e.w, e.h)
        for y in range(e.h):
            for x in range(e.w):
                c = px[e.x + x, e.y + y]
                if c != MAG:
                    cv.set(x, y, c)
        out[e.name] = cv
    return out


def write_all(out, preview=False):
    os.makedirs(os.path.join(out, "tiles"), exist_ok=True)
    written = []
    for sheet, s in ls_sheets.SHEETS.items():
        im = build_sheet(sheet)
        im.save(os.path.join(out, s["file"]))
        written.append(s["file"])
    logo().save(os.path.join(out, "title_logo.png"))
    written.append("title_logo.png")
    pano = {"seabed": seabed(), "backdrop": backdrop(),
            "midground": midground(props_from_sheet(Image.open(os.path.join(out, "props.png"))))}
    for k, cv in pano.items():
        cv.image().save(os.path.join(out, ls_sheets.PANORAMAS[k]["file"]))
        written.append(ls_sheets.PANORAMAS[k]["file"])
    if preview:
        pdir = os.path.join(out, "preview")
        os.makedirs(pdir, exist_ok=True)
        for f in written:
            im = Image.open(os.path.join(out, f))
            im.resize((im.width * 4, im.height * 4), Image.NEAREST).save(os.path.join(pdir, os.path.basename(f)))
    return written


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", default=os.path.join(GAME, "art"))
    ap.add_argument("--preview", action="store_true", help="also write 4x previews in <out>/preview")
    a = ap.parse_args(argv)
    for f in write_all(a.out, a.preview):
        print("wrote %s/%s" % (a.out, f))


if __name__ == "__main__":
    main()

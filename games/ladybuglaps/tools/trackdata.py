#!/usr/bin/env python3
"""Ladybug Laps: the tracks of src/tracks.c, read for the art (make_art.py draws them, build_assets.py cuts them).

src/tracks.c is the one source of the tracks (the game builds its rules from it); this reads the grids, the start,
the table, the objects, the zones and the leaves, and the objects' sizes, and answers the same geometry questions as
play.c (where the band of a piece is) in floating point.

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/ladybuglaps/LICENSE.
"""
import math
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.dirname(HERE)
SRC = os.path.join(GAME, "src")

CELL = 64
HALF_W = 24
NAMES = ["tea", "desk", "picnic", "veg", "patio"]


def _define(src, name):
    m = re.search(r"#define\s+%s\s+(-?\d+)" % name, src)
    return int(m.group(1))


class Track:
    pass


def load():
    """The tracks of src/tracks.c, in order: a list of Track (grid, gw, gh, sx, sy, sdir, table, rect, objs, zones,
    leaves, theme, name)."""
    src = open(os.path.join(SRC, "tracks.c"), encoding="utf-8").read()
    hdr = open(os.path.join(SRC, "game.h"), encoding="utf-8").read()
    assert _define(hdr, "CELL") == CELL and _define(hdr, "HALF_W") == HALF_W
    kinds = {}
    for m in re.finditer(r"/\*\s*(OB_\w+)\s*\*/\s*\{(SH_\w+),\s*(OE_\w+),\s*(\d+),\s*(\d+)\}", src):
        kinds[m.group(1)] = (m.group(2), m.group(3), int(m.group(4)), int(m.group(5)))
    grids, objs, zones, leaves = {}, {}, {}, {}
    for m in re.finditer(r"static const char \*const (\w+)_grid\[\] = \{(.*?)\};", src, re.S):
        grids[m.group(1)] = re.findall(r'"([^"]*)"', m.group(2))
    for m in re.finditer(r"static const tobj (\w+)_obj\[\] = \{(.*?)\n\};", src, re.S):
        objs[m.group(1)] = [(k, int(v), int(x), int(y)) for k, v, x, y in
                            re.findall(r"\{(OB_\w+),\s*(\d+),\s*(-?\d+),\s*(-?\d+)\}", m.group(2))]
    for m in re.finditer(r"static const tzone (\w+)_zone\[\] = \{(.*?)\n\};", src, re.S):
        zones[m.group(1)] = [(s, sh, int(x), int(y), int(rx), int(ry)) for s, sh, x, y, rx, ry in
                             re.findall(r"\{(SF_\w+),\s*(ZS_\w+),\s*(-?\d+),\s*(-?\d+),\s*(\d+),\s*(\d+)\}", m.group(2))]
    for m in re.finditer(r"static const tleaf (\w+)_leaf\[\] = \{(.*?)\};", src, re.S):
        leaves[m.group(1)] = [(k, int(f), int(x), int(y)) for k, f, x, y in
                              re.findall(r"\{(LF_\w+),\s*(\d+),\s*(-?\d+),\s*(-?\d+)\}", m.group(2))]
    body = re.search(r"const track_def tracks\[TRACKS\] = \{(.*?)\n\};", src, re.S).group(1)
    out = []
    pat = (r"\{(\w+)_grid,\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),"
           r"\s*\w+_obj,\s*N\(\w+\),\s*\w+_zone,\s*N\(\w+\),\s*\w+_leaf,\s*(N\(\w+\)|0),\s*(TH_\w+)\}")
    for m in re.finditer(pat, body):
        t = Track()
        t.name = m.group(1)
        t.grid = grids[t.name]
        t.gw, t.gh, t.sx, t.sy, t.sdir, t.table = [int(m.group(i)) for i in range(2, 8)]
        t.rect = tuple(int(m.group(i)) for i in range(8, 12))
        t.objs = objs[t.name]
        t.zones = zones[t.name]
        t.leaves = leaves[t.name] if m.group(12) != "0" else []
        t.theme = m.group(13)
        t.w, t.h = t.gw * CELL, t.gh * CELL
        assert len(t.grid) == t.gh and all(len(r) == t.gw for r in t.grid), t.name
        out.append(t)
    assert [t.name for t in out] == NAMES, [t.name for t in out]
    return out, kinds


# ---- geometry (play.c on_band, in floating point) ---------------------------------------------------------------------
def cell(t, cx, cy):
    if cx < 0 or cy < 0 or cx >= t.gw or cy >= t.gh:
        return "."
    return t.grid[cy][cx]


def block_of(t, cx, cy):
    for ox, oy in ((cx, cy), (cx - 1, cy), (cx, cy - 1), (cx - 1, cy - 1)):
        c = cell(t, ox, oy)
        if c in "ABCD" and cell(t, ox + 1, oy) == "#" and cell(t, ox, oy + 1) == "#" and cell(t, ox + 1, oy + 1) == "#":
            return c, ox, oy
    return None


def band(t, x, y):
    """(offset from the centre line in px, along: a coordinate along the band) of world pixel (x, y) (its centre), or
    None when it is not in a piece. |offset| <= HALF_W: on the band."""
    px, py = x + 0.5, y + 0.5
    cx, cy = int(px // CELL), int(py // CELL)
    lx, ly = px - cx * CELL, py - cy * CELL
    c = cell(t, cx, cy)
    if c == "-":
        return ly - CELL / 2, px
    if c == "|":
        return lx - CELL / 2, py
    if c == "+":
        a, b = ly - CELL / 2, lx - CELL / 2
        return (a, px) if abs(a) < abs(b) else (b, py)
    if c in "r7LJ":
        ox = CELL if c in "rL" else 0
        oy = CELL if c in "r7" else 0
        d = math.hypot(lx - ox, ly - oy)
        return d - CELL / 2, math.atan2(ly - oy, lx - ox) * CELL / 2
    if c in "ABCD#":
        b = block_of(t, cx, cy)
        if not b:
            return None
        k, ox, oy = b
        ccx = (ox + (2 if k in "AC" else 0)) * CELL
        ccy = (oy + (2 if k in "AB" else 0)) * CELL
        d = math.hypot(px - ccx, py - ccy)
        return d - CELL * 1.5, math.atan2(py - ccy, px - ccx) * CELL * 1.5
    return None


def start_line(t):
    """The start line: its centre (px) and the direction raced (0 E, 1 S, 2 W, 3 N)."""
    e = (t.sdir + 2) % 4
    x = t.sx * CELL + (CELL if e == 0 else 0 if e == 2 else CELL // 2)
    y = t.sy * CELL + (CELL if e == 1 else 0 if e == 3 else CELL // 2)
    return x, y, t.sdir

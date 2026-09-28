#!/usr/bin/env python3
"""RetroStone VC asset tool: PNG sheet -> 4bpp tiles, 16-colour palettes, metatiles.

Library (used by the games' asset builds) and command line:

    rsasset.py SHEET.png --grid 16x16 --mode bg  --name terrain --out terrain.c  [--max-pals 4] [--pal-base 1]
    rsasset.py SHEET.png --grid 16x16 --mode obj --name mole    --out mole.bin

Transparency: alpha < 128 or exact magenta #FF00FF. Colours are snapped to
RGB555. BG mode packs the cells into up to --max-pals palettes of 15 colours
(+ transparent), removes duplicate tiles (also flipped ones) and writes one
metatile (map entries) per cell. OBJ mode keeps each cell as a sprite frame of
consecutive tiles (row-major), all sharing one palette. See docs/assets.md.

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import argparse
import struct
import sys

from PIL import Image

MAGENTA = (255, 0, 255)


# ---- colours ------------------------------------------------------------------
def to555(rgb):
    r, g, b = rgb[:3]
    return ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3)


def rgb888(c):
    r, g, b = c & 31, (c >> 5) & 31, (c >> 10) & 31
    return ((r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2))


def dist2(a, b):
    """Weighted RGB distance (a cheap perceptual approximation)."""
    dr, dg, db = a[0] - b[0], a[1] - b[1], a[2] - b[2]
    return 2 * dr * dr + 4 * dg * dg + 3 * db * db


def nearest(rgb, pal_rgb):
    best, bi = None, 0
    for i, p in enumerate(pal_rgb):
        d = dist2(rgb, p)
        if best is None or d < best:
            best, bi = d, i
    return bi


# ---- loading ------------------------------------------------------------------
def load(path):
    return Image.open(path).convert("RGBA")


def cell(img, x, y, w, h):
    """h rows of w pixels: 555 colour or None (transparent)."""
    out = []
    px = img.load()
    for j in range(h):
        row = []
        for i in range(w):
            r, g, b, a = px[x + i, y + j]
            row.append(None if a < 128 or (r, g, b) == MAGENTA else to555((r, g, b)))
        out.append(row)
    return out


def is_empty(c):
    return all(v is None for row in c for v in row)


def colors_of(c):
    return {v for row in c for v in row if v is not None}


def quantize(cells, n):
    """Reduce the colours of a list of cells to at most n (median cut)."""
    cols = [v for c in cells for row in c for v in row if v is not None]
    if len(set(cols)) <= n:
        return cells
    strip = Image.new("RGB", (len(cols), 1))
    strip.putdata([rgb888(v) for v in cols])
    q = strip.quantize(colors=n, method=Image.Quantize.MEDIANCUT)
    pal = q.getpalette()[:n * 3]
    pal_rgb = [tuple(pal[i * 3:i * 3 + 3]) for i in range(n)]
    pal555 = [to555(p) for p in pal_rgb]
    cache = {}
    out = []
    for c in cells:
        nc = []
        for row in c:
            nr = []
            for v in row:
                if v is None:
                    nr.append(None)
                else:
                    if v not in cache:
                        cache[v] = pal555[nearest(rgb888(v), pal_rgb)]
                    nr.append(cache[v])
            nc.append(nr)
        out.append(nc)
    return out


# ---- tiles ---------------------------------------------------------------------
def split_tiles(c):
    """A w x h cell -> list of 8x8 tiles (row-major), each a list of 64 values."""
    h, w = len(c), len(c[0])
    tiles = []
    for ty in range(0, h, 8):
        for tx in range(0, w, 8):
            tiles.append([c[ty + y][tx + x] for y in range(8) for x in range(8)])
    return tiles


def hflip(t):
    return [t[y * 8 + 7 - x] for y in range(8) for x in range(8)]


def vflip(t):
    return [t[(7 - y) * 8 + x] for y in range(8) for x in range(8)]


def pack4(t):
    """64 indices (0..15) -> 32 bytes, high nibble = left pixel."""
    return bytes(((t[i] & 15) << 4) | (t[i + 1] & 15) for i in range(0, 64, 2))


def rs_map(tile, pal, prio=0, hf=0, vf=0):
    return (tile & 1023) | ((pal & 7) << 10) | ((prio & 1) << 13) | ((hf & 1) << 14) | ((vf & 1) << 15)


def pack_palettes(sets, max_pals):
    """Greedy bin packing of colour sets into palettes of <= 15 colours.
    Returns (palettes, assignment) or None if more than max_pals are needed."""
    order = sorted(range(len(sets)), key=lambda i: -len(sets[i]))
    pals, assign = [], [0] * len(sets)
    for i in order:
        s = sets[i]
        best, bp = None, -1
        for p, pal in enumerate(pals):
            u = len(pal | s)
            if u <= 15 and (best is None or u - len(pal) < best):
                best, bp = u - len(pal), p
        if bp < 0:
            pals.append(set(s))
            bp = len(pals) - 1
        else:
            pals[bp] |= s
        assign[i] = bp
    if len(pals) > max_pals:
        return None
    return [sorted(p) for p in pals], assign


class BGSet:
    """Result of convert_bg: tiles (64 indices each), palettes, metatiles."""

    def __init__(self):
        self.tiles, self.palettes, self.metas = [], [], []


def cluster_cells(cells, k, iters=12):
    """Group cells into k clusters by colour (k-means on each cell's mean and
    spread), then reduce each cluster to 15 colours. Used when the cells'
    colours cannot be packed into k palettes as they are."""
    feats = []
    for c in cells:
        cols = [rgb888(v) for row in c for v in row if v is not None] or [(0, 0, 0)]
        n = float(len(cols))
        mean = [sum(p[i] for p in cols) / n for i in range(3)]
        feats.append(mean)
    k = min(k, len(cells))
    cent = [feats[i * len(feats) // k] for i in range(k)]
    assign = [0] * len(cells)
    for _ in range(iters):
        assign = [min(range(k), key=lambda j: sum((f[i] - cent[j][i]) ** 2 for i in range(3))) for f in feats]
        for j in range(k):
            mem = [feats[i] for i in range(len(feats)) if assign[i] == j]
            if mem:
                cent[j] = [sum(m[i] for m in mem) / len(mem) for i in range(3)]
    out = list(cells)
    for j in range(k):
        idx = [i for i in range(len(cells)) if assign[i] == j]
        red = quantize([cells[i] for i in idx], 15)
        for i, c in zip(idx, red):
            out[i] = c
    return out, assign


def convert_bg(cells, max_pals=8, pal_base=0, tile_base=0, reserve_blank=True):
    """cells: list of w x h cells (w, h multiples of 8). Each becomes a
    metatile: (w/8)*(h/8) map entries (row-major) relative to tile_base."""
    work = [quantize([c], 15)[0] if len(colors_of(c)) > 15 else c for c in cells]
    packed = pack_palettes([colors_of(c) for c in work], max_pals)
    if packed is None:
        print("rsasset: the colours do not fit %d palettes; clustering the cells" % max_pals, file=sys.stderr)
        work, cl = cluster_cells(work, max_pals)
        used = sorted(set(cl))
        pals = [sorted(set().union(*[colors_of(work[i]) for i in range(len(work)) if cl[i] == j])) for j in used]
        packed = (pals, [used.index(j) for j in cl])
    pals, assign = packed
    out = BGSet()
    out.palettes = pals
    index = {}
    if reserve_blank:
        out.tiles.append([0] * 64)
        index[tuple([0] * 64)] = (0, 0, 0)
    for ci, c in enumerate(work):
        p = assign[ci]
        pal = pals[p]
        meta = []
        for t in split_tiles(c):
            idx = [0 if v is None else pal.index(v) + 1 for v in t]
            key = tuple(idx)
            hit = index.get(key)
            if hit is None:
                for hf, vf, f in ((1, 0, hflip), (0, 1, vflip), (1, 1, lambda x: vflip(hflip(x)))):
                    k2 = tuple(f(idx))
                    if k2 in index:
                        base = index[k2]
                        hit = (base[0], hf ^ base[1], vf ^ base[2])
                        break
            if hit is None:
                out.tiles.append(idx)
                hit = (len(out.tiles) - 1, 0, 0)
                index[key] = hit
            meta.append(rs_map(tile_base + hit[0], pal_base + p, 0, hit[1], hit[2]))
        out.metas.append(meta)
    return out


class OBJSet:
    def __init__(self):
        self.tiles, self.palette, self.frames = [], [], []   # frames: (first_tile, w, h)


def convert_obj(frames, palette=None):
    """frames: list of w x h cells sharing one palette (<= 15 colours; reduced
    by median cut otherwise). Each frame -> consecutive tiles, row-major."""
    if palette is None:
        cols = set()
        for c in frames:
            cols |= colors_of(c)
        if len(cols) > 15:
            frames = quantize(frames, 15)
            cols = set()
            for c in frames:
                cols |= colors_of(c)
        palette = sorted(cols)
    out = OBJSet()
    out.palette = palette
    pal_rgb = [rgb888(p) for p in palette]
    for c in frames:
        first = len(out.tiles)
        for t in split_tiles(c):
            idx = []
            for v in t:
                if v is None:
                    idx.append(0)
                elif v in palette:
                    idx.append(palette.index(v) + 1)
                else:
                    idx.append(nearest(rgb888(v), pal_rgb) + 1)
            out.tiles.append(idx)
        out.frames.append((first, len(c[0]), len(c)))
    return out


def palette16(colors):
    """15 colours -> 16 RGB555 entries (entry 0 = transparent/0)."""
    p = [0] + list(colors)
    return (p + [0] * 16)[:16]


# ---- output ---------------------------------------------------------------------
def c_bytes(name, data, static=False):
    s = ["%sconst uint8_t %s[%d] = {" % ("static " if static else "", name, len(data))]
    for i in range(0, len(data), 16):
        s.append("    " + ", ".join("0x%02x" % b for b in data[i:i + 16]) + ",")
    s.append("};")
    return "\n".join(s)


def c_u16(name, data, static=False):
    s = ["%sconst uint16_t %s[%d] = {" % ("static " if static else "", name, len(data))]
    for i in range(0, len(data), 12):
        s.append("    " + ", ".join("0x%04x" % v for v in data[i:i + 12]) + ",")
    s.append("};")
    return "\n".join(s)


def tiles_bytes(tiles):
    return b"".join(pack4(t) for t in tiles)


def write_bin(path, kind, tiles, palettes, table, cw, ch):
    """RSA1 container (little endian), see docs/assets.md."""
    with open(path, "wb") as f:
        f.write(b"RSA1")
        f.write(struct.pack("<BBHHHHH", 0 if kind == "bg" else 1, 0, len(tiles), len(palettes), len(table), cw, ch))
        f.write(tiles_bytes(tiles))
        for p in palettes:
            f.write(struct.pack("<16H", *palette16(p)))
        for e in table:
            f.write(struct.pack("<%dH" % len(e), *e))


def main():
    ap = argparse.ArgumentParser(description="PNG sheet -> RetroStone VC tiles, palettes and metatiles")
    ap.add_argument("png")
    ap.add_argument("--grid", default="16x16", help="cell size WxH (multiples of 8)")
    ap.add_argument("--mode", choices=["bg", "obj"], default="bg")
    ap.add_argument("--name", default="asset")
    ap.add_argument("--out", required=True, help=".c or .bin")
    ap.add_argument("--max-pals", type=int, default=8)
    ap.add_argument("--pal-base", type=int, default=0, help="first palette number (bg map entries)")
    ap.add_argument("--tile-base", type=int, default=0)
    a = ap.parse_args()
    cw, ch = (int(v) for v in a.grid.lower().split("x"))
    if cw % 8 or ch % 8:
        sys.exit("grid must be a multiple of 8")
    img = load(a.png)
    cells, where = [], []
    for y in range(0, img.height - ch + 1, ch):
        for x in range(0, img.width - cw + 1, cw):
            c = cell(img, x, y, cw, ch)
            if not is_empty(c):
                cells.append(c)
                where.append((x // cw, y // ch))
    if a.mode == "bg":
        r = convert_bg(cells, a.max_pals, a.pal_base, a.tile_base)
        tiles, pals, table = r.tiles, r.palettes, r.metas
    else:
        r = convert_obj(cells)
        tiles, pals, table = r.tiles, [r.palette], [list(f) for f in r.frames]
    if a.out.endswith(".bin"):
        write_bin(a.out, a.mode, tiles, pals, table, cw, ch)
    else:
        n = a.name
        s = ["/* Generated by tools/rsasset.py from %s (%s mode, %dx%d cells). */" % (a.png, a.mode, cw, ch),
             "#include <stdint.h>", "",
             "const int %s_tile_count = %d;" % (n, len(tiles)),
             c_bytes("%s_tiles" % n, tiles_bytes(tiles)),
             "const int %s_pal_count = %d;" % (n, len(pals)),
             c_u16("%s_pals" % n, [v for p in pals for v in palette16(p)]),
             "/* cells in reading order: %s */" % " ".join("(%d,%d)" % w for w in where),
             "const int %s_cell_count = %d;" % (n, len(table)),
             c_u16("%s_cells" % n, [v for e in table for v in e])]
        with open(a.out, "w") as f:
            f.write("\n".join(s) + "\n")
    print("%s: %d cells, %d tiles, %d palettes -> %s" % (a.png, len(cells), len(tiles), len(pals), a.out))


if __name__ == "__main__":
    main()

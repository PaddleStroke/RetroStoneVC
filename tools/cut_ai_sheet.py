#!/usr/bin/env python3
"""Cut an upscaled (AI-generated) sprite sheet back into the RetroStone VC sheet format.

    cut_ai_sheet.py AI.png --sheet characters --out characters.png
                    [--base placeholder.png] [--map map.txt] [--scale 8]
                    [--filter area|nearest] [--palette-from sheet.png] [--debug dbg.png]

Real AI output is only roughly regular, so the cutter does not assume exact
cell positions:
  1. the background (almost magenta, or transparent) is estimated from the
     image border and masked out, including pink anti-aliasing fringes;
  2. sprites are found as connected blobs; small blobs (sparks, dirt, stars)
     are attached to the nearest sprite, isolated stray pixels are dropped,
     blobs that are clearly 2+ tiles glued together are split;
  3. blobs are sorted in reading order (rows, then left to right) and mapped
     onto the table order of tools/sheets.py (docs/art-sheets.md), or onto
     the order given by --map. A count mismatch is reported (per row).
  4. each sprite is scaled into its cell (one scale per palette group so the
     frames of a character do not jitter; --scale forces one scale), anchored
     bottom-centre for characters and centred otherwise; tiles fill the cell;
  5. downscaling uses area averaging (default) or nearest sampling;
  6. colours are snapped to RGB555 and reduced per palette group (15 colours
     for sprites, 60 per season for terrain), or snapped to the palette of an
     existing sheet with --palette-from.
Cells that get no sprite keep the --base sheet's art (e.g. the placeholders).

Map file (--map): one table name per detected sprite, in reading order,
whitespace separated. Tiles take a season suffix (grass@spring); "skip"
ignores a sprite; "name:N" is frame N; a bare name takes the next frame.

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import argparse
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sheets  # noqa: E402
import rsasset  # noqa: E402

MAG = np.array(sheets.MAGENTA, dtype=np.float32)
# l/r/t/b: stick the sprite to that cell edge; X/Y: stretch it across the cell
EDGE_ANCHOR = {"expl_h": "X", "expl_v": "Y", "expl_end_left": "r", "expl_end_right": "l",
               "expl_end_up": "b", "expl_end_down": "t"}


# ---- 1. background mask ---------------------------------------------------------
def background_color(rgb):
    b = np.concatenate([rgb[0], rgb[-1], rgb[:, 0], rgb[:, -1]])
    return np.median(b, axis=0)


def despill(rgb, fg, edge):
    """Magenta despill. AI images have pink anti-aliasing and pink halos (on
    fire and explosions especially). Foreground pixels near the background
    whose colour leans toward magenta (red and blue both above green) take
    the colour of the nearest clean foreground pixel. Returns (rgb, count)."""
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    spill = np.minimum(r, b) - g
    near = dilate(~fg, edge) & fg
    tinted = near & (spill > 25)
    clean = fg & ~tinted
    if not tinted.any() or not clean.any():
        return rgb, 0
    try:
        from scipy import ndimage
    except ImportError:            # without scipy: pull the colour toward green (simple despill)
        out = rgb.copy()
        k = np.clip(spill, 0, None)[..., None] * tinted[..., None]
        out[..., 0:1] -= k
        out[..., 2:3] -= k
        return np.clip(out, 0, 255), int(tinted.sum())
    idx = ndimage.distance_transform_edt(~clean, return_distances=False, return_indices=True)
    out = rgb.copy()
    out[tinted] = rgb[idx[0][tinted], idx[1][tinted]]
    return out, int(tinted.sum())


def foreground_mask(img, thresh=90.0, edge=None):
    """(fg mask, despilled rgb, background colour) of an AI image."""
    a = np.asarray(img.convert("RGBA"), dtype=np.float32)
    rgb, alpha = a[..., :3], a[..., 3]
    bg = background_color(rgb)
    d = np.sqrt(((rgb - bg) ** 2).sum(-1))
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    # pixels close to magenta, and light mixes of magenta with something else, are background
    fringe = (r > g + 90) & (b > g + 90) & (np.abs(r - b) < 60) & (r > 150) & (b > 150)
    fg = (d > thresh) & ~fringe & (alpha >= 128)
    if edge is None:
        edge = max(2, int(round(min(fg.shape) / 250)))
    rgb, _ = despill(rgb, fg, edge)
    return fg, rgb, bg


def dilate(m, r):
    if r <= 0:
        return m
    im = Image.fromarray((m * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(2 * r + 1))
    return np.asarray(im) > 0


def erode(m, r):
    if r <= 0:
        return m
    im = Image.fromarray((m * 255).astype(np.uint8)).filter(ImageFilter.MinFilter(2 * r + 1))
    return np.asarray(im) > 0


# ---- 2. blobs ---------------------------------------------------------------------
def components(mask):
    """Connected components (8-neighbourhood) -> list of [x0, y0, x1, y1, area]."""
    try:
        from scipy import ndimage
        lab, n = ndimage.label(mask, structure=np.ones((3, 3)))
        objs = ndimage.find_objects(lab)
        areas = ndimage.sum(mask, lab, range(1, n + 1))
        return [[s[1].start, s[0].start, s[1].stop, s[0].stop, int(a)] for s, a in zip(objs, areas)]
    except ImportError:
        pass
    # pure Python fallback (union-find over runs)
    h, w = mask.shape
    parent = {}

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a
    runs, prev = [], []
    for y in range(h):
        row = mask[y]
        cur, x = [], 0
        while x < w:
            if row[x]:
                s = x
                while x < w and row[x]:
                    x += 1
                rid = len(runs)
                runs.append((y, s, x))
                parent[rid] = rid
                for (pid, ps, pe) in prev:
                    if ps <= x and pe >= s:
                        ra, rb = find(rid), find(pid)
                        if ra != rb:
                            parent[ra] = rb
                cur.append((rid, s, x))
            x += 1
        prev = cur
    boxes = {}
    for rid, (y, s, e) in enumerate(runs):
        r = find(rid)
        b = boxes.setdefault(r, [s, y, e, y + 1, 0])
        b[0], b[1], b[2], b[3] = min(b[0], s), min(b[1], y), max(b[2], e), max(b[3], y + 1)
        b[4] += e - s
    return list(boxes.values())


def box_gap(a, b):
    dx = max(0, max(a[0], b[0]) - min(a[2], b[2]))
    dy = max(0, max(a[1], b[1]) - min(a[3], b[3]))
    return (dx * dx + dy * dy) ** 0.5


def _union_boxes(boxes):
    return [min(b[0] for b in boxes), min(b[1] for b in boxes), max(b[2] for b in boxes),
            max(b[3] for b in boxes), sum(b[4] for b in boxes)]


def _cluster(items, radius):
    """Single-link clustering of boxes whose gap is <= radius."""
    parent = list(range(len(items)))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a
    for i in range(len(items)):
        for j in range(i + 1, len(items)):
            if box_gap(items[i], items[j]) <= radius:
                parent[find(i)] = find(j)
    groups = {}
    for i in range(len(items)):
        groups.setdefault(find(i), []).append(items[i])
    return list(groups.values())


def find_sprites(mask, solid=False, report=print):
    """Boxes [x0, y0, x1, y1] of the sprites on the sheet.
    Big blobs are sprites. A medium blob very close to another blob belongs
    to it (a lock's shackle). Small blobs are grouped: a group near a sprite
    is part of it (dirt, sparks, stars), a big enough group far from any
    sprite is a sprite made of strokes, anything else is a stray pixel."""
    h, w = mask.shape
    comps = components(dilate(mask, 0 if solid else 1))
    if not comps:
        return []
    maxa = max(c[4] for c in comps)
    typ = float(np.median([c[4] for c in comps if c[4] >= 0.02 * maxa]))
    unit = typ ** 0.5
    big = [c for c in comps if c[4] >= 0.12 * typ]
    small = [c for c in comps if c[4] < 0.12 * typ]
    if not solid:
        # medium blobs glued to a neighbour
        groups = []
        for g in _cluster(big, 0.12 * unit):
            # only merge when all but the largest are medium sized
            g.sort(key=lambda c: -c[4])
            if len(g) > 1 and any(c[4] >= 0.35 * typ for c in g[1:]):
                groups.extend([c] for c in g)
            else:
                groups.append(g)
        big = [_union_boxes(g) for g in groups]
    dropped = 0
    for g in _cluster(small, 0.3 * unit):
        gb = _union_boxes(g)
        near = min(big, key=lambda b: box_gap(b, gb)) if big else None
        if near is not None and box_gap(near, gb) <= 0.35 * unit:
            near[:] = _union_boxes([near, gb])
        elif gb[4] >= 0.12 * typ:
            big.append(gb)
        else:
            dropped += 1
    if dropped:
        report("  dropped %d stray blob(s)" % dropped)
    # split blobs that are clearly several tiles glued together
    mw = float(np.median([b[2] - b[0] for b in big]))
    mh = float(np.median([b[3] - b[1] for b in big]))
    out = []
    for b in big:
        bw, bh = b[2] - b[0], b[3] - b[1]
        nx, ny = int(round(bw / mw)), int(round(bh / mh))
        if solid and (nx > 1 or ny > 1) and abs(bw / mw - nx) < 0.25 and abs(bh / mh - ny) < 0.25:
            report("  split a blob of %dx%d px into %dx%d tiles" % (bw, bh, nx, ny))
            for j in range(ny):
                for i in range(nx):
                    out.append([b[0] + i * bw // nx, b[1] + j * bh // ny,
                                b[0] + (i + 1) * bw // nx, b[1] + (j + 1) * bh // ny])
        else:
            out.append(b[:4])
    return out


# ---- 3. reading order ------------------------------------------------------------------
def reading_rows(boxes):
    """Group boxes into rows by vertical overlap (a small sprite next to a big
    one joins its row even when not centred), then sort each row by x."""
    if not boxes:
        return []
    rows = []
    for b in sorted(boxes, key=lambda b: (b[1] + b[3]) / 2):
        bh = b[3] - b[1]
        if rows:
            r = rows[-1]
            ov = min(b[3], r[1]) - max(b[1], r[0])
            if ov >= 0.5 * min(bh, r[3]):
                r[0], r[1] = min(r[0], b[1]), max(r[1], b[3])
                r[2].append(b)
                r[3] = sorted(x[3] - x[1] for x in r[2])[len(r[2]) // 2]
                continue
        rows.append([b[1], b[3], [b], bh])
    return [sorted(r[2], key=lambda b: b[0]) for r in rows]


def expected_order(sheet, map_path=None):
    """[(entry, frame, season)] in the order sprites should appear."""
    if not map_path:
        return [(e, f, s) for e, f, s, _ in sheets.reading_order(sheet)]
    out, nextf = [], {}
    with open(map_path) as fh:
        toks = [t for line in fh for t in line.split("#")[0].split()]
    for t in toks:
        if t == "skip":
            out.append(None)
            continue
        season = 0
        if "@" in t:
            t, sn = t.split("@")
            season = sheets.SEASONS.index(sn)
        frame = None
        if ":" in t:
            t, fr = t.split(":")
            frame = int(fr)
        e = sheets.BY_NAME[t]
        if e.sheet != sheet:
            raise SystemExit("map: %s is not on the %s sheet" % (t, sheet))
        if frame is None:
            frame = nextf.get((t, season), 0)
        nextf[(t, season)] = frame + 1
        out.append((e, frame, season))
    return out


# ---- 4/5. scaling and downscaling ------------------------------------------------------
PAD = 512


def prepare(rgb, fg, core):
    """PIL images reused for every cell (padded so windows may cross the
    image border): colour, masks, premultiplied channels."""
    rgb = np.pad(rgb, ((PAD, PAD), (PAD, PAD), (0, 0)), mode="edge")
    fg = np.pad(fg, PAD)
    core = np.pad(core, PAD)
    P = {"img": Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8)),
         "m": Image.fromarray((fg * 255).astype(np.uint8)), "layers": []}
    for mk in (fg, core):
        mk = mk.astype(np.float32)
        P["layers"].append(([Image.fromarray(np.ascontiguousarray(rgb[..., k] * mk)) for k in range(3)],
                            Image.fromarray(mk)))
    return P


def downscale(P, win, w, h, method):
    win = tuple(v + PAD for v in win)
    if method == "nearest":
        c = np.asarray(P["img"].resize((w, h), Image.NEAREST, box=win), dtype=np.float32)
        a = np.asarray(P["m"].resize((w, h), Image.NEAREST, box=win)) > 127
        return c, a
    cov = np.asarray(P["m"].resize((w, h), Image.BOX, box=win), dtype=np.float32) / 255.0
    out = np.zeros((h, w, 3), np.float32)
    for chans, mk in P["layers"]:     # full mask first, then overwrite with the core colour
        num = np.stack([np.asarray(c.resize((w, h), Image.BOX, box=win)) for c in chans], -1)
        den = np.asarray(mk.resize((w, h), Image.BOX, box=win))
        ok = den > 0.05
        out[ok] = num[ok] / den[ok][:, None]
    return out, cov >= 0.5


# ---- 6. palettes ----------------------------------------------------------------------------
def reduce_colors(pixels, n):
    """list of (r,g,b) -> dict colour -> reduced RGB888 (RGB555 exact)."""
    uniq = sorted(set(pixels))
    snapped = {p: rsasset.rgb888(rsasset.to555(p)) for p in uniq}
    if len(set(snapped.values())) <= n:
        return snapped
    strip = Image.new("RGB", (len(pixels), 1))
    strip.putdata([snapped[p] for p in pixels])
    q = strip.quantize(colors=n, method=Image.Quantize.MEDIANCUT)
    pal = q.getpalette()[:n * 3]
    pal = [rsasset.rgb888(rsasset.to555(tuple(pal[i * 3:i * 3 + 3]))) for i in range(n)]
    return {p: pal[rsasset.nearest(snapped[p], pal)] for p in uniq}


def palette_from(sheet_png, sheet):
    im = Image.open(sheet_png).convert("RGB")
    pals = {}
    for e, f, s, (x, y, w, h) in sheets.reading_order(sheet):
        key = (e.group, s) if sheet == "tiles" else e.group
        cols = pals.setdefault(key, set())
        cols.update(c for c in im.crop((x, y, x + w, y + h)).getdata() if c != sheets.MAGENTA)
    return {k: sorted(v) for k, v in pals.items()}


# ---- main ---------------------------------------------------------------------------------
def cut(ai_path, sheet, out_path, base=None, map_path=None, scale=None, method="area",
        pal_sheet=None, debug=None, report=print):
    ai = Image.open(ai_path).convert("RGBA")
    solid = sheet == "tiles"
    fg, rgb, bg = foreground_mask(ai)
    report("%s: %dx%d, background ~(%d,%d,%d)" % (ai_path, ai.width, ai.height, *bg))
    boxes = find_sprites(fg, solid, report)
    rows = reading_rows(boxes)
    found = [b for r in rows for b in r]
    want = expected_order(sheet, map_path)
    report("  detected %d sprites in %d rows (%s); the table expects %d" %
           (len(found), len(rows), " ".join(str(len(r)) for r in rows), len(want)))
    status = 0
    if len(found) != len(want):
        status = 1
        report("  COUNT MISMATCH: mapping the first %d in reading order; check the debug image "
               "and pass --map to fix the order" % min(len(found), len(want)))
    pairs = [(b, w) for b, w in zip(found, want) if w is not None]

    # one scale per palette group (or forced)
    core = erode(fg, max(1, int(round(min(ai.width, ai.height) / 700))))
    group_scale = {}
    for b, (e, f, s) in pairs:
        k = e.group
        r = max((b[2] - b[0]) / (e.w - (0 if solid else 1)), (b[3] - b[1]) / (e.h - (0 if solid else 1)))
        group_scale[k] = max(group_scale.get(k, 0), r)
    if scale:
        group_scale = {k: scale for k in group_scale}
    report("  scale per group: " + ", ".join("%s %.2f" % kv for kv in sorted(group_scale.items())))

    P = prepare(rgb, fg, core)
    W, H = sheets.sheet_size(sheet)
    if base:
        out = Image.open(base).convert("RGB")
        if out.size != (W, H):
            raise SystemExit("--base has the wrong size %s, expected %dx%d" % (out.size, W, H))
    else:
        out = Image.new("RGB", (W, H), sheets.MAGENTA)
    cells = []
    for b, (e, f, s) in pairs:
        x, y, w, h = sheets.frame_rects(e, s)[f]
        if solid and not scale:
            win = (b[0], b[1], b[2], b[3])
        else:
            sc = group_scale[e.group]
            ww, wh = w * sc, h * sc
            # centre on the box, rounded to whole sheet pixels so that the
            # sampling grid stays aligned with the drawing's pixel grid
            x0 = b[0] - round((ww - (b[2] - b[0])) / 2 / sc) * sc
            if e.sheet == "characters":     # feet on the bottom row
                y0 = b[3] - wh
            else:
                y0 = b[1] - round((wh - (b[3] - b[1])) / 2 / sc) * sc
            x1, y1 = x0 + ww, y0 + wh
            # explosion pieces must reach the cell edges to connect in game
            a = EDGE_ANCHOR.get(e.name, "")
            if "l" in a: x0, x1 = b[0], b[0] + ww
            if "r" in a: x0, x1 = b[2] - ww, b[2]
            if "t" in a: y0, y1 = b[1], b[1] + wh
            if "b" in a: y0, y1 = b[3] - wh, b[3]
            if "X" in a: x0, x1 = b[0], b[2]
            if "Y" in a: y0, y1 = b[1], b[3]
            win = (x0, y0, x1, y1)
        col, alpha = downscale(P, win, w, h, method)
        cells.append((e, f, s, x, y, col, alpha))

    # palettes
    fixed = palette_from(pal_sheet, sheet) if pal_sheet else None
    groups = {}
    for e, f, s, x, y, col, alpha in cells:
        key = (e.group, s) if solid else e.group
        groups.setdefault(key, []).extend(tuple(int(v) for v in col[j, i])
                                          for j in range(col.shape[0]) for i in range(col.shape[1]) if alpha[j, i])
    maps = {}
    for key, pix in groups.items():
        if fixed and key in fixed:
            pal = fixed[key]
            maps[key] = {p: pal[rsasset.nearest(p, pal)] for p in set(pix)}
        else:
            maps[key] = reduce_colors(pix, 60 if solid else 15)
    for e, f, s, x, y, col, alpha in cells:
        key = (e.group, s) if solid else e.group
        m = maps[key]
        tile = Image.new("RGB", (e.w, e.h), sheets.MAGENTA)
        tp = tile.load()
        for j in range(e.h):
            for i in range(e.w):
                if alpha[j, i]:
                    tp[i, j] = m[tuple(int(v) for v in col[j, i])]
        out.paste(tile, (x, y))
    out.save(out_path)
    report("  wrote %s (%d cells from the AI sheet%s)" % (out_path, len(cells), ", rest from " + base if base else ""))

    if debug:
        k = 1024.0 / max(ai.width, ai.height)
        dimg = ai.convert("RGB").resize((int(ai.width * k), int(ai.height * k)))
        d = ImageDraw.Draw(dimg)
        for i, b in enumerate(found):
            name = ""
            if i < len(want):
                name = "skip" if want[i] is None else "%s%s" % (want[i][0].name, ":%d" % want[i][1] if want[i][0].frames > 1 else "")
            d.rectangle([b[0] * k, b[1] * k, b[2] * k, b[3] * k], outline=(0, 255, 0))
            d.text((b[0] * k + 1, b[1] * k + 1), "%d %s" % (i, name), fill=(255, 255, 255))
        dimg.save(debug)
        report("  wrote %s" % debug)
    return status, len(found), len(want)


def main():
    ap = argparse.ArgumentParser(description="AI sprite sheet -> RetroStone VC sheet")
    ap.add_argument("ai_png")
    ap.add_argument("--sheet", required=True, choices=sorted(sheets.SHEETS))
    ap.add_argument("--out", required=True)
    ap.add_argument("--base", help="sheet whose cells are kept when not in the AI image")
    ap.add_argument("--map", help="order of the AI sprites (see the module doc)")
    ap.add_argument("--scale", type=float, help="force the scale (AI pixels per sheet pixel)")
    ap.add_argument("--filter", choices=["area", "nearest"], default="area")
    ap.add_argument("--palette-from", help="snap to the colours of this sheet (same layout)")
    ap.add_argument("--debug", help="write an overlay of the detected boxes and names")
    a = ap.parse_args()
    st, _, _ = cut(a.ai_png, a.sheet, a.out, a.base, a.map, a.scale, a.filter, a.palette_from, a.debug)
    sys.exit(st)


if __name__ == "__main__":
    main()

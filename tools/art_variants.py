#!/usr/bin/env python3
"""Contact sheet of the palette-swap enemy variants (tools/palette_variants.py),
with the placeholder art and with the owner's AI art (GENERATED strips).

    art_variants.py [--out docs/art-preview/enemy_variants.png]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import argparse
import os
import sys

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import art_sync  # noqa: E402

FRAMES = {"ferret": [("ferret_walk_down", 0), ("ferret_walk_left", 1), ("ferret_stunned", 0)],
          "cat": [("cat_walk_down", 0), ("cat_walk_right", 0), ("cat_pounce", 1)]}
TIER = {"sleepy_ferret": 1, "brown_ferret": 2, "polecat": 3, "stoat": 4,
        "ginger_cat": 1, "grey_cat": 2, "black_cat": 3, "siamese_cat": 4}


def group_art(sheet_img, sheets, rsasset, group):
    """-> (palette, counts, {frame key: tile-index image})"""
    ents = [e for e in sheets.ENTRIES if e.group == group]
    cells, keys = [], []
    for e in ents:
        for i, (x, y, w, h) in enumerate(sheets.frame_rects(e)):
            cells.append(rsasset.cell(sheet_img, x, y, w, h))
            keys.append((e.name, i))
    r = rsasset.convert_obj(cells)
    counts = [0] * len(r.palette)
    for t in r.tiles:
        for v in t:
            if v:
                counts[v - 1] += 1
    frames = {}
    for k, (first, w, h) in zip(keys, r.frames):
        idx = [[0] * w for _ in range(h)]
        tw = w // 8
        for n in range((w // 8) * (h // 8)):
            t = r.tiles[first + n]
            ox, oy = (n % tw) * 8, (n // tw) * 8
            for p in range(64):
                idx[oy + p // 8][ox + p % 8] = t[p]
        frames[k] = idx
    return r.palette, counts, frames


def render(idx, pal, pv, Z=4, bg=(52, 56, 76)):
    h, w = len(idx), len(idx[0])
    im = Image.new("RGB", (w, h), bg)
    p = im.load()
    for y in range(h):
        for x in range(w):
            if idx[y][x]:
                p[x, y] = pv.to_rgb(pal[idx[y][x] - 1])
    return im.resize((w * Z, h * Z), Image.NEAREST)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(ROOT, "docs", "art-preview", "enemy_variants.png"))
    a = ap.parse_args()
    sheets, mp, cut, brief = art_sync.load_modules(16)
    import rsasset
    import palette_variants as pv
    sources = [("placeholder art", mp.build_sheet("characters").convert("RGBA"))]
    gen = os.path.join(ROOT, "build", "art-variants")
    args = argparse.Namespace(incoming=os.path.join(ROOT, "games", "bombermole", "art", "incoming"), out=gen,
                              report=os.path.join(gen, "report.md"), dry_run=False, include_generated=True,
                              char_size=16, scale=None, filter="area")
    os.makedirs(gen, exist_ok=True)
    art_sync.sync(args, report_print=lambda s: None)
    sources.append(("AI art (first batch, not validated)", Image.open(os.path.join(gen, "characters.png")).convert("RGBA")))
    Z, cw = 4, 16 * 4 + 6
    rows = []
    for name, group, target, light in pv.VARIANTS:
        rows.append((name, group, target, light))
    W = 170 + len(sources) * (3 * cw + 24)
    H = 40 + len(rows) * (16 * Z + 10)
    out = Image.new("RGB", (W, H), (24, 24, 32))
    d = ImageDraw.Draw(out)
    d.text((6, 4), "Enemy variants: one sprite, one palette swap each (tier in brackets)", fill=(255, 255, 255))
    base = {}
    for si, (label, img) in enumerate(sources):
        d.text((170 + si * (3 * cw + 24), 22), label, fill=(255, 220, 120))
        for g in ("ferret", "cat"):
            base[(si, g)] = group_art(img, sheets, rsasset, g)
    for ri, (name, group, target, light) in enumerate(rows):
        y = 40 + ri * (16 * Z + 10)
        d.text((6, y + 24), "%s [%d]" % (name, TIER[name]), fill=(220, 220, 220))
        for si in range(len(sources)):
            pal, counts, frames = base[(si, group)]
            vpal = pv.variant_palette(pal, counts, target, light)
            for fi, key in enumerate(FRAMES[group]):
                out.paste(render(frames[key], vpal, pv, Z), (170 + si * (3 * cw + 24) + fi * cw, y))
    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    out.save(a.out)
    print("wrote", a.out)


if __name__ == "__main__":
    main()

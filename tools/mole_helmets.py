#!/usr/bin/env python3
"""Preview of the players' moles: the same art, only the miner's helmet recoloured
(red, blue, green, yellow; tools/palette_variants.py helmet_palette). Two rows:
the placeholder art at the game's 16 px and the owner's AI art (GENERATED strips,
through the art pipeline) at 24 px, each mole at 4x, with the helmet ramps.

    mole_helmets.py [--out docs/art-preview/mole-helmets.png]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import argparse
import importlib
import os
import sys

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import art_sync  # noqa: E402
import art_variants  # noqa: E402

FRAMES = [("mole_walk_down", 0), ("mole_walk_left", 0)]
Z = 4


def source(char_size, ai):
    """-> (label, palette, frames) of the mole group at char_size, placeholder or AI art."""
    sheets, mp, _, _ = art_sync.load_modules(char_size)
    os.environ["BM_CHAR_SIZE"] = str(char_size)
    importlib.reload(sheets)                   # the entries' sizes follow BM_CHAR_SIZE
    import rsasset
    if ai:
        gen = os.path.join(ROOT, "build", "art-helmets")
        args = argparse.Namespace(incoming=os.path.join(ROOT, "games", "bombermole", "art", "incoming"), out=gen,
                                  report=os.path.join(gen, "report.md"), dry_run=False, include_generated=True,
                                  char_size=char_size, scale=None, filter="area")
        os.makedirs(gen, exist_ok=True)
        art_sync.sync(args, report_print=lambda s: None)
        img = Image.open(os.path.join(gen, "characters.png")).convert("RGBA")
        label = "AI art (GENERATED strips, %d px)" % char_size
    else:
        img = mp.build_sheet("characters").convert("RGBA")
        label = "placeholder art (%d px, the game's size)" % char_size
    import palette_variants as pv
    cells, keys = [], []                       # as the game's asset build: the helmet's colours made its own
    for e in sheets.ENTRIES:
        if e.group == "mole":
            for i, (x, y, w, h) in enumerate(sheets.frame_rects(e)):
                cells.append(rsasset.cell(img, x, y, w, h))
                keys.append((e.name, i))
    cells, helmet = pv.helmet_frames(cells, rsasset.quantize)
    r = rsasset.convert_obj(cells)
    frames = {}
    for k, (first, w, h) in zip(keys, r.frames):
        idx = [[0] * w for _ in range(h)]
        for n in range((w // 8) * (h // 8)):
            t = r.tiles[first + n]
            ox, oy = (n % (w // 8)) * 8, (n // (w // 8)) * 8
            for p in range(64):
                idx[oy + p // 8][ox + p % 8] = t[p]
        frames[k] = idx
    return label, r.palette, frames, pv.helmet_ramp(r.palette, helmet)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(ROOT, "docs", "art-preview", "mole-helmets.png"))
    a = ap.parse_args()
    import palette_variants as pv
    rows = [source(16, False), source(24, True)]
    cell = 24 * Z + 8
    W = 16 + len(pv.HELMETS) * (len(FRAMES) * cell + 16)
    H = 30 + len(rows) * (24 + cell + 40)
    out = Image.new("RGB", (W, H), (24, 24, 32))
    d = ImageDraw.Draw(out)
    d.text((8, 6), "The players' moles: only the helmet changes colour (fur, nose, claws and lamp as drawn)",
           fill=(255, 255, 255))
    y = 30
    for label, pal, frames, ramp in rows:
        d.text((8, y), "%s - helmet ramp: palette entries %s" % (label, ", ".join(str(i + 1) for i in ramp)),
               fill=(255, 220, 120))
        y += 16
        for k, (name, helmet) in enumerate(pv.HELMETS):
            x = 16 + k * (len(FRAMES) * cell + 16)
            hp = pv.helmet_palette(pal, helmet, ramp)
            d.text((x, y), "P%d %s" % (k + 1, name), fill=(220, 220, 220))
            for fi, key in enumerate(FRAMES):
                im = art_variants.render(frames[key], hp, pv, Z)
                out.paste(im, (x + fi * cell, y + 12 + (24 * Z - im.size[1])))
            for j, i in enumerate(sorted(ramp, key=lambda i: pv._hsv(pal[i])[2])):   # the ramp, dark to light
                d.rectangle((x + j * 14, y + 16 + 24 * Z, x + j * 14 + 11, y + 27 + 24 * Z), fill=pv.to_rgb(hp[i]))
        y += cell + 40
    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    out.save(a.out)
    print("wrote", a.out)


if __name__ == "__main__":
    main()

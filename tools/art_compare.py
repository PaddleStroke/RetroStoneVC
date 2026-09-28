#!/usr/bin/env python3
"""Character size comparison for the owner: the AI characters (GENERATED
strips in the drop folder) downscaled to 16, 24 and 32 pixels (the boss to
32, 48, 64) with nearest, area, and area + 15-colour palette snap, at 4x.

    art_compare.py [--out docs/art-preview]

The in-game 1x comparison is made by tools/art_compare_ingame.sh (real game
builds with BM_CHAR_SIZE=16/24/32 and the generated art).
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

SUBJECTS = [("mole_walk_down", 0), ("mole_walk_left", 0), ("mole_dig_right", 1), ("ferret_walk_down", 0),
            ("ferret_walk_left", 0), ("cat_walk_down", 0), ("cat_pounce", 1), ("boss", 0)]
METHODS = ["nearest", "area", "area+snap"]


def render(col, alpha, bg=(48, 52, 72)):
    h, w = alpha.shape
    im = Image.new("RGB", (w, h), bg)
    p = im.load()
    for j in range(h):
        for i in range(w):
            if alpha[j, i]:
                p[i, j] = tuple(int(v) for v in col[j, i])
    return im


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--incoming", default=os.path.join(ROOT, "games", "bombermole", "art", "incoming"))
    ap.add_argument("--out", default=os.path.join(ROOT, "docs", "art-preview"))
    a = ap.parse_args()
    sheets, mp, cut, brief = art_sync.load_modules(16)
    strips = {s.id: s for s in art_sync.all_strips(sheets)}
    os.makedirs(a.out, exist_ok=True)
    Z = 4
    cells = {}          # (subject, size, method) -> image
    for sid, frame in SUBJECTS:
        path = os.path.join(a.incoming, sid + ".png")
        if not os.path.exists(path):
            print("missing", path)
            continue
        base = strips[sid]
        for size in (16, 24, 32):
            k = size // 16
            s = art_sync.Strip(sid, base.entry, 0, base.w * k, base.h * k, base.frames, base.sheet, base.group)
            for m in METHODS:
                frames, warn, sc = art_sync.import_strip(path, s, cut, None, "nearest" if m == "nearest" else "area")
                col, alpha = frames[frame]
                if m == "area+snap":
                    pix = [tuple(int(v) for v in c[j, i]) for c, al in frames
                           for j in range(al.shape[0]) for i in range(al.shape[1]) if al[j, i]]
                    mapping = cut.reduce_colors(pix, 15)
                    col = col.copy()
                    for j in range(alpha.shape[0]):
                        for i in range(alpha.shape[1]):
                            if alpha[j, i]:
                                col[j, i] = mapping[tuple(int(v) for v in col[j, i])]
                cells[(sid, size, m)] = render(col, alpha)
    # contact sheet: one block per size; rows = subjects, columns = methods
    colw = 64 * Z + 16
    rowh = 64 * Z + 10
    W = 150 + len(METHODS) * colw
    blocks = []
    for size in (16, 24, 32):
        H = 30 + len(SUBJECTS) * (size * 2 * Z // 2 + 14) + 40
        img = Image.new("RGB", (W, 0 + 30 + sum((strips[s].h * size // 16) * Z + 14 for s, _ in SUBJECTS)), (24, 24, 32))
        d = ImageDraw.Draw(img)
        d.text((4, 4), "characters at %dx%d (boss %dx%d), 4x zoom" % (size, size, size * 2, size * 2), fill=(255, 255, 255))
        for mi, m in enumerate(METHODS):
            d.text((150 + mi * colw, 16), m, fill=(255, 220, 120))
        y = 30
        for sid, fr in SUBJECTS:
            hgt = strips[sid].h * size // 16 * Z
            d.text((4, y + 4), "%s %d" % (sid, fr + 1), fill=(200, 200, 200))
            for mi, m in enumerate(METHODS):
                c = cells.get((sid, size, m))
                if c:
                    img.paste(c.resize((c.width * Z, c.height * Z), Image.NEAREST), (150 + mi * colw, y))
            y += hgt + 14
        p = os.path.join(a.out, "character_size_%d_4x.png" % size)
        img.save(p)
        blocks.append(p)
        print("wrote", p)
    # side by side, area + snap, at 2x: one row per size
    strip = Image.new("RGB", (70 + 7 * 70, 3 * 76 + 10), (48, 52, 72))
    d = ImageDraw.Draw(strip)
    for r, size in enumerate((16, 24, 32)):
        y = 8 + r * 76
        d.text((6, y + 24), "%d px" % size, fill=(255, 255, 255))
        for i, (sid, fr) in enumerate(SUBJECTS[:7]):
            c = cells.get((sid, size, "area+snap"))
            if c:
                strip.paste(c.resize((c.width * 2, c.height * 2), Image.NEAREST), (70 + i * 70, y + 64 - c.height * 2))
    p = os.path.join(a.out, "character_size_side_by_side_2x.png")
    strip.save(p)
    print("wrote", p)


if __name__ == "__main__":
    main()

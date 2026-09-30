#!/usr/bin/env python3
"""Tests of the art pipeline: sheet layout, placeholders, asset tool, AI cutter.

The cutter test builds a fake "AI" image from each placeholder sheet: cells
upscaled 8x, laid out on an uneven grid with a different number of sprites
per row, jittered, blurred, on a noisy almost-magenta background with stray
pixels. The cutter must find every sprite, in order, and give back the
placeholder cells.

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import os
import random
import sys
import tempfile

import numpy as np
from PIL import Image, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools"))
# the layout at 16-px characters: the size of games/bombermole/art/ and of docs/art-sheets.md's table
# (make exports BM_CHAR_SIZE=24 for the default art-ai set)
os.environ["BM_CHAR_SIZE"] = "16"
import sheets  # noqa: E402
import rsasset  # noqa: E402
import make_placeholders  # noqa: E402
import cut_ai_sheet  # noqa: E402

failures = 0


def check(cond, msg):
    global failures
    print(("  ok   " if cond else "  FAIL ") + msg)
    if not cond:
        failures += 1


def test_layout():
    print("sheet layout")
    check(sheets.validate(), "no overlapping cells, all inside their sheet")
    doc = os.path.join(ROOT, "docs", "art-sheets.md")
    if os.path.exists(doc):
        check(sheets.markdown() in open(doc).read(), "docs/art-sheets.md contains the current table")


def test_placeholders(tmp):
    print("placeholders")
    ims = {}
    for sheet in sheets.SHEETS:
        im = make_placeholders.build_sheet(sheet)
        ims[sheet] = im
        check(im.size == sheets.sheet_size(sheet), "%s is %dx%d" % (sheet, *im.size))
        im.save(os.path.join(tmp, sheets.SHEETS[sheet]["file"]))
        groups = {}
        for e, f, s, (x, y, w, h) in sheets.reading_order(sheet):
            if sheet == "tiles":
                continue
            c = groups.setdefault(e.group, set())
            c.update(im.crop((x, y, x + w, y + h)).getdata())
        for g, cols in groups.items():
            cols.discard(sheets.MAGENTA)
            check(len(cols) <= 15, "sprite group %s has %d colours (<= 15)" % (g, len(cols)))
    # the committed art must match the generator unless it has been replaced by real art
    art = os.path.join(ROOT, "games", "bombermole", "art")
    for sheet, im in ims.items():
        p = os.path.join(art, sheets.SHEETS[sheet]["file"])
        if os.path.exists(p):
            check(Image.open(p).size == im.size, "%s in the game has the sheet size" % p)
    return ims


def test_rsasset(ims):
    print("asset tool")
    im = ims["tiles"].convert("RGBA")
    for s in range(4):
        cells = [rsasset.cell(im, x, y, w, h) for e, f, ss, (x, y, w, h) in sheets.reading_order("tiles") if ss == s]
        r = rsasset.convert_bg(cells, max_pals=5, pal_base=1)
        check(len(r.palettes) <= 5, "season %d terrain fits %d palettes, %d tiles" % (s, len(r.palettes), len(r.tiles)))
        # rebuild every cell from tiles + palettes + map entries: lossless
        bad = 0
        for c, meta in zip(cells, r.metas):
            for q, e in enumerate(meta):
                t = r.tiles[e & 1023]
                pal = r.palettes[((e >> 10) & 7) - 1]
                if e & 0x4000:
                    t = rsasset.hflip(t)
                if e & 0x8000:
                    t = rsasset.vflip(t)
                ox, oy = (q % 2) * 8, (q // 2) * 8
                for i, v in enumerate(t):
                    want = c[oy + i // 8][ox + i % 8]
                    got = None if v == 0 else pal[v - 1]
                    bad += want != got
        check(bad == 0, "season %d round trip is lossless (%d bad pixels)" % (s, bad))
    # too many colours for the palettes -> clustering keeps it working
    rng = random.Random(1)
    noisy = [[[rsasset.to555((rng.randrange(256), rng.randrange(256), rng.randrange(256))) for _ in range(16)]
              for _ in range(16)] for _ in range(6)]
    r = rsasset.convert_bg(noisy, max_pals=2)
    check(len(r.palettes) <= 2, "noisy cells reduced into 2 palettes")
    o = rsasset.convert_obj([rsasset.cell(ims["characters"].convert("RGBA"), 0, 64, 32, 32)])
    check(len(o.tiles) == 16 and o.frames[0] == (0, 32, 32), "32x32 sprite -> 16 consecutive tiles")


def fake_ai(sheet_im, sheet, per_row, seed, scale=8):
    rng = random.Random(seed)
    frames = sheets.reading_order(sheet)
    cells = [sheet_im.crop((x, y, x + w, y + h)).resize((w * scale, h * scale), Image.NEAREST)
             for e, f, s, (x, y, w, h) in frames]
    rows = [cells[i:i + per_row] for i in range(0, len(cells), per_row)]
    gap = 48
    W = max(sum(c.width for c in r) + gap * (len(r) + 1) for r in rows) + 20
    H = sum(max(c.height for c in r) + gap for r in rows) + gap + 60
    bg = np.zeros((H, W, 3), np.float32) + np.array([250, 6, 244], np.float32)
    bg += np.random.default_rng(seed).normal(0, 4, bg.shape)
    img = Image.fromarray(np.clip(bg, 0, 255).astype(np.uint8))
    y = gap
    for r in rows:
        x = gap
        rh = max(c.height for c in r)
        for c in r:
            mask = Image.fromarray((np.any(np.asarray(c) != np.array(sheets.MAGENTA), -1) * 255).astype(np.uint8))
            img.paste(c, (x + rng.randint(-6, 6), y + rh - c.height + rng.randint(-6, 6)), mask)
            x += c.width + gap
        y += rh + gap
    # stray pixels in the bottom margin
    for i in range(6):
        sx, sy = rng.randint(10, W - 10), H - rng.randint(8, 30)
        img.paste((rng.randint(0, 255), 200, 30), (sx, sy, sx + 3, sy + 3))
    return img.filter(ImageFilter.GaussianBlur(1.5))


def cell_match(a, b):
    """Best fraction of equal pixels over shifts of +-2 px."""
    A, B = np.asarray(a), np.asarray(b)
    h, w = A.shape[:2]
    best = 0.0
    for dy in range(-2, 3):
        for dx in range(-2, 3):
            ya, yb = max(0, dy), max(0, -dy)
            xa, xb = max(0, dx), max(0, -dx)
            hh, ww = h - abs(dy), w - abs(dx)
            eq = np.all(A[ya:ya + hh, xa:xa + ww] == B[yb:yb + hh, xb:xb + ww], -1).sum()
            best = max(best, eq / float(h * w))
    return best


def test_cutter(ims, tmp):
    print("AI sheet cutter")
    for sheet, per_row in (("characters", 9), ("tiles", 18), ("items_fx", 12)):
        ai = fake_ai(ims[sheet], sheet, per_row, seed=len(sheet))
        ai_path = os.path.join(tmp, "ai_%s.png" % sheet)
        ai.save(ai_path)
        out = os.path.join(tmp, "cut_%s.png" % sheet)
        pal = os.path.join(tmp, sheets.SHEETS[sheet]["file"])
        st, found, want = cut_ai_sheet.cut(ai_path, sheet, out, scale=8, method="nearest", pal_sheet=pal,
                                           debug=os.path.join(tmp, "dbg_%s.png" % sheet), report=lambda s: None)
        check(found == want, "%s: found %d of %d sprites" % (sheet, found, want))
        got = Image.open(out).convert("RGB")
        scores = []
        for e, f, s, (x, y, w, h) in sheets.reading_order(sheet):
            scores.append(cell_match(got.crop((x, y, x + w, y + h)), ims[sheet].crop((x, y, x + w, y + h))))
        # 1-px isolated sparkles are lost in the blur, the rest comes back
        check(np.mean(scores) >= 0.88 and min(scores) >= 0.7,
              "%s: cells recovered, mean %.3f, worst %.3f" % (sheet, np.mean(scores), min(scores)))
    # area filter and automatic scale also work (no exact comparison)
    ai_path = os.path.join(tmp, "ai_items_fx.png")
    st, found, want = cut_ai_sheet.cut(ai_path, "items_fx", os.path.join(tmp, "cut_auto.png"), report=lambda s: None)
    check(found == want, "area filter + automatic scale runs (%d sprites)" % found)
    # count mismatch is reported (status 1)
    im = Image.open(ai_path)
    im.crop((0, 0, im.width, im.height // 2)).save(os.path.join(tmp, "ai_half.png"))
    st, found, want = cut_ai_sheet.cut(os.path.join(tmp, "ai_half.png"), "items_fx",
                                       os.path.join(tmp, "cut_half.png"), report=lambda s: None)
    check(st == 1 and found < want, "a count mismatch is reported (%d of %d)" % (found, want))


def main():
    keep = os.environ.get("TOOLS_TEST_DIR")
    if keep:
        os.makedirs(keep, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        tmp = keep or tmp
        test_layout()
        ims = test_placeholders(tmp)
        test_rsasset(ims)
        test_cutter(ims, tmp)
    print("tools: %s" % ("all passed" if not failures else "%d FAILED" % failures))
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()

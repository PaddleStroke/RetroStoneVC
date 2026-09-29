#!/usr/bin/env python3
"""End-to-end test of tools/art_sync.py with fake "AI" strips.

A temporary drop folder gets a fresh TODO.md; placeholder frames are
upscaled 8x, blurred, jittered and put on a noisy near-magenta background as
<id>.png; the rows are marked VALIDATED (as the owner would) and synced into
a temporary art folder. Checks: imported cells match the placeholder, a
missing file and a frame-count mismatch are reported, other cells keep the
placeholder, TODO.md is not modified, --dry-run writes nothing.
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import argparse
import os
import random
import shutil
import sys
import tempfile

import numpy as np
from PIL import Image, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))
os.environ["BM_CHAR_SIZE"] = "16"
import art_sync  # noqa: E402

failures = 0


def check(cond, msg):
    global failures
    print(("  ok   " if cond else "  FAIL ") + msg)
    failures += 0 if cond else 1


def fake_strip(frames, seed, scale=8, gap=40):
    """Placeholder frames -> an AI-looking strip."""
    rng = random.Random(seed)
    w, h = frames[0].size
    W, H = len(frames) * (w * scale + gap) + gap, h * scale + 2 * gap
    bg = np.zeros((H, W, 3), np.float32) + np.array([248, 8, 246], np.float32)
    bg += np.random.default_rng(seed).normal(0, 4, bg.shape)
    img = Image.fromarray(np.clip(bg, 0, 255).astype(np.uint8))
    for i, f in enumerate(frames):
        big = f.resize((w * scale, h * scale), Image.NEAREST)
        mask = Image.fromarray((np.any(np.asarray(big) != np.array([255, 0, 255]), -1) * 255).astype(np.uint8))
        img.paste(big, (gap + i * (w * scale + gap) + rng.randint(-4, 4), gap + rng.randint(-4, 4)), mask)
    return img.filter(ImageFilter.GaussianBlur(1.2))


def mark(todo, sid, status):
    lines = []
    for line in open(todo, encoding="utf-8"):
        if line.startswith("| %s |" % sid):
            cells = [c.strip() for c in line.strip().strip("|").split("|")]
            cells[1] = status
            line = "| " + " | ".join(cells) + " |\n"
        lines.append(line)
    open(todo, "w", encoding="utf-8").writelines(lines)


def match(a, b):
    A, B = np.asarray(a.convert("RGB")), np.asarray(b.convert("RGB"))
    best = 0.0
    for dy in range(-2, 3):
        for dx in range(-2, 3):
            ya, yb, xa, xb = max(0, dy), max(0, -dy), max(0, dx), max(0, -dx)
            hh, ww = A.shape[0] - abs(dy), A.shape[1] - abs(dx)
            a, b = A[ya:ya + hh, xa:xa + ww].astype(int), B[yb:yb + hh, xb:xb + ww].astype(int)
            best = max(best, (np.abs(a - b).max(-1) <= 48).mean())   # colours are re-quantised
    return best


def main():
    sheets, mp, cut, brief = art_sync.load_modules(16)
    with tempfile.TemporaryDirectory() as tmp:
        inc, out = os.path.join(tmp, "incoming"), os.path.join(tmp, "art")
        os.makedirs(inc)
        todo = os.path.join(inc, "TODO.md")
        n = art_sync.write_todo(todo, inc, art_sync.all_strips(sheets), brief, sheets, {})
        rows = art_sync.read_todo(todo)
        check(n == len(rows) and all(r["status"] == "TODO" for r in rows.values()), "TODO.md: %d rows, all TODO" % n)
        ph = {s: mp.build_sheet(s) for s in sheets.SHEETS}
        e = sheets.BY_NAME["mole_walk_left"]
        frames = [ph["characters"].crop((x, y, x + w, y + h)) for x, y, w, h in sheets.frame_rects(e)]
        fake_strip(frames, 1).save(os.path.join(inc, "mole_walk_left.png"))
        t = sheets.BY_NAME["grass"]
        x, y, w, h = sheets.frame_rects(t, 0)[0]
        tile = ph["tiles"].crop((x, y, x + w, y + h))
        big = tile.resize((128, 128), Image.NEAREST)
        canvas = Image.new("RGB", (200, 200), (250, 4, 250))
        canvas.paste(big, (37, 30))
        canvas.filter(ImageFilter.GaussianBlur(0.8)).save(os.path.join(inc, "tile_spring_grass.png"))
        e2 = sheets.BY_NAME["cat_walk_down"]
        f2 = [ph["characters"].crop((x, y, x + w, y + h)) for x, y, w, h in sheets.frame_rects(e2)][:1]
        fake_strip(f2, 2).save(os.path.join(inc, "cat_walk_down.png"))       # 1 frame for 2: mismatch
        for sid in ("mole_walk_left", "tile_spring_grass", "cat_walk_down", "ferret_stunned"):
            mark(todo, sid, "VALIDATED")
        mark(todo, "bomb", "GENERATED")
        before = open(todo, encoding="utf-8").read()

        args = argparse.Namespace(incoming=inc, out=out, report=None, dry_run=True, include_generated=False,
                                  char_size=16, scale=8, filter="area")
        art_sync.sync(args, report_print=lambda s: None)
        check(not os.path.exists(out), "--dry-run writes nothing")
        args.dry_run = False
        counts, imported = art_sync.sync(args, report_print=lambda s: None)
        rep = open(os.path.join(inc, "IMPORT_REPORT.md"), encoding="utf-8").read()
        check(counts["imported"] == 2, "2 strips imported (%s)" % counts)
        check("| ferret_stunned | VALIDATED | missing" in rep, "missing file reported")
        check("| cat_walk_down | VALIDATED | error: frame-count mismatch" in rep, "frame-count mismatch reported")
        check("bomb" not in imported, "GENERATED rows are not imported by default")
        check(open(todo, encoding="utf-8").read() == before, "TODO.md is not modified")
        got = Image.open(os.path.join(out, "characters.png"))
        scores = [match(got.crop((x, y, x + w, y + h)), f) for (x, y, w, h), f in zip(sheets.frame_rects(e), frames)]
        check(min(scores) >= 0.85, "imported mole frames match the source (%s)" % ", ".join("%.2f" % s for s in scores))
        tx, ty = sheets.frame_rects(t, 0)[0][:2]
        gt = Image.open(os.path.join(out, "tiles.png")).crop((tx, ty, tx + 16, ty + 16))
        check(match(gt, tile) >= 0.85, "imported grass tile matches (%.2f)" % match(gt, tile))
        x, y, w, h = sheets.frame_rects(sheets.BY_NAME["ferret_walk_left"])[0]
        check(match(got.crop((x, y, x + w, y + h)), ph["characters"].crop((x, y, x + w, y + h))) == 1.0,
              "cells without validated art keep their placeholder")
        check(os.path.exists(os.path.join(out, "title_logo.png")), "title logo (placeholder) written")
        check(not os.path.exists(os.path.join(out, "tilesets", "ai_v2")), "no tile_v2 row imported: no ai_v2 tileset")
        # the low-detail tileset (TILESET=ai_v2): a validated tile_v2 row lands in <art>/tilesets/ai_v2
        st = sheets.BY_NAME["stone"]
        x, y, w, h = sheets.frame_rects(st, 1)[0]
        stone = ph["tiles"].crop((x, y, x + w, y + h))
        canvas = Image.new("RGB", (200, 200), (250, 4, 250))
        canvas.paste(stone.resize((128, 128), Image.NEAREST), (36, 34))
        canvas.save(os.path.join(inc, "tile_v2_summer_stone.png"))
        mark(todo, "tile_v2_summer_stone", "VALIDATED")
        art_sync.sync(args, report_print=lambda s: None)
        v2 = os.path.join(out, "tilesets", "ai_v2")
        ok = os.path.exists(os.path.join(v2, "tiles.png")) and os.path.exists(os.path.join(v2, "tiles_extra.png"))
        check(ok, "tile_v2 row imported: tilesets/ai_v2/tiles.png + tiles_extra.png")
        if ok:
            v = Image.open(os.path.join(v2, "tiles.png")).crop((x, y, x + w, y + h))
            check(match(v, stone) >= 0.85, "ai_v2: the summer stone cell is the imported one (%.2f)" % match(v, stone))
            ex = Image.open(os.path.join(v2, "tiles_extra.png"))
            check(ex.size == (16 * len(sheets.TILE_EXTRAS), 64), "ai_v2 extras: one row per season")
        prev = os.path.join(tmp, "preview")
        pargs = argparse.Namespace(**vars(args))
        pargs.out = prev
        art_sync.preview(pargs)
        check(os.path.exists(os.path.join(prev, "contact_sheet.png")) and
              os.path.exists(os.path.join(prev, "mole_walk_left.gif")), "preview: contact sheet and GIFs")
        shutil.rmtree(prev, ignore_errors=True)
    print("art_sync: %s" % ("all passed" if not failures else "%d FAILED" % failures))
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()

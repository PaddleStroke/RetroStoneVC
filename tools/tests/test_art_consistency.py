#!/usr/bin/env python3
"""Tests of tools/art_frames.py and tools/art_consistency.py with fake AI strips:
transparent (alpha) backgrounds with soft edges, near-magenta with pink fringes,
uneven spacing, attached effects, a frame drawn in two pieces, frame-count
mismatches (too few and too many drawings: reported, never guessed), opaque tiles
with holes, per-strip scale normalisation, one palette per group, the redrawn
outline and the seamless check.
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import argparse
import os
import sys
import tempfile

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))
os.environ["BM_CHAR_SIZE"] = "16"
import art_sync  # noqa: E402
import art_frames  # noqa: E402
import art_consistency as ac  # noqa: E402

failures = 0


def check(cond, msg):
    global failures
    print(("  ok   " if cond else "  FAIL ") + msg)
    failures += 0 if cond else 1


def rgba_frame(f, scale):
    big = f.convert("RGB").resize((f.width * scale, f.height * scale), Image.NEAREST)
    a = np.any(np.asarray(big) != np.array([255, 0, 255]), -1)
    return big, a


def make_strip(frames, scale=8, gaps=(30, 70, 40, 50), bg="alpha", extra=None, soft=True, seed=1):
    """Frames -> an AI-looking strip on a transparent or near-magenta background."""
    rng = np.random.default_rng(seed)
    parts = [rgba_frame(f, scale) for f in frames]
    W = sum(p[0].width for p in parts) + sum(gaps[:len(parts) + 1])
    H = max(p[0].height for p in parts) + 60
    canvas = np.zeros((H, W, 4), np.float32)
    if bg == "magenta":
        canvas[..., :3] = np.array([250, 5, 248]) + rng.normal(0, 4, (H, W, 3))
        canvas[..., 3] = 255
    x = gaps[0]
    for i, (im, a) in enumerate(parts):
        y = 30 + int(rng.integers(-3, 4))
        rgb = np.asarray(im, np.float32)
        sub = canvas[y:y + im.height, x:x + im.width]
        sub[a, :3] = rgb[a]
        sub[a, 3] = 255
        x += im.width + gaps[i + 1]
    img = Image.fromarray(np.clip(canvas, 0, 255).astype(np.uint8), "RGBA")
    if extra:
        extra(ImageDraw.Draw(img))
    if bg == "magenta":
        return img.convert("RGB").filter(ImageFilter.GaussianBlur(1.0))
    if soft:                                   # soft alpha edges, as AI exports have
        r, g, b, al = img.split()
        img = Image.merge("RGBA", (r, g, b, al.filter(ImageFilter.GaussianBlur(1.2))))
    return img


def mark_generated(todo, inc):
    """Rows whose PNG exists -> GENERATED (as the image agent does)."""
    out = []
    for line in open(todo, encoding="utf-8"):
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if line.startswith("| ") and len(cells) >= 7 and os.path.exists(os.path.join(inc, cells[0] + ".png")):
            cells[1] = "GENERATED"
            line = "| " + " | ".join(cells) + " |\n"
        out.append(line)
    open(todo, "w", encoding="utf-8").writelines(out)


def match(a, b):
    A, B = np.asarray(a.convert("RGB")).astype(int), np.asarray(b.convert("RGB")).astype(int)
    best = 0.0
    for dy in range(-2, 3):
        for dx in range(-2, 3):
            ya, yb, xa, xb = max(0, dy), max(0, -dy), max(0, dx), max(0, -dx)
            hh, ww = A.shape[0] - abs(dy), A.shape[1] - abs(dx)
            d = np.abs(A[ya:ya + hh, xa:xa + ww] - B[yb:yb + hh, xb:xb + ww]).max(-1)
            best = max(best, (d <= 56).mean())
    return best


def main():
    sheets, mp, cut, brief = art_sync.load_modules(16)
    ph = mp.build_sheet("characters")
    strips = {s.id: s for s in art_sync.all_strips(sheets)}

    def src(name):
        e = sheets.BY_NAME[name]
        return [ph.crop((x, y, x + w, y + h)) for x, y, w, h in sheets.frame_rects(e)]

    with tempfile.TemporaryDirectory() as tmp:
        inc = os.path.join(tmp, "incoming")
        os.makedirs(inc)
        todo = os.path.join(inc, "TODO.md")
        art_sync.write_todo(todo, inc, art_sync.all_strips(sheets), brief, sheets, {})
        walk = src("mole_walk_left")

        # 1. transparent background, soft edges, uneven spacing, an effect next to frame 2
        star = lambda d: d.ellipse((30 + 128 + 70 + 128 + 6, 20, 30 + 128 + 70 + 128 + 22, 36), fill=(255, 230, 40, 255))
        p = os.path.join(inc, "mole_walk_left.png")
        make_strip(walk, extra=star).save(p)
        s = art_frames.cut_strip(p, 3)
        check(s.mode == "alpha" and len(s.frames) == 3, "alpha background: 3 frames on uneven spacing (%s)" % s.mode)
        check(s.frames[1].mask[20:36, 30 + 128 + 70 + 128 + 6:30 + 128 + 70 + 128 + 22].any(),
              "the effect is attached to its frame")
        check(all(f.body[3] - f.body[1] <= 16 * 8 + 4 for f in s.frames), "the body is measured without the effect")
        dark = [f.mask & (s.rgb.sum(-1) < 150) for f in s.frames]
        check(all(d.sum() > 100 for d in dark), "dark outline pixels are kept on a transparent background")

        # 2. near-magenta background with pink fringes (the first batch)
        p2 = os.path.join(inc, "mole_walk_right.png")
        make_strip(src("mole_walk_right"), bg="magenta", gaps=(40, 25, 90, 40)).save(p2)
        s2 = art_frames.cut_strip(p2, 3)
        check(s2.mode == "magenta" and len(s2.frames) == 3, "near-magenta background: 3 frames")

        # 3. too few drawings: a mismatch, never a guess
        p3 = os.path.join(inc, "mole_walk_up.png")
        make_strip(src("mole_walk_up")[:2]).save(p3)
        try:
            art_frames.cut_strip(p3, 3)
            check(False, "2 drawings for 3 frames raises a mismatch")
        except art_frames.FrameCountError as ex:
            check(ex.found == 2 and ex.expected == 3, "2 drawings for 3 frames: %s" % ex)

        # 4. too many drawings (4 evenly spaced for 3 frames): not silently merged
        p4 = os.path.join(inc, "mole_walk_down.png")
        four = src("mole_walk_down") + src("mole_walk_down")[:1]
        make_strip(four, gaps=(40, 40, 40, 40, 40)).save(p4)
        try:
            art_frames.cut_strip(p4, 3)
            check(False, "4 drawings for 3 frames raises a mismatch")
        except art_frames.FrameCountError as ex:
            check(ex.found == 4, "4 drawings for 3 frames: %s" % ex)

        # 5. one frame drawn in two pieces (a big detached spray) is joined
        def spray(d):
            d.rectangle((166, 40, 216, 150), fill=(80, 160, 255, 255))
        p5 = os.path.join(inc, "cat_walk_down.png")
        make_strip(src("cat_walk_down"), gaps=(30, 110, 30), extra=spray).save(p5)
        s5 = art_frames.cut_strip(p5, 2)
        check(len(s5.frames) == 2, "a frame with a detached piece: 2 frames (%s)" % "; ".join(s5.notes))

        # 6. opaque full-bleed tile (no background at all) with transparent holes
        t = Image.new("RGBA", (130, 126), (90, 160, 60, 255))
        dd = ImageDraw.Draw(t)
        for i in range(0, 130, 16):
            dd.line((i, 0, i, 126), fill=(60, 120, 40, 255), width=3)
        dd.rectangle((50, 50, 58, 58), fill=(0, 0, 0, 0))
        tp = os.path.join(inc, "tile_spring_grass.png")
        t.save(tp)

        # 7. scale normalisation: ferret_walk_up drawn 1.5x bigger than ferret_walk_down
        make_strip(src("ferret_walk_down"), scale=8).save(os.path.join(inc, "ferret_walk_down.png"))
        make_strip(src("ferret_walk_up"), scale=12, gaps=(40, 60, 40)).save(os.path.join(inc, "ferret_walk_up.png"))

        mark_generated(todo, inc)
        rows = art_sync.read_todo(todo)
        res = ac.process(inc, rows, strips, ("GENERATED",))
        r = res["mole_walk_left"]
        check(r.imported and len(r.idx) == 3, "mole_walk_left processed (alpha background)")
        out = ac.process(inc, rows, strips, ("GENERATED",), forced_scale=8)["mole_walk_left"].rgba_strip()
        mag = Image.new("RGBA", out.size, (255, 0, 255, 255))
        mag.alpha_composite(out)
        out = mag
        sc = [match(out.crop((i * 16, 0, i * 16 + 16, 16)), f) for i, f in enumerate(walk)]
        check(min(sc) >= 0.85, "processed frames match the source at 8x (%s)" % ", ".join("%.2f" % v for v in sc))
        rgb = np.asarray(res["mole_walk_right"].rgba_strip())
        pink = (rgb[..., 3] > 0) & (rgb[..., 0] > 200) & (rgb[..., 2] > 200) & (rgb[..., 1] < 90)
        check(not pink.any(), "no pink fringe left after despill (%d px)" % pink.sum())
        check(res["mole_walk_up"].error and "mismatch" in res["mole_walk_up"].error, "mismatch kept as an error")
        check(res["mole_walk_down"].error and "found 4" in res["mole_walk_down"].error, "4-for-3 kept as an error")
        g = res["tile_spring_grass"]
        a = g.idx[0]
        check(g.imported and (a > 0).all(), "opaque tile: fills its cell, holes filled")
        mole = [x for x in res.values() if x.pal_key == "mole" and x.palette]
        check(all(len(x.palette) <= 15 for x in mole) and len({tuple(map(tuple, x.palette)) for x in mole}) == 1,
              "one shared palette of <= 15 colours for the mole group")
        idx = r.idx[0]
        edge = (idx > 0) & ~np.pad(idx > 0, 1)[2:, 1:-1]          # bottom silhouette pixels
        oc = len(r.palette)                                         # the outline colour is the last entry
        check((idx[edge] == oc).mean() > 0.9, "a 1-px outline is redrawn in the outline colour")
        fd, fu = res["ferret_walk_down"], res["ferret_walk_up"]

        def height(f):
            a = np.any(np.asarray(f.convert("RGB")) != np.array([255, 0, 255]), -1)
            ys = np.nonzero(a.any(1))[0]
            return float(ys.max() - ys.min() + 1)
        up, down = src("ferret_walk_up"), src("ferret_walk_down")
        want = 1.5 * np.median([height(f) for f in up]) / height(down[0])
        check(abs(fu.k - want) < 0.12, "ferret_walk_up drawn 1.5x bigger: normalised x%.2f (expected %.2f)" % (fu.k, want))
        hd = np.mean([m["h"] for m in fd.metrics])
        hu = np.mean([m["h"] for m in fu.metrics])
        ratio = np.mean([height(f) for f in up]) / np.mean([height(f) for f in down])
        check(abs(hu / hd - ratio) < 0.12, "after normalisation up/down keep the placeholders' proportions "
              "(%.1f / %.1f px, ratio %.2f vs %.2f)" % (hu, hd, hu / hd, ratio))

        # 8. seamless check: a tile with a hard left/right seam is flagged
        seam = Image.new("RGBA", (128, 128), (90, 160, 60, 255))
        ImageDraw.Draw(seam).rectangle((0, 0, 30, 127), fill=(200, 200, 40, 255))
        seam.save(os.path.join(inc, "tile_summer_grass.png"))
        mark_generated(todo, inc)
        rows = art_sync.read_todo(todo)
        res = ac.process(inc, rows, strips, ("GENERATED",))
        check(any("not seamless" in f for f in res["tile_summer_grass"].frame_flags), "a seam is flagged")
        check(not any("not seamless" in f for f in res["tile_spring_grass"].frame_flags), "a seamless tile is not")

        # 9. through art_sync: the report says mismatch, the good strips are imported
        args = argparse.Namespace(incoming=inc, out=os.path.join(tmp, "art"), report=None, dry_run=False,
                                  include_generated=True, char_size=16, scale=None, filter="area")
        counts, imported = art_sync.sync(args, report_print=lambda s: None)
        rep = open(os.path.join(inc, "IMPORT_REPORT.md"), encoding="utf-8").read()
        check("mole_walk_left" in imported and "mole_walk_up" not in imported, "sync imports the good strips only")
        check("| mole_walk_up | GENERATED | error: frame-count mismatch" in rep, "sync reports the mismatch")
    print("art_consistency: %s" % ("all passed" if not failures else "%d FAILED" % failures))
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Leady Squid: the shared art tools with --game leadysquid, end to end on fake "AI" strips.

- TODO.md: every strip has a row, in the Bomber Mole format; statuses and notes survive a rewrite;
- sync: VALIDATED strips drawn at 8x (from the placeholders, slightly blurred) are cut, scaled,
  palettised and pasted into the sheets; other cells keep their placeholder; the report is written;
- the review tool (tools/art_review.py) processes the game's strips;
- the placeholder tool (tools/make_placeholders.py --game leadysquid) runs.
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/leadysquid/LICENSE.
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.dirname(HERE)
ROOT = os.path.abspath(os.path.join(GAME, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import art_sync  # noqa: E402

fails = 0


def check(cond, msg):
    global fails
    print("  %s %s" % ("ok  " if cond else "FAIL", msg))
    if not cond:
        fails += 1


def fake_ai(sheet_img, rects, path):
    """The frames of a placeholder strip, drawn at 8x on magenta with soft edges, 40 px apart."""
    w, h = rects[0][2], rects[0][3]
    out = Image.new("RGB", (len(rects) * (w * 8 + 40) + 40, h * 8 + 80), (255, 0, 255))
    for i, (x, y, ww, hh) in enumerate(rects):
        fr = sheet_img.crop((x, y, x + ww, y + hh)).resize((ww * 8, hh * 8), Image.NEAREST)
        a = np.asarray(fr.convert("RGB")).copy()
        mask = ~((a[..., 0] == 255) & (a[..., 1] == 0) & (a[..., 2] == 255))
        big = Image.fromarray(a).filter(ImageFilter.GaussianBlur(1.2))
        m = Image.fromarray((mask * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.0))
        out.paste(big, (40 + i * (ww * 8 + 40), 40), m)
    out.save(path)


def main():
    game = art_sync.game_module("leadysquid")
    import ls_sheets
    tmp = tempfile.mkdtemp(prefix="ls-art-")
    try:
        inc, out = os.path.join(tmp, "incoming"), os.path.join(tmp, "art")
        os.makedirs(inc)
        todo = os.path.join(inc, "TODO.md")
        n = game.write_todo(todo, {})
        rows = art_sync.read_todo(todo)
        st = game.strips()
        check(n == len(st) == len(rows) and set(rows) == set(st), "TODO.md: one row per strip (%d)" % n)
        check(all(r["status"] == "TODO" for r in rows.values()), "every row starts as TODO")
        text = open(todo, encoding="utf-8").read()
        check("| ID | Status | Sheet | Frame size | Frames | Description / prompt | Notes |" in text and
              "Instructions for the image agent" in text and "Model sheet" in text,
              "the Bomber Mole format: instructions, style guide, model sheet, table")
        # the agent generates two strips, the owner validates them
        sheets = {k: Image.open(os.path.join(GAME, "art", v["file"])).convert("RGB") for k, v in ls_sheets.SHEETS.items()}
        for sid in ("squid_tilt", "coral_cap_bottom"):
            e = ls_sheets.entry(sid)
            fake_ai(sheets[e.sheet], ls_sheets.frame_rects(e), os.path.join(inc, sid + ".png"))
        lines = open(todo, encoding="utf-8").read().split("\n")
        for i, l in enumerate(lines):
            for sid in ("squid_tilt", "coral_cap_bottom"):
                if l.startswith("| %s | TODO |" % sid):
                    cells = l.split("|")
                    cells[2] = " VALIDATED "
                    cells[7] = " Owner: ok "
                    lines[i] = "|".join(cells)
        open(todo, "w", encoding="utf-8").write("\n".join(lines))
        game.write_todo(todo, art_sync.read_todo(todo))
        rows = art_sync.read_todo(todo)
        check(rows["squid_tilt"]["status"] == "VALIDATED" and "Owner: ok" in rows["squid_tilt"]["notes"],
              "a rewrite keeps the statuses and notes")
        a = argparse.Namespace(command="sync", incoming=inc, out=out, report=None, dry_run=False,
                               include_generated=False, scale=None)
        code = game.main(a)
        check(code == 0, "sync ran without errors")
        rep = open(os.path.join(inc, "IMPORT_REPORT.md"), encoding="utf-8").read()
        check("squid_tilt | VALIDATED | imported" in rep and "coral_cap_bottom | VALIDATED | imported" in rep,
              "the report lists the two imported strips")
        new = Image.open(os.path.join(out, "sprites.png")).convert("RGB")
        e = ls_sheets.entry("squid_tilt")
        ious, shifts = [], []
        for (x, y, w, h) in ls_sheets.frame_rects(e):
            A = np.asarray(sheets["sprites"].crop((x, y, x + w, y + h)), np.int32)
            B = np.asarray(new.crop((x, y, x + w, y + h)), np.int32)
            ma, mb = (A != [255, 0, 255]).any(-1), (B != [255, 0, 255]).any(-1)
            best = (0, None)
            for dy in range(-2, 3):
                for dx in range(-2, 3):
                    s = np.roll(np.roll(mb, dy, 0), dx, 1)
                    iou = (ma & s).sum() / float((ma | s).sum())
                    best = max(best, (iou, (dx, dy)))
            ious.append(best[0])
            shifts.append(best[1])
        check(min(ious) > 0.72 and all(abs(dx) <= 1 and abs(dy) <= 1 for dx, dy in shifts),
              "the imported squid frames match the drawings (silhouette overlap >= %.2f, shifts %s)" %
              (min(ious), shifts))
        other = ls_sheets.entry("digits")
        x, y, w, h = ls_sheets.frame_rects(other)[3]
        check(np.array_equal(np.asarray(sheets["sprites"].crop((x, y, x + w, y + h))),
                             np.asarray(new.crop((x, y, x + w, y + h)))), "a cell not imported keeps its placeholder")
        for f in ("obstacles.png", "props.png", "title_logo.png", "tiles/midground.png", "tiles/seabed.png"):
            check(os.path.exists(os.path.join(out, f)), "written: " + f)
        # the review tool on the game's strips
        import art_review
        rv = art_review.Review(inc, os.path.join(tmp, "cache"), 24, game)
        rv.process()
        check(rv.error is None and rv.index.get("squid_tilt", {}).get("imported"),
              "art_review processes the game's strips (%s)" % (rv.error or "ok"))
        # the placeholders through the shared tool
        r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "make_placeholders.py"), "--game",
                            "leadysquid", "--out", os.path.join(tmp, "ph")], capture_output=True, text=True)
        check(r.returncode == 0 and os.path.exists(os.path.join(tmp, "ph", "sprites.png")),
              "tools/make_placeholders.py --game leadysquid")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print("leadysquid art tools: %d failed" % fails)
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()

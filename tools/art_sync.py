#!/usr/bin/env python3
"""Art pipeline between an image-generation agent, the owner and the game.

The drop folder (default games/bombermole/art/incoming/) holds TODO.md and one
PNG per animation strip, <id>.png. The image agent generates TODO rows and
marks them GENERATED; the owner marks them VALIDATED or REJECTED. This tool:

  todo           create TODO.md, or add rows for new strips (never changes a
                 Status or Notes cell that exists)
  sync           import every VALIDATED strip: find its frames on the
                 (almost) magenta background, check the frame count, despill,
                 downscale into the frame size, reduce the colours per palette
                 group, and ASSEMBLE the game sheets (art/*.png): validated art
                 replaces the placeholder cell by cell, everything else keeps
                 its placeholder, so the game always builds. Writes
                 IMPORT_REPORT.md next to TODO.md. Never edits TODO.md.
                 --dry-run shows what would change. --include-generated also
                 takes GENERATED rows (for previews; use --out elsewhere).
  preview        contact sheet (4x) and animated GIFs (4x) of the imported
                 strips, for the owner's review
  import-sheet   one-off: cut a whole AI sheet made before this workflow into
                 <id>.png strips (IDs in reading order from a map file) and
                 mark those rows GENERATED with a note

    python3 tools/art_sync.py todo
    python3 tools/art_sync.py sync [--dry-run] [--include-generated] [--out DIR] [--char-size 24]
    python3 tools/art_sync.py preview [--include-generated] [--out build/art-preview]
    python3 tools/art_sync.py import-sheet SHEET.png --map MAP.txt --note "from first-batch sheet"

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import argparse
import datetime
import fnmatch
import os
import re
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
GAME = os.path.join(ROOT, "games", "bombermole")
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(GAME, "tools"))

STATUSES = ("TODO", "GENERATED", "VALIDATED", "REJECTED")
FILL_GROUPS = ("terrain", "propbg")          # strips whose frames fill their cell
LOGO_SIZE = (256, 64)


def load_modules(char_size):
    """sheets.py reads BM_CHAR_SIZE when imported."""
    if char_size:
        os.environ["BM_CHAR_SIZE"] = str(char_size)
    import sheets
    import make_placeholders
    import cut_ai_sheet
    import art_brief
    return sheets, make_placeholders, cut_ai_sheet, art_brief


# ---- strips -------------------------------------------------------------------------------
class Strip:
    def __init__(self, sid, entry, season, w, h, frames, sheet, group):
        self.id, self.entry, self.season = sid, entry, season
        self.w, self.h, self.frames, self.sheet, self.group = w, h, frames, sheet, group


def brief_module():
    import art_brief
    return art_brief


def all_strips(sheets):
    out = [Strip("title_logo", None, 0, LOGO_SIZE[0], LOGO_SIZE[1], 1, "title", "logo")]
    for e in sheets.ENTRIES:
        if e.sheet == "tiles":
            continue
        out.append(Strip(e.name, e, 0, e.w, e.h, e.frames, e.sheet, e.group))
    for s, season in enumerate(sheets.SEASONS):
        for e in sheets.entries("tiles"):
            out.append(Strip("tile_%s_%s" % (season, e.name), e, s, e.w, e.h, 1, "tiles", "terrain"))
    for sid, sheet, w, h, n, _ in getattr(brief_module(), "EXTRAS", []):
        out.append(Strip(sid, None, 0, w, h, n, sheet, "extra"))
    return out


def prompt(strip, brief, sheets):
    if strip.group == "extra":
        return {x[0]: x[5] for x in brief.EXTRAS}[strip.id]
    if strip.id == "title_logo":
        return brief.LOGO[1]
    if strip.sheet == "tiles":
        season = sheets.SEASONS[strip.season]
        return ("%s tile, %s: %s. One frame, %dx%d (draw about %dx%d). %s palette. Must tile seamlessly; "
                "no magenta inside." % (season.capitalize(), strip.entry.name.replace("_", " "),
                                        brief.TILES[strip.entry.name], strip.w, strip.h, strip.w * 8, strip.h * 8,
                                        season.capitalize()))
    subject, frames = brief.S.get(strip.id, (strip.entry.desc, ["frame %d" % (i + 1) for i in range(strip.frames)]))
    if len(frames) != strip.frames:
        frames = (frames + ["frame %d" % (i + 1) for i in range(strip.frames)])[:strip.frames]
    fill = "Fills the whole cell edge to edge, no magenta inside. " if strip.group in FILL_GROUPS or \
        strip.id == "hud_panel" else "Flat magenta around the drawing. "
    if strip.frames == 1:
        fr = "One frame"
    else:
        fr = "%d frames left to right, evenly spaced: %s" % (
            strip.frames, "; ".join("%d) %s" % (i + 1, f) for i, f in enumerate(frames)))
    return "%s. %s. Frame size %dx%d (draw each about %dx%d). %s" % (
        subject, fr, strip.w, strip.h, strip.w * 8, strip.h * 8, fill.strip())


def ordered(strips, brief):
    """Strips grouped under the brief's priority headings."""
    left = list(strips)
    groups = []
    # the catch-all group ("*") takes only what no other group claims
    for title, pats in sorted(brief.GROUPS, key=lambda g: g[1] == ["*"]):
        rows = []
        for p in pats:
            for s in list(left):
                if fnmatch.fnmatch(s.id, p):
                    rows.append(s)
                    left.remove(s)
        groups.append((title, rows))
    groups.sort(key=lambda g: [t for t, _ in brief.GROUPS].index(g[0]))
    if left:
        groups[-1][1].extend(left)
    return groups


# ---- TODO.md ---------------------------------------------------------------------------------
ROW_RE = re.compile(r"^\|\s*([a-z0-9_]+)\s*\|\s*([A-Z]+)\s*\|(.*)\|\s*$")


def read_todo(path):
    """{id: {"status", "notes", "line"}} from TODO.md."""
    rows = {}
    if not os.path.exists(path):
        return rows
    for line in open(path, encoding="utf-8"):
        m = ROW_RE.match(line.rstrip("\n"))
        if not m or m.group(1) in ("id",):
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) < 7:
            continue
        rows[cells[0]] = {"status": cells[1], "notes": cells[6], "cells": cells}
    return rows


def write_todo(path, incoming, strips, brief, sheets, keep):
    lines = []
    rel = os.path.abspath(incoming)
    win = rel.replace("/mnt/c/", "C:/").replace("/", "\\") if rel.startswith("/mnt/") else rel
    lines += ["# Bomber Mole art TODO", "",
              "Drop folder (this folder): `%s`" % win, "",
              "## Instructions for the image agent", "",
              "Workflow:",
              "1. Pick rows whose Status is **TODO** (top to bottom: the groups are in priority order) or "
              "**REJECTED**.",
              "2. Generate ONE PNG per row, named `<ID>.png`, in this folder. When you regenerate a row, first "
              "rename the old `<ID>.png` to `<ID>.v1.png` (then `.v2.png`, and so on), then write the new "
              "`<ID>.png`.",
              "3. Set the row's Status to **GENERATED** and write a short note in Notes if useful.",
              "4. For a **REJECTED** row, read the owner's note in Notes, regenerate taking it into account, "
              "then set the Status back to **GENERATED** (keep the owner's note, add yours after it).",
              "",
              "Rules:",
              "- Edit ONLY the Status and Notes cells of the rows you handle. Do not add, remove or reorder rows,"
              " and do not touch the other columns.",
              "- NEVER mark anything VALIDATED: only the owner validates (VALIDATED or REJECTED + a note).",
              "- One horizontal strip per PNG: the frames sit left to right in the order given, evenly spaced, "
              "all the same size.",
              "- Background: flat magenta `#FF00FF` everywhere around the drawings. Tiles are the exception: "
              "each tile fills its whole cell edge to edge, with no magenta inside.",
              "- Scale: draw at 8x the final size (a 16x16 frame about 128x128 pixels, a 32x32 frame about "
              "256x256). The title logo is drawn at 4x (1024x256).",
              "- `tools/art_sync.py` imports VALIDATED rows into the game and writes `IMPORT_REPORT.md` here; "
              "it never edits this file.", "",
              "## Style guide", "", brief.STYLE,
              "### Model sheet", "", brief.MODEL_SHEET,
              "### Season palettes", "", brief.SEASONS, ""]
    for title, rows in ordered(strips, brief):
        lines += ["## %s" % title, "",
                  "| ID | Status | Sheet | Frame size | Frames | Description / prompt | Notes |",
                  "|---|---|---|---|---|---|---|"]
        for s in rows:
            old = keep.get(s.id)
            status = old["status"] if old and old["status"] in STATUSES else "TODO"
            notes = old["notes"] if old else ""
            desc = prompt(s, brief, sheets).replace("|", "/")
            sheet = "title logo" if s.id == "title_logo" else s.sheet if s.group == "extra" else s.sheet + ".png"
            lines.append("| %s | %s | %s | %dx%d | %d | %s | %s |" % (s.id, status, sheet, s.w, s.h, s.frames,
                                                                    desc, notes.replace("|", "/")))
        lines.append("")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))
    return sum(len(r) for _, r in ordered(strips, brief))


def set_status(path, updates):
    """Only used by import-sheet (one-off): {id: (status, note)} for TODO rows."""
    out = []
    for line in open(path, encoding="utf-8"):
        m = ROW_RE.match(line.rstrip("\n"))
        if m and m.group(1) in updates:
            cells = [c.strip() for c in line.strip().strip("|").split("|")]
            st, note = updates[m.group(1)]
            if cells[1] == "TODO":
                cells[1] = st
                cells[6] = (cells[6] + " " + note).strip() if cells[6] else note
                line = "| " + " | ".join(cells) + " |\n"
        out.append(line)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.writelines(out)


# ---- frame detection and import -------------------------------------------------------------
def detect_frames(img, strip, cut):
    """Boxes of the strip's frames, and a warning. Raises ValueError on a mismatch."""
    fg, rgb, bg = cut.foreground_mask(img)
    solid = strip.group in FILL_GROUPS or strip.id == "hud_panel"
    if not fg.any():
        raise ValueError("empty image")
    boxes = cut.find_sprites(fg, solid=solid, report=lambda s: None)
    boxes = sorted(boxes, key=lambda b: (b[0] + b[2]) / 2)
    warn = ""
    if len(boxes) != strip.frames:
        # frames are evenly spaced: split the width and take each column's bounding box,
        # unless a drawing straddles a split line (then the count really is wrong)
        H, W = fg.shape
        cuts = [W * i // strip.frames for i in range(1, strip.frames)]
        straddle = any(b[0] < c - W // (4 * strip.frames) and b[2] > c + W // (4 * strip.frames)
                       for b in boxes for c in cuts)
        even = [] if not straddle else None
        for i in range(strip.frames if even is not None else 0):
            x0, x1 = W * i // strip.frames, W * (i + 1) // strip.frames
            ys, xs = np.nonzero(fg[:, x0:x1])
            if len(xs) == 0:
                even = None
                break
            even.append([x0 + xs.min(), ys.min(), x0 + xs.max() + 1, ys.max() + 1])
        if even is None:
            raise ValueError("frame-count mismatch: found %d, expected %d" % (len(boxes), strip.frames))
        warn = "found %d blobs for %d frames: used an even split" % (len(boxes), strip.frames)
        boxes = even
    return boxes, fg, rgb, warn


def fill_ratio(path, strip, cut):
    """AI pixels per final pixel if the strip's biggest frame filled its cell."""
    img = Image.open(path).convert("RGBA")
    boxes, fg, rgb, warn = detect_frames(img, strip, cut)
    return max(max((b[2] - b[0]) / max(1, strip.w - 1), (b[3] - b[1]) / max(1, strip.h - 1)) for b in boxes)


def import_strip(path, strip, cut, scale=None, method="area"):
    """-> (frames [(rgb float array h x w x 3, alpha bool)], warning, raw box sizes)."""
    img = Image.open(path).convert("RGBA")
    boxes, fg, rgb, warn = detect_frames(img, strip, cut)
    core = cut.erode(fg, max(1, int(round(min(img.size) / 700))))
    P = cut.prepare(rgb, fg, core)
    solid = strip.group in FILL_GROUPS or strip.id == "hud_panel"
    out = []
    sc = scale or max(max((b[2] - b[0]) / max(1, strip.w - (0 if solid else 1)),
                          (b[3] - b[1]) / max(1, strip.h - (0 if solid else 1))) for b in boxes)
    for b in boxes:
        if solid and not scale:
            win = (b[0], b[1], b[2], b[3])
        else:
            ww, wh = strip.w * sc, strip.h * sc
            x0 = b[0] - round((ww - (b[2] - b[0])) / 2 / sc) * sc
            y0 = (b[3] - wh) if strip.sheet == "characters" else b[1] - round((wh - (b[3] - b[1])) / 2 / sc) * sc
            x1, y1 = x0 + ww, y0 + wh
            a = cut.EDGE_ANCHOR.get(strip.id, "")
            if "l" in a: x0, x1 = b[0], b[0] + ww
            if "r" in a: x0, x1 = b[2] - ww, b[2]
            if "t" in a: y0, y1 = b[1], b[1] + wh
            if "b" in a: y0, y1 = b[3] - wh, b[3]
            if "X" in a: x0, x1 = b[0], b[2]
            if "Y" in a: y0, y1 = b[1], b[3]
            win = (x0, y0, x1, y1)
        col, alpha = cut.downscale(P, win, strip.w, strip.h, method)
        out.append((col, alpha))
    return out, warn, sc


def import_logo(path, cut):
    img = Image.open(path).convert("RGBA")
    fg, rgb, bg = cut.foreground_mask(img)
    ys, xs = np.nonzero(fg)
    box = (xs.min(), ys.min(), xs.max() + 1, ys.max() + 1)
    bw, bh = box[2] - box[0], box[3] - box[1]
    k = min(LOGO_SIZE[0] / bw, LOGO_SIZE[1] / bh)
    tw, th = max(8, int(bw * k)), max(8, int(bh * k))
    rgba = np.dstack([np.clip(rgb, 0, 255), fg * 255]).astype(np.uint8)
    small = Image.fromarray(rgba, "RGBA").crop(box).resize((tw, th), Image.BOX)
    logo = Image.new("RGBA", LOGO_SIZE, (255, 0, 255, 255))
    a = np.asarray(small)
    mask = Image.fromarray(((a[..., 3] >= 128) * 255).astype(np.uint8))
    logo.paste(small.convert("RGB"), ((LOGO_SIZE[0] - tw) // 2, (LOGO_SIZE[1] - th) // 2), mask)
    return logo.convert("RGB")


def placeholder_logo():
    """Blocky gold letters with a dark outline and a red drop shadow."""
    from gen_font import G as FONT
    im = Image.new("RGB", LOGO_SIZE, (255, 0, 255))
    d = ImageDraw.Draw(im)
    text = "BOMBER MOLE"
    sc = 4
    x0 = (LOGO_SIZE[0] - len(text) * 6 * sc + sc) // 2
    for layer, (dx, dy, col) in enumerate(((3, 3, (150, 30, 20)), (0, 0, (255, 200, 40)))):
        for i, ch in enumerate(text):
            g = FONT[ch]
            for y, row in enumerate(g):
                for x, c in enumerate(row):
                    if c == "#":
                        px, py = x0 + (i * 6 + x) * sc + dx, 18 + y * sc + dy
                        if layer:
                            d.rectangle((px - 1, py - 1, px + sc, py + sc), fill=(40, 20, 10))
                        d.rectangle((px, py, px + sc - 1, py + sc - 1), fill=col)
    for i, ch in enumerate(text):              # re-draw the gold on top of the outlines
        g = FONT[ch]
        for y, row in enumerate(g):
            for x, c in enumerate(row):
                if c == "#":
                    px, py = x0 + (i * 6 + x) * sc, 18 + y * sc
                    d.rectangle((px, py, px + sc - 1, py + sc - 2), fill=(255, 200, 40))
                    d.rectangle((px, py + sc - 1, px + sc - 1, py + sc - 1), fill=(220, 150, 30))
    return im


def colours_555(frames):
    return {tuple(int(v) >> 3 for v in col[j, i]) for col, alpha in frames
            for j in range(alpha.shape[0]) for i in range(alpha.shape[1]) if alpha[j, i]}


# ---- sync ------------------------------------------------------------------------------------------
def sync(args, report_print=print):
    sheets, mp, cut, brief = load_modules(args.char_size)
    incoming = args.incoming
    todo = os.path.join(incoming, "TODO.md")
    rows = read_todo(todo)
    if not rows:
        raise SystemExit("no rows in %s (run: art_sync.py todo)" % todo)
    strips = {s.id: s for s in all_strips(sheets)}
    take = ("VALIDATED", "GENERATED") if args.include_generated else ("VALIDATED",)
    cons = None
    if getattr(args, "consistency", True):      # tools/art_consistency.py (the default import)
        import art_consistency
        cons = art_consistency.import_for_sync(incoming, rows, strips, take, args.scale)
        rows = {}                                # skips the legacy import below
    # one scale per palette group, so the frames of a character keep one size across its strips
    group_scale = {}
    if not args.scale:
        for sid, r in rows.items():
            s = strips.get(sid)
            if r["status"] not in take or not s or sid == "title_logo" or s.group in FILL_GROUPS or s.group == "extra":
                continue
            path = os.path.join(incoming, sid + ".png")
            try:
                k = fill_ratio(path, s, cut)
            except Exception:   # noqa: BLE001 - reported below
                continue
            group_scale[s.group] = max(group_scale.get(s.group, 0), k)
    report, imported, counts = [], {}, {"imported": 0, "missing": 0, "error": 0}
    for sid, r in rows.items():
        if r["status"] not in take or sid not in strips:
            continue
        if strips[sid].group == "extra":
            report.append((sid, r["status"], "not imported: the game still draws this itself (import to be added)"))
            continue
        s = strips[sid]
        path = os.path.join(incoming, sid + ".png")
        if not os.path.exists(path):
            report.append((sid, r["status"], "missing: %s.png not found" % sid))
            counts["missing"] += 1
            continue
        try:
            if sid == "title_logo":
                imported[sid] = import_logo(path, cut)
                report.append((sid, r["status"], "imported (logo, %dx%d)" % LOGO_SIZE))
            else:
                frames, warn, sc = import_strip(path, s, cut, args.scale or group_scale.get(s.group), args.filter)
                imported[sid] = frames
                report.append((sid, r["status"], "imported: %d frame(s), scale %.1fx%s" %
                               (len(frames), sc, "; " + warn if warn else "")))
            counts["imported"] += 1
        except Exception as ex:   # noqa: BLE001 - reported per row
            report.append((sid, r["status"], "error: %s" % ex))
            counts["error"] += 1

    # colour reduction per palette group (15 colours; terrain: 60 per season)
    groups = {}
    for sid, frames in imported.items():
        if sid == "title_logo":
            continue
        s = strips[sid]
        key = (s.group, s.season) if s.group == "terrain" else s.group
        groups.setdefault(key, []).append(sid)
    notes = {}
    for key, sids in groups.items():
        pix = [tuple(int(v) for v in col[j, i]) for sid in sids for col, alpha in imported[sid]
               for j in range(alpha.shape[0]) for i in range(alpha.shape[1]) if alpha[j, i]]
        m = cut.reduce_colors(pix, 60 if isinstance(key, tuple) else 15)
        for sid in sids:
            before = len(colours_555(imported[sid]))
            new = []
            for col, alpha in imported[sid]:
                c2 = col.copy()
                for j in range(alpha.shape[0]):
                    for i in range(alpha.shape[1]):
                        if alpha[j, i]:
                            c2[j, i] = m[tuple(int(v) for v in col[j, i])]
                new.append((c2, alpha))
            imported[sid] = new
            after = len(colours_555(new))
            if before > after:
                notes[sid] = "colours reduced from %d to %d" % (before, after)
    report = [(sid, st, msg + ("; " + notes[sid] if sid in notes else "")) for sid, st, msg in report]
    if cons is not None:
        imported, report, counts = cons

    # assemble the sheets: placeholders + imported cells
    out_dir = args.out
    written = []
    for sheet in sheets.SHEETS:
        im = mp.build_sheet(sheet)
        n = 0
        for s in strips.values():
            if s.sheet != sheet or s.id not in imported:
                continue
            rects = sheets.frame_rects(s.entry, s.season)
            for (x, y, w, h), (col, alpha) in zip(rects, imported[s.id]):
                tile = Image.new("RGB", (w, h), sheets.MAGENTA)
                tp = tile.load()
                for j in range(h):
                    for i in range(w):
                        if alpha[j, i]:
                            tp[i, j] = tuple(int(v) for v in col[j, i])
                im.paste(tile, (x, y))
                n += 1
        written.append((sheet, n, im))
    logo = imported.get("title_logo") or placeholder_logo()

    stamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")
    lines = ["# Art import report", "",
             "Written by `tools/art_sync.py sync` on %s. Rows taken: %s. The sheets were written to `%s`." %
             (stamp, " + ".join(take), os.path.relpath(out_dir, ROOT)), "",
             "Summary: %d imported, %d missing, %d errors. Every other cell keeps its placeholder." %
             (counts["imported"], counts["missing"], counts["error"]), "",
             "| ID | Status | Import |", "|---|---|---|"]
    lines += ["| %s | %s | %s |" % r for r in sorted(report)]
    for sid, st, msg in sorted(report):
        report_print("  %-28s %-10s %s" % (sid, st, msg))
    report_print("sync: %d imported, %d missing, %d errors -> %s" %
                 (counts["imported"], counts["missing"], counts["error"], out_dir))
    if args.dry_run:
        report_print("(dry run: nothing written)")
        for sheet, n, _ in written:
            report_print("  would write %s/%s (%d cells from imported art)" % (out_dir, sheets.SHEETS[sheet]["file"], n))
        return counts, imported
    os.makedirs(out_dir, exist_ok=True)
    for sheet, n, im in written:
        im.save(os.path.join(out_dir, sheets.SHEETS[sheet]["file"]))
    logo.save(os.path.join(out_dir, "title_logo.png"))
    rep = args.report or os.path.join(incoming, "IMPORT_REPORT.md")
    with open(rep, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    return counts, imported


# ---- preview -----------------------------------------------------------------------------------------
def preview(args):
    args.dry_run = True
    sheets, mp, cut, brief = load_modules(args.char_size)
    counts, imported = sync(args, report_print=lambda s: None)
    out = args.out
    os.makedirs(out, exist_ok=True)
    Z = 4
    tiles = []
    for sid, frames in sorted(imported.items()):
        if sid == "title_logo":
            frames_img = [frames]
        else:
            frames_img = []
            for col, alpha in frames:
                h, w = alpha.shape
                im = Image.new("RGB", (w, h), (40, 40, 56))
                p = im.load()
                for j in range(h):
                    for i in range(w):
                        if alpha[j, i]:
                            p[i, j] = tuple(int(v) for v in col[j, i])
                frames_img.append(im)
        big = [f.resize((f.width * Z, f.height * Z), Image.NEAREST) for f in frames_img]
        if len(big) > 1:
            big[0].save(os.path.join(out, sid + ".gif"), save_all=True, append_images=big[1:],
                        duration=160, loop=0)
        tiles.append((sid, big))
    if not tiles:
        print("preview: nothing imported")
        return
    W = 1024
    x = y = 0
    rowh = 0
    placed = []
    for sid, big in tiles:
        w = sum(b.width for b in big) + 4 * (len(big) - 1)
        h = max(b.height for b in big) + 14
        if x + w > W:
            x, y = 0, y + rowh + 8
            rowh = 0
        placed.append((sid, big, x, y))
        x += max(w, 8 * len(sid)) + 12
        rowh = max(rowh, h)
    sheet = Image.new("RGB", (W, y + rowh + 8), (24, 24, 32))
    d = ImageDraw.Draw(sheet)
    for sid, big, x, y in placed:
        d.text((x, y), sid, fill=(230, 230, 230))
        cx = x
        for b in big:
            sheet.paste(b, (cx, y + 12))
            cx += b.width + 4
    p = os.path.join(out, "contact_sheet.png")
    sheet.save(p)
    print("preview: %d strips -> %s (+ animated GIFs)" % (len(tiles), p))


# ---- import-sheet (one-off) --------------------------------------------------------------------
def import_sheet(args):
    sheets, mp, cut, brief = load_modules(args.char_size)
    strips = {s.id: s for s in all_strips(sheets)}
    ids = [t for line in open(args.map, encoding="utf-8") for t in line.split("#")[0].split()]
    img = Image.open(args.sheet).convert("RGBA")
    fg, rgb, bg = cut.foreground_mask(img)
    solid = all(i == "skip" or (i in strips and strips[i].group in FILL_GROUPS) for i in ids)
    boxes = [b for r in cut.reading_rows(cut.find_sprites(fg, solid=solid, report=lambda s: None)) for b in r]
    print("%s: %d sprites detected, %d in the map" % (args.sheet, len(boxes), len(ids)))
    if len(boxes) != len(ids):
        print("  COUNT MISMATCH: mapping the first %d" % min(len(boxes), len(ids)))
    per = {}
    for b, sid in zip(boxes, ids):
        if sid == "skip":
            continue
        if sid not in strips:
            raise SystemExit("unknown strip id %s" % sid)
        per.setdefault(sid, []).append(b)
    src = Image.fromarray(np.dstack([np.clip(rgb, 0, 255), np.full(fg.shape, 255)]).astype(np.uint8), "RGBA")
    updates = {}
    for sid, bs in per.items():
        s = strips[sid]
        if len(bs) != s.frames:
            print("  %s: %d frames found, %d expected (skipped)" % (sid, len(bs), s.frames))
            continue
        fw = max(b[2] - b[0] for b in bs) + (0 if s.group in FILL_GROUPS else 24)
        fh = max(b[3] - b[1] for b in bs) + (0 if s.group in FILL_GROUPS else 24)
        gap = 32
        strip = Image.new("RGB", (len(bs) * fw + (len(bs) + 1) * gap, fh + 2 * gap), (255, 0, 255))
        for i, b in enumerate(bs):
            crop = src.crop(tuple(b))
            m = Image.fromarray((fg[b[1]:b[3], b[0]:b[2]] * 255).astype(np.uint8))
            ox = gap + i * (fw + gap) + (fw - crop.width) // 2
            oy = gap + (fh - crop.height if s.sheet == "characters" else (fh - crop.height) // 2)
            strip.paste(crop.convert("RGB"), (ox, oy), m)
        dst = os.path.join(args.incoming, sid + ".png")
        if os.path.exists(dst):
            v = 1
            while os.path.exists(os.path.join(args.incoming, "%s.v%d.png" % (sid, v))):
                v += 1
            os.rename(dst, os.path.join(args.incoming, "%s.v%d.png" % (sid, v)))
        strip.save(dst)
        updates[sid] = ("GENERATED", args.note)
    set_status(os.path.join(args.incoming, "TODO.md"), updates)
    print("  wrote %d strips, marked GENERATED: %s" % (len(updates), args.note))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("command", choices=["todo", "sync", "preview", "import-sheet"])
    ap.add_argument("sheet", nargs="?", help="import-sheet: the whole AI sheet")
    ap.add_argument("--incoming", default=os.path.join(GAME, "art", "incoming"))
    ap.add_argument("--out", help="sync: sheets folder (default games/bombermole/art); preview: output folder")
    ap.add_argument("--report", help="sync: report path (default <incoming>/IMPORT_REPORT.md)")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--include-generated", action="store_true")
    ap.add_argument("--char-size", type=int, default=0, help="16, 24 or 32 (default: BM_CHAR_SIZE or 16)")
    ap.add_argument("--scale", type=float, help="force AI pixels per final pixel")
    ap.add_argument("--filter", choices=["area", "nearest"], default="area")
    ap.add_argument("--legacy-import", dest="consistency", action="store_false",
                    help="sync: the old import (one scale per group) instead of tools/art_consistency.py")
    ap.add_argument("--map", help="import-sheet: strip ids in reading order")
    ap.add_argument("--note", default="from first-batch sheet")
    a = ap.parse_args()
    if a.command == "todo":
        sheets, mp, cut, brief = load_modules(a.char_size)
        os.makedirs(a.incoming, exist_ok=True)
        path = os.path.join(a.incoming, "TODO.md")
        n = write_todo(path, a.incoming, all_strips(sheets), brief, sheets, read_todo(path))
        print("TODO: %d rows -> %s" % (n, path))
    elif a.command == "sync":
        a.out = a.out or os.path.join(GAME, "art")
        counts, _ = sync(a)
        sys.exit(1 if counts["error"] else 0)
    elif a.command == "preview":
        a.out = a.out or os.path.join(ROOT, "build", "art-preview")
        preview(a)
    else:
        if not a.sheet or not a.map:
            raise SystemExit("import-sheet needs SHEET.png and --map")
        import_sheet(a)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Leady Squid's side of the shared art tools (tools/art_sync.py, tools/art_review.py,
tools/art_consistency.py): the strips of TODO.md, the TODO writer and the import.

    python3 tools/art_sync.py --game leadysquid todo            # create / extend art/incoming/TODO.md
    python3 tools/art_sync.py --game leadysquid sync [--dry-run] [--include-generated] [--out DIR]
    python3 tools/art_review.py --game leadysquid                # the owner's review tool

The workflow and the TODO.md format are Bomber Mole's (docs/art-workflow.md): one PNG per strip,
TODO -> GENERATED (image agent) -> VALIDATED / REJECTED (owner), the consistency pass of
tools/art_consistency.py, placeholders kept for every cell not imported.
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/leadysquid/LICENSE.
"""
import datetime
import fnmatch
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.dirname(HERE)
ROOT = os.path.abspath(os.path.join(GAME, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "tools"))
import ls_sheets  # noqa: E402
import ls_art_brief as art_brief  # noqa: E402

INCOMING = os.path.join(GAME, "art", "incoming")
ART = os.path.join(GAME, "art")
TITLE = art_brief.TITLE
BG_DIRS = ["games/leadysquid/docs/screenshots"]
STATUSES = ("TODO", "GENERATED", "VALIDATED", "REJECTED")


def strips(char_size=0):
    """{id: Strip} in the form tools/art_consistency.py expects. The squid strips are one
    character family (one drawing scale, the tilt strip as reference, centred on the body, since
    its frames are rotations); the bodies are solid tiles; the rest are sprites fitted per strip."""
    import art_sync
    out = {}
    s = art_sync.Strip("title_logo", None, 0, 256, 64, 1, "title", "logo")
    out[s.id] = s
    for e in ls_sheets.ENTRIES:
        if e.group == "squid":
            s = art_sync.Strip(e.name, e, 0, e.w, e.h, e.frames, "characters", "squid")
            s.anchor, s.reference = "center", e.name == "squid_tilt"
            # the cell centre is the hitbox centre, not the drawing's: the importer centres the AI
            # drawing like the placeholder's (the offset of its silhouette centre, per frame), and the
            # squid keeps the placeholder's size (about 24x20 in its 32x32 cell)
            import make_art
            s.center_offsets = []
            for i in range(e.frames):
                cv = make_art.sprite_frame(e, i)
                pts = [(x, y) for y in range(cv.h) for x in range(cv.w) if cv.p[y][x] is not None]
                ys = [p[1] for p in pts]
                s.center_offsets.append((sum(p[0] + 0.5 for p in pts) / len(pts) - e.w / 2.0,
                                         (min(ys) + max(ys) + 1) / 2.0 - e.h / 2.0))
                if s.reference and i == 0:
                    s.target_h = max(ys) - min(ys) + 1
        elif e.name.endswith("_body"):
            s = art_sync.Strip(e.name, e, ls_sheets.THEMES.index(e.name.split("_")[0]), e.w, e.h, e.frames,
                               "obstacles", "terrain")
        else:
            s = art_sync.Strip(e.name, e, 0, e.w, e.h, e.frames, e.sheet, e.group)
        out[s.id] = s
    return out


def prompt(s):
    if s.id == "title_logo":
        return art_brief.LOGO[1]
    subject, frames = art_brief.S.get(s.id, (s.entry.desc, ["frame %d" % (i + 1) for i in range(s.frames)]))
    frames = (list(frames) + ["frame %d" % (i + 1) for i in range(s.frames)])[:s.frames]
    fill = "Fills the whole cell edge to edge, no magenta inside." if s.group == "terrain" else \
        "Flat magenta around the drawing."
    fr = "One frame" if s.frames == 1 else "%d frames left to right, evenly spaced: %s" % (
        s.frames, "; ".join("%d) %s" % (i + 1, f) for i, f in enumerate(frames)))
    return "%s. %s. Frame size %dx%d (draw each about %dx%d). %s" % (
        subject, fr, s.w, s.h, s.w * 8, s.h * 8, fill)


def ordered(all_strips):
    left = list(all_strips)
    groups = []
    for title, pats in art_brief.GROUPS:
        rows = []
        for p in pats:
            for s in list(left):
                if fnmatch.fnmatch(s.id, p):
                    rows.append(s)
                    left.remove(s)
        groups.append((title, rows))
    if left:
        groups[-1][1].extend(left)
    return groups


def write_todo(path, keep):
    lines = ["# %s art TODO" % TITLE, "",
             "Drop folder (this folder): `<your checkout>\\games\\leadysquid\\art\\incoming`", "",
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
             "- Background: flat magenta `#FF00FF` everywhere around the drawings. Tiles (the obstacle bodies) "
             "are the exception: each fills its whole cell edge to edge, with no magenta inside.",
             "- Scale: draw at 8x the final size (a 32x32 frame about 256x256 pixels, a 24x16 frame about "
             "192x128). The title logo is drawn at 4x (1024x256).",
             "- `tools/art_sync.py --game leadysquid sync` imports VALIDATED rows into the game and writes "
             "`IMPORT_REPORT.md` here; it never edits this file.",
             "- The BG tiles of the game (obstacle bodies, seabed, reef, rays) are code-drawn by default "
             "(AI tiles at this size read as noise): the body rows are the last, optional group.", "",
             "## Style guide", "", art_brief.STYLE,
             "### Model sheet", "", art_brief.MODEL_SHEET,
             "### Palettes", "", art_brief.PALETTES, ""]
    n = 0
    for title, rows in ordered(strips().values()):
        lines += ["## %s" % title, "",
                  "| ID | Status | Sheet | Frame size | Frames | Description / prompt | Notes |",
                  "|---|---|---|---|---|---|---|"]
        for s in rows:
            old = keep.get(s.id)
            status = old["status"] if old and old["status"] in STATUSES else "TODO"
            notes = old["notes"] if old else ""
            sheet = "title logo" if s.id == "title_logo" else s.entry.sheet + ".png"
            lines.append("| %s | %s | %s | %dx%d | %d | %s | %s |" % (
                s.id, status, sheet, s.w, s.h, s.frames, prompt(s).replace("|", "/"), notes.replace("|", "/")))
            n += 1
        lines.append("")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))
    return n


def sync(a, report_print=print):
    import art_sync
    import art_consistency
    import make_art
    incoming = a.incoming
    todo = os.path.join(incoming, "TODO.md")
    rows = art_sync.read_todo(todo)
    if not rows:
        raise SystemExit("no rows in %s (run: art_sync.py --game leadysquid todo)" % todo)
    take = ("VALIDATED", "GENERATED") if a.include_generated else ("VALIDATED",)
    st = strips()
    imported, report, counts = art_consistency.import_for_sync(incoming, rows, st, take, a.scale)
    out = a.out or ART
    written = []
    for sheet, spec in ls_sheets.SHEETS.items():
        im = make_art.build_sheet(sheet)
        k = 0
        for sid, frames in imported.items():
            s = st.get(sid)
            if not s or not s.entry or s.entry.sheet != sheet:
                continue
            for (x, y, w, h), (col, alpha) in zip(ls_sheets.frame_rects(s.entry), frames):
                tile = Image.new("RGB", (w, h), ls_sheets.MAGENTA)
                tp = tile.load()
                for j in range(h):
                    for i in range(w):
                        if alpha[j, i]:
                            tp[i, j] = tuple(int(v) for v in col[j, i])
                im.paste(tile, (x, y))
                k += 1
        written.append((spec["file"], k, im))
    logo = imported.get("title_logo") or make_art.logo()
    stamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")
    lines = ["# Art import report", "",
             "Written by `tools/art_sync.py --game leadysquid sync` on %s. Rows taken: %s. The sheets were "
             "written to `%s`." % (stamp, " + ".join(take), os.path.relpath(out, ROOT)), "",
             "Summary: %d imported, %d missing, %d errors. Every other cell keeps its placeholder." %
             (counts["imported"], counts["missing"], counts["error"]), "",
             "| ID | Status | Import |", "|---|---|---|"]
    lines += ["| %s | %s | %s |" % r for r in sorted(report)]
    for r in sorted(report):
        report_print("  %-24s %-10s %s" % r)
    report_print("sync: %d imported, %d missing, %d errors -> %s" %
                 (counts["imported"], counts["missing"], counts["error"], out))
    if a.dry_run:
        for f, k, _ in written:
            report_print("  would write %s/%s (%d frames from imported art)" % (out, f, k))
        return counts
    os.makedirs(os.path.join(out, "tiles"), exist_ok=True)
    for f, k, im in written:
        im.save(os.path.join(out, f))
    logo.save(os.path.join(out, "title_logo.png"))
    if os.path.abspath(out) != os.path.abspath(ART):     # a complete art folder elsewhere (previews)
        for k, spec in ls_sheets.PANORAMAS.items():
            if k != "midground":
                Image.open(os.path.join(ART, spec["file"])).save(os.path.join(out, spec["file"]))
    props = make_art.props_from_sheet(Image.open(os.path.join(out, "props.png")))
    make_art.midground(props).image().save(os.path.join(out, ls_sheets.PANORAMAS["midground"]["file"]))
    with open(a.report or os.path.join(incoming, "IMPORT_REPORT.md"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    return counts


def main(a):
    """Called by tools/art_sync.py --game leadysquid with its parsed arguments."""
    a.incoming = a.incoming or INCOMING
    if a.command == "todo":
        import art_sync
        os.makedirs(a.incoming, exist_ok=True)
        path = os.path.join(a.incoming, "TODO.md")
        n = write_todo(path, art_sync.read_todo(path))
        print("TODO: %d rows -> %s" % (n, path))
        return 0
    if a.command == "sync":
        return 1 if sync(a)["error"] else 0
    raise SystemExit("art_sync.py --game leadysquid: '%s' is not supported (todo, sync)" % a.command)

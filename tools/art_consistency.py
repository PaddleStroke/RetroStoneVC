#!/usr/bin/env python3
"""Consistency pass for the image agent's strips: make a family of separately
generated strips fit together, and flag what cannot be fixed automatically.

Per character or object family (mole_*, ferret_*, cat_*, boss (the barn cat),
dog_*, farmer, fox, owl, badger; every other strip is its own family):
  - frames are cut with tools/art_frames.py (magenta or alpha background,
    despill, uneven spacing, effects attached to their frame; a frame-count
    mismatch is an error, never a guess);
  - scale: the image agent draws every strip at its own size. Each strip is
    normalised against the family's reference frame (<family>_walk_down frame 1
    by default), measured on the body without effects (the largest blob):
    walking/digging strips by body height, pose strips (hurt, death, victory,
    pounce...) by body area. The family scale makes the walking and digging
    bodies fit the cell; a pose frame that would not fit shrinks its own strip;
  - anchoring: characters bottom-centre on the body (feet on the bottom row,
    body centroid in the middle), items centred, explosion pieces stuck to
    their cell edge, tiles and terrain props fill their cell;
  - one palette per palette group (the game's 16-colour sprite palette: mole,
    ferret, cat, critter (dog + bees), each boss...): 15 colours + transparent,
    median cut over every frame of every strip of the group, one of them the
    outline colour; frames are downscaled by colour coverage (each game pixel
    takes the palette colour covering most of it), which keeps pixel art crisp;
  - a uniform 1-px dark outline is redrawn around every sprite;
  - flags: frames whose silhouette area, height, width or colour histogram
    deviate from the family median, per-strip frame-to-frame jitter, drawings
    cropped by the cell, strips drawn at a very different scale.
Tiles: one palette per season row (60 colours = the 4 terrain palettes), holes
filled, and a seamless check (edge difference when the tile is repeated).

Used by `art_sync.py sync` (the import), `art_review.py` (the owner's review
tool) and on its own:

    python3 tools/art_consistency.py [--char-size 24] [--out build/art-review] [--statuses GENERATED,VALIDATED]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import argparse
import json
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import art_frames  # noqa: E402

FILL_GROUPS = ("terrain", "terrain_v2", "propbg")
SOLID_IDS = ("hud_panel",)
EDGE_ANCHOR = {"expl_h": "X", "expl_v": "Y", "expl_end_left": "r", "expl_end_right": "l",
               "expl_end_up": "b", "expl_end_down": "t"}
NO_OUTLINE_IDS = ("hud_panel", "tomato_shadow", "spray", "wind", "steam")
LOGO_SIZE = (256, 64)
# tiles that are repeated next to each other in the levels: they must tile seamlessly
SEAMLESS = {"grass": "hv", "soft_dirt": "hv", "tunnel": "hv", "water": "hv", "ice": "hv", "mud": "hv",
            "tall_grass": "hv", "corn": "hv", "burnt": "hv", "thin_ice": "hv", "rails_h": "h", "rails_v": "v"}
JITTER_IDS = ("bomb", "grub", "windmill", "tomato")
IDLE_PAIRS = {"farmer": (0, 1), "badger": (0, 1), "fox": (0, 1)}   # frames 1-2 are an idle/run pair
# flag thresholds (relative deviation from the family median)
TH_AREA, TH_HEIGHT, TH_WIDTH, TH_HIST = 0.45, 0.30, 0.60, 0.55
TH_JITTER_X, TH_JITTER_Y = 1.5, 2.0
TH_SCALE = (0.6, 1.6)


# ---- families -----------------------------------------------------------------------------------
def character_like(s):
    return s.sheet == "characters" or s.group == "critter" or s.group.startswith("boss_")


def scale_family(s):
    """Strips sharing one drawing scale (a character), or None (the strip is on its own)."""
    if s.id == "title_logo" or not character_like(s):
        return None
    return s.id.split("_")[0]


def display_family(s):
    """Family shown in the review tool."""
    if s.id == "title_logo":
        return "logo"
    f = scale_family(s)
    if f == "boss":
        return "boss_cat"
    if f:
        return f
    if s.sheet == "tiles":
        return "tiles_" + ["spring", "summer", "autumn", "winter"][s.season]
    if s.group == "terrain_v2":
        return "tiles_v2_" + ["spring", "summer", "autumn", "winter"][s.season]
    return {"pickup": "items", "prop": "props", "propbg": "terrain_props", "bomb": "bomb_dust"}.get(s.group, s.group)


def palette_key(s):
    """The palette a strip is drawn with in the game: its sprite/BG group, a season row of
    the terrain, or a colour family of the terrain-like props (sheets.PROP_PALETTES)."""
    if s.group in ("terrain", "terrain_v2"):
        return (s.group, s.season)
    if s.group == "propbg":
        import sheets
        return ("propbg", sheets.prop_palette(s.entry.name))
    return s.group


def palette_size(key):
    return 60 if isinstance(key, tuple) and key[0] in ("terrain", "terrain_v2") else 15


def solid(s):
    return s.group in FILL_GROUPS or s.id in SOLID_IDS


def outlined(s):
    return not solid(s) and s.group != "fx" and s.id not in NO_OUTLINE_IDS and s.id != "title_logo"


def locomotion(s):
    return "_walk_" in s.id or "_dig_" in s.id


# ---- colours ---------------------------------------------------------------------------------------
W_RGB = np.array([2.0, 4.0, 3.0], np.float32)     # the weighted distance of tools/rsasset.py


def snap555(c):
    c = np.asarray(c, np.int32)
    v = c >> 3
    return (v << 3) | (v >> 2)


def nearest_index(pix, pal):
    """pix: (..., 3) float; pal: (k, 3) -> index array (...)."""
    flat = pix.reshape(-1, 3).astype(np.float32)
    best = np.zeros(len(flat), np.int32)
    bestd = np.full(len(flat), np.inf, np.float32)
    for i, p in enumerate(np.asarray(pal, np.float32)):
        d = (((flat - p) ** 2) * W_RGB).sum(-1)
        m = d < bestd
        best[m], bestd[m] = i, d[m]
    return best.reshape(pix.shape[:-1])


def median_cut(pix, n):
    """(N, 3) pixels -> up to n RGB555-exact colours (list of tuples)."""
    pix = snap555(np.asarray(pix, np.float32).round())
    uniq = np.unique(pix, axis=0)
    if len(uniq) <= n:
        return [tuple(int(v) for v in c) for c in uniq]
    im = Image.fromarray(pix.reshape(1, -1, 3).astype(np.uint8), "RGB")
    q = im.quantize(colors=n, method=Image.Quantize.MEDIANCUT)
    pal = np.array(q.getpalette()[:n * 3], np.int32).reshape(-1, 3)
    used = np.unique(np.asarray(q))
    pal = snap555(pal[used[used < len(pal)]])
    out = []
    for c in pal:
        t = tuple(int(v) for v in c)
        if t not in out:
            out.append(t)
    return out


def luma(c):
    c = np.asarray(c, np.float32)
    return 0.299 * c[..., 0] + 0.587 * c[..., 1] + 0.114 * c[..., 2]


# ---- downscaling ------------------------------------------------------------------------------------
def _region(arr, x0, y0, x1, y1):
    """arr[y0:y1, x0:x1] with zero padding outside the image (ints)."""
    H, W = arr.shape[:2]
    out = np.zeros((y1 - y0, x1 - x0) + arr.shape[2:], arr.dtype)
    sx0, sy0, sx1, sy1 = max(0, x0), max(0, y0), min(W, x1), min(H, y1)
    if sx1 > sx0 and sy1 > sy0:
        out[sy0 - y0:sy1 - y0, sx0 - x0:sx1 - x0] = arr[sy0:sy1, sx0:sx1]
    return out


def _box_resize(a, box, w, h):
    """Area-average a 2-D float array over the (float) box into w x h."""
    im = Image.fromarray(np.ascontiguousarray(a, np.float32), "F")
    return np.asarray(im.resize((w, h), Image.BOX, box=box), np.float32)


def window_crop(rgb, mask, win):
    x0, y0 = int(np.floor(win[0])), int(np.floor(win[1]))
    x1, y1 = int(np.ceil(win[2])), int(np.ceil(win[3]))
    x1, y1 = max(x1, x0 + 1), max(y1, y0 + 1)
    box = (win[0] - x0, win[1] - y0, win[2] - x0, win[3] - y0)
    return _region(rgb, x0, y0, x1, y1), _region(mask, x0, y0, x1, y1), box


def area_downscale(rgb, mask, win, w, h):
    """-> (colour h x w x 3, coverage h x w) from the pixels of mask inside win."""
    crgb, cm, box = window_crop(rgb, mask, win)
    m = cm.astype(np.float32)
    cov = _box_resize(m, box, w, h)
    core = art_frames.erode(cm, 1).astype(np.float32) if cm.sum() > 64 else m
    out = np.zeros((h, w, 3), np.float32)
    for mk in (m, core):                         # full mask first, then the core colour on top
        den = _box_resize(mk, box, w, h)
        ok = den > 0.05
        for k in range(3):
            num = _box_resize(crgb[..., k] * mk, box, w, h)
            out[..., k][ok] = num[ok] / den[ok]
    return out, cov


def coverage_downscale(rgb, mask, win, w, h, pal):
    """Each game pixel takes the palette colour covering most of it -> (index map 1..k or 0, coverage)."""
    crgb, cm, box = window_crop(rgb, mask, win)
    idx = np.where(cm, nearest_index(crgb, pal) + 1, 0)
    cov = _box_resize(cm.astype(np.float32), box, w, h)
    best = np.zeros((h, w), np.int32)
    bestc = np.zeros((h, w), np.float32)
    for k in np.unique(idx):
        if k == 0:
            continue
        c = _box_resize((idx == k).astype(np.float32), box, w, h)
        m = c > bestc
        best[m], bestc[m] = k, c[m]
    return best, cov


def fill_holes(idx):
    """Solid cells: transparent pixels take the nearest opaque colour. -> (idx, count)."""
    holes = idx == 0
    if not holes.any() or holes.all():
        return idx, int(holes.sum()) if not holes.all() else 0
    from scipy import ndimage
    ii = ndimage.distance_transform_edt(holes, return_distances=False, return_indices=True)
    return idx[ii[0], ii[1]], int(holes.sum())


def redraw_outline(idx, outline_index, min_area=10):
    """Inner 1-px outline: every opaque pixel with a transparent 4-neighbour (or the cell
    edge) takes the outline colour. Specks smaller than min_area keep their colour."""
    from scipy import ndimage
    a = idx > 0
    pad = np.pad(a, 1)
    inner = pad[:-2, 1:-1] & pad[2:, 1:-1] & pad[1:-1, :-2] & pad[1:-1, 2:]
    edge = a & ~inner
    lab, n = ndimage.label(a, structure=np.ones((3, 3)))
    if n:
        sizes = ndimage.sum(a, lab, range(1, n + 1))
        small = np.isin(lab, [i + 1 for i, s in enumerate(sizes) if s < min_area])
        edge &= ~small
    out = idx.copy()
    out[edge] = outline_index
    return out, int(edge.sum())


# ---- per strip ----------------------------------------------------------------------------------------
class Result:
    def __init__(self, s, status):
        self.id, self.strip, self.status = s.id, s, status
        self.family, self.pal_key = display_family(s), palette_key(s)
        self.error, self.notes, self.flags, self.frame_flags = None, [], [], []
        self.cut, self.k, self.sc, self.found = None, 1.0, None, None
        self.idx, self.palette, self.windows, self.metrics = None, None, None, []
        self.logo = None
        self.facing = None

    @property
    def imported(self):
        return self.error is None and (self.idx is not None or self.logo is not None)

    def rgb_frames(self):
        """[(colour h x w x 3 float, alpha bool)] for art_sync."""
        pal = np.array([(0, 0, 0)] + list(self.palette), np.float32)
        return [(pal[i], i > 0) for i in self.idx]

    def rgba_strip(self):
        """All frames side by side, RGBA, 1x."""
        h, w = self.strip.h, self.strip.w
        im = Image.new("RGBA", (w * len(self.idx), h), (0, 0, 0, 0))
        pal = np.array([(0, 0, 0, 0)] + [tuple(c) + (255,) for c in self.palette], np.uint8)
        for i, fr in enumerate(self.idx):
            im.paste(Image.fromarray(pal[fr], "RGBA"), (i * w, 0))
        return im

    def summary(self):
        return {"id": self.id, "status": self.status, "family": self.family, "group": self.strip.group,
                "sheet": self.strip.sheet, "w": self.strip.w, "h": self.strip.h,
                "frames_expected": self.strip.frames, "frames_found": self.found, "error": self.error,
                "flags": self.flags, "frame_flags": self.frame_flags, "notes": self.notes,
                "ai_scale": round(self.sc, 2) if self.sc else None, "norm": round(self.k, 3),
                "palette": [list(c) for c in self.palette] if self.palette else None,
                "imported": self.imported, "facing": self.facing}


def _fit(box, cw, ch, mw=1, mh=1):
    return max((box[2] - box[0]) / max(1.0, cw - mw), (box[3] - box[1]) / max(1.0, ch - mh))


def window(s, fr, sc):
    cw, ch = s.w, s.h
    ww, wh = cw * sc, ch * sc
    if solid(s):
        return tuple(float(v) for v in fr.box)
    b = fr.box
    if character_like(s):
        cx = fr.body_centroid_x()
        bot = fr.body[3]
        return (cx - ww / 2.0, bot - wh, cx + ww / 2.0, bot)
    x0 = (b[0] + b[2]) / 2.0 - ww / 2.0
    y0 = (b[1] + b[3]) / 2.0 - wh / 2.0
    x1, y1 = x0 + ww, y0 + wh
    a = EDGE_ANCHOR.get(s.id, "")
    if "l" in a: x0, x1 = b[0], b[0] + ww
    if "r" in a: x0, x1 = b[2] - ww, b[2]
    if "t" in a: y0, y1 = b[1], b[1] + wh
    if "b" in a: y0, y1 = b[3] - wh, b[3]
    if "X" in a: x0, x1 = b[0], b[2]
    if "Y" in a: y0, y1 = b[1], b[3]
    return (x0, y0, x1, y1)


def family_scales(results, forced=None):
    """AI pixels per game pixel for every cut strip (Result.sc), with the per-strip normalisation."""
    fams = {}
    for r in results.values():
        if r.cut is None:
            continue
        if forced:
            r.sc = float(forced)
            continue
        f = scale_family(r.strip)
        if f is None:
            s = r.strip
            if solid(s):
                r.sc = max(_fit(fr.box, s.w, s.h, 0, 0) for fr in r.cut.frames)
            else:
                r.sc = max(_fit(fr.box, s.w, s.h) for fr in r.cut.frames)
            continue
        fams.setdefault(f, []).append(r)
    for f, members in fams.items():
        by = {r.id: r for r in members}
        ref = None
        for cand in ("%s_walk_down" % f, "%s_walk_right" % f, "%s_walk_left" % f):
            if cand in by:
                ref = by[cand]
                break
        ref = ref or sorted(members, key=lambda r: r.id)[0]
        f0 = ref.cut.frames[0]
        h_ref, a_ref = float(f0.body[3] - f0.body[1]), float(f0.body_area)
        for r in members:
            if r is ref:
                r.k = 1.0
                continue
            hs = float(np.median([fr.body[3] - fr.body[1] for fr in r.cut.frames]))
            As = float(np.median([fr.body_area for fr in r.cut.frames]))
            # front and back views: body height; side views (a long ferret is lower than it is
            # tall from the front) and poses (hurt, death, victory...): body area
            by_height = locomotion(r.strip) and ("_down" in r.id or "_up" in r.id) and \
                ("_down" in ref.id or "_up" in ref.id)
            k = hs / h_ref if by_height else (As / a_ref) ** 0.5
            how = "body height" if by_height else "body area"
            if abs(k - 1.0) < 0.10:
                k = 1.0
            elif not (TH_SCALE[0] <= k <= TH_SCALE[1]):
                r.flags.append("drawn at x%.2f the size of %s (by %s): check the proportions" % (k, ref.id, how))
                k = min(max(k, TH_SCALE[0]), TH_SCALE[1])
            else:
                r.notes.append("drawn at x%.2f the size of %s (by %s): normalised" % (k, ref.id, how))
            r.k = k
        # family scale: the walking/digging bodies fit the cell (1 px of slack across)
        loco = [r for r in members if locomotion(r.strip)] or members
        S = max(_fit(fr.body, r.strip.w, r.strip.h, 1, 0) / r.k for r in loco for fr in r.cut.frames)
        for r in members:
            r.sc = S * r.k
            need = max(_fit(fr.body, r.strip.w, r.strip.h, 1, 0) for fr in r.cut.frames)
            if need > r.sc * 1.02:
                r.notes.append("pose larger than the cell at the family scale: reduced to %d%%" % (100 * r.sc / need))
                r.sc = need
            r.notes.insert(0, "family %s: reference %s frame 1" % (f, ref.id))


def process(incoming, rows, strips, take, measure=("VALIDATED", "GENERATED"), forced_scale=None,
            method="coverage", log=None):
    """Cut, normalise, palettise and flag. rows: {id: {"status", ...}} from TODO.md;
    strips: {id: art_sync.Strip}. Every row whose status is in take or measure and whose
    PNG exists is processed (measure rows set the family scales and palettes; only the take
    rows are meant to be imported). -> {id: Result}"""
    want = set(take) | set(measure)
    results = {}
    for sid, row in rows.items():
        s = strips.get(sid)
        if s is None or row["status"] not in want:
            continue
        r = Result(s, row["status"])
        results[sid] = r
        path = os.path.join(incoming, sid + ".png")
        if not os.path.exists(path):
            r.error = "missing: %s.png not found" % sid
            continue
        if sid == "title_logo":
            try:
                r.logo, note = import_logo(path)
                r.notes.append(note)
                r.found = 1
            except Exception as ex:   # noqa: BLE001 - reported per row
                r.error = "error: %s" % ex
            continue
        try:
            r.cut = art_frames.cut_strip(path, s.frames, solid(s))
            r.found = len(r.cut.frames)
            r.notes += r.cut.notes
        except art_frames.FrameCountError as ex:
            r.error = "error: %s" % ex
            r.found = ex.found
        except Exception as ex:       # noqa: BLE001
            r.error = "error: %s" % ex
    family_scales(results, forced_scale)

    # area downscale first (palette statistics), per frame
    cells = {}
    for r in results.values():
        if r.cut is None:
            continue
        s = r.strip
        r.windows = [window(s, fr, r.sc) for fr in r.cut.frames]
        cells[r.id] = [area_downscale(r.cut.rgb, fr.mask, win, s.w, s.h) for fr, win in zip(r.cut.frames, r.windows)]
        for i, (fr, win) in enumerate(zip(r.cut.frames, r.windows)):
            if solid(s):
                continue
            ys, xs = np.nonzero(fr.mask)
            inside = (xs >= win[0]) & (xs < win[2]) & (ys >= win[1]) & (ys < win[3])
            lost = 1.0 - inside.mean() if len(xs) else 0.0
            if lost > 0.04:
                r.frame_flags.append("frame %d: %d%% of the drawing (effects included) is outside the %dx%d cell" %
                                     (i + 1, round(100 * lost), s.w, s.h))

    # one palette per palette group
    groups = {}
    for r in results.values():
        if r.cut is not None:
            groups.setdefault(r.pal_key, []).append(r)
    palettes, outline_idx = {}, {}
    rng = np.random.default_rng(1)
    for key, members in groups.items():
        # every strip gets the same weight, so a small icon keeps its colours next to the
        # ten big digits of the same palette
        per = []
        for r in members:
            p = [col[cov >= (0.3 if solid(r.strip) else 0.5)] for col, cov in cells[r.id]]
            p = np.concatenate(p) if p else np.zeros((0, 3), np.float32)
            if len(p):
                per.append(p)
        n_each = max([len(p) for p in per] + [1])
        pix = np.concatenate([p if len(p) == n_each else p[rng.integers(0, len(p), n_each)] for p in per]) \
            if per else np.zeros((1, 3), np.float32)
        n = palette_size(key)
        use_outline = any(outlined(r.strip) for r in members)
        if use_outline:
            dark = pix[luma(pix) <= np.percentile(luma(pix), 3)]
            oc = tuple(int(v) for v in snap555(np.median(dark, axis=0).round())) if len(dark) else (16, 12, 20)
            if luma(np.array(oc, np.float32)) > 70:
                oc = (16, 12, 24)
            if max(oc) > 40:          # a really dark outline (value <= 16%), in the drawing's hue
                oc = tuple(int(v) for v in snap555(np.array(oc, np.float32) * 40.0 / max(oc)))
            rest = pix[luma(pix) > luma(np.array(oc, np.float32)) + 6]
            pal = median_cut(rest if len(rest) else pix, n - 1)
            pal = [c for c in pal if c != oc] + [oc]
            outline_idx[key] = len(pal)
        else:
            pal = median_cut(pix, n)
        palettes[key] = pal

    # final frames: palette indices, holes, outline
    for r in results.values():
        if r.cut is None:
            continue
        s, pal = r.strip, palettes[r.pal_key]
        r.palette = pal
        r.idx = []
        filled = 0
        for (col, cov), fr, win in zip(cells[r.id], r.cut.frames, r.windows):
            if method == "coverage":
                idx, cov2 = coverage_downscale(r.cut.rgb, fr.mask, win, s.w, s.h, np.array(pal, np.float32))
            else:
                idx = nearest_index(col, pal) + 1
            if solid(s):
                idx, n = fill_holes(np.where(cov > 0.3, idx, 0))
                filled += n
            else:
                idx = np.where(cov >= 0.5, idx, 0)
                if outlined(s) and r.pal_key in outline_idx:
                    idx, _ = redraw_outline(idx, outline_idx[r.pal_key])
            r.idx.append(idx.astype(np.uint8))
        if filled:
            r.notes.append("%d transparent pixel(s) inside the tile filled" % filled)
    flag_all(results)
    check_facing(results)
    return results


# ---- facing (left/right strips) ------------------------------------------------------------------
def _shape(idx, pal, size=(16, 12)):
    """A frame reduced to its bounding box, resized: (mask, luma) arrays for comparisons."""
    a = idx > 0
    ys, xs = np.nonzero(a)
    if not len(xs):
        return None
    crop = idx[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
    lum = np.array([0.0] + [float(luma(np.array(c, np.float32))) for c in pal], np.float32)[crop]
    m = Image.fromarray(((crop > 0) * 255).astype(np.uint8)).resize(size, Image.BILINEAR)
    l = Image.fromarray(lum).resize(size, Image.BILINEAR)
    return np.asarray(m, np.float32) / 255.0, np.asarray(l, np.float32)


def _profile(shape):
    """Mean brightness of each column of the drawing (pose-independent: a light face at one end
    and a dark tail at the other tell the facing even when the tail is raised)."""
    m, l = shape
    cols = (m * l).sum(0) / np.maximum(m.sum(0), 1e-3)
    return cols - cols.mean()


def _similar(a, b):
    ma, la = a
    mb, lb = b
    iou = (np.minimum(ma, mb).sum() + 1e-6) / (np.maximum(ma, mb).sum() + 1e-6)
    w = np.minimum(ma, mb)
    if w.sum() > 1:
        x, y = la[w > 0.5] - la[w > 0.5].mean(), lb[w > 0.5] - lb[w > 0.5].mean()
        corr = float((x * y).sum() / (np.sqrt((x * x).sum() * (y * y).sum()) + 1e-6)) if len(x) > 3 else 0.0
    else:
        corr = 0.0
    pa, pb = _profile(a), _profile(b)
    prof = float((pa * pb).sum() / (np.sqrt((pa * pa).sum() * (pb * pb).sum()) + 1e-6))
    return float(iou) + 0.5 * corr + 1.5 * prof


def facing_of(idx, pal, ref_left):
    """'left', 'right' or None (unsure): compares the frame and its mirror with a left-facing reference."""
    s = _shape(idx, pal)
    if s is None or ref_left is None:
        return None, 0.0
    f = _shape(idx[:, ::-1], pal)
    refs = ref_left if isinstance(ref_left, list) else [ref_left]
    same, flip = max(_similar(s, r) for r in refs), max(_similar(f, r) for r in refs)
    if abs(same - flip) < 0.04:
        return None, same - flip
    return ("left" if same > flip else "right"), same - flip


def _shape_hr(rgb, mask, flip=False, size=(32, 20)):
    """Like _shape, from the full-resolution AI drawing (the downscaled frame's redrawn outline
    hides the light face / dark tail contrast)."""
    ys, xs = np.nonzero(mask)
    if not len(xs):
        return None
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    m = mask[y0:y1, x0:x1].astype(np.float32)
    l = luma(rgb[y0:y1, x0:x1].astype(np.float32)) * m
    if flip:
        m, l = m[:, ::-1], l[:, ::-1]
    mm = np.asarray(Image.fromarray((m * 255).astype(np.uint8)).resize(size, Image.BOX), np.float32) / 255.0
    ll = np.asarray(Image.fromarray(l.astype(np.float32)).resize(size, Image.BOX), np.float32)
    return mm, ll / np.maximum(mm, 1e-3)


def _frame_shapes(r, i):
    if r.cut is not None and i < len(r.cut.frames):
        fr = r.cut.frames[i]
        return _shape_hr(r.cut.rgb, fr.mask), _shape_hr(r.cut.rgb, fr.mask, flip=True)
    return _shape(r.idx[i], r.palette), _shape(r.idx[i][:, ::-1], r.palette)


def check_facing(results):
    """Flag every frame of a *_left / *_right strip pair that faces the wrong way, without trusting
    any single frame: all frames of the pair are split into two orientation groups so that frames
    of the same group look alike and frames of different groups look alike once mirrored (best of
    all splits); the group holding most frames labelled "left" is the left-facing one."""
    import itertools
    bases = sorted({r.id.rsplit("_", 1)[0] for r in results.values()
                    if r.idx is not None and (r.id.endswith("_left") or r.id.endswith("_right"))})
    for base in bases:
        items = []
        for side in ("left", "right"):
            r = results.get(base + "_" + side)
            if r is None or r.idx is None:
                continue
            r.facing = [None] * len(r.idx)
            for i, fr in enumerate(r.idx):
                s, f = _frame_shapes(r, i)
                if s is not None:
                    items.append((r, i, side, s, f))
        n = len(items)
        if n < 2 or n > 12:
            continue
        sim = {}
        for a in range(n):
            for b in range(a + 1, n):
                sim[a, b] = (_similar(items[a][3], items[b][3]), _similar(items[a][3], items[b][4]))
        best, best_o = None, None
        for rest in itertools.product((0, 1), repeat=n - 1):
            o = (0,) + rest
            score = sum(v[0] if o[a] == o[b] else v[1] for (a, b), v in sim.items())
            if best is None or score > best:
                best, best_o = score, o
        # which group faces left: the labelling that agrees with most strip labels
        agree0 = sum((best_o[k] == 0) == (items[k][2] == "left") for k in range(n))
        left_group = 0 if agree0 * 2 >= n else 1
        for k, (r, i, side, s, f) in enumerate(items):
            got = "left" if best_o[k] == left_group else "right"
            r.facing[i] = got
            if got != side:
                r.frame_flags.append("frame %d faces %s (the strip should face %s): mirrored automatically, "
                                     "regenerate it" % (i + 1, got.upper(), side))
                r.idx[i] = np.ascontiguousarray(r.idx[i][:, ::-1])   # fixed in the build meanwhile


# ---- flags ------------------------------------------------------------------------------------------
def frame_metrics(idx, k):
    from scipy import ndimage
    a = idx > 0
    ys, xs = np.nonzero(a)
    if not len(xs):
        return {"area": 0, "h": 0, "w": 0, "cx": 0.0, "top": 0, "hist": np.zeros(k)}
    hist = np.bincount(idx[a].ravel(), minlength=k + 1)[1:k + 1].astype(np.float32)
    lab, n = ndimage.label(a, structure=np.ones((3, 3)))
    body = lab == (int(np.argmax(ndimage.sum(a, lab, range(1, n + 1)))) + 1) if n > 1 else a
    by, bx = np.nonzero(body)
    return {"area": int(a.sum()), "h": int(ys.max() - ys.min() + 1), "w": int(xs.max() - xs.min() + 1),
            "cx": float(bx.mean()), "top": int(by.min()), "hist": hist / max(1.0, hist.sum())}


def _dev(v, med):
    return (v - med) / med if med else 0.0


def flag_all(results):
    fams = {}
    for r in results.values():
        if r.idx is None:
            continue
        k = len(r.palette)
        r.metrics = [frame_metrics(i, k) for i in r.idx]
        if scale_family(r.strip):
            fams.setdefault(scale_family(r.strip), []).append(r)
        if solid(r.strip):
            flag_tile(r)
    for f, members in fams.items():
        if len(members) < 2:           # one strip of different poses (a boss): nothing to compare with
            continue
        allm = [m for r in members for m in r.metrics]
        med = {key: float(np.median([m[key] for m in allm])) for key in ("area", "h", "w")}
        loco = [m for r in members if locomotion(r.strip) for m in r.metrics] or allm
        med_loco = {key: float(np.median([m[key] for m in loco])) for key in ("area", "h", "w")}
        hist = np.median(np.stack([m["hist"] for m in allm]), axis=0)
        hist = hist / max(1e-6, hist.sum())
        for r in members:
            for i, m in enumerate(r.metrics):
                out = []
                ref = med_loco if locomotion(r.strip) else med
                d = _dev(m["area"], ref["area"])
                if abs(d) > TH_AREA:
                    out.append("area %d px vs family median %d (%+d%%)" % (m["area"], ref["area"], 100 * d))
                if locomotion(r.strip):
                    d = _dev(m["h"], ref["h"])
                    if abs(d) > TH_HEIGHT:
                        out.append("height %d vs family median %d (%+d%%)" % (m["h"], ref["h"], 100 * d))
                    d = _dev(m["w"], ref["w"])
                    if abs(d) > TH_WIDTH:
                        out.append("width %d vs family median %d (%+d%%)" % (m["w"], ref["w"], 100 * d))
                hd = 0.5 * float(np.abs(m["hist"] - hist).sum())
                if len(allm) >= 3 and hd > TH_HIST:
                    out.append("colours differ from the family (histogram distance %.2f)" % hd)
                if out:
                    r.frame_flags.append("frame %d: %s" % (i + 1, "; ".join(out)))
    for r in results.values():
        if r.idx is None or len(r.idx) < 2:
            continue
        s = r.strip
        pairs = []
        if locomotion(s) or s.id in JITTER_IDS:
            pairs = list(zip(range(len(r.idx) - 1), range(1, len(r.idx))))
        elif s.id in IDLE_PAIRS:
            pairs = [IDLE_PAIRS[s.id]]
        for a, b in pairs:
            ma, mb = r.metrics[a], r.metrics[b]
            dx, dy = abs(ma["cx"] - mb["cx"]), abs(ma["top"] - mb["top"])
            if dx > TH_JITTER_X + (1 if "_walk_left" in s.id or "_walk_right" in s.id else 0) or dy > TH_JITTER_Y:
                r.frame_flags.append("jitter frame %d->%d: centre moves %.1f px, top moves %d px" % (a + 1, b + 1, dx, dy))


def seam_scores(idx, pal):
    """(horizontal, vertical) edge difference when repeated / mean neighbour difference."""
    rgb = np.array([(0, 0, 0)] + list(pal), np.float32)[idx]
    L = luma(rgb)
    inner_h = np.abs(np.diff(L, axis=1)).mean()
    inner_v = np.abs(np.diff(L, axis=0)).mean()
    seam_h = np.abs(L[:, 0] - L[:, -1]).mean()
    seam_v = np.abs(L[0, :] - L[-1, :]).mean()
    return seam_h / max(4.0, inner_h), seam_v / max(4.0, inner_v), seam_h, seam_v


def flag_tile(r):
    name = r.strip.entry.name if r.strip.entry is not None else r.id
    axes = SEAMLESS.get(name)
    for i, idx in enumerate(r.idx):
        sh, sv, ah, av = seam_scores(idx, r.palette)
        r.notes.append("seam check frame %d: left/right x%.1f, top/bottom x%.1f" % (i + 1, sh, sv))
        if not axes:
            continue
        bad = []
        if "h" in axes and sh > 2.0 and ah > 20:
            bad.append("left/right edges differ (x%.1f the inner contrast)" % sh)
        if "v" in axes and sv > 2.0 and av > 20:
            bad.append("top/bottom edges differ (x%.1f the inner contrast)" % sv)
        if bad:
            r.frame_flags.append("frame %d: not seamless: %s" % (i + 1, "; ".join(bad)))


# ---- title logo ----------------------------------------------------------------------------------------
def import_logo(path):
    rgb, alpha = art_frames.load_rgba(path)
    fg, rgb, mode, notes = art_frames.foreground(rgb, alpha)
    ys, xs = np.nonzero(fg)
    box = (int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1)
    bw, bh = box[2] - box[0], box[3] - box[1]
    kk = min(LOGO_SIZE[0] / float(bw), LOGO_SIZE[1] / float(bh))
    tw, th = max(8, int(bw * kk)), max(8, int(bh * kk))
    win = (box[0], box[1], box[0] + tw / kk, box[1] + th / kk)
    col, cov = area_downscale(rgb, fg, win, tw, th)
    a = cov >= 0.5
    pal = median_cut(col[a], 15)
    idx = nearest_index(col, pal)
    palarr = np.array(pal, np.uint8)
    logo = Image.new("RGB", LOGO_SIZE, (255, 0, 255))
    small = Image.fromarray(palarr[idx], "RGB")
    logo.paste(small, ((LOGO_SIZE[0] - tw) // 2, (LOGO_SIZE[1] - th) // 2),
               Image.fromarray((a * 255).astype(np.uint8)))
    return logo, "logo %dx%d from a %dx%d drawing (%s background), %d colours" % (tw, th, bw, bh, mode, len(pal))


# ---- entry points ------------------------------------------------------------------------------------------
def import_for_sync(incoming, rows, strips, take, forced_scale=None, method="coverage"):
    """For art_sync.sync: -> (imported {id: frames or logo image}, report [(id, status, msg)], counts)."""
    results = process(incoming, rows, strips, take, forced_scale=forced_scale, method=method)
    imported, report = {}, []
    counts = {"imported": 0, "missing": 0, "error": 0}
    for sid, r in results.items():
        if r.status not in take:
            continue
        if r.error:
            report.append((sid, r.status, r.error))
            counts["missing" if r.error.startswith("missing") else "error"] += 1
            continue
        if r.logo is not None:
            imported[sid] = r.logo
            msg = "imported (%s)" % r.notes[0]
        else:
            imported[sid] = r.rgb_frames()
            msg = "imported: %d frame(s), AI scale %.1fx" % (len(r.idx), r.sc)
            if r.k != 1.0:
                msg += ", normalised x%.2f" % r.k
        fl = r.flags + r.frame_flags
        if fl:
            msg += "; FLAGS: " + "; ".join(fl)
        report.append((sid, r.status, msg.replace("|", "/")))
        counts["imported"] += 1
    return imported, report, counts


def write_review(results, out):
    """Review cache: out/index.json and out/frames/<id>.png (frames side by side, 1x RGBA)."""
    os.makedirs(os.path.join(out, "frames"), exist_ok=True)
    index = {}
    for sid, r in results.items():
        index[sid] = r.summary()
        if r.idx is not None:
            r.rgba_strip().save(os.path.join(out, "frames", sid + ".png"))
        elif r.logo is not None:
            rgba = np.asarray(r.logo.convert("RGBA")).copy()
            rgba[(rgba[..., 0] == 255) & (rgba[..., 1] == 0) & (rgba[..., 2] == 255)] = 0
            Image.fromarray(rgba, "RGBA").save(os.path.join(out, "frames", sid + ".png"))
            index[sid]["w"], index[sid]["h"] = LOGO_SIZE
    with open(os.path.join(out, "index.json"), "w", encoding="utf-8") as f:
        json.dump(index, f, indent=1)
    return index


def summarise(results):
    fams = {}
    for r in results.values():
        d = fams.setdefault(r.family, {"strips": 0, "imported": 0, "flagged": 0, "mismatch": 0, "missing": 0})
        d["strips"] += 1
        if r.error and "mismatch" in r.error:
            d["mismatch"] += 1
        elif r.error and r.error.startswith("missing"):
            d["missing"] += 1
        if r.imported:
            d["imported"] += 1
            if r.flags or r.frame_flags:
                d["flagged"] += 1
    return fams


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--incoming", default=os.path.join(ROOT, "games", "bombermole", "art", "incoming"))
    ap.add_argument("--char-size", type=int, default=24)
    ap.add_argument("--statuses", default="GENERATED,VALIDATED,REJECTED")
    ap.add_argument("--out", default=os.path.join(ROOT, "build", "art-review"))
    ap.add_argument("--method", choices=["coverage", "area"], default="coverage")
    ap.add_argument("--verbose", action="store_true")
    a = ap.parse_args()
    import art_sync
    sheets, _, _, _ = art_sync.load_modules(a.char_size)
    rows = art_sync.read_todo(os.path.join(a.incoming, "TODO.md"))
    strips = {s.id: s for s in art_sync.all_strips(sheets)}
    st = tuple(a.statuses.split(","))
    results = process(a.incoming, rows, strips, st, measure=st, method=a.method)
    write_review(results, a.out)
    for fam, d in sorted(summarise(results).items()):
        print("%-16s %2d strips: %2d imported, %2d flagged, %d mismatch, %d missing" %
              (fam, d["strips"], d["imported"], d["flagged"], d["mismatch"], d["missing"]))
    if a.verbose:
        for sid, r in sorted(results.items()):
            print("--", sid, r.error or "", "k=%.2f sc=%s" % (r.k, "%.2f" % r.sc if r.sc else "-"))
            for x in r.flags + r.frame_flags:
                print("   FLAG", x)
            for x in r.notes:
                print("   note", x)
    print("review cache -> %s" % a.out)


if __name__ == "__main__":
    main()

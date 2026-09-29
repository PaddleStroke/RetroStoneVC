#!/usr/bin/env python3
"""Robust frame cutting for the image agent's strips (one PNG per animation).

Real AI strips are only roughly regular. This module finds the frames of a
strip without assuming exact positions, and refuses to guess when it cannot
tell the frames apart:

  background   flat magenta #FF00FF (with near-magenta noise and pink
               anti-aliasing fringes), a transparent background (alpha), a mix
               of both, or none at all (an opaque full-bleed image);
  despill      pink fringes and halos take the colour of the nearest clean
               pixel (only when magenta is present);
  frames       connected blobs; big blobs are drawings, small ones (dirt,
               sparks, stars, a thrown tomato) are effects attached to the
               nearest drawing; drawings that overlap in x form one frame; a
               frame drawn in several pieces (a sprinkler and its detached
               spray) is joined only when the gap inside it is clearly
               smaller than the gaps between frames; frames that touch are
               split on an even grid only when no drawing straddles a split
               line. Anything else raises FrameCountError (found N, expected M):
               a strip is never silently misassigned.

    frames = cut_strip("mole_walk_left.png", n=3, solid=False)
    for f in frames.frames: f.box, f.body, f.mask ...

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import numpy as np
from PIL import Image

try:
    from scipy import ndimage
except ImportError:          # pragma: no cover - listed in the README's dependencies
    raise SystemExit("art_frames.py needs scipy (apt-get install python3-scipy)")

MAGENTA = np.array([255.0, 0.0, 255.0], np.float32)


class FrameCountError(ValueError):
    def __init__(self, found, expected, detail=""):
        self.found, self.expected, self.detail = found, expected, detail
        super().__init__("frame-count mismatch: found %d drawing(s), the TODO row expects %d%s" %
                         (found, expected, " (" + detail + ")" if detail else ""))


class Frame:
    """One frame of a strip, in AI pixels.
    box   [x0, y0, x1, y1] of the whole frame (drawing + attached effects)
    body  [x0, y0, x1, y1] of the main drawing without effects (largest blob)
    mask  bool image (whole strip size) of the pixels that belong to this frame
    body_mask  bool image of the body blob only"""

    def __init__(self, box, body, mask, body_mask):
        self.box, self.body, self.mask, self.body_mask = box, body, mask, body_mask

    @property
    def body_area(self):
        return int(self.body_mask.sum())

    def body_centroid_x(self):
        ys, xs = np.nonzero(self.body_mask)
        return float(xs.mean()) + 0.5 if len(xs) else (self.body[0] + self.body[2]) / 2.0


class Strip:
    def __init__(self, rgb, fg, frames, mode, notes):
        self.rgb, self.fg, self.frames, self.mode, self.notes = rgb, fg, frames, mode, notes

    @property
    def size(self):
        return self.fg.shape[1], self.fg.shape[0]


# ---- background -------------------------------------------------------------------------
def load_rgba(path):
    a = np.asarray(Image.open(path).convert("RGBA"), dtype=np.float32)
    return a[..., :3].copy(), a[..., 3].copy()


def _border(a):
    return np.concatenate([a[0], a[-1], a[:, 0], a[:, -1]])


def background_mode(rgb, alpha):
    """'magenta' (+ its colour), 'alpha', or 'none' (opaque full-bleed image)."""
    transparent = alpha < 128
    b_rgb, b_a = _border(rgb), _border(alpha)
    opaque = b_rgb[b_a >= 128]
    if len(opaque) >= 0.5 * len(b_rgb):
        med = np.median(opaque, axis=0)
        if np.sqrt(((med - MAGENTA) ** 2).sum()) < 110:
            return "magenta", med
    if transparent.mean() > 0.002:
        return "alpha", None
    return "none", None


def dilate(m, r):
    if r <= 0:
        return m
    st = ndimage.generate_binary_structure(2, 2)
    return ndimage.binary_dilation(m, st, iterations=int(r))


def erode(m, r):
    if r <= 0:
        return m
    st = ndimage.generate_binary_structure(2, 1)
    return ndimage.binary_erosion(m, st, iterations=int(r), border_value=0)


def despill(rgb, fg, edge):
    """Pink anti-aliasing next to the background takes the nearest clean colour."""
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    spill = np.minimum(r, b) - g
    near = dilate(~fg, edge) & fg
    tinted = near & (spill > 25)
    clean = fg & ~tinted
    if not tinted.any() or not clean.any():
        return rgb, 0
    idx = ndimage.distance_transform_edt(~clean, return_distances=False, return_indices=True)
    out = rgb.copy()
    out[tinted] = rgb[idx[0][tinted], idx[1][tinted]]
    return out, int(tinted.sum())


def foreground(rgb, alpha):
    """(fg mask, despilled rgb, mode, notes)."""
    mode, bg = background_mode(rgb, alpha)
    notes = []
    opaque = alpha >= 128
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    fringe = (r > g + 90) & (b > g + 90) & (np.abs(r - b) < 60) & (r > 150) & (b > 150)
    pure = np.sqrt(((rgb - MAGENTA) ** 2).sum(-1)) < 60
    if mode == "magenta":
        d = np.sqrt(((rgb - bg) ** 2).sum(-1))
        fg = opaque & (d > 90) & ~fringe
        has_magenta = True
    elif mode == "alpha":
        fg = opaque.copy()
        has_magenta = (pure & opaque).sum() > 0.001 * opaque.size
        if has_magenta:            # magenta left inside a transparent image: background too
            fg &= ~(pure | fringe)
            notes.append("magenta pixels inside a transparent background removed")
    else:
        fg = np.ones_like(opaque)
        has_magenta = False
    partial = ((alpha > 8) & (alpha < 247)).sum()
    if mode == "alpha" and partial > 0.5 * max(1, fg.sum()):
        notes.append("the drawing is semi-transparent (alpha < 100%%): treated as opaque")
    elif mode == "alpha" and partial > 0.02 * max(1, fg.sum()):
        notes.append("soft alpha edges (%d semi-transparent pixels, cut at 50%%)" % partial)
    if has_magenta and fg.any():
        edge = max(2, int(round(min(fg.shape) / 250)))
        rgb, n = despill(rgb, fg, edge)
        if n:
            notes.append("despilled %d pink fringe pixels" % n)
    return fg, rgb, mode, notes


# ---- blobs ---------------------------------------------------------------------------------
def components(mask, r=0):
    """Blobs of mask (8-connected, optionally joined by a dilation of r) ->
    (label image on mask pixels, list of dicts)."""
    lab, n = ndimage.label(dilate(mask, r) if r else mask, structure=np.ones((3, 3)))
    lab = np.where(mask, lab, 0)
    out = []
    if n == 0:
        return lab, out
    objs = ndimage.find_objects(lab)
    areas = ndimage.sum(mask, lab, range(1, n + 1))
    for i, (s, a) in enumerate(zip(objs, areas)):
        if s is None or a <= 0:
            continue
        out.append({"id": i + 1, "box": [s[1].start, s[0].start, s[1].stop, s[0].stop], "area": int(a)})
    return lab, out


def _gap(a, b):
    dx = max(0, max(a[0], b[0]) - min(a[2], b[2]))
    dy = max(0, max(a[1], b[1]) - min(a[3], b[3]))
    return (dx * dx + dy * dy) ** 0.5


def _union(boxes):
    return [min(b[0] for b in boxes), min(b[1] for b in boxes), max(b[2] for b in boxes), max(b[3] for b in boxes)]


def _columns(blobs, tol):
    """Group blobs whose x-extents overlap (within tol px) into columns, left to right."""
    cols = []
    for b in sorted(blobs, key=lambda c: c["box"][0]):
        if cols and b["box"][0] < cols[-1]["x1"] + tol:
            cols[-1]["blobs"].append(b)
            cols[-1]["x1"] = max(cols[-1]["x1"], b["box"][2])
        else:
            cols.append({"x0": b["box"][0], "x1": b["box"][2], "blobs": [b]})
    return cols


def _even_split(fg, blobs, n, x0, x1):
    """Columns from an even split of [x0, x1), or None if a drawing straddles a line."""
    w = (x1 - x0) / float(n)
    cuts = [x0 + w * i for i in range(1, n)]
    for b in blobs:
        bw = b["box"][2] - b["box"][0]
        for c in cuts:
            left, right = c - b["box"][0], b["box"][2] - c
            if left > 0.12 * bw and right > 0.12 * bw and left > 0.03 * w and right > 0.03 * w:
                return None
    cols = []
    for i in range(n):
        a, z = int(round(x0 + w * i)), int(round(x0 + w * (i + 1)))
        mem = [b for b in blobs if a <= (b["box"][0] + b["box"][2]) / 2.0 < z]
        if not mem:
            return None
        cols.append({"x0": a, "x1": z, "blobs": mem})
    return cols


def find_frames(fg, n, solid=False):
    """Split a strip's foreground into n frames. Returns (frames, notes).
    Raises FrameCountError when the drawings cannot be matched to n frames."""
    H, W = fg.shape
    if not fg.any():
        raise FrameCountError(0, n, "empty image")
    notes = []
    join = max(1, int(round(min(H, W) / 200.0)))
    lab, blobs = components(fg, 2 * join if solid else join)
    total = float(sum(b["area"] for b in blobs))
    blobs = [b for b in blobs if b["area"] >= max(3, 0.0004 * total)]        # dust / stray pixels
    areas = sorted((b["area"] for b in blobs), reverse=True)
    ref = float(np.median(areas[:n])) if len(areas) >= n else float(areas[0])
    major = [b for b in blobs if b["area"] >= 0.12 * ref]
    minor = [b for b in blobs if b["area"] < 0.12 * ref]
    tol = max(2, int(0.01 * W))
    cols = _columns(major, tol)
    found = len(cols)
    if len(cols) > n:
        # a frame drawn in pieces: join across the smallest gaps, but only when those gaps
        # are clearly smaller than the gaps between frames
        while len(cols) > n:
            gaps = [cols[i + 1]["x0"] - cols[i]["x1"] for i in range(len(cols) - 1)]
            i = int(np.argmin(gaps))
            others = sorted(gaps[:i] + gaps[i + 1:])
            inter = others[-(n - 1):] if n > 1 else []
            if inter and gaps[i] > 0.5 * min(inter):
                raise FrameCountError(found, n, "the gaps between drawings are all alike")
            if not inter and n == 1 and gaps[i] > 0.25 * W:
                raise FrameCountError(found, n, "separate drawings")
            a, b = cols[i], cols[i + 1]
            cols[i:i + 2] = [{"x0": a["x0"], "x1": b["x1"], "blobs": a["blobs"] + b["blobs"]}]
        widths = [c["x1"] - c["x0"] for c in cols]
        if n > 1 and max(widths) > 2.2 * float(np.median(widths)):
            raise FrameCountError(found, n, "joining the pieces gives one frame much wider than the others")
        notes.append("%d drawing pieces joined into %d frames (detached parts)" % (found, n))
    elif len(cols) < n:
        ys, xs = np.nonzero(fg)
        split = _even_split(fg, major, n, 0, W) or _even_split(fg, major, n, int(xs.min()), int(xs.max()) + 1)
        if split is None:
            raise FrameCountError(found, n, "drawings touch or a frame is missing")
        cols = split
        notes.append("%d separate drawing(s) for %d frames: split on an even grid" % (found, n))
    # attach the small blobs (effects) to the nearest frame
    boxes = [_union([b["box"] for b in c["blobs"]]) for c in cols]
    med_w = float(np.median([b[2] - b[0] for b in boxes]))
    far = 0
    for m in minor:
        d = [_gap(bx, m["box"]) + 0.01 * abs((bx[0] + bx[2]) - (m["box"][0] + m["box"][2])) for bx in boxes]
        i = int(np.argmin(d))
        if d[i] > 0.75 * med_w:
            far += 1
        cols[i]["blobs"].append(m)
    if far:
        notes.append("%d effect blob(s) far from any drawing, attached to the nearest frame" % far)
    frames = []
    for c in cols:
        ids = [b["id"] for b in c["blobs"]]
        mask = np.isin(lab, ids)
        big = max((b for b in c["blobs"]), key=lambda b: b["area"])
        box = _union([b["box"] for b in c["blobs"]])
        if solid:
            body_mask = mask
            body = box
        else:
            # body = the largest blob of the frame without the join dilation
            sub = mask[box[1]:box[3], box[0]:box[2]]
            sl, sn = ndimage.label(sub, structure=np.ones((3, 3)))
            if sn > 1:
                sizes = ndimage.sum(sub, sl, range(1, sn + 1))
                keep = sl == (int(np.argmax(sizes)) + 1)
            else:
                keep = sub
            body_mask = np.zeros_like(mask)
            body_mask[box[1]:box[3], box[0]:box[2]] = keep
            ys, xs = np.nonzero(body_mask)
            body = [int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1] if len(xs) else big["box"]
        frames.append(Frame(box, body, mask, body_mask))
    frames.sort(key=lambda f: f.box[0])
    touch = [i + 1 for i, f in enumerate(frames)
             if not solid and (f.box[0] <= 0 or f.box[1] <= 0 or f.box[2] >= W or f.box[3] >= H)]
    if touch:
        notes.append("frame %s touches the image edge (the drawing may be cut off)" % ", ".join(map(str, touch)))
    return frames, notes


def cut_strip(path, n, solid=False):
    """Load an AI strip and find its n frames -> Strip. Raises FrameCountError."""
    rgb, alpha = load_rgba(path)
    fg, rgb, mode, notes = foreground(rgb, alpha)
    if mode == "none":
        H, W = fg.shape
        if not solid:
            notes.append("no background (opaque image): the whole image is the drawing")
        if n == 1:
            frames = [Frame([0, 0, W, H], [0, 0, W, H], fg, fg)]
        else:
            frames = []
            for i in range(n):
                a, z = W * i // n, W * (i + 1) // n
                m = np.zeros_like(fg)
                m[:, a:z] = True
                frames.append(Frame([a, 0, z, H], [a, 0, z, H], m, m))
            notes.append("no background between the frames: split on an even grid")
        return Strip(rgb, fg, frames, mode, notes)
    frames, more = find_frames(fg, n, solid)
    return Strip(rgb, fg, frames, mode, notes + more)

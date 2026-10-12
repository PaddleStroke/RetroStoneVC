#!/usr/bin/env python3
"""The French tarot's scanned faces: apps/boardgames/art/tarot/*.jpg, from scans of the 1898 "Tarot nouveau".

    python3 -I apps/boardgames/tools/prep_tarot_art.py SCANS [--out apps/boardgames/art/tarot] [--debug DIR]
                                                      [--only Trumps_10 ...]

SCANS: a folder of the 38 scans: Trumps_01.jpg .. Trumps_21.jpg, Trumps_Excuse.jpg and
{Spades,Hearts,Diamonds,Clubs}_{Jack,Knight,Queen,King}.jpg, about 1490 x 2645 each. The 22 trumps and the 16 courts:
the number cards and the back stay drawn by the kit (src/cards.c).

Each scan, at full size:
- the card found on the scanner's grey (its four edges fitted as lines), turned straight, cropped just inside its
  cut edge (the printed border kept); the paper filled in where the scan's own rounded corners were (the kit rounds
  the card itself);
- old marks retouched out: the red oval stamp at the bottom (found by its ring; on the paper
  its ink lifted and the paper and the frame's line painted back, on a figure the stamp's own shape fitted and the
  colours round it carried in: see unstamp()), the printer's words on the courts and the Excuse (letters on the
  paper beside the figure: the house draws no words on its cards), specks of dirt on the margin. Those and what
  is left of the stamp on a figure are filled from the card round them, a patch at a time (exemplar inpainting:
  the best matching patch nearby, the edges first), so the paper's grain and the figures' outlines go on through;
- harmonised: the paper's tone brought to the kit's ivory (the drawn number cards' face, 250 246 236) and the ink's
  black to one dark, channel by channel (the same paper and contrast in all 38);
- padded with paper at the sides to the kit's tarot proportion (cards_aspect(DECK_TAROT): 1.6 as tall as wide; the
  1898 cards are about 1.8: nothing of the picture is cut), resized to 400 x 640, lightly sharpened, JPEG quality 80.

--debug DIR: before / after crops of every retouched place and the masks, for checking.
Needs python3 with Pillow, numpy and scipy (the system's packages). The output is deterministic: re-running it gives
the same files.
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import argparse
import os
import sys

import numpy as np
from numpy.lib.stride_tricks import sliding_window_view
from PIL import Image, ImageFilter
from scipy import ndimage
from scipy.spatial import Delaunay

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(os.path.dirname(HERE), "art", "tarot")
ASPECT = 1.6                                    # cards_aspect(DECK_TAROT)
WIDTH = 400
IVORY = np.array([250, 246, 236], np.float32)   # the kit's card face (src/cards.c IVORY)
INK = 22.0                                      # the ink's black after harmonising
INSET = 7                                       # cropped this far inside the cut edge (its dark line)
SUITS = ["Spades", "Hearts", "Diamonds", "Clubs"]
RANKS = ["Jack", "Knight", "Queen", "King"]


def out_name(scan):
    """the asset's name: trump01..trump21, excuse, spades-jack.. (tools/gen_art.py reads it)"""
    if scan == "Trumps_Excuse":
        return "excuse"
    if scan.startswith("Trumps_"):
        return "trump%02d" % int(scan[7:])
    s, r = scan.split("_")
    return "%s-%s" % (s.lower(), r.lower())


def scans_in(folder):
    names = ["Trumps_%02d" % i for i in range(1, 22)] + ["Trumps_Excuse"]
    names += ["%s_%s" % (s, r) for s in SUITS for r in RANKS]
    missing = [n for n in names if not os.path.exists(os.path.join(folder, n + ".jpg"))]
    if missing:
        sys.exit("prep_tarot_art: missing in %s: %s" % (folder, " ".join(missing)))
    return names


# ---- the card on the scan -------------------------------------------------------------------------------------------
def card_mask(a, bg):
    """pixels of the card (the paper is darker and yellower than the scanner's grey)"""
    L = a.mean(axis=2)
    yb = (a[..., 0] - a[..., 2]) - (bg[0] - bg[2])
    d = np.abs(L - bg.mean()) + np.abs(yb)
    return ndimage.uniform_filter(d, 5) > 9


def edges(m):
    """the four edges as lines: left / right x = k*y + c, top / bottom y = k*x + c (robust fits)"""
    h, w = m.shape

    def first(rows):                            # the first card pixel in each row (a run of 5)
        run = rows[:, :-4] & rows[:, 1:-3] & rows[:, 2:-2] & rows[:, 3:-1] & rows[:, 4:]
        idx = np.argmax(run, axis=1).astype(np.float32)
        idx[~run.any(axis=1)] = np.nan
        return idx

    def fit(t, v):
        ok = ~np.isnan(v)
        t, v = t[ok], v[ok]
        for _ in range(3):
            k, c = np.polyfit(t, v, 1)
            r = np.abs(v - (k * t + c))
            keep = r < max(2.0, 2.5 * np.median(r))
            t, v = t[keep], v[keep]
        return k, c

    ys = np.arange(int(h * 0.15), int(h * 0.85), 4)
    xs = np.arange(int(w * 0.15), int(w * 0.85), 4)
    left = fit(ys, first(m[ys]))
    right = fit(ys, (w - 1) - first(m[ys][:, ::-1]))
    top = fit(xs, first(m[:, xs].T))
    bottom = fit(xs, (h - 1) - first(m[::-1, xs].T))
    return left, right, top, bottom


def straighten(img, bg):
    a = np.asarray(img, np.float32)
    l, r, t, b = edges(card_mask(a, bg))
    angle = np.degrees(np.mean([np.arctan(l[0]), np.arctan(r[0]), -np.arctan(t[0]), -np.arctan(b[0])]))
    if abs(angle) > 0.03:
        img = img.rotate(-angle, resample=Image.BICUBIC, fillcolor=tuple(int(v) for v in bg))
        a = np.asarray(img, np.float32)
        l, r, t, b = edges(card_mask(a, bg))
    h, w = a.shape[:2]
    x0 = l[0] * h / 2 + l[1]
    x1 = r[0] * h / 2 + r[1]
    y0 = t[0] * w / 2 + t[1]
    y1 = b[0] * w / 2 + b[1]
    return img, angle, (int(round(x0)) + INSET, int(round(y0)) + INSET, int(round(x1)) - INSET + 1,
                        int(round(y1)) - INSET + 1)


def rounded(h, w, rad):
    """coverage of a rounded rectangle h x w (1 inside)"""
    y, x = np.mgrid[0:h, 0:w].astype(np.float32) + 0.5
    qx = np.abs(x - w / 2) - (w / 2 - rad)
    qy = np.abs(y - h / 2) - (h / 2 - rad)
    d = np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - rad
    return np.clip(0.5 - d, 0, 1)


def paper_tone(a):
    """the margin's paper: the band just inside the crop, left and right, the middle of the card"""
    h, w = a.shape[:2]
    band = np.concatenate([a[int(h * .2):int(h * .8), 3:12].reshape(-1, 3),
                           a[int(h * .2):int(h * .8), w - 12:w - 3].reshape(-1, 3)])
    return np.median(band, axis=0)


# ---- the marks ------------------------------------------------------------------------------------------------------
def ring(a, b, t):
    h, w = int(2 * b + 3 * t), int(2 * a + 3 * t)
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    r = np.hypot((x - w / 2) / a, (y - h / 2) / b)
    k = (np.abs(r - 1) * min(a, b) < t / 2).astype(np.float32)
    return k - k.mean()



STAMP_A, STAMP_B = 57, 84                       # the oval stamp's half axes on the scans (pixels)
STEPS = np.linspace(0, 2.0, 41)                 # ink strengths tried
REF_SCAN = "Diamonds_Knight"                    # its stamp lies on bare paper: the stamp's own shape


def redness_of(a):
    return a[..., 0] - (a[..., 1] + a[..., 2]) / 2


def find_stamp(b, H, band):
    """the stamp in the card's bottom band b (from row band of H): its centre (the oval ring correlated with the thin
    red strokes) and the strokes' map"""
    w = b.shape[1]
    red = redness_of(b)
    hat = red - ndimage.grey_opening(red, size=(17, 17))        # thin red strokes over whatever is under them
    y0, x0 = int(H * 0.84) - band, int(w * 0.26)
    small = hat[y0:, x0:int(w * 0.74)][::2, ::2]
    corr = ndimage.correlate(small, ring(STAMP_A / 2, STAMP_B / 2, 4), mode="constant")
    cy, cx = np.unravel_index(np.argmax(corr), corr.shape)
    return x0 + 2 * cx, y0 + 2 * cy, hat


def ink_colour(b, pc, where):
    """the ink: a red that multiplies what is under it, channel by channel; T at full strength (the strongest ink on
    bare paper), kappa = log T"""
    v = b - pc
    cand = where & (v[..., 0] > -35) & (v[..., 1] < -70) & (v[..., 2] < -70)
    if cand.sum() > 40:
        ex = redness_of(b)[cand]
        T = np.clip(np.median(b[cand][np.argsort(ex)[-max(20, cand.sum() // 10):]], axis=0) / pc, 0.2, 1.0)
    else:
        T = np.array([0.9, 0.55, 0.45], np.float32)
    return T, np.log(T)


def paper_strength(b, pc, kappa):
    """each pixel seen as the paper under ink: the strength s that explains it best, and whether it does"""
    err = np.full(b.shape[:2], np.inf, np.float32)
    s_paper = np.zeros(b.shape[:2], np.float32)
    for s in STEPS:
        e = np.max(np.abs(b - pc * np.exp(s * kappa)), axis=2)
        s_paper[e < err] = s
        err = np.minimum(err, e)
    return s_paper, err < 16 + 0.15 * np.max(np.abs(b - pc), axis=2)


def bottom(a):
    H = a.shape[0]
    band = int(H * 0.78)                        # the stamps are in the bottom fifth
    return a[band:], H, band


def reference(scans):
    """the stamp's shape: its ink on the reference scan (stamped on bare paper), the oval ring made whole (the
    impression is faint on one side): the band inside the ink's outline"""
    ink = np.pad(stamp_ink(os.path.join(scans, REF_SCAN + ".jpg")) > 0, 8)
    pts = np.argwhere(ink)
    grid = np.argwhere(np.ones_like(ink))
    hull = (Delaunay(pts).find_simplex(grid) >= 0).reshape(ink.shape)
    ring_ = hull & ~ndimage.binary_erosion(hull, iterations=12)
    return (ink | ring_).astype(np.float32)


def stamp_ink(path):
    """a scan's stamp: its ink on the paper (a mask) round its centre"""
    a, pc = card_of(path)[:2]
    b, H, band = bottom(a)
    cx, cy, hat = find_stamp(b, H, band)
    ry, rx = STAMP_B + 70, STAMP_A + 70
    win = np.pad(b, ((ry, ry), (rx, rx), (0, 0)), mode="edge")[cy:cy + 2 * ry + 1, cx:cx + 2 * rx + 1]
    T, kappa = ink_colour(win, pc, np.ones(win.shape[:2], bool))
    s, on = paper_strength(win, pc, kappa)
    ink = ndimage.binary_opening(on & (s > 0.15) & (s <= 1.6))
    lab, n = ndimage.label(ndimage.binary_dilation(ink, iterations=6))
    sizes = ndimage.sum(ink, lab, range(1, n + 1))
    ink &= lab == 1 + int(np.argmax(sizes))     # the stamp: the biggest group of ink (no foxing spot)
    ys, xs = np.nonzero(ink)
    return ink[ys.min() - 4:ys.max() + 5, xs.min() - 4:xs.max() + 5].astype(np.float32)


def place(hat, cx, cy, ref):
    """the reference stamp turned (-30..30 degrees), scaled (0.94..1.06) and moved to fit the strokes round (cx, cy):
    its mask on hat's grid"""
    h, w = hat.shape
    P = 120                                     # the stamp may run off the card's edge: room beyond it
    hp = np.pad(hat - ndimage.grey_opening(hat, size=(11, 11)), P)     # strokes, not areas
    Y0, Y1, X0, X1 = cy - 150 + P, cy + 150 + P, cx - 120 + P, cx + 120 + P
    win = hp[Y0:Y1, X0:X1]
    best = (-np.inf, None, 0, 0)
    for ang in range(-30, 31, 3):
        for sc in (0.94, 1.0, 1.06):
            t = ndimage.zoom(ndimage.rotate(ref, ang, order=1, reshape=True), sc, order=1)
            if t.shape[0] > win.shape[0] or t.shape[1] > win.shape[1]:
                continue
            tz = t - t.mean()
            shp = (win.shape[0] + t.shape[0], win.shape[1] + t.shape[1])
            vy, vx = win.shape[0] - t.shape[0] + 1, win.shape[1] - t.shape[1] + 1

            def corr(img, k):
                return np.fft.irfft2(np.fft.rfft2(img, shp) * np.conj(np.fft.rfft2(k, shp)), shp)[:vy, :vx]
            ones = np.ones_like(t)
            s1, s2 = corr(win, ones), corr(win * win, ones)       # normalised: a bright patch is no stamp
            var = np.maximum(s2 - s1 * s1 / t.size, 1e-6)
            c = corr(win, tz) / (np.linalg.norm(tz) * np.sqrt(var))
            k = np.unravel_index(np.argmax(c), c.shape)
            if c[k] > best[0]:
                best = (c[k], t > 0.5, Y0 + k[0], X0 + k[1])
    m = np.zeros((h + 2 * P, w + 2 * P), bool)
    _, t, y, x = best
    m[y:y + t.shape[0], x:x + t.shape[1]] = t
    return m[P:P + h, P:P + w]


def line_rows(b, pc, region):
    """the frame's horizontal lines across the stamp's place: the rows dark just left and right of it; their
    colours there"""
    ys, xs = np.nonzero(region)
    x0, x1 = xs.min(), xs.max()
    left = b[:, max(0, x0 - 24):max(1, x0 - 4)].mean(axis=1)
    right = b[:, min(b.shape[1] - 1, x1 + 5):x1 + 25].mean(axis=1)
    if right.size == 0 or np.isnan(right).any():
        right = left
    dark = np.ones(b.shape[0], bool)
    for side in (left.mean(axis=1), right.mean(axis=1)):  # thin: darker than 7 rows above and below
        above, below = np.roll(side, 7), np.roll(side, -7)
        dark &= (side < np.minimum(above, below) - 35) & (np.minimum(above, below) > pc.mean() - 45)
    dark = ndimage.binary_dilation(dark, iterations=1)    # the line's soft edges
    rows = np.zeros(b.shape[0], bool)
    rows[ys.min():ys.max() + 1] = dark[ys.min():ys.max() + 1]
    return rows, left, right, x0 - 14, x1 + 15


def draw_lines(b, where, lines):
    """the lines drawn across where, each row's colour going from its left to its right one"""
    rows, left, right, xa, xb = lines
    for y in np.nonzero(rows)[0]:
        cols = np.nonzero(where[y])[0]
        t = ((cols - xa) / max(1, xb - xa))[:, None]
        b[y, cols] = left[y] * (1 - t) + right[y] * t


def paper_fill(b, pc, hole, known):
    """hole painted with the paper round about (known), smoothly"""
    known = known.astype(np.float32)
    for sigma in (6, 14, 30, 60, 120):
        k = ndimage.gaussian_filter(known, sigma)
        fill = np.stack([ndimage.gaussian_filter(b[..., c] * known, sigma) for c in range(3)], axis=2)
        ok = hole & (k > 0.05)
        b[ok] = fill[ok] / k[ok][:, None]
        hole = hole & ~ok
    if hole.any() and known.any():
        b[hole] = np.median(b[known > 0], axis=0)




def unstamp(a, pc, trump, ref):
    """the red stamp at the bottom centre lifted off (a is changed in place); the pixels it covered, its centre.

    On the paper (the trumps' number band, the margins of the courts and the Excuse) any pixel that is the paper
    under some strength of the ink is ink, a smeared stamp too: it is painted with the paper round about, and the
    frame's straight lines are drawn on through it. On the figure (a hat, a crown, a ruff: their colours can look
    like inked paper) the stamp's own shape (the reference scan's, turned and moved to fit the strokes) says where
    the ink is; under each inked pixel lies the colour of the pixels round the ink, at the pixel's own lightness
    (read through the ink: the red channel, which it dims least)."""
    b, H, band = bottom(a)
    h, w = b.shape[:2]
    full = np.zeros((H, w), bool)
    cx, cy, hat = find_stamp(b, H, band)
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    grow = 2.0 if trump else 1.3
    region = np.hypot((x - cx) / (STAMP_A * grow + 16), (y - cy) / (STAMP_B * grow + 16)) <= 1
    near = ndimage.binary_dilation(region, iterations=30) & ~region
    excess = redness_of(b) - redness_of(pc)
    T, kappa = ink_colour(b, pc, region)
    s_paper, on_paper = paper_strength(b, pc, kappa)
    on_paper &= region & (s_paper <= 1.6)
    lines = line_rows(b, pc, region)
    in_line = ndimage.binary_dilation(lines[0], iterations=2)[:, None] & region
    fig = np.zeros((h, w), bool)
    # below the frame's line lies only the margin's paper: there, as on the trumps, all the stamp's place
    margin = np.zeros(h, bool)
    if lines[0].any():
        margin[np.nonzero(lines[0])[0].min():] = True
    if not trump:
        sp = np.where(on_paper, s_paper, 0)
        inkness = np.clip(sp - ndimage.median_filter(sp, size=25), 0, 1)   # stronger than round about
        shape = place(inkness, cx, cy, ref)
        # this impression's own strokes beyond the reference's (a fuller ring, a heavier letter): strong ink near it
        extra = ndimage.binary_dilation(shape, iterations=8) & (inkness > 0.5)
        stamp = ndimage.binary_dilation(shape | extra, iterations=4)
        # the colours round the ink, carried in (the nearest outside pixel's, then smoothed in from the edges)
        known = (~stamp).astype(np.float32)
        k = ndimage.gaussian_filter(known, 2)
        col = np.stack([ndimage.gaussian_filter(b[..., c] * known, 2) for c in range(3)], axis=2)
        col /= np.maximum(k, 1e-3)[..., None]
        _, (iy, ix) = ndimage.distance_transform_edt(stamp, return_indices=True)
        col = col[iy, ix]
        ys, xs = np.nonzero(stamp)
        sl = (slice(ys.min() - 1, ys.max() + 2), slice(xs.min() - 1, xs.max() + 2))
        c, m = col[sl], stamp[sl]
        for _ in range(300):
            nb = (np.roll(c, 1, 0) + np.roll(c, -1, 0) + np.roll(c, 1, 1) + np.roll(c, -1, 1)) / 4
            c[m] = nb[m]
        fig = stamp & (np.max(np.abs(col - pc), axis=2) >= 30) & ~in_line & ~margin[:, None]
        Tf, kf = ink_colour(b, pc, stamp)
        under, obs = col[fig], b[fig]
        s = np.clip(np.log(np.maximum(obs[:, 1], 1) / np.maximum(under[:, 1], 1)) / kf[1], 0, 1.4)
        light = obs[:, 0] / np.exp(s * kf[0]) / np.maximum(under[:, 0], 1)
        b[fig] = np.clip(under * np.clip(light, 0.1, 1.15)[:, None], 0, 255)
    # the paper: the ink on it (strong ink, the fainter touching it, any redder than the paper)
    # (the trumps: anywhere in the stamp's place; the courts: on and just round the stamp's shape, off the figure)
    paper_zone = region & ~ndimage.binary_dilation(fig, iterations=3)
    if not trump:   # off the figure's areas: wider than a thin stroke, reaching out beyond the stamp
        far = np.max(np.abs(ndimage.uniform_filter(b, (3, 3, 1)) - pc), axis=2) >= 30
        far &= ~ndimage.binary_dilation(lines[0], iterations=4)[:, None]   # not the frame's line
        area = ndimage.binary_opening(far,
                                      structure=np.ones((9, 9)))
        lab, n = ndimage.label(area)
        out = np.unique(lab[area & ~ndimage.binary_dilation(stamp, iterations=12)])
        area = np.isin(lab, out[out > 0])
        zone = ndimage.binary_dilation(stamp, iterations=6) & ~ndimage.binary_dilation(area, iterations=2)
        paper_zone &= zone | margin[:, None]
    strong = on_paper & (s_paper >= 0.55)
    take = ndimage.binary_dilation(strong, iterations=3, mask=on_paper) | strong | on_paper & (excess > 4)
    if not trump:
        take |= stamp
    inked = ndimage.binary_dilation(take, iterations=2) & paper_zone
    lum = b.mean(axis=2)
    # red ink on a black numeral or line: back to its black (dark ink alone on the paper is ink)
    black = (lum <= 90) & (np.ptp(b, axis=2) < 30) & ~inked
    black = ndimage.binary_opening(black, iterations=1)
    near_black = ndimage.binary_dilation(black, iterations=6) | in_line
    dark = paper_zone & near_black & (inked & (lum <= 90) | (lum <= 140) & (excess > 3))
    b[dark] = b[dark].min(axis=1)[:, None]
    hole = (inked | paper_zone & (excess > 3) & (lum > 90)) & ~dark
    paperlike = np.max(np.abs(b - pc), axis=2) < 45
    paper_fill(b, pc, hole, paperlike & ~hole & (redness_of(b) - redness_of(pc) < 4) & (region | near) & ~in_line)
    inked |= hole
    draw_lines(b, inked | fig & in_line, lines)
    full[band:] = inked | fig
    return full, (cx, cy + band)


def frame_lines(L, pl):
    """how far in from each edge (left, right, top, bottom) the printed frame's line runs: the first dark row or
    column along the middle of the side"""
    h, w = L.shape

    def first(profile):
        k = np.nonzero(profile < pl - 40)[0]
        return int(k[0]) if len(k) else 12

    rows, cols = L[int(h * .3):int(h * .7)], L[:, int(w * .3):int(w * .7)]
    return (first(rows[:, :90].mean(axis=0)), first(rows[:, ::-1][:, :90].mean(axis=0)),
            first(cols[:90].mean(axis=1)), first(cols[::-1][:90].mean(axis=1)))


def letters_mask(a, pc):
    """the printer's words (FRANCE, the maker's name) beside the courts' figure: small dark letters on the paper, in a
    column of four or more; and specks of dirt on the margin"""
    h, w = a.shape[:2]
    L = a.mean(axis=2)
    pl = pc.mean()
    paper = np.max(np.abs(a - pc), axis=2) < 35
    dark = L < pl - 55
    lab, n = ndimage.label(dark, structure=np.ones((3, 3)))
    objs = ndimage.find_objects(lab)
    cand = []
    small_dark = np.zeros_like(dark)
    for i, sl in enumerate(objs):
        hh, ww = sl[0].stop - sl[0].start, sl[1].stop - sl[1].start
        if hh <= 45 and ww <= 45:
            small_dark[sl] |= lab[sl] == i + 1
    for i, sl in enumerate(objs):
        hh, ww = sl[0].stop - sl[0].start, sl[1].stop - sl[1].start
        if hh > 45 or ww > 45:
            continue
        cy, cx = (sl[0].start + sl[0].stop) / 2, (sl[1].start + sl[1].stop) / 2
        if not (0.02 * w < cx < 0.22 * w or 0.78 * w < cx < 0.98 * w):
            continue
        Y0, Y1 = max(0, sl[0].start - 8), min(h, sl[0].stop + 8)
        X0, X1 = max(0, sl[1].start - 8), min(w, sl[1].stop + 8)
        ok = (paper | small_dark)[Y0:Y1, X0:X1]
        if ok.mean() < 0.9:
            continue
        cand.append((cy, cx, sl, i + 1))
    # columns of letters: centres within 22 px across, gaps under 34 px along
    groups, used = [], set()
    cand.sort()
    for j, c in enumerate(cand):
        if j in used:
            continue
        g, last = [j], c
        for k in range(j + 1, len(cand)):
            if k in used:
                continue
            o = cand[k]
            if abs(o[1] - c[1]) < 22 and o[2][0].start - last[2][0].stop < 34:
                g.append(k)
                last = o
        if len(g) >= 4:
            groups.append(g)
            used.update(g)
    m = np.zeros_like(dark)
    words = []
    for g in groups:
        ys = [cand[k][2][0] for k in g]
        xs = [cand[k][2][1] for k in g]
        Y0, Y1 = min(s.start for s in ys) - 6, max(s.stop for s in ys) + 6
        X0, X1 = min(s.start for s in xs) - 6, max(s.stop for s in xs) + 6
        m[Y0:Y1, X0:X1] |= small_dark[Y0:Y1, X0:X1] | (L[Y0:Y1, X0:X1] < pl - 25)
        words.append((X0, Y0, X1, Y1))
    # specks on the margin (between the cut edge and the printed frame's line): small grey marks, paper all round
    speck = (L < pl - 22)
    lab2, n2 = ndimage.label(speck, structure=np.ones((3, 3)))
    fl, fr, ft, fb = frame_lines(L, pl)
    fh, fw = h - ft - fb + 8, w - fl - fr + 8          # outside the frame (its corners rounded)
    edge = np.ones_like(dark)
    edge[ft - 4:ft - 4 + fh, fl - 4:fl - 4 + fw] = rounded(fh, fw, 0.09 * w) < 0.5
    for i, sl in enumerate(ndimage.find_objects(lab2)):
        hh, ww = sl[0].stop - sl[0].start, sl[1].stop - sl[1].start
        if hh > 40 or ww > 40 or not edge[sl].all():
            continue
        Y0, Y1 = max(0, sl[0].start - 6), min(h, sl[0].stop + 6)
        X0, X1 = max(0, sl[1].start - 6), min(w, sl[1].stop + 6)
        if paper[Y0:Y1, X0:X1].mean() < 0.8:
            continue
        m[sl] |= lab2[sl] == i + 1
        words.append((X0, Y0, X1, Y1))
    return ndimage.binary_dilation(m, iterations=4), words


# ---- the retouch: exemplar inpainting ---------------------------------------------------------------------------------
def inpaint(a, hole, valid, patch=9, win=56):
    """fills hole (bool) in a (float RGB, in place) a patch at a time: the front's patch with the most known pixels
    and the strongest edges first, copied from the best matching whole patch within win pixels (valid: allowed as
    a source)"""
    r = patch // 2
    lab, n = ndimage.label(ndimage.binary_dilation(hole, iterations=r + 2))
    for sl in ndimage.find_objects(lab):
        Y0, Y1 = max(0, sl[0].start - win - r), min(a.shape[0], sl[0].stop + win + r)
        X0, X1 = max(0, sl[1].start - win - r), min(a.shape[1], sl[1].stop + win + r)
        sub = a[Y0:Y1, X0:X1]
        fill = hole[Y0:Y1, X0:X1].copy()
        _inpaint_part(sub, fill, valid[Y0:Y1, X0:X1], r, win)


def _inpaint_part(img, fill, valid, r, win):
    H, W = fill.shape
    patch = 2 * r + 1
    conf = (~fill).astype(np.float32)
    gray = img.mean(axis=2)
    gm = np.hypot(ndimage.sobel(gray, 0), ndimage.sobel(gray, 1)) / 8
    border = np.zeros_like(fill)
    border[r:H - r, r:W - r] = True
    while fill.any():
        known = ~fill
        front = fill & ndimage.binary_dilation(known) & border
        if not front.any():
            front = fill & border
            if not front.any():
                break
        ys, xs = np.nonzero(front)
        C = ndimage.uniform_filter(conf, patch)[ys, xs]
        D = ndimage.maximum_filter(gm * known, patch)[ys, xs] / 40 + 0.15
        k = int(np.argmax(C * np.minimum(D, 1.5)))
        y, x = ys[k], xs[k]
        tgt = img[y - r:y + r + 1, x - r:x + r + 1]
        tk = known[y - r:y + r + 1, x - r:x + r + 1].astype(np.float32)
        sy0, sy1 = max(r, y - win), min(H - r - 1, y + win)
        sx0, sx1 = max(r, x - win), min(W - r - 1, x + win)
        reg = img[sy0 - r:sy1 + r + 1, sx0 - r:sx1 + r + 1]
        okreg = (known & valid)[sy0 - r:sy1 + r + 1, sx0 - r:sx1 + r + 1]
        src_ok = sliding_window_view(okreg, (patch, patch)).all(axis=(2, 3))
        if not src_ok.any():                    # nothing whole nearby: the known neighbours' mean
            m = tk > 0
            fillv = tgt[m].mean(axis=0) if m.any() else img[known].mean(axis=0)
            hole = ~(tk > 0)
            tgt[hole] = fillv
            fill[y - r:y + r + 1, x - r:x + r + 1] = False
            conf[y - r:y + r + 1, x - r:x + r + 1][hole] = 0.1
            continue
        wv = sliding_window_view(reg, (patch, patch), axis=(0, 1))     # (ny, nx, 3, p, p)
        diff = wv - tgt.transpose(2, 0, 1)[None, None]
        ssd = np.einsum("ijcuv,uv->ij", diff * diff, tk)
        jy, jx = np.mgrid[sy0:sy1 + 1, sx0:sx1 + 1]
        ssd = ssd / max(tk.sum(), 1) + 0.02 * np.hypot(jy - y, jx - x)  # near ones win ties
        ssd[~src_ok] = np.inf
        by, bx = np.unravel_index(np.argmin(ssd), ssd.shape)
        by, bx = by + sy0, bx + sx0
        src = img[by - r:by + r + 1, bx - r:bx + r + 1]
        hole = fill[y - r:y + r + 1, x - r:x + r + 1].copy()
        tgt[hole] = src[hole]
        conf[y - r:y + r + 1, x - r:x + r + 1][hole] = C[k]
        fill[y - r:y + r + 1, x - r:x + r + 1] = False


# ---- one card ---------------------------------------------------------------------------------------------------------
def card_of(path):
    """the card cut out of its scan, straight (float RGB), its paper's tone, the turn"""
    img = Image.open(path).convert("RGB")
    a0 = np.asarray(img, np.float32)
    bg = np.median(np.concatenate([a0[:6].reshape(-1, 3), a0[-6:].reshape(-1, 3), a0[:, :6].reshape(-1, 3),
                                   a0[:, -6:].reshape(-1, 3)]), axis=0)
    img, angle, box = straighten(img, bg)
    a = np.array(img.crop(box), np.float32)
    return a, paper_tone(a), angle


def prepare(path, name, debug, ref):
    a, pc, angle = card_of(path)
    h, w = a.shape[:2]
    shape = rounded(h, w, 0.068 * w - INSET)
    inside = shape > 0.999
    trump = name.startswith("Trumps_") and name != "Trumps_Excuse"
    before = a.copy()
    sm, (cx, cy) = unstamp(a, pc, trump, ref)
    lm, words = letters_mask(a, pc)
    hole = lm & inside
    valid = inside
    if not trump:   # what is still redder than round about in the stamp's place: retouched from the figure round it
        place_ = ndimage.binary_dilation(sm, iterations=4)
        red = redness_of(a)
        known = (~place_).astype(np.float32)
        bg = ndimage.gaussian_filter(red * known, 10) / np.maximum(ndimage.gaussian_filter(known, 10), 1e-3)
        rest = place_ & (red - bg > 12)
        hole |= ndimage.binary_dilation(rest, iterations=2) & inside
        valid = inside & ~ndimage.binary_dilation(place_, iterations=2)
        sm = sm | rest
    inpaint(a, hole, valid)
    after = a.copy()
    a = a * shape[..., None] + pc * (1 - shape[..., None])       # the scan's corners: paper (the kit rounds)
    # harmonise: the paper to ivory, the darkest ink to one black, channel by channel
    flat = a.reshape(-1, 3)
    lumf = flat.mean(axis=1)
    blk = flat[lumf <= np.percentile(lumf, 0.5)].mean(axis=0)     # the darkest ink's own colour: to neutral
    blk = np.minimum(blk, pc - 80)
    a = INK + (a - blk) * (IVORY - INK) / (pc - blk)
    a = np.clip(a, 0, 255)
    # padded with paper to the kit's proportion
    want_w = int(round(h / ASPECT))
    if want_w >= w:
        pl = (want_w - w) // 2
        out = np.empty((h, want_w, 3), np.float32)
        out[:] = IVORY
        out[:, pl:pl + w] = a
        pad = "sides %d+%d px (%.1f%% of the width each)" % (pl, want_w - w - pl, 100.0 * pl / want_w)
    else:
        want_h = int(round(w * ASPECT))
        pt = (want_h - h) // 2
        out = np.empty((want_h, w, 3), np.float32)
        out[:] = IVORY
        out[pt:pt + h] = a
        pad = "top and bottom %d px" % pt
    size = (WIDTH, int(round(WIDTH * ASPECT)))
    res = Image.fromarray(out.round().astype(np.uint8)).resize(size, Image.LANCZOS)
    res = res.filter(ImageFilter.UnsharpMask(radius=1.0, percent=45, threshold=4))
    if debug:
        y0, y1 = max(0, cy - 150), min(h, cy + 140)
        x0, x1 = max(0, cx - 150), min(w, cx + 150)
        dbg = np.concatenate([before[y0:y1, x0:x1], np.where(sm[y0:y1, x0:x1, None], [255, 0, 255],
                              before[y0:y1, x0:x1]), after[y0:y1, x0:x1]], axis=1)
        Image.fromarray(np.clip(dbg, 0, 255).astype(np.uint8)).save(os.path.join(debug, name + "_stamp.png"))
        k = WIDTH / out.shape[1]                # the same place on the face as shipped, three times as big
        fx, fy = (cx + (out.shape[1] - w) // 2) * k, (cy + (out.shape[0] - h) // 2) * k
        res.crop((int(fx) - 50, int(fy) - 45, int(fx) + 50, int(fy) + 35)).resize((300, 240), Image.NEAREST).save(
            os.path.join(debug, name + "_final.png"))
        for k, (X0, Y0, X1, Y1) in enumerate(words):
            X0, Y0, X1, Y1 = max(0, X0 - 20), max(0, Y0 - 20), min(w, X1 + 20), min(h, Y1 + 20)
            dbg = np.concatenate([before[Y0:Y1, X0:X1], after[Y0:Y1, X0:X1]], axis=1)
            Image.fromarray(np.clip(dbg, 0, 255).astype(np.uint8)).save(os.path.join(debug, "%s_mark%d.png" % (name, k)))
    return res, "%s: turned %.2f deg, card %d x %d (%.3f), padded %s, stamp at %d,%d, %d other marks" % (
        name, angle, w, h, h / w, pad, cx, cy, len(words))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("scans")
    ap.add_argument("--out", default=OUT)
    ap.add_argument("--debug")
    ap.add_argument("--only", nargs="*")
    a = ap.parse_args()
    names = scans_in(a.scans)
    if a.only:
        names = [n for n in names if n in a.only]
    os.makedirs(a.out, exist_ok=True)
    if a.debug:
        os.makedirs(a.debug, exist_ok=True)
    ref = reference(a.scans)
    print("prep_tarot_art: the stamp's shape %d x %d, %d px of ink" % (ref.shape[1], ref.shape[0], ref.sum()))
    if a.debug:
        Image.fromarray((ref * 255).astype(np.uint8)).save(os.path.join(a.debug, "_reference.png"))
    total = 0
    for n in names:
        img, log = prepare(os.path.join(a.scans, n + ".jpg"), n, a.debug, ref)
        path = os.path.join(a.out, out_name(n) + ".jpg")
        img.save(path, "JPEG", quality=80, optimize=True)
        size = os.path.getsize(path)
        total += size
        print("%s -> %s (%d bytes)" % (log, os.path.basename(path), size), flush=True)
    print("prep_tarot_art: %d faces, %d bytes" % (len(names), total))


if __name__ == "__main__":
    main()

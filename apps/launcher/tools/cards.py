"""The launcher's game cards (tools/gen_catalog.py): a moment of play, pixel for pixel, in SNES-style palettes.

A card is cut out of a screen of the game played headless at 1x (apps/launcher/cards/<id>.png, made by
tools/card_frames.sh from the table apps/launcher/cards/cards.cfg): RSB_CARD_W x RSB_THUMB_H pixels taken as they are,
never scaled (a scaled picture blurs pixel art). Its colours: each 8x8 tile picks one of CARD_PALS palettes of 15
colours, the palettes chosen for that card (tile_palettes()); most cards keep every colour of the game exactly. The
launcher gives each card column its own palettes (a viewport per column: launcher.c, "Video"), so every card on the
screen has its CARD_PALS palettes.
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import os

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
CARDS = os.path.join(ROOT, "apps", "launcher", "cards")
CARD_W, THUMB_H = 96, 72
CARD_PALS, CARD_COLS = 6, 15          # BG palettes 1..6 of the card's column, 15 colours each (0 is clear)
SCREEN_W, SCREEN_H = 320, 240
WEIGHT = np.array([3, 4, 2], np.int64)   # the colour distance: weighted RGB (green counts most)


def to555(a):
    """uint8 RGB -> the nearest RGB555 colour, back in 8 bits."""
    q = (a.astype(np.int32) * 31 + 127) // 255
    return ((q << 3) | (q >> 2)).astype(np.uint8)


# ---- the table ----------------------------------------------------------------------------------------------------
def read_table(path=os.path.join(CARDS, "cards.cfg")):
    """{id: {"frame": n, "x": x, "y": y, "opts": {k: v}, "sprites": [(png, (x, y, w, h), (dx, dy))]}}"""
    table = {}
    if not os.path.exists(path):
        return table
    for n, line in enumerate(open(path, encoding="utf-8"), 1):
        tok = line.split("#", 1)[0].split()
        if not tok:
            continue
        if len(tok) < 3:
            raise SystemExit(f"{path}:{n}: <id> <frame> <x>,<y> [...]")
        x, y = (int(v) for v in tok[2].split(","))
        if not (0 <= x <= SCREEN_W - CARD_W and 0 <= y <= SCREEN_H - THUMB_H):
            raise SystemExit(f"{path}:{n}: the card {x},{y} is not inside the {SCREEN_W}x{SCREEN_H} screen")
        e = {"frame": int(tok[1]), "x": x, "y": y, "opts": {}, "sprites": []}
        for kv in tok[3:]:
            k, _, v = kv.partition("=")
            if k == "sprite":                       # art/sprites.png:66,0,36,48@-2,24
                png, _, rest = v.partition(":")
                rect, _, at = rest.partition("@")
                e["sprites"].append((png, tuple(int(c) for c in rect.split(",")), tuple(int(c) for c in at.split(","))))
            else:
                e["opts"][k] = v
        table[tok[0]] = e
    return table


def title_fallback(gid):
    """A game with no line in the table: the middle of its title screen (the docs' screenshots are integer scaled:
    taken back to 1x, pixel for pixel)."""
    src = Image.open(os.path.join(ROOT, "games", gid, "docs", "screenshots", "title.png")).convert("RGB")
    k = max(1, src.size[1] // SCREEN_H)
    a = np.array(src)[::k, ::k]
    h, w = a.shape[:2]
    x, y = (w - CARD_W) // 2, (h - THUMB_H) // 2
    return a[y:y + THUMB_H, x:x + CARD_W]


def source(gid, table):
    """The card's pixels (THUMB_H x CARD_W x 3, uint8, RGB555 colours), as the game shows them."""
    e = table.get(gid)
    png = os.path.join(CARDS, gid + ".png")
    if not e or not os.path.exists(png):
        return to555(title_fallback(gid))
    a = np.array(Image.open(png).convert("RGB"))
    if a.shape[:2] != (SCREEN_H, SCREEN_W):
        raise SystemExit(f"{png}: {a.shape[1]}x{a.shape[0]}, a 1x screen is {SCREEN_W}x{SCREEN_H}")
    a = a[e["y"]:e["y"] + THUMB_H, e["x"]:e["x"] + CARD_W].copy()
    for spng, (sx, sy, sw, sh), (dx, dy) in e["sprites"]:
        s = np.array(Image.open(os.path.join(ROOT, "games", gid, spng)).convert("RGB"))[sy:sy + sh, sx:sx + sw]
        for y in range(sh):
            for x in range(sw):
                X, Y = dx + x, dy + y
                c = s[y, x]
                if 0 <= X < CARD_W and 0 <= Y < THUMB_H and not (c[0] == 255 and c[1] == 0 and c[2] == 255):
                    a[Y, X] = c
    return to555(a)


# ---- a locked game (the free app): its colours as they are, a gold ribbon across the top-right corner (the star of
# the "FULL VERSION" pill) and a padlock badge in the bottom-right corner ------------------------------------------------
INK = {"o": (22, 18, 40), "S": (222, 230, 238), "T": (128, 138, 156), "H": (255, 240, 150), "G": (252, 216, 72),
       "D": (186, 128, 22), "R": (232, 88, 76), "N": (42, 68, 118)}     # 8 colours: little room taken in the palettes
PADLOCK = [                                     # 16x16 (launcher.c drew it as a sprite before)
    "................", ".....oooooo.....", "....oSSSSSSo....", "...oSSooooSSo...",
    "...oSo....oTo...", "...oSo....oTo...", "...oSo....oTo...", ".oooooooooooooo.",
    ".oHHHHHHHHHHHGo.", ".oHGGGGGGGGGGDo.", ".oHGGGGooGGGGDo.", ".oHGGGGooGGGGDo.",
    ".oHGGGGGoGGGGDo.", ".oGDDDDDDDDDDDo.", ".oooooooooooooo.", "................"]
STAR = ["..R..", ".RRR.", "RRRRR", ".RRR.", "R...R"]
RIBBON = (11, 21)                               # the band: (W - 1 - x) + y in this range, outline included


def locked_look(a):
    """a: THUMB_H x CARD_W x 3. The locked card: the picture untouched (vivid: it should make one want it), the
    ribbon and the padlock badge drawn over it."""
    a = a.copy()
    lo, hi = RIBBON
    for y in range(hi + 1):
        for x in range(CARD_W - hi - 1, CARD_W):
            u = (CARD_W - 1 - x) + y
            if lo <= u <= hi:
                k = "o" if u in (lo, hi) else "H" if u == lo + 1 else "D" if u == hi - 1 else "G"
                a[y, x] = INK[k]
    m = (lo + hi) // 2                          # the star in the middle of the band, on the corner's diagonal
    cx, cy = CARD_W - 1 - (m + 1) // 2, m // 2
    for y, row in enumerate(STAR):
        for x, ch in enumerate(row):
            if ch != ".":
                a[cy - 2 + y, cx - 2 + x] = INK[ch]
    # the badge: a navy plate with rounded corners, outlined, lit on its top and left; the padlock on it
    bw = bh = 20
    x0, y0 = CARD_W - bw - 2, THUMB_H - bh - 2
    for y in range(bh):
        for x in range(bw):
            cut = (x in (0, bw - 1) and y in (0, 1, bh - 2, bh - 1)) or (y in (0, bh - 1) and x in (1, bw - 2))
            if cut:
                continue
            edge = x in (0, bw - 1) or y in (0, bh - 1) or (x in (1, bw - 2) and y in (1, bh - 2))
            lit = not edge and (x == 1 or y == 1) and x < bw - 2 and y < bh - 2
            a[y0 + y, x0 + x] = INK["o" if edge else "T" if lit else "N"]
    for y, row in enumerate(PADLOCK):
        for x, ch in enumerate(row):
            if ch != ".":
                a[y0 + 2 + y, x0 + 2 + x] = INK[ch]
    return to555(a)


# ---- SNES-style palettes: each 8x8 tile picks one of npal palettes of ncol colours -----------------------------------
def _dist(px, pal):
    """weighted squared distances, len(px) x len(pal)"""
    d = px[:, None, :].astype(np.int64) - pal[None, :, :].astype(np.int64)
    return (d * d * WEIGHT).sum(-1)


def _reduce(colours, counts, n):
    """n colours of the game's for these colours (with their pixel counts): weighted k-means, each centre then moved
    to the real colour of its cluster nearest to it (a card shows only the game's own colours)."""
    if len(colours) <= n:
        return colours.copy()
    order = np.argsort(-counts)
    cent = [colours[order[0]].astype(np.float64)]
    for _ in range(1, n):                          # farthest point, weighted by use
        d = _dist(colours, np.array(cent)).min(1) * np.sqrt(counts)
        cent.append(colours[int(d.argmax())].astype(np.float64))
    cent = np.array(cent)
    for _ in range(12):
        lab = _dist(colours, cent).argmin(1)
        for k in range(n):
            m = lab == k
            if m.any():
                cent[k] = (colours[m] * counts[m, None]).sum(0) / counts[m].sum()
    lab = _dist(colours, cent).argmin(1)
    out = []
    for k in range(n):
        m = np.where(lab == k)[0]
        if len(m):
            d = _dist(colours[m], cent[k:k + 1])[:, 0]
            out.append(colours[m[int(np.argmin(d))]])
    return np.unique(np.array(out, np.uint8), axis=0)


def _palette_for(colours, hist, ncol):
    """the best palette (<= ncol colours) for tiles of these colour histograms (N x len(colours))"""
    counts = hist.sum(0)
    used = counts > 0
    return _reduce(colours[used], counts[used], ncol)


def _pack(sets, order, npal, ncol):
    """the tiles' colour sets packed into npal palettes, in this order, each into the palette it grows least (the
    first that fits); a tile that fits nowhere goes where the union grows least (that palette is reduced later)"""
    bins, assign = [], np.zeros(len(sets), int)
    for t in order:
        best, grow = -1, None
        for b, s in enumerate(bins):
            g = len(sets[t] - s)
            if len(s) + g <= ncol and (grow is None or g < grow):
                best, grow = b, g
        if best < 0 and len(bins) < npal:
            bins.append(set())
            best = len(bins) - 1
        if best < 0:
            best = min(range(len(bins)), key=lambda b: (len(sets[t] - bins[b]), len(bins[b])))
        bins[best] |= sets[t]
        assign[t] = best
    return assign, len(bins), max(len(s) for s in bins) <= ncol


def _refine(colours, hist, assign, nbins, ncol):
    """the palettes of a packing (reduced to ncol colours), then each tile moves to its best palette and the
    palettes are made again, until it settles. Returns (palettes, assign, total error)."""
    pals = [_palette_for(colours, hist[assign == b], ncol) for b in range(nbins)]
    for _ in range(8):
        # a tile's error with a palette: its colour histogram times each colour's distance to that palette
        err = np.stack([hist @ _dist(colours, p).min(1) for p in pals], 1)
        new = err.argmin(1)
        if (new == assign).all():
            break
        assign = new
        pals = [_palette_for(colours, hist[assign == b], ncol) if (assign == b).any() else pals[b]
                for b in range(nbins)]
    err = np.stack([hist @ _dist(colours, p).min(1) for p in pals], 1)
    assign = err.argmin(1)
    return pals, assign, int(err.min(1).sum())


def tile_palettes(img, npal=CARD_PALS, ncol=CARD_COLS, tries=32):
    """img: H x W x 3 RGB555 colours (H, W multiples of 8). Returns (indices H x W of 1..ncol, palettes npal x 16 x 3
    (entry 0 unused), tile_pal th x tw). Exact whenever the tiles' colour sets can be packed into the palettes (first
    fit decreasing, then other orders); else the packing that leaves the least error."""
    h, w = img.shape[:2]
    th, tw = h // 8, w // 8
    tiles = img.reshape(th, 8, tw, 8, 3).transpose(0, 2, 1, 3, 4).reshape(th * tw, 64, 3)
    colours, inv = np.unique(tiles.reshape(-1, 3), axis=0, return_inverse=True)
    inv = inv.reshape(len(tiles), 64)
    hist = np.zeros((len(tiles), len(colours)), np.int64)        # each tile's colour histogram
    np.add.at(hist, (np.repeat(np.arange(len(tiles)), 64), inv.ravel()), 1)
    sets = [frozenset(np.nonzero(h)[0].tolist()) for h in hist]
    rng = np.random.default_rng(1)                 # fixed: the same cards at every build
    size = np.array([len(s) for s in sets])
    best = None
    for k in range(tries):
        # the richest tiles first; then the same with ties shuffled, then partly shuffled orders
        noise = rng.random(len(sets)) * (0 if k == 0 else 1 if k < tries // 2 else 6)
        order = np.argsort(-(size + noise), kind="stable")
        assign, nbins, exact = _pack(sets, order, npal, ncol)
        pals, assign, err = _refine(colours, hist, assign, nbins, ncol)
        if best is None or err < best[2]:
            best = (pals, assign, err)
        if err == 0:
            break
    pals, assign, _ = best
    idx = np.zeros((len(tiles), 64), int)
    full = np.zeros((npal, 16, 3), np.uint8)
    for b, p in enumerate(pals):
        full[b, 1:1 + len(p)] = p
        m = assign == b
        if m.any():
            idx[m] = _dist(tiles[m].reshape(-1, 3), p).argmin(1).reshape(-1, 64) + 1
    img_idx = idx.reshape(th, tw, 8, 8).transpose(0, 2, 1, 3).reshape(h, w)
    return img_idx, full, assign.reshape(th, tw)


def render(idx, pals, tile_pal):
    """the card as the PPU shows it (H x W x 3)"""
    h, w = idx.shape
    tp = np.repeat(np.repeat(tile_pal, 8, 0), 8, 1)
    return pals[tp, idx]


def error(a, b):
    """(share of the pixels exactly right, mean colour distance in RGB units 0..441)"""
    d = np.sqrt(((a.astype(np.float64) - b.astype(np.float64)) ** 2).sum(-1))
    return float((d == 0).mean()), float(d.mean())

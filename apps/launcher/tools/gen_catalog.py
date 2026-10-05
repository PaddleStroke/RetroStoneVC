#!/usr/bin/env python3
"""The catalogue of a RetroStone Arcade app: build/bundle/gen/catalog.c (apps/launcher/src/rsb.h, rsb_entry).

    gen_catalog.py --conf apps/android/app.conf --out build/bundle/gen [--cards-png DIR]

From the app's config (FREE_GAMES, HIDDEN_GAMES, the names and ids) and the games folder:
  - every game of games/ (a game.mk and docs/screenshots/title.png; not HIDDEN_GAMES, not _template) gets a
    card: the free games first, in FREE_GAMES order, then the others, LOCKED, by name. A new game in games/
    therefore appears locked in the free app at the next build, with nothing to edit;
  - a card = the title screenshot cut to 96x72 (13 colours + the name's cream and navy, one BG palette), with
    the game's name under it (the house font, 6-px advance); a locked card is dimmed (the launcher adds the lock);
  - a locked game also gets the picture of its "Get the full version" panel: a play screenshot at 160x120, in up
    to 6 palettes chosen tile by tile (SNES style);
  - the touch scheme and the best-score location of each game: apps/launcher/games.cfg.
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import argparse
import os
import re
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "sdk", "tools"))
from gen_font import G as FONT  # noqa: E402  (the house font)

CARD_W, THUMB_H, CARD_H = 96, 72, 80
SHOT_W, SHOT_H = 160, 120
CREAM, NAVY = (255, 246, 220), (16, 24, 56)

BUTTONS = {"B": 0x8000, "Y": 0x4000, "SELECT": 0x2000, "START": 0x1000, "UP": 0x0800, "DOWN": 0x0400,
           "LEFT": 0x0200, "RIGHT": 0x0100, "A": 0x0080, "X": 0x0040, "L": 0x0020, "R": 0x0010}
ZONES = {"all": (0, 0, 100, 100), "left": (0, 0, 50, 100), "right": (50, 0, 100, 100),
         "top": (0, 0, 100, 50), "bottom": (0, 50, 100, 100)}


def read_conf(path):
    conf = {}
    for line in open(path, encoding="utf-8"):
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        k, v = line.split("=", 1)
        conf[k.strip()] = v.strip()
    return conf


def read_games_cfg(path):
    """apps/launcher/games.cfg: '<id|default> key=value ...' (see the file)."""
    cfg = {}
    if not os.path.exists(path):
        return cfg
    for line in open(path, encoding="utf-8"):
        line = line.split("#", 1)[0].split()
        if len(line) < 2:
            continue
        d = cfg.setdefault(line[0], {"zones": []})
        for tok in line[1:]:
            k, _, v = tok.partition("=")
            if k == "touch":
                where, _, btn = v.partition(":")
                rect = ZONES[where] if where in ZONES else tuple(int(n) for n in where.split(","))
                d["zones"].append((rect, buttons(btn)))
            elif k == "pause":
                d["pause"] = buttons(v)
            else:
                d[k] = v
    return cfg


def buttons(s):
    m = 0
    for b in s.upper().split("+"):
        if b not in BUTTONS:
            raise SystemExit(f"games.cfg: unknown button {b}")
        m |= BUTTONS[b]
    return m


def game_name(gid):
    src = open(os.path.join(ROOT, "games", gid, "src", "main.c"), encoding="utf-8").read()
    i = src.find("rs_game_main")
    m = re.search(r'rs_game\s+\w+\s*=\s*\{\s*"([^"]+)"', src[i:], re.S) if i >= 0 else None
    if m:
        return m.group(1)
    m = re.search(r'rs_game\s+\w+\s*=\s*\{\s*(\w+)', src[i:], re.S) if i >= 0 else None
    if m:                                       # a macro: #define GAME_NAME "..." in the game's sources
        sdir = os.path.join(ROOT, "games", gid, "src")
        for f in sorted(os.listdir(sdir)):
            d = re.search(r'#define\s+' + m.group(1) + r'\s+"([^"]+)"', open(os.path.join(sdir, f), encoding="utf-8").read())
            if d:
                return d.group(1)
    mk = open(os.path.join(ROOT, "games", gid, "game.mk"), encoding="utf-8").readline()
    m = re.match(r"#\s*(.+?)\s*\(games/", mk)
    return m.group(1) if m else gid


def list_games(conf):
    free = conf.get("FREE_GAMES", "").split()
    hidden = set(conf.get("HIDDEN_GAMES", "").split())
    found = []
    for gid in sorted(os.listdir(os.path.join(ROOT, "games"))):
        d = os.path.join(ROOT, "games", gid)
        if gid.startswith(("_", ".", "zz")) or gid in ("common",) or gid in hidden:
            continue
        if not (os.path.exists(os.path.join(d, "game.mk")) and os.path.exists(os.path.join(d, "src", "main.c"))
                and os.path.exists(os.path.join(d, "docs", "screenshots", "title.png"))):
            continue
        found.append(gid)
    for gid in free:
        if gid not in found:
            raise SystemExit(f"FREE_GAMES: {gid} is not a game of games/ with docs/screenshots/title.png")
    locked = sorted((g for g in found if g not in free), key=lambda g: game_name(g).upper())
    return [(g, False) for g in free] + [(g, True) for g in locked]


# ---- pictures -------------------------------------------------------------------------------------------------------
def to555(a):
    """uint8 RGB -> the nearest RGB555 colour, back in 8 bits."""
    q = (a.astype(np.int32) * 31 + 127) // 255
    return ((q << 3) | (q >> 2)).astype(np.uint8)


def fit(img, w, h):
    """img cut to the aspect w:h (centred) and resized to w x h."""
    img = img.convert("RGB")
    sw, sh = img.size
    if sw * h > sh * w:
        nw = sh * w // h
        img = img.crop(((sw - nw) // 2, 0, (sw - nw) // 2 + nw, sh))
    elif sw * h < sh * w:
        nh = sw * h // w
        img = img.crop((0, (sh - nh) // 2, sw, (sh - nh) // 2 + nh))
    return img.resize((w, h), Image.LANCZOS)


def median_palette(px, n):
    """n colours for the pixels px (N x 3 uint8), median cut, RGB555."""
    if len(px) == 0:
        return np.zeros((1, 3), np.uint8)
    side = int(np.ceil(np.sqrt(len(px))))
    buf = np.zeros((side * side, 3), np.uint8)
    buf[:len(px)] = px
    buf[len(px):] = px[0]
    im = Image.fromarray(buf.reshape(side, side, 3), "RGB").quantize(colors=n, method=Image.Quantize.MEDIANCUT)
    pal = np.array(im.getpalette()[:3 * n], np.uint8).reshape(-1, 3)
    used = sorted(set(im.getdata()))
    return np.unique(to555(pal[used]), axis=0)


def nearest(px, pal):
    """index of the nearest palette colour for each pixel (weighted RGB distance)."""
    w = np.array([3, 4, 2], np.int32)
    d = (((px[:, None, :].astype(np.int32) - pal[None, :, :].astype(np.int32)) ** 2) * w).sum(-1)
    return d.argmin(1), d.min(1)


def tile_quantize(img, npal, ncol):
    """SNES-style: the picture's 8x8 tiles share npal palettes of ncol colours; each tile picks the palette that
    fits it best. Returns (indices HxW of 0..ncol-1, palettes [npal x ncol x 3], tile_pal [th x tw])."""
    a = to555(np.array(img, np.uint8))
    h, w = a.shape[:2]
    th, tw = h // 8, w // 8
    tiles = a.reshape(th, 8, tw, 8, 3).transpose(0, 2, 1, 3, 4).reshape(th * tw, 64, 3)
    # start: tiles grouped by their mean colour (k-means), a median-cut palette per group
    means = tiles.reshape(-1, 64, 3).mean(1)
    rng = np.random.default_rng(1)
    cent = means[rng.choice(len(means), npal, replace=False)] if len(means) >= npal else means
    assign = np.zeros(len(tiles), int)
    for _ in range(8):
        assign = ((means[:, None, :] - cent[None, :, :]) ** 2).sum(-1).argmin(1)
        for k in range(len(cent)):
            if (assign == k).any():
                cent[k] = means[assign == k].mean(0)
    pals = []
    for it in range(4):
        pals = [median_palette(tiles[assign == k].reshape(-1, 3), ncol) if (assign == k).any()
                else median_palette(tiles.reshape(-1, 3), ncol) for k in range(npal)]
        err = np.stack([nearest(tiles.reshape(-1, 3), p)[1].reshape(len(tiles), 64).sum(1) for p in pals], 1)
        assign = err.argmin(1)
    idx = np.zeros((len(tiles), 64), int)
    for k in range(npal):
        sel = assign == k
        if sel.any():
            idx[sel] = nearest(tiles[sel].reshape(-1, 3), pals[k])[0].reshape(-1, 64)
    full = np.zeros((npal, ncol, 3), np.uint8)
    for k, p in enumerate(pals):
        full[k, :len(p)] = p
    img_idx = idx.reshape(th, tw, 8, 8).transpose(0, 2, 1, 3).reshape(h, w)
    return img_idx, full, assign.reshape(th, tw)


def dim(img, locked):
    if not locked:
        return img
    a = np.array(img, np.float32)
    grey = a @ np.array([0.30, 0.59, 0.11], np.float32)
    a = (a * 0.55 + grey[..., None] * 0.45) * 0.72
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8), "RGB")


def label(name, width):
    """The name in the house font with a 6-px advance: a width x 8 array (0 clear, 1 cream, 2 navy shadow)."""
    s = name.upper()
    while len(s) * 6 - 1 > width and " " in s:      # too long: drop the last word
        s = s.rsplit(" ", 1)[0]
    s = s[:(width + 1) // 6]
    out = np.zeros((8, width), np.uint8)
    x0 = (width - (len(s) * 6 - 1)) // 2
    for i, ch in enumerate(s):
        rows = FONT.get(ch, FONT["?"])
        for y in range(7):
            for x in range(5):
                if rows[y][x] == "#":
                    X = x0 + i * 6 + x
                    out[y, X] = 1
                    if y + 1 < 8 and X + 1 < width and out[y + 1, X + 1] == 0:
                        out[y + 1, X + 1] = 2
    return out


def card(gid, name, locked):
    """The card: 12 x 10 tiles, one palette: 0 clear, 1 cream, 2 navy, 3..15 the thumbnail."""
    src = Image.open(os.path.join(ROOT, "games", gid, "docs", "screenshots", "title.png"))
    thumb = dim(fit(src, CARD_W, THUMB_H), locked)
    idx, pals, _ = tile_quantize(thumb, 1, 13)
    pix = np.zeros((CARD_H, CARD_W), np.uint8)
    pix[:THUMB_H] = idx + 3
    pix[THUMB_H:] = label(name, CARD_W)
    pal = [(0, 0, 0), CREAM, NAVY] + [tuple(int(c) for c in p) for p in pals[0]]
    return pix, [pal], np.zeros((CARD_H // 8, CARD_W // 8), int)


def pick_shot(gid, wanted=None):
    d = os.path.join(ROOT, "games", gid, "docs", "screenshots")
    if wanted and os.path.exists(os.path.join(d, wanted)):
        return os.path.join(d, wanted)
    names = sorted(f for f in os.listdir(d) if f.endswith(".png"))
    skip = ("title", "gameover", "game-over", "pause", "results", "players", "over", "versus")
    good = [f for f in names if not any(s in f for s in skip)]
    return os.path.join(d, (good or ["title.png"])[0])


def shot(gid, wanted=None):
    src = Image.open(pick_shot(gid, wanted))
    img = fit(src, SHOT_W, SHOT_H)
    idx, pals, tp = tile_quantize(img, 6, 15)
    pix = (idx + 1).astype(np.uint8)
    out = []
    for p in pals:
        out.append([(0, 0, 0)] + [tuple(int(c) for c in col) for col in p])
    return pix, out, tp


def pack(pix, tile_pal):
    """pixels (palette indices 0..15) -> packed 4bpp tiles, row-major, as rs_tiles_load() reads them."""
    h, w = pix.shape
    out = bytearray()
    for ty in range(h // 8):
        for tx in range(w // 8):
            for y in range(8):
                row = pix[ty * 8 + y, tx * 8:tx * 8 + 8]
                for x in range(0, 8, 2):
                    out.append((int(row[x]) << 4) | int(row[x + 1]))
    return bytes(out)


def rgb555(c):
    r, g, b = c
    return ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3)


def c_bytes(name, data):
    lines = [f"static const uint8_t {name}[{len(data)}] = {{"]
    for i in range(0, len(data), 24):
        lines.append("    " + ", ".join(f"0x{b:02x}" for b in data[i:i + 24]) + ",")
    lines.append("};")
    return "\n".join(lines)


def c_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def preview(pix, pals, tile_pal, path):
    h, w = pix.shape
    im = np.zeros((h, w, 3), np.uint8)
    for y in range(h):
        for x in range(w):
            v = pix[y, x]
            im[y, x] = pals[tile_pal[y // 8, x // 8]][v] if v else (40, 52, 90)
    Image.fromarray(im, "RGB").resize((w * 2, h * 2), Image.NEAREST).save(path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--conf", required=True)
    ap.add_argument("--games-cfg", default=os.path.join(ROOT, "apps", "launcher", "games.cfg"))
    ap.add_argument("--out", required=True)
    ap.add_argument("--preview", help="write the cards as PNG here (review)")
    a = ap.parse_args()
    conf = read_conf(a.conf)
    gcfg = read_games_cfg(a.games_cfg)
    games = list_games(conf)
    os.makedirs(a.out, exist_ok=True)
    if a.preview:
        os.makedirs(a.preview, exist_ok=True)
    c = ["/* Generated by apps/launcher/tools/gen_catalog.py from " + os.path.relpath(a.conf, ROOT) +
         " and games/: do not edit. */", '#include "rsb.h"', ""]
    for gid, locked in games:
        if not locked:
            c.append(f"extern const rsb_module rsb_module_{gid};")
    c.append("")
    entries = []
    for gid, locked in games:
        name = game_name(gid)
        g = dict(gcfg.get("default", {"zones": []}))
        g.update({k: v for k, v in gcfg.get(gid, {}).items() if k != "zones" or v})
        zones = g.get("zones") or [((0, 0, 100, 100), BUTTONS["A"])]
        pause = g.get("pause", BUTTONS["START"])
        pix, pals, tp = card(gid, name, locked)
        c.append(c_bytes(f"card_{gid}_tiles", pack(pix, tp)))
        c.append(f"static const uint16_t card_{gid}_pals[16] = {{" + ", ".join(
            f"0x{rgb555(col):04x}" for col in pals[0]) + "};")
        c.append(f"static const uint8_t card_{gid}_tp[{tp.size}];")
        if a.preview:
            preview(pix, pals, tp, os.path.join(a.preview, f"card-{gid}.png"))
        shot_c = "{0, 0, 0, 0, 0, 0}"
        if locked:
            spix, spals, stp = shot(gid, g.get("shot"))
            c.append(c_bytes(f"shot_{gid}_tiles", pack(spix, stp)))
            c.append(f"static const uint16_t shot_{gid}_pals[{16 * len(spals)}] = {{" + ", ".join(
                f"0x{rgb555(col):04x}" for p in spals for col in p) + "};")
            c.append(c_bytes(f"shot_{gid}_tp", bytes(int(v) for v in stp.flatten())))
            shot_c = (f"{{{SHOT_W // 8}, {SHOT_H // 8}, shot_{gid}_tiles, {len(spals)}, shot_{gid}_pals, "
                      f"shot_{gid}_tp}}")
            if a.preview:
                preview(spix, spals, stp, os.path.join(a.preview, f"shot-{gid}.png"))
        zc = ", ".join(f"{{{r[0]}, {r[1]}, {r[2]}, {r[3]}, 0x{b:04x}}}" for r, b in zones[:6])
        best = int(g.get("best", "8"))
        magic = g.get("magic")
        entries.append(
            f"    {{{c_str(gid)}, {c_str(name)}, {'NULL' if locked else '&rsb_module_' + gid},\n"
            f"     {{{CARD_W // 8}, {CARD_H // 8}, card_{gid}_tiles, 1, card_{gid}_pals, card_{gid}_tp}},\n"
            f"     {shot_c},\n"
            f"     {{{len(zones[:6])}, {{{zc}}}, 0x{pause:04x}}}, {best}, "
            f"{c_str(magic) if magic else 'NULL'}}},")
        print(f"  {'locked' if locked else 'free  '} {gid:16s} {name}")
    c.append("")
    c.append("const rsb_entry rsb_games[] = {")
    c.extend(entries)
    c.append("};")
    c.append(f"const int rsb_ngames = {len(games)};")
    for k, var in (("APP_NAME", "rsb_app_name"), ("APP_ID", "rsb_app_id"), ("APP_VERSION_NAME", "rsb_app_version"),
                   ("PAID_APP_NAME", "rsb_paid_name"), ("PAID_APP_ID", "rsb_paid_id"),
                   ("PAID_STORE_URL", "rsb_paid_url")):
        c.append(f"const char {var}[] = {c_str(conf.get(k, ''))};")
    path = os.path.join(a.out, "catalog.c")
    text = "\n".join(c) + "\n"
    if not os.path.exists(path) or open(path, encoding="utf-8").read() != text:
        with open(path, "w", encoding="utf-8") as f:
            f.write(text)
    print(f"{path}: {len(games)} games, {sum(1 for _, l in games if l)} locked")


if __name__ == "__main__":
    main()

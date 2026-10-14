#!/usr/bin/env python3
"""The catalogue of a RetroStone Arcade app: build/bundle/gen/catalog.c (apps/launcher/src/rsb.h, rsb_entry).

    gen_catalog.py --conf apps/android/app.conf --out build/bundle/gen [--cards-png DIR]

    gen_catalog.py --conf apps/android/app-full.conf --list      the playable game ids (bundle.mk's module list)

From the app's config (FREE_GAMES, HIDDEN_GAMES, the names and ids) and the games folder:
  - every game of games/ (a game.mk and docs/screenshots/title.png; not HIDDEN_GAMES, not _template) gets a
    card: the free games first, in FREE_GAMES order, then the others, LOCKED, by name. A new game in games/
    therefore appears locked in the free app at the next build, with nothing to edit. FREE_GAMES=all (the full
    app): every game is playable, by name;
  - a card = a moment of play cut out of the game's screen at 1x, 96x72 pixels never scaled (the table
    apps/launcher/cards/cards.cfg, tools/cards.py), in up to 6 palettes chosen tile by tile (SNES style; the
    launcher gives each card column its own), with the game's name under it (the house font, 6-px advance, the UI
    palette); a locked card keeps its colours, with a gold ribbon and a padlock badge drawn on it;
  - a locked game also gets the picture of its "Get the full version" panel: a larger cut of the same screen at 1x,
    176x88 pixels never scaled (around the card, or cards.cfg panel=<x>,<y>), each row of tiles in its own 6 palettes
    (the launcher loads them line by line), each tile one of them: the game's colours, exactly;
  - the touch scheme and the best-score location of each game: apps/launcher/games.cfg;
  - IAP_PRODUCT (the iOS app: one free app, an in-app purchase opens the rest, apps/common/iap.h): the locked games
    are built too (modules; --list names them), each with its open card and its locked one (card_locked); the
    launcher shows one or the other as the purchase says.
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
import cards  # noqa: E402  (the cards: the table, the 1x crops, the palettes)

CARD_W, THUMB_H, CARD_H = 96, 72, 80
CREAM, NAVY = (255, 246, 220), (16, 24, 56)
UI_PAL = 255                                    # a tile in the launcher's UI palette (the name under a card)

BUTTONS = {"B": 0x8000, "Y": 0x4000, "SELECT": 0x2000, "START": 0x1000, "UP": 0x0800, "DOWN": 0x0400,
           "LEFT": 0x0200, "RIGHT": 0x0100, "A": 0x0080, "X": 0x0040, "L": 0x0020, "R": 0x0010}
ZONES = {"all": (0, 0, 100, 100), "left": (0, 0, 50, 100), "right": (50, 0, 100, 100),
         "top": (0, 0, 100, 50), "bottom": (0, 50, 100, 100)}
# rsb_zone.shown: 0 not drawn, 1 drawn (an arrow for a direction, else the button's letter), 2..5 an arrow icon,
# 6 a shot: a bow arrow (shaft, head, feathers) flying up-right, so it reads as an attack, not as "go up"
ICONS = {"": 1, "up": 2, "down": 3, "left": 4, "right": 5, "shot": 6}


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
            if k in ("touch", "button"):           # button=: the same, drawn on the screen by the frontend
                where, _, btn = v.partition(":")
                btn, _, icon = btn.partition("@")  # button=<zone>:<BUTTONS>@up|down|left|right: an arrow icon
                rect = ZONES[where] if where in ZONES else tuple(int(n) for n in where.split(","))
                shown = ICONS[icon] if k == "button" else 0
                d["zones"].append((rect, buttons(btn), shown))
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
    every = free == ["all"]
    for gid in sorted(os.listdir(os.path.join(ROOT, "games"))):
        d = os.path.join(ROOT, "games", gid)
        if gid.startswith(("_", ".", "zz")) or gid in ("common",) or gid in hidden:
            continue
        if not (os.path.exists(os.path.join(d, "game.mk")) and os.path.exists(os.path.join(d, "src", "main.c"))
                and os.path.exists(os.path.join(d, "docs", "screenshots", "title.png"))):
            continue
        found.append(gid)
    if every:
        free = sorted(found, key=lambda g: game_name(g).upper())
    for gid in free:
        if gid not in found:
            raise SystemExit(f"FREE_GAMES: {gid} is not a game of games/ with docs/screenshots/title.png")
    locked = sorted((g for g in found if g not in free), key=lambda g: game_name(g).upper())
    return [(g, False) for g in free] + [(g, True) for g in locked]


# ---- pictures -------------------------------------------------------------------------------------------------------
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


def card(gid, name, locked, table):
    """The card: 12 x 10 tiles. The picture (9 rows of tiles): its pixels at 1x in up to cards.CARD_PALS palettes, one
    per tile; the name (the last row): the UI palette's cream (1) and navy (2), tile palette UI_PAL. Returns (pixels,
    palettes, tile palettes, the picture as the game shows it, as the card shows it)."""
    src = cards.source(gid, table)
    if locked:
        src = cards.locked_look(src)
    idx, pals, tp = cards.tile_palettes(src)
    npal = int(tp.max()) + 1
    pix = np.zeros((CARD_H, CARD_W), np.uint8)
    pix[:THUMB_H] = idx
    pix[THUMB_H:] = label(name, CARD_W)
    tile_pal = np.full((CARD_H // 8, CARD_W // 8), UI_PAL, np.uint8)
    tile_pal[:THUMB_H // 8] = tp
    out = [[(0, 0, 0)] + [tuple(int(c) for c in col) for col in pals[k, 1:]] for k in range(npal)]
    return pix, out, tile_pal, src, cards.render(idx, pals, tp)


def shot(gid, table):
    """The panel's picture: (pixels, palettes [rows x 6], tile palettes within the row's set, picture, as shown)."""
    src = cards.panel_source(gid, table)
    idx, pals, tp = cards.band_palettes(src)
    out = [[(0, 0, 0)] + [tuple(int(c) for c in col) for col in p[1:]] for p in pals]
    return idx.astype(np.uint8), out, tp, src, cards.band_render(idx, pals, tp)


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
            v, p = pix[y, x], tile_pal[y // 8, x // 8]
            im[y, x] = (40, 52, 90) if not v else (CREAM, NAVY)[v - 1] if p == UI_PAL else pals[p][v]
    Image.fromarray(im, "RGB").resize((w * 2, h * 2), Image.NEAREST).save(path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--conf", required=True)
    ap.add_argument("--games-cfg", default=os.path.join(ROOT, "apps", "launcher", "games.cfg"))
    ap.add_argument("--out")
    ap.add_argument("--list", action="store_true", help="print the playable game ids and stop")
    ap.add_argument("--preview", help="write the cards as PNG here (review)")
    a = ap.parse_args()
    conf = read_conf(a.conf)
    gcfg = read_games_cfg(a.games_cfg)
    table = cards.read_table()
    games = list_games(conf)
    iap = bool(conf.get("IAP_PRODUCT"))         # the locked games are modules too, opened by the purchase
    if a.list:
        print(" ".join(g for g, locked in games if iap or not locked))
        return
    if not a.out:
        raise SystemExit("--out is needed")
    os.makedirs(a.out, exist_ok=True)
    if a.preview:
        os.makedirs(a.preview, exist_ok=True)
    c = ["/* Generated by apps/launcher/tools/gen_catalog.py from " + os.path.relpath(a.conf, ROOT) +
         " and games/: do not edit. */", '#include "rsb.h"', ""]
    for gid, locked in games:
        if iap or not locked:
            c.append(f"extern const rsb_module rsb_module_{gid};")
    c.append("")
    entries = []
    for gid, locked in games:
        name = game_name(gid)
        g = dict(gcfg.get("default", {"zones": []}))
        g.update({k: v for k, v in gcfg.get(gid, {}).items() if k != "zones" or v})
        zones = g.get("zones") or [((0, 0, 100, 100), BUTTONS["A"], False)]
        if len(zones) > 8:
            raise SystemExit(f"games.cfg: {gid}: at most 8 zones")
        pause = g.get("pause", BUTTONS["START"])
        pix, pals, tp, want, got = card(gid, name, locked and not iap, table)
        exact, err = cards.error(got, want)
        c.append(c_bytes(f"card_{gid}_tiles", pack(pix, tp)))
        c.append(f"static const uint16_t card_{gid}_pals[{16 * len(pals)}] = {{" + ", ".join(
            f"0x{rgb555(col):04x}" for p in pals for col in p) + "};")
        c.append(c_bytes(f"card_{gid}_tp", bytes(int(v) for v in tp.flatten())))
        locked_c = "{0, 0, 0, 0, 0, 0, 0}"
        if locked and iap:                      # its locked look, until the purchase
            lpix, lpals, ltp, _, _ = card(gid, name, True, table)
            c.append(c_bytes(f"cardlk_{gid}_tiles", pack(lpix, ltp)))
            c.append(f"static const uint16_t cardlk_{gid}_pals[{16 * len(lpals)}] = {{" + ", ".join(
                f"0x{rgb555(col):04x}" for p in lpals for col in p) + "};")
            c.append(c_bytes(f"cardlk_{gid}_tp", bytes(int(v) for v in ltp.flatten())))
            locked_c = (f"{{{CARD_W // 8}, {CARD_H // 8}, cardlk_{gid}_tiles, {len(lpals)}, cardlk_{gid}_pals, "
                        f"cardlk_{gid}_tp, 0}}")
        if a.preview:
            preview(pix, pals, tp, os.path.join(a.preview, f"card-{gid}.png"))
        shot_c = "{0, 0, 0, 0, 0, 0, 0}"
        panel = ""
        if locked:
            spix, spals, stp, swant, sgot = shot(gid, table)
            pexact, _ = cards.error(sgot, swant)
            panel = f"; panel {pexact * 100:5.1f}% exact"
            c.append(c_bytes(f"shot_{gid}_tiles", pack(spix, stp)))
            c.append(f"static const uint16_t shot_{gid}_pals[{16 * len(spals)}] = {{" + ", ".join(
                f"0x{rgb555(col):04x}" for p in spals for col in p) + "};")
            c.append(c_bytes(f"shot_{gid}_tp", bytes(int(v) for v in stp.flatten())))
            shot_c = (f"{{{cards.PANEL_W // 8}, {cards.PANEL_H // 8}, shot_{gid}_tiles, {len(spals)}, shot_{gid}_pals, "
                      f"shot_{gid}_tp, 1}}")
            if a.preview:
                preview(spix, spals, stp + (np.arange(stp.shape[0])[:, None] * cards.CARD_PALS),
                        os.path.join(a.preview, f"shot-{gid}.png"))
        zc = ", ".join(f"{{{r[0]}, {r[1]}, {r[2]}, {r[3]}, 0x{b:04x}, {int(shown)}}}" for r, b, shown in zones)
        best = int(g.get("best", "8"))
        magic = g.get("magic")
        entries.append(
            f"    {{{c_str(gid)}, {c_str(name)}, {'NULL' if locked and not iap else '&rsb_module_' + gid},\n"
            f"     {{{CARD_W // 8}, {CARD_H // 8}, card_{gid}_tiles, {len(pals)}, card_{gid}_pals, card_{gid}_tp, 0}},\n"
            f"     {shot_c},\n"
            f"     {{{len(zones)}, {{{zc}}}, 0x{pause:04x}}}, {best}, "
            f"{c_str(magic) if magic else 'NULL'},\n"
            f"     {locked_c}}},")
        print(f"  {'locked' if locked else 'free  '} {gid:16s} {name:18s} card: {len(pals)} palettes, "
              f"{exact * 100:5.1f}% of the pixels exact (mean error {err:.2f}){panel}"
              f"{'' if gid in table else ' [no line in cards.cfg: the title screen]'}")
    c.append("")
    c.append("const rsb_entry rsb_games[] = {")
    c.extend(entries)
    c.append("};")
    c.append(f"const int rsb_ngames = {len(games)};")
    for k, var in (("APP_NAME", "rsb_app_name"), ("APP_ID", "rsb_app_id"), ("APP_VERSION_NAME", "rsb_app_version"),
                   ("PAID_APP_NAME", "rsb_paid_name"), ("PAID_APP_ID", "rsb_paid_id"),
                   ("PAID_STORE_URL", "rsb_paid_url"), ("IAP_PRODUCT", "rsb_iap_product")):
        c.append(f"const char {var}[] = {c_str(conf.get(k, ''))};")
    path = os.path.join(a.out, "catalog.c")
    text = "\n".join(c) + "\n"
    if not os.path.exists(path) or open(path, encoding="utf-8").read() != text:
        with open(path, "w", encoding="utf-8") as f:
            f.write(text)
    print(f"{path}: {len(games)} games, {sum(1 for _, l in games if l)} locked")


if __name__ == "__main__":
    main()

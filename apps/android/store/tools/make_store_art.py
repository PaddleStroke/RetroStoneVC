#!/usr/bin/env python3
"""The store graphics of both apps, code-drawn in the house style (docs/art-direction.md), like the games:

    python3 apps/android/store/tools/make_store_art.py

  apps/android/store/<free|full>/graphics/icon-512.png          the 512 x 512 icon (apps/android/tools/make_icon.py)
  apps/android/store/<free|full>/graphics/feature-1024x500.png  the feature graphic: the launcher's backdrop (navy,
      the house 2-line dither), the app's name in the house 5x7 font (gold, outlined, with a shadow) and the games'
      launcher cards (their moments of play, at 2x: crisp) in gold frames; the free app's locked game with the
      launcher's ribbon and padlock badge.
No text but the app's name (a brand, the same in every language).
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
STORE = os.path.dirname(HERE)
ROOT = os.path.abspath(os.path.join(STORE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "sdk", "tools"))
sys.path.insert(0, os.path.join(ROOT, "apps", "android", "tools"))
sys.path.insert(0, os.path.join(ROOT, "apps", "launcher", "tools"))
from gen_font import G as FONT  # noqa: E402
import make_icon  # noqa: E402
import gen_catalog as gc  # noqa: E402
import cards  # noqa: E402  (the launcher cards: apps/launcher/tools/cards.py)

OUT, NAVY = (22, 18, 40), (16, 24, 56)
GOLD_L, GOLD, GOLD_D = (255, 240, 150), (252, 216, 72), (186, 128, 22)
CREAM, RED = (255, 246, 220), (232, 88, 76)


def backdrop(w, h):
    """the launcher's raster gradient, with the house dither (every other 4-line band 3 steps lighter)"""
    a = np.zeros((h, w, 3), np.uint8)
    for y in range(h):
        k = y * 256 // h
        d = 3 if (y // 2) & 4 else 0
        a[y] = (28 - k * 14 // 256 + d, 44 - k * 22 // 256 + d, 90 - k * 44 // 256 + d)
    return Image.fromarray(a, "RGB")


def text_mask(s, scale):
    """the house font, 6-px advance, as a boolean array scaled by scale"""
    cols = len(s) * 6 - 1
    m = np.zeros((7, cols), bool)
    for i, ch in enumerate(s.upper()):
        rows = FONT.get(ch, FONT["?"])
        for y in range(7):
            for x in range(5):
                if rows[y][x] == "#":
                    m[y, i * 6 + x] = True
    return np.kron(m, np.ones((scale, scale), bool))


def big_text(img, s, cx, y, scale, fill=GOLD, top=GOLD_L, shade=GOLD_D):
    """centred at cx: an outline (dark), a drop shadow, the letters lit from the top (the kit's banner letters)"""
    m = text_mask(s, scale)
    h, w = m.shape
    x0 = cx - w // 2
    a = np.array(img)
    o = max(2, scale // 3)
    pad = np.zeros((h + 4 * o, w + 4 * o), bool)
    pad[2 * o:2 * o + h, 2 * o:2 * o + w] = m
    grown = pad.copy()
    for dy in range(-o, o + 1):
        for dx in range(-o, o + 1):
            grown |= np.roll(np.roll(pad, dy, 0), dx, 1)
    sx, sy = x0 - 2 * o, y - 2 * o
    reg = a[sy + o:sy + o + pad.shape[0], sx + o:sx + o + pad.shape[1]]          # the shadow, offset down-right
    reg[grown[:reg.shape[0], :reg.shape[1]]] = (10, 12, 28)
    reg = a[sy:sy + pad.shape[0], sx:sx + pad.shape[1]]
    reg[grown[:reg.shape[0], :reg.shape[1]]] = OUT
    band = np.zeros_like(pad, dtype=np.uint8)
    band[2 * o:2 * o + h] = np.repeat(np.arange(7), scale)[:, None]
    for row, col in ((0, top), (1, top), (2, fill), (3, fill), (4, fill), (5, shade), (6, shade)):
        reg[pad & (band == row)] = col
    return Image.fromarray(a)


TABLE = cards.read_table()


def card(gid, scale, locked=False):
    """a game's launcher card (its moment of play, 96 x 72 at 1x: apps/launcher/cards), pixel for pixel, scaled by an
    integer (never blurred); a locked one with the launcher's ribbon and padlock badge"""
    a = cards.source(gid, TABLE)
    if locked:
        a = cards.locked_look(a)
    im = Image.fromarray(a.astype(np.uint8), "RGB")
    return im.resize((im.width * scale, im.height * scale), Image.NEAREST)


def framed(img, im, x, y, locked=False):
    """the launcher's selection frame around a card: outline, light gold, gold, dark gold; the padlock if locked"""
    d = ImageDraw.Draw(img)
    w, h = im.size
    for i, col in enumerate((OUT, GOLD_L, GOLD, GOLD_D, OUT)):
        r = 6 - i
        d.rounded_rectangle((x - r - 1, y - r - 1, x + w + r, y + h + r), radius=max(1, r + 2), fill=col)
    img.paste(im, (x, y))
    if locked:
        lock = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
        px = lock.load()
        cols = {"o": OUT, "S": (222, 230, 238), "T": (128, 138, 156), "H": GOLD_L, "G": GOLD, "D": GOLD_D}
        rows = ["................", ".....oooooo.....", "....oSSSSSSo....", "...oSSooooSSo...",
                "...oSo....oTo...", "...oSo....oTo...", "...oSo....oTo...", ".oooooooooooooo.",
                ".oHHHHHHHHHHHGo.", ".oHGGGGGGGGGGDo.", ".oHGGGGooGGGGDo.", ".oHGGGGooGGGGDo.",
                ".oHGGGGGoGGGGDo.", ".oGDDDDDDDDDDDo.", ".oooooooooooooo.", "................"]
        for yy, r in enumerate(rows):
            for xx, ch in enumerate(r):
                if ch in cols:
                    px[xx, yy] = cols[ch] + (255,)
        k = max(2, w // 48)
        big = lock.resize((16 * k, 16 * k), Image.NEAREST)
        img.paste(big, (x + (w - 16 * k) // 2, y + (h - 16 * k) // 2), big)


def pill(img, s, cx, y, scale):
    """a small red pill with cream letters (the free app's FREE)"""
    m = text_mask(s, scale)
    h, w = m.shape
    d = ImageDraw.Draw(img)
    x0 = cx - w // 2
    d.rounded_rectangle((x0 - 3 * scale - 2, y - 2 * scale - 2, x0 + w + 3 * scale + 1, y + h + 2 * scale + 1),
                        radius=h // 2 + 2 * scale, fill=OUT)
    d.rounded_rectangle((x0 - 3 * scale, y - 2 * scale, x0 + w + 3 * scale - 1, y + h + 2 * scale - 1),
                        radius=h // 2 + 2 * scale, fill=RED)
    a = np.array(img)
    a[y:y + h, x0:x0 + w][m] = CREAM
    return Image.fromarray(a)


def feature(app, games, locked_games):
    W, H = 1024, 500
    img = backdrop(W, H)
    if app == "free":
        img = big_text(img, "RETROSTONE ARCADE", W // 2, 50, 8)
        img = pill(img, "FREE", W // 2, 136, 5)
        show = [(g, False) for g in games[:3]] + [(g, True) for g in locked_games[:1]]
        cw, gap = 192, 24                         # the cards at 2x
        x0 = (W - (len(show) * cw + (len(show) - 1) * gap)) // 2
        for i, (g, lk) in enumerate(show):
            framed(img, card(g, 2, lk), x0 + i * (cw + gap), 256)
    else:
        img = big_text(img, "RETROSTONE ARCADE", W // 2, 34, 8)
        cw, ch, gap, per = 192, 144, 18, 6        # two staggered rows of cards at 2x, cut by the edges: "many games"
        for r in range(2):
            row = games[r * per:(r + 1) * per]
            x0 = (W - (len(row) * cw + (len(row) - 1) * gap)) // 2 + (-52, 52)[r]
            for i, g in enumerate(row):
                framed(img, card(g, 2), x0 + i * (cw + gap), 146 + r * (ch + 20))
    return img.convert("RGB")


def main():
    for app, conf in (("free", "app.conf"), ("full", "app-full.conf")):
        path = os.path.join(ROOT, "apps", "android", conf)
        lst = gc.list_games(gc.read_conf(path))
        playable = [g for g, lk in lst if not lk]
        locked = [g for g, lk in lst if lk]
        d = os.path.join(STORE, app, "graphics")
        os.makedirs(d, exist_ok=True)
        icon_games, full = make_icon.conf_games(path)
        make_icon.store_icon(icon_games, full).save(os.path.join(d, "icon-512.png"))
        if app == "full":                       # a varied first row: the icon's games first, then the others
            first = [g for g in icon_games if g in playable]
            playable = first + [g for g in playable if g not in first]
        feature(app, playable, locked).save(os.path.join(d, "feature-1024x500.png"))
        print(f"{d}: icon-512.png, feature-1024x500.png")


if __name__ == "__main__":
    main()

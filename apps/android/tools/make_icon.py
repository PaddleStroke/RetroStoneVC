#!/usr/bin/env python3
"""The apps' icons, code-drawn in the house style (docs/art-direction.md): a smooth gold frame (drawn at the icon's
own resolution, never a blown-up small picture) around a tile holding four heroes of the games, 2 x 2, each a 40 x 40
close-up of its launcher card (apps/launcher/cards: the game's pixels) at exactly 5x, so they stay sharp. The free
app: a navy tile; the full app (PROFILE=full): a ruby tile. The heroes: the config's ICON_GAMES (4 game ids).

    make_icon.py --conf apps/android/app.conf --res build/android/free/res [--store dist/android/X-icon-512.png]

Writes the legacy icons (mipmap-*/ic_launcher.png, 48..192 px), the adaptive icon (Android 8+: the tile's colour as
the background and the foreground in mipmap-*/ic_launcher_foreground.png, mipmap-anydpi-v26/ic_launcher.xml) and,
with --store, the 512x512 picture Google Play asks for (store_icon(), also used by make_store_art.py).
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import argparse
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "apps", "launcher", "tools"))
import cards  # noqa: E402  (the launcher cards: the game's pixels at 1x)

NAVY, NAVY_D, FILL = (16, 24, 56), (12, 20, 44), (42, 68, 118)
RUBY, RUBY_FILL = (72, 14, 36), (128, 34, 58)
GOLD_L, GOLD, GOLD_D, OUT = (255, 240, 150), (252, 216, 72), (186, 128, 22), (22, 18, 40)

# each hero's close-up: the top-left corner of a HERO x HERO square in its card (96 x 72, cards.source())
HERO, ZOOM = 40, 5
HEROES = {"beaverrush": (55, 32), "crabcoconut": (8, 32), "duckparade": (20, 22), "frogreach": (13, 2),
          "hedgehoghills": (21, 32), "leadysquid": (25, 8), "molemarbles": (30, 30), "otterrapids": (6, 32),
          "pancaketower": (0, 24), "penguinslalom": (53, 4), "radishrascals": (30, 4), "spiderswing": (33, 32)}

TILE = 480                                  # the framed tile (the store icon: 16 px of the tile's colour around it)
SS = 4                                      # the frame is drawn SS times larger, then smoothed down
CELL, GAP, INSET = HERO * ZOOM, 8, 36       # the heroes' squares: 200 px, 8 px apart, from (36, 36)


def read_conf(path):
    conf = {}
    for line in open(path, encoding="utf-8"):
        if "=" in line and not line.lstrip().startswith("#"):
            k, v = line.split("=", 1)
            conf[k.strip()] = v.strip()
    return conf


def conf_games(path):
    """(the icon's 4 heroes, full app?)"""
    conf = read_conf(path)
    full = conf.get("PROFILE") == "full"
    games = conf.get("ICON_GAMES", "").split()
    first = conf.get("FREE_GAMES", "").split() if conf.get("FREE_GAMES") != "all" else []
    games += [g for g in first + sorted(HEROES) if g not in games]
    return [g for g in games if g in HEROES][:4], full


def _rounded(size, box, radius, fill, img=None):
    img = img or Image.new("RGBA", (size * SS, size * SS), (0, 0, 0, 0))
    ImageDraw.Draw(img).rounded_rectangle(tuple(v * SS for v in box), radius * SS, fill=fill)
    return img


def tile_icon(games, full=False):
    """TILE x TILE, RGBA (clear outside the rounded frame): the gold frame, the tile, the 4 heroes' close-ups"""
    tile = RUBY if full else NAVY
    big = None
    # the frame, outside in: an outline, the gold lit from the top left (light) and shaded at the bottom right (dark)
    for box, r, col in (((0, 0, 479, 479), 60, OUT), ((5, 5, 474, 474), 56, GOLD_D), ((5, 5, 468, 468), 56, GOLD),
                        ((10, 10, 463, 463), 52, GOLD_L), ((15, 15, 463, 463), 48, GOLD),
                        ((22, 22, 457, 457), 42, OUT), ((26, 26, 453, 453), 38, tile)):
        big = _rounded(TILE, box, r, col + (255,), big)
    for i in range(len(games)):             # each hero's dark outline
        x, y = INSET + (i % 2) * (CELL + GAP), INSET + (i // 2) * (CELL + GAP)
        _rounded(TILE, (x - 3, y - 3, x + CELL + 2, y + CELL + 2), 16, OUT + (255,), big)
    im = big.resize((TILE, TILE), Image.LANCZOS)
    table = cards.read_table()
    for i, g in enumerate(games):
        x, y = INSET + (i % 2) * (CELL + GAP), INSET + (i // 2) * (CELL + GAP)
        cx, cy = HEROES[g]
        a = Image.fromarray(cards.source(g, table).astype(np.uint8), "RGB")
        hero = a.crop((cx, cy, cx + HERO, cy + HERO)).resize((CELL, CELL), Image.NEAREST)
        mask = _rounded(CELL, (0, 0, CELL - 1, CELL - 1), 13, 255, Image.new("L", (CELL * SS, CELL * SS), 0))
        im.paste(hero, (x, y), mask.resize((CELL, CELL), Image.LANCZOS))
    return im


def store_icon(games, full=False):
    """the 512 x 512 store icon: the tile on its colour (Google Play rounds the corners itself)"""
    bg = RUBY if full else NAVY
    store = Image.new("RGBA", (512, 512), bg + (255,))
    t = tile_icon(games, full)
    store.alpha_composite(t, (16, 16))
    return store.convert("RGB")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--conf", required=True)
    ap.add_argument("--res", required=True)
    ap.add_argument("--store")
    a = ap.parse_args()
    games, full = conf_games(a.conf)
    icon = tile_icon(games, full)
    bg = RUBY if full else NAVY
    dens = {"mdpi": 1, "hdpi": 1.5, "xhdpi": 2, "xxhdpi": 3, "xxxhdpi": 4}
    for name, k in dens.items():
        d = os.path.join(a.res, f"mipmap-{name}")
        os.makedirs(d, exist_ok=True)
        icon.resize((int(48 * k), int(48 * k)), Image.LANCZOS).save(os.path.join(d, "ic_launcher.png"))
        # adaptive foreground: 108 dp, the icon in the 66-dp safe zone (centred)
        fg = Image.new("RGBA", (int(108 * k), int(108 * k)), (0, 0, 0, 0))
        s = int(66 * k)
        fg.alpha_composite(icon.resize((s, s), Image.LANCZOS), ((fg.width - s) // 2, (fg.height - s) // 2))
        fg.save(os.path.join(d, "ic_launcher_foreground.png"))
    d = os.path.join(a.res, "mipmap-anydpi-v26")
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, "ic_launcher.xml"), "w") as f:
        f.write('<?xml version="1.0" encoding="utf-8"?>\n'
                '<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">\n'
                '    <background android:drawable="@color/ic_launcher_background" />\n'
                '    <foreground android:drawable="@mipmap/ic_launcher_foreground" />\n'
                '</adaptive-icon>\n')
    d = os.path.join(a.res, "values")
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, "ic_launcher_background.xml"), "w") as f:
        f.write('<?xml version="1.0" encoding="utf-8"?>\n<resources>\n'
                '    <color name="ic_launcher_background">#%02x%02x%02x</color>\n</resources>\n' % bg)
    if a.store:
        os.makedirs(os.path.dirname(a.store), exist_ok=True)
        store_icon(games, full).save(a.store)


if __name__ == "__main__":
    main()

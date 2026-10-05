#!/usr/bin/env python3
"""The app's icon, code-drawn in the house style (docs/art-direction.md): a gold-framed navy tile holding four
game cards (the free games' title screens first), like the launcher's grid.

    make_icon.py --conf apps/android/app.conf --res build/android/res [--store dist/android/icon-512.png]

Writes the legacy icons (mipmap-*/ic_launcher.png, 48..192 px), the adaptive icon (Android 8+: a navy background
and the foreground in mipmap-*/ic_launcher_foreground.png, mipmap-anydpi-v26/ic_launcher.xml) and, with --store,
the 512x512 picture Google Play asks for.
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import argparse
import os

from PIL import Image, ImageDraw

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
NAVY, NAVY_D, FILL = (16, 24, 56), (12, 20, 44), (42, 68, 118)
GOLD_L, GOLD, GOLD_D, OUT = (255, 240, 150), (252, 216, 72), (186, 128, 22), (22, 18, 40)


def conf_games(path):
    conf = {}
    for line in open(path, encoding="utf-8"):
        if "=" in line and not line.lstrip().startswith("#"):
            k, v = line.split("=", 1)
            conf[k.strip()] = v.strip()
    free = conf.get("FREE_GAMES", "").split()
    hidden = set(conf.get("HIDDEN_GAMES", "").split())
    others = sorted(g for g in os.listdir(os.path.join(ROOT, "games"))
                    if g not in free and g not in hidden and not g.startswith(("_", "zz")) and
                    os.path.exists(os.path.join(ROOT, "games", g, "docs", "screenshots", "title.png")))
    return (free + others)[:4]


def base_icon(games):
    """48 x 48, pixel art: the frame, 4 cards of 18 x 14 (the title screens), the gaps navy."""
    im = Image.new("RGBA", (48, 48), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((0, 0, 47, 47), 7, fill=OUT)
    d.rounded_rectangle((1, 1, 46, 46), 6, fill=GOLD_D)
    d.rounded_rectangle((1, 1, 45, 45), 6, fill=GOLD)
    d.rounded_rectangle((2, 2, 44, 44), 5, fill=GOLD_L)
    d.rounded_rectangle((3, 3, 44, 44), 5, fill=GOLD)
    d.rounded_rectangle((4, 4, 43, 43), 4, fill=OUT)
    d.rounded_rectangle((5, 5, 42, 42), 3, fill=NAVY)
    for i, g in enumerate(games):
        src = Image.open(os.path.join(ROOT, "games", g, "docs", "screenshots", "title.png")).convert("RGB")
        card = src.resize((16, 12), Image.LANCZOS)
        x, y = 7 + (i % 2) * 18, 9 + (i // 2) * 16
        d.rectangle((x - 1, y - 1, x + 16, y + 12), fill=FILL)
        im.paste(card, (x, y))
    return im


def scaled(im, size):
    """nearest to the multiple of 48 at or above size (crisp pixels), then smoothed down when needed"""
    k = -(-size // 48)
    big = im.resize((48 * k, 48 * k), Image.NEAREST)
    return big if 48 * k == size else big.resize((size, size), Image.LANCZOS)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--conf", required=True)
    ap.add_argument("--res", required=True)
    ap.add_argument("--store")
    a = ap.parse_args()
    icon = base_icon(conf_games(a.conf))
    dens = {"mdpi": 1, "hdpi": 1.5, "xhdpi": 2, "xxhdpi": 3, "xxxhdpi": 4}
    for name, k in dens.items():
        d = os.path.join(a.res, f"mipmap-{name}")
        os.makedirs(d, exist_ok=True)
        scaled(icon, int(48 * k)).save(os.path.join(d, "ic_launcher.png"))
        # adaptive foreground: 108 dp, the icon in the 66-dp safe zone (centred)
        fg = Image.new("RGBA", (int(108 * k), int(108 * k)), (0, 0, 0, 0))
        s = int(66 * k)
        fg.paste(scaled(icon, s), ((fg.width - s) // 2, (fg.height - s) // 2))
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
                '    <color name="ic_launcher_background">#101838</color>\n</resources>\n')
    if a.store:
        os.makedirs(os.path.dirname(a.store), exist_ok=True)
        store = Image.new("RGBA", (512, 512), NAVY + (255,))
        store.paste(scaled(icon, 480), (16, 16), scaled(icon, 480))
        store.convert("RGB").save(a.store)


if __name__ == "__main__":
    main()

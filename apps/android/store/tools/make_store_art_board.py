#!/usr/bin/env python3
"""The store graphics of RetroStone Board, RetroStone Board Free and RetroStone (the one free app), from the app's own
pictures (the Arcade's: make_store_art.py):

    python3 apps/android/store/tools/make_store_art_board.py [--build build/boardgames-store] [board boardfree retrostone]

  apps/android/store/<board|boardfree>/graphics/icon-512.png          the 512 x 512 icon: make_icon.py's store_icon()
      with ICON_STYLE=board (the gold frame round the green table, the white knight, a die and a card), the launcher
      icon's own picture. Both apps have the same launcher icon today (app-board.conf and app-boardfree.conf); the
      Arcade's tell theirs apart by the tile's colour (CHECKLIST-board.md: the owner's choice).
  apps/android/store/<board|boardfree>/graphics/feature-1024x500.png  the feature graphic: the app's green table in a
      walnut frame with a gold fillet, the app's name in gold (Noto Sans Bold, the app's own font), and some games'
      menu pictures (each game's thumb(), drawn by the app's code: board_thumbs.c) laid on the table with their
      shadows, the knight and the die in front; the free app: its free games only, and a FREE pill (the Arcade's).
  apps/android/store/retrostone/graphics/...  RetroStone's: the icon with ICON_STYLE=retrostone (make_icon.py's
      retrostone_icon: an Arcade hero on a little screen in the die's place); the feature graphic: "RetroStone" alone,
      the board games' pictures and two Arcade heroes on their little screens (make_icon.arcade_screen).
No text but the app's name (a brand, the same in every language).

It builds what it needs in its own folder (--build, default build/boardgames-store): the app's icon pictures (make
icon-art) and board_thumbs (the games' pictures), then draws. Needs WSL/Linux: gcc, SDL2, python3 with Pillow and
numpy, and the Noto CJK fonts (as apps/boardgames/tools/gen_text.py).
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import argparse
import glob
import os
import subprocess
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
STORE = os.path.dirname(HERE)
ROOT = os.path.abspath(os.path.join(STORE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "apps", "android", "tools"))
import make_icon  # noqa: E402  (the icons; its gold, the table's green, the pictures laid with their shadows)

BG = "apps/boardgames"
FONT = "/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc"
WALNUT, WALNUT_D, WALNUT_L = (92, 60, 40), (58, 36, 22), (122, 82, 54)   # the boards' frames (the app's walnut)
RED, RED_D, IVORY = (178, 44, 38), (112, 22, 20), (246, 238, 220)       # art_player 2 (red lacquer) and 0 (ivory)
GOLD_L, GOLD, GOLD_D, OUT = make_icon.GOLD_L, make_icon.GOLD, make_icon.GOLD_D, make_icon.OUT

# the games on each app's table (the free app: free games only): (id, centre x, centre y, size, turn in degrees)
LAYOUT = {
    "board": [("tarot", 872, 150, 220, 9), ("chess", 690, 170, 220, -6), ("backgammon", 600, 372, 200, -8),
              ("sudoku", 905, 360, 185, 7), ("mahjong", 768, 368, 200, 2)],
    "boardfree": [("hearts", 872, 158, 215, 9), ("chess", 690, 170, 220, -6), ("backgammon", 600, 372, 200, -8),
                  ("sudoku", 905, 360, 185, 7), ("klondike", 768, 372, 195, 2)],
    "retrostone": [("chess", 700, 148, 190, -6), ("mahjong", 893, 150, 180, 7), ("sudoku", 900, 380, 165, 6)],
}
# the knight and the die in front (icon-art's pictures): (name, centre x, centre y, size, turn)
PIECES = {"board": [("knight", 478, 372, 170, 0), ("die", 676, 440, 66, 18)],
          "retrostone": [("knight", 250, 370, 170, 0), ("die", 380, 440, 64, 18)]}
# RetroStone's Arcade heroes on their little screens (make_icon.arcade_screen), laid among the games: (hero, centre x,
# centre y, zoom, turn)
ARCADE_LAYOUT = {"retrostone": [("frogreach", 520, 372, 3, -7), ("crabcoconut", 712, 372, 4, 5)]}
THUMB = 480                                   # the pictures drawn at this size, then scaled down (smooth)


def run(cmd, log):
    with open(log, "w") as fh:
        if subprocess.run(cmd, cwd=ROOT, stdout=fh, stderr=subprocess.STDOUT).returncode:
            sys.exit(f"failed: {' '.join(cmd)} (see {log})")


def build(b):
    """the app's icon pictures ($b/icon-art) and the games' pictures ($b/thumbs): drawn by the app's code"""
    os.makedirs(os.path.join(ROOT, b, "host"), exist_ok=True)
    run(["make", "--no-print-directory", "-f", f"{BG}/Makefile", "icon-art", f"B={b}", f"ICON_DIR={b}/icon-art"],
        os.path.join(ROOT, b, "icon-art.log"))
    cflags = ["-O2", "-std=c11", "-Wall", "-Wextra", f"-I{BG}/src", "-Isdk/third_party/stb"]
    sdl_c = subprocess.check_output(["sdl2-config", "--cflags"], text=True).split()
    sdl_l = subprocess.check_output(["sdl2-config", "--libs"], text=True).split()
    shell_o = f"{b}/host/thumbs_shell.o"
    run(["gcc"] + cflags + sdl_c + ["-Dmain=board_main", "-Dgame_register=shell_game_register", "-c", "-o", shell_o,
                                    f"{BG}/src/shell.c"], os.path.join(ROOT, b, "thumbs.log"))
    src = [f"{BG}/src/{n}.c" for n in ("save", "sound", "art", "ai", "bots", "ai_worker", "text")]
    src += sorted(os.path.relpath(p, ROOT) for p in glob.glob(os.path.join(ROOT, BG, "src", "card*.c")))
    src += [f"{b}/gen/{n}.c" for n in ("text_data", "art_data", "edition")]
    src += sorted(os.path.relpath(p, ROOT) for p in glob.glob(os.path.join(ROOT, BG, "src", "games", "*.c")))
    exe = f"{b}/host/board_thumbs"
    run(["gcc"] + cflags + sdl_c + ["-o", exe, "apps/android/store/tools/board_thumbs.c", shell_o] + src + sdl_l +
        ["-lm"], os.path.join(ROOT, b, "thumbs.log"))
    ids = sorted({g for lay in LAYOUT.values() for g, *_ in lay})
    os.makedirs(os.path.join(ROOT, b, "thumbs"), exist_ok=True)
    run([exe, f"{b}/thumbs", str(THUMB)] + ids, os.path.join(ROOT, b, "thumbs.log"))


def felt(w, h):
    """the app's green table, lighter in the middle (as make_icon.board_icon draws it)"""
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    d = np.sqrt(((xx - w * 0.62) / w) ** 2 + ((yy - h * 0.45) / h) ** 2) / 0.62
    k = (1.28 - 0.5 * np.clip(d, 0, 1))[..., None]
    rng = np.random.default_rng(7)                # the felt's grain, steady
    grain = rng.normal(0, 1.6, (h, w, 1)).astype(np.float32)
    a = np.clip(np.array(make_icon.TABLE, np.float32) * k + grain, 0, 255).astype(np.uint8)
    return Image.fromarray(a, "RGB").convert("RGBA")


def frame(img, t=16):
    """a walnut frame round the table, lit from the top left, with a gold fillet inside it"""
    w, h = img.size
    a = np.array(img).astype(np.float32)
    yy, xx = np.mgrid[0:h, 0:w]
    edge = np.minimum(np.minimum(xx, yy), np.minimum(w - 1 - xx, h - 1 - yy))
    rng = np.random.default_rng(3)
    streak = np.repeat(rng.normal(0, 6, (h, 1)), w, 1) + np.repeat(rng.normal(0, 6, (1, w)), h, 0)
    for c in range(3):
        wood = WALNUT[c] + 0.55 * streak + np.where(edge < 3, (WALNUT_D[c] - WALNUT[c]), 0) \
            + np.where((edge >= t - 4) & (edge < t), (WALNUT_D[c] - WALNUT[c]) * 0.8, 0)
        a[..., c] = np.where(edge < t, wood, a[..., c])
    lit = (xx + yy) < (w + h) * 0.5                # the top left lighter, the bottom right darker
    a[..., :3] = np.where(((edge < t) & lit)[..., None], a[..., :3] * 1.12, a[..., :3])
    for i, col in ((t, OUT), (t + 1, GOLD_D), (t + 2, GOLD), (t + 3, GOLD_L), (t + 4, GOLD), (t + 5, OUT)):
        a[edge == i, :3] = col
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8), "RGBA")


def lay(img, picture, cx, cy, size, deg):
    """a picture laid on the table at (cx, cy), size across, turned deg, over its soft shadow"""
    p = picture.resize((size, int(round(picture.height * size / picture.width))), Image.LANCZOS)
    make_icon._shadowed(img, p, cx, cy, deg, 0.62)


def gold_text(img, s, x, y, size):
    """s at (x, y) (its top left) in gold: dark outline, soft shadow, lit from the top"""
    font = ImageFont.truetype(FONT, size, index=0)
    x0, y0, x1, y1 = font.getbbox(s)
    w, h = x1 - x0 + 24, y1 - y0 + 24
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).text((12 - x0, 12 - y0), s, font=font, fill=255)
    out = mask.filter(ImageFilter.MaxFilter(7))
    shadow = out.filter(ImageFilter.GaussianBlur(6)).point(lambda v: int(v * 0.7))
    img.alpha_composite(Image.merge("RGBA", (Image.new("L", (w, h), 0),) * 3 + (shadow,)), (x + 5, y + 7))
    img.alpha_composite(Image.merge("RGBA", [Image.new("L", (w, h), c) for c in OUT] + [out]), (x, y))
    g = np.zeros((h, w, 3), np.float32)
    t = np.linspace(0, 1, h)[:, None]
    for c in range(3):                            # light gold at the top, gold, dark gold at the bottom
        g[..., c] = np.where(t < 0.45, GOLD_L[c] + (GOLD[c] - GOLD_L[c]) * t / 0.45,
                             GOLD[c] + (GOLD_D[c] - GOLD[c]) * (t - 0.45) / 0.55)
    fill = Image.fromarray(g.astype(np.uint8), "RGB").convert("RGBA")
    fill.putalpha(mask)
    img.alpha_composite(fill, (x, y))
    return w, h


def pill(img, s, x, y, size):
    """the free app's FREE: ivory letters on a red lacquer pill (the Arcade's free banner has one)"""
    font = ImageFont.truetype(FONT, size, index=0)
    x0, y0, x1, y1 = font.getbbox(s)
    pw, ph = x1 - x0 + size * 1.1, y1 - y0 + size * 0.62
    layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    d.rounded_rectangle((x - 3, y - 3, x + pw + 3, y + ph + 3), radius=ph / 2 + 3, fill=OUT + (255,))
    d.rounded_rectangle((x, y, x + pw, y + ph), radius=ph / 2, fill=RED_D + (255,))
    d.rounded_rectangle((x, y, x + pw, y + ph - 4), radius=ph / 2, fill=RED + (255,))
    d.text((x + (pw - (x1 - x0)) / 2 - x0, y + (ph - (y1 - y0)) / 2 - y0 - 1), s, font=font, fill=IVORY + (255,))
    img.alpha_composite(layer)


def feature(app, b):
    W, H = 1024, 500
    img = frame(felt(W, H))
    thumbs = os.path.join(ROOT, b, "thumbs")
    for gid, cx, cy, size, deg in LAYOUT[app]:
        lay(img, Image.open(os.path.join(thumbs, gid + ".png")).convert("RGBA"), cx, cy, size, deg)
    art = os.path.join(ROOT, b, "icon-art")
    for name, cx, cy, size, deg in PIECES.get(app, PIECES["board"]):
        lay(img, Image.open(os.path.join(art, name + ".png")).convert("RGBA"), cx, cy, size, deg)
    for hero, cx, cy, zoom, deg in ARCADE_LAYOUT.get(app, []):
        make_icon._shadowed(img, make_icon.arcade_screen(hero, zoom, 4 + 2 * zoom), cx, cy, deg, 0.62)
    if app == "retrostone":                       # one word: the name alone, larger, by the knight
        gold_text(img, "RetroStone", 46, 88, 88)
        return img.convert("RGB")
    top = 96 if app == "board" else 66
    gold_text(img, "RetroStone", 46, top, 84)
    gold_text(img, "Board", 48, top + 102, 84)
    if app == "boardfree":
        pill(img, "FREE", 56, top + 226, 40)
    return img.convert("RGB")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", default="build/boardgames-store", help="the folder for the app's pictures")
    ap.add_argument("--no-build", action="store_true", help="use the pictures already in --build")
    ap.add_argument("apps", nargs="*", default=["board", "boardfree", "retrostone"], help="the apps (default: all)")
    a = ap.parse_args()
    if not a.no_build:
        build(a.build)
    art = os.path.join(ROOT, a.build, "icon-art")
    for app in a.apps:
        d = os.path.join(STORE, app, "graphics")
        os.makedirs(d, exist_ok=True)
        make_icon.store_icon(None, False, art, "retrostone" if app == "retrostone" else "board").save(
            os.path.join(d, "icon-512.png"))
        feature(app, a.build).save(os.path.join(d, "feature-1024x500.png"))
        print(f"{d}: icon-512.png, feature-1024x500.png")


if __name__ == "__main__":
    main()

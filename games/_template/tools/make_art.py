#!/usr/bin/env python3
"""@NAME@: the code-drawn art, in the 8BCraft house style (docs/art-direction.md).

Code-drawn art only, no image generation: every sprite and panorama is drawn here with
games/common/tools/house_style.py (1-px outlines, 3-4-shade ramps lit from the top-left,
squash and stretch). The sheet layout (SPRITES) is shared with build_assets.py.

Writes, in the art folder (default games/@ID@/art):
  sprites.png (256x64), tiles/ground.png (512x48), tiles/hills.png (512x96)

    python3 games/@ID@/tools/make_art.py [--out DIR] [--preview]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/@ID@/LICENSE.
"""
import argparse
import math
import os
import random
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.dirname(HERE)
ROOT = os.path.abspath(os.path.join(GAME, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "games", "common", "tools"))
import house_style as hs  # noqa: E402

Canvas = hs.Canvas
SHEET_W, SHEET_H = 256, 64
# name, x, y, w, h, frames, palette group (hero: OBJ 0, players 2-4 = OBJ 1, 4, 5 recoloured; props: OBJ 2)
SPRITES = [
    ("hero_idle", 0, 0, 32, 32, 2, "hero"),
    ("hero_squash", 64, 0, 32, 32, 1, "hero"),
    ("hero_stretch", 96, 0, 32, 32, 1, "hero"),
    ("hero_air", 128, 0, 32, 32, 1, "hero"),
    ("hero_hit", 160, 0, 32, 32, 1, "hero"),
    ("crate", 192, 0, 16, 16, 1, "props"),
    ("puff", 208, 0, 8, 8, 3, "props"),
    ("hero_icon", 0, 32, 16, 16, 1, "hero"),     # the title's player slots, the results
]
PANORAMAS = {"ground": ("tiles/ground.png", 512, 48), "hills": ("tiles/hills.png", 512, 96)}

# the hero's colours: an accent ramp (house_style.ACCENTS) and the house outline
HERO = hs.ramp("pink")          # light .. dark; REPLACE with the game's own ramp
OUT = (40, 16, 58)              # the hero's outline: the darkest shade of its material, towards violet
FEET = hs.ramp("wood")


# ---- the hero: a round body, big eyes, two feet; drawn at rest, then squashed or stretched -------------------------
def hero(pose="idle", frame=0):
    cv = Canvas(32, 32)
    bob = 1 if (pose == "idle" and frame == 1) else 0
    # feet (behind the body)
    for fx in (11, 19):
        hs.ellipse(cv, fx + 0.5, 28.5, 3.2, 2.0, FEET)
    # the body: a shaded ellipse, a highlight dot top-left
    hs.ellipse(cv, 16.0, 18.5 + bob, 9.5, 9.0, HERO)
    cv.set(11, 13 + bob, hs.HOUSE["white"][0])
    # eyes (the house eye), or X eyes when hit
    if pose == "hit":
        for ex in (15, 20):
            for d in range(3):
                cv.set(ex + d, 16 + d, hs.HOUSE["ink"][0]), cv.set(ex + 2 - d, 16 + d, hs.HOUSE["ink"][0])
    else:
        hs.eye(cv, 15, 15 + bob, look=(1, 0), big=True)
        hs.eye(cv, 20, 15 + bob, look=(1, 0), big=True)
    if pose == "squash":
        cv = hs.squash(cv, 1.25)
    elif pose == "stretch":
        cv = hs.squash(cv, 0.82)
    cv.outline(OUT)
    return cv


def hero_icon():
    """The 16x16 icon of the title's player slots and the results: the hero's face (the same colours, so the
    palette swaps of players 2-4 apply)."""
    cv = Canvas(16, 16)
    hs.ellipse(cv, 8.0, 8.5, 6.5, 6.0, HERO)
    cv.set(4, 5, hs.HOUSE["white"][0])
    hs.eye(cv, 7, 6, look=(1, 0))
    hs.eye(cv, 10, 6, look=(1, 0))
    cv.outline(OUT)
    return cv


def crate():
    P = hs.ramp_dict(hs.ramp("wood"))
    cv = Canvas(16, 16)
    for y in range(1, 15):
        for x in range(1, 15):
            cv.set(x, y, hs.shade3(P, x, y, 14, 1))
    for i in range(1, 15):                        # the diagonal brace and the frame
        cv.set(i, i, P["d"]), cv.set(i, 1, P["h"]), cv.set(1, i, P["h"])
        cv.set(i, 14, P["d"]), cv.set(14, i, P["d"])
    cv.outline((34, 20, 12))
    return cv


def puff(frame):
    """Dust: a ring that grows and fades with the house checker dither (never dither a body)."""
    cv = Canvas(8, 8)
    r = [1.6, 2.6, 3.4][frame]
    for y in range(8):
        for x in range(8):
            d = math.hypot(x + 0.5 - 4, y + 0.5 - 4)
            if d < r and (frame == 0 or hs.checker(x, y, frame)):
                cv.set(x, y, hs.HOUSE["sand"][0] if d < r - 1 else hs.HOUSE["sand"][1])
    return cv


def sprite_frame(name, i):
    if name == "hero_icon":
        return hero_icon()
    if name.startswith("hero_"):
        return hero(name[5:], i)
    if name == "crate":
        return crate()
    if name == "puff":
        return puff(i)
    raise KeyError(name)


# ---- panoramas (BG tiles) ------------------------------------------------------------------------------------------------
def ground():
    W, H = 512, 48
    leaf, sand = hs.ramp("leaf"), hs.ramp("sand")
    cv = Canvas(W, H)
    rng = random.Random(3)
    for x in range(W):
        top = 1 + int(round(0.8 * math.sin(x * 2 * math.pi / 64)))
        for y in range(top, H):
            d = y - top
            col = leaf[0] if d == 0 else leaf[1] if d < 3 else leaf[2] if d < 5 else sand[2]
            if d >= 5 and (y + int(2 * math.sin((x + y * 3) / 13.0))) % 10 == 0:
                col = sand[3]
            cv.set(x, y, col)
        cv.set(x, top - 1, leaf[3])
    for _ in range(30):                                # pebbles
        x, y = rng.randint(2, W - 5), rng.randint(12, H - 4)
        cv.rect(x, y, x + 2, y + 1, sand[1])
        cv.set(x + 2, y + 1, sand[3])
    return cv


def hills():
    """The far hills: two muted ranges (low contrast, no outline: the background never competes with sprites)."""
    W, H = 512, 96
    far = [(150, 196, 214), (124, 172, 196)]
    near = [(104, 156, 150), (84, 136, 132), (68, 116, 116)]
    cv = Canvas(W, H)
    for x in range(W):
        h1 = 40 + int(14 * math.sin(x * 2 * math.pi / 256) + 6 * math.sin(x * 2 * math.pi / 64 + 1))
        h2 = 64 + int(10 * math.sin(x * 2 * math.pi / 128 + 2) + 4 * math.sin(x * 2 * math.pi / 32))
        for y in range(h1, H):
            cv.set(x, y, far[0] if y - h1 < 2 else far[1])
        for y in range(h2, H):
            cv.set(x, y, near[0] if y - h2 < 2 else near[1] if y < 88 else near[2])
    return cv


def build_sheet():
    im = Image.new("RGB", (SHEET_W, SHEET_H), hs.MAGENTA)
    for name, x, y, w, h, frames, _g in SPRITES:
        for i in range(frames):
            im.paste(sprite_frame(name, i).image(), (x + i * w, y))
    return im


def write_all(out, preview=False):
    os.makedirs(os.path.join(out, "tiles"), exist_ok=True)
    files = {"sprites.png": build_sheet()}
    files[PANORAMAS["ground"][0]] = ground().image()
    files[PANORAMAS["hills"][0]] = hills().image()
    for f, im in files.items():
        im.save(os.path.join(out, f))
        if preview:
            os.makedirs(os.path.join(out, "preview"), exist_ok=True)
            im.resize((im.width * 4, im.height * 4), Image.NEAREST).save(
                os.path.join(out, "preview", os.path.basename(f)))
    return sorted(files)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", default=os.path.join(GAME, "art"))
    ap.add_argument("--preview", action="store_true", help="also write 4x previews in <out>/preview")
    a = ap.parse_args(argv)
    for f in write_all(a.out, a.preview):
        print("wrote %s/%s" % (a.out, f))


if __name__ == "__main__":
    main()

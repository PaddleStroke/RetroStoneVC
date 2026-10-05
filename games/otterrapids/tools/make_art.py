#!/usr/bin/env python3
"""Otter Rapids: the code-drawn art, in the 8BCraft house style (docs/art-direction.md).

Code-drawn art only, no image generation: every sprite and panorama is drawn here with
games/common/tools/house_style.py (1-px outlines, 3-4-shade ramps lit from the top-left,
squash and stretch). The sheet layout (SPRITES) is shared with build_assets.py.

Writes, in the art folder (default games/otterrapids/art):
  sprites.png (256x64), tiles/ground.png (512x48), tiles/hills.png (512x96)

    python3 games/otterrapids/tools/make_art.py [--out DIR] [--preview]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/otterrapids/LICENSE.
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
    ("fish", 192, 0, 16, 16, 1, "props"),
    ("puff", 208, 0, 8, 8, 3, "props"),
    ("hero_icon", 0, 32, 16, 16, 1, "hero"),     # the title's player slots, the results
]
PANORAMAS = {"ground": ("tiles/ground.png", 512, 48), "hills": ("tiles/hills.png", 512, 96)}

# the hero's colours: an accent ramp (house_style.ACCENTS) and the house outline
HERO = hs.ramp("wood")          # light .. dark; warm otter fur
OUT = (36, 27, 46)              # the hero's outline: the darkest shade of its material, towards violet
FEET = hs.ramp("wood")


# ---- the hero: a round body, big eyes, two feet; drawn at rest, then squashed or stretched -------------------------
def hero(pose="idle", frame=0):
    cv = Canvas(32, 32)
    tan = hs.ramp("sand")
    # Head upriver, tail behind: a single consistent swimming anatomy.
    hs.ellipse(cv,16,28,3,3,HERO)
    for side in (-1,1):hs.ellipse(cv,16+side*5,25,3,2,HERO)
    hs.ellipse(cv,16,19,8,9,HERO);hs.ellipse(cv,15,18,5,6,tan)
    stroke=2 if frame else -2
    for side in (-1,1):hs.ellipse(cv,16+side*10,18+stroke*side,3,2,HERO)
    hs.ellipse(cv,9,4,3,3,HERO);hs.ellipse(cv,23,4,3,3,HERO)
    hs.ellipse(cv,16,8,9,7,HERO);hs.ellipse(cv,16,11,6,3,tan)
    if pose=="hit":
        for ex in (11,19):
            for d in range(3):cv.set(ex+d,5+d,OUT);cv.set(ex+2-d,5+d,OUT)
    else:
        hs.eye(cv,11,5,look=(0,-1));hs.eye(cv,19,5,look=(0,-1))
    cv.rect(15,9,17,10,OUT);cv.set(15,9,hs.HOUSE["cream"][0]);cv.set(16,12,OUT)
    for yy in (10,12):cv.rect(6,yy,10,yy,OUT);cv.rect(22,yy,26,yy,OUT)
    cv.outline(OUT)
    return cv


def hero_icon():
    cv=Canvas(16,16)
    hs.ellipse(cv,8,9,6,5,HERO)
    hs.ellipse(cv,3,5,2,2,HERO);hs.ellipse(cv,13,5,2,2,HERO)
    hs.ellipse(cv,8,12,4,2,hs.ramp("sand"))
    hs.eye(cv,4,7,look=(0,1));hs.eye(cv,10,7,look=(0,1))
    cv.rect(7,11,9,12,OUT);cv.outline(OUT)
    return cv


def fish():
    cv=Canvas(16,16)
    gold=[(255,237,151),(248,190,58),(207,122,36),(127,67,32)]
    hs.ellipse(cv,7,8,5,3,gold)
    for y in range(5,12):
        for x in range(12,15):
            if abs(y-8)<=x-11:cv.set(x,y,gold[1 if y<9 else 2])
    cv.set(4,7,OUT);cv.set(5,7,hs.HOUSE["cream"][0]);cv.outline(OUT)
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
    if name == "fish":
        return fish()
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

#!/usr/bin/env python3
"""Mole Marbles: the code-drawn art, in the 8BCraft house style (docs/art-direction.md).

Code-drawn art only, no image generation: every sprite and panorama is drawn here with
games/common/tools/house_style.py (1-px outlines, 3-4-shade ramps lit from the top-left,
squash and stretch). The sheet layout (SPRITES) is shared with build_assets.py.

Writes, in the art folder (default games/molemarbles/art):
  sprites.png (256x64), tiles/ground.png (512x48), tiles/hills.png (512x96)

    python3 games/molemarbles/tools/make_art.py [--out DIR] [--preview]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/molemarbles/LICENSE.
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
PANORAMAS = {"ground": ("tiles/ground.png", 512, 32), "hills": ("tiles/hills.png", 512, 208)}

# the hero's colours: an accent ramp (house_style.ACCENTS) and the house outline
HERO = [(251,211,126),(222,161,83),(172,104,66),(104,61,53)]
OUT=(39,29,53)
FEET=HERO
GOLD=[(255,246,202),(242,199,72),(196,142,46),(127,87,36)]
METAL=[(221,225,219),(142,170,180),(78,108,129),(43,55,82)]

def hero(pose="idle",frame=0):
    cv=Canvas(32,32)
    # Low stout mole, enormous digging paws, little pink snout and miner helmet.
    hs.ellipse(cv,16,22,8,7,HERO)
    hs.ellipse(cv,16,15,8,7,HERO)
    hs.ellipse(cv,7,23+(frame if pose=="idle" else 0),4,3,HERO)
    hs.ellipse(cv,25,23-(frame if pose=="idle" else 0),4,3,HERO)
    for x in (4,7,10,22,25,28):cv.rect(x,25,x,26,METAL[0])
    for x in (11,20):hs.ellipse(cv,x,29,4,1.5,HERO)
    cv.rect(10,7,22,11,GOLD[1]);cv.rect(11,5,21,8,GOLD[1]);cv.rect(9,11,24,12,GOLD[2]);cv.rect(12,5,17,6,GOLD[0])
    hs.ellipse(cv,17,8,3,3,METAL)
    cv.rect(16,6,18,8,GOLD[0]);cv.set(17,9,GOLD[1])
    for ex in (11,19):
        if pose=="hit":
            for d in range(3):cv.set(ex+d,15+d,OUT);cv.set(ex+2-d,15+d,OUT)
        else:hs.eye(cv,ex,15,look=(0,0),big=True)
    # Snout uses existing gold shades to remain inside the hardware palette.
    hs.ellipse(cv,17,21,3,2,GOLD)
    cv.set(16,20,OUT);cv.set(19,20,OUT)
    cv.rect(14,24,16,25,METAL[0])
    cv.outline(OUT)
    return cv

def hero_icon():
    cv=Canvas(16,16);hs.ellipse(cv,8,10,6,5,HERO)
    cv.rect(3,3,12,5,GOLD[1]);cv.rect(4,2,11,3,GOLD[1]);cv.rect(2,6,13,6,GOLD[2])
    cv.rect(7,3,9,5,GOLD[0]);hs.eye(cv,4,8);hs.eye(cv,10,8);hs.ellipse(cv,8,12,2,1.5,GOLD);cv.outline(OUT);return cv


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
    cv=Canvas(512,32)
    P=[(142,136,153),(91,86,111),(62,56,83),(37,33,56)]
    for y in range(32):
        for x in range(512):cv.set(x,y,P[0] if y==0 else P[1] if y<4 else P[2] if y<20 else P[3])
    for x in range(0,512,32):
        cv.rect(x,4,x+1,17,P[3]);cv.rect(x+1,17,x+30,18,P[3]);cv.rect(x+16,20,x+17,31,P[2])
    rng=random.Random(4)
    for _ in range(70):
        x=rng.randrange(512);y=rng.randrange(8,31);cv.rect(x,y,x+2,y,P[1])
    return cv

def hills():
    # Complete low-contrast cavern: rock ribs, distant arched mine passages and crystal veins.
    cv=Canvas(512,208)
    P=[(26,30,50),(34,38,62),(43,45,69),(50,53,76),(60,65,85),(76,88,109),(97,114,132),(119,134,147)]
    for y in range(208):
        for x in range(512):cv.set(x,y,P[0] if y<64 else P[1] if y<136 else P[2])
    for x in range(0,512,64):
        cv.rect(x+4,4,x+9,190,P[2]);cv.rect(x+10,4,x+13,190,P[3])
        for y in range(10,150):
            yy=25+int(18*math.sin((y+x)/32));cv.set(x+yy,y,P[3])
        # The arch is distant architecture, not a foreground obstacle.
        for yy in range(90,208):
            for xx in range(x+16,x+51):
                dx=(xx-x-33)/17
                if yy>115 or dx*dx+((yy-115)/28)**2<1:cv.set(xx,yy,P[0])
        for xx in range(x+14,x+54):
            y=115-int(28*math.sqrt(max(0,1-((xx-x-34)/20)**2)))
            cv.rect(xx,y,xx,y+3,P[4])
    for cx,cy in ((17,161),(71,199),(145,164),(226,188),(301,153),(355,197),(426,166),(489,189)):
        for dx,height in ((-6,13),(0,24),(7,17)):
            for yy in range(height):
                width=min(4,yy//3+1)
                cv.rect(cx+dx-width,cy-height+yy,cx+dx+width,cy-height+yy,P[5])
                cv.set(cx+dx-width,cy-height+yy,P[6]);cv.set(cx+dx,cy-height+yy,P[4])
            cv.set(cx+dx-1,cy-height+2,P[7])
    # ceiling stone silhouettes; clear playing foreground remains readable
    for x in range(512):
        for y in range(3+int(4*(1+math.sin(x/31)))):cv.set(x,y,P[3])
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

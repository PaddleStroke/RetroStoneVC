#!/usr/bin/env python3
"""Snail Switch: the code-drawn art, in the 8BCraft house style (docs/art-direction.md).

Code-drawn art only, no image generation: every sprite and panorama is drawn here with
games/common/tools/house_style.py (1-px outlines, 3-4-shade ramps lit from the top-left,
squash and stretch). The sheet layout (SPRITES) is shared with build_assets.py.

Writes, in the art folder (default games/snailswitch/art):
  sprites.png (256x64), tiles/ground.png (512x48), tiles/hills.png (512x96)

    python3 games/snailswitch/tools/make_art.py [--out DIR] [--preview]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/snailswitch/LICENSE.
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
PANORAMAS = {"ground": ("tiles/ground.png", 512, 48), "hills": ("tiles/hills.png", 512, 240)}

# the hero's colours: an accent ramp (house_style.ACCENTS) and the house outline
HERO = hs.ramp("pink")          # light .. dark; REPLACE with the game's own ramp
OUT = (40, 16, 58)              # the hero's outline: the darkest shade of its material, towards violet
FEET = hs.ramp("wood")


# ---- the hero: a round body, big eyes, two feet; drawn at rest, then squashed or stretched -------------------------

def hero(pose="idle", frame=0):
    cv=Canvas(32,32)
    skin=[(228,237,164),(178,205,99),(105,153,64),(58,89,52)]
    shell=HERO
    # Low muscular foot and raised head; feelers end in large expressive eyes.
    hs.ellipse(cv,16,27,13,3,skin)
    hs.ellipse(cv,25,22,4,6,skin)
    hs.ellipse(cv,13,18,10,10,shell)
    # A readable inward spiral, shaded ochre-purple on the player's shell.
    for k in range(40):
        ang=k*0.26
        r=7-k*0.14
        cv.set(round(13+math.cos(ang)*r),round(18+math.sin(ang)*r),shell[3])
    cv.set(8,11,shell[0]);cv.set(9,10,shell[0])
    for ex,ey in [(24,12),(29,13+frame)]:
        for y in range(ey+2,23):
            cv.set(ex,y,skin[2]);cv.set(ex-1,y,skin[1])
        hs.eye(cv,ex-2,ey-2,look=(1,0),big=True)
    if pose=="hit":
        for ex in [23,28]:
            for d in range(3):
                cv.set(ex+d,12+d,OUT);cv.set(ex+2-d,12+d,OUT)
    if pose=="squash":cv=hs.squash(cv,1.16)
    cv.outline(OUT)
    return cv

def hero_icon():
    cv=Canvas(16,16)
    hs.ellipse(cv,6,9,5,5,HERO)
    hs.ellipse(cv,9,13,6,2,[(228,237,164),(178,205,99),(105,153,64),(58,89,52)])
    for k in range(24):
        r=3-k*.10
        cv.set(round(6+math.cos(k*.4)*r),round(9+math.sin(k*.4)*r),HERO[3])
    cv.rect(12,5,12,12,(178,205,99))
    hs.eye(cv,11,3,look=(1,0))
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
    W,H=512,240
    cv=Canvas(W,H)
    rng=random.Random(812)
    sky=(199,221,183)
    distant=[(176,202,169),(162,192,160),(148,182,151)]
    bark=[(142,174,145),(126,160,135),(111,145,124)]
    foliage=[(130,165,136),(117,153,126),(104,141,116)]
    near=[(104,138,119),(92,127,111),(84,115,103)]
    cv.rect(0,0,W-1,H-1,sky)
    def ellipse(cx,cy,rx,ry,col):
        for y in range(max(0,cy-ry),min(H,cy+ry+1)):
            for x in range(cx-rx,cx+rx+1):
                if (x-cx)**2*ry**2+(y-cy)**2*rx**2<=rx**2*ry**2:cv.set(x%W,y,col)
    def branch(x0,y0,x1,y1,r,col):
        n=max(abs(x1-x0),abs(y1-y0),1)
        for i in range(n+1):
            x=x0+(x1-x0)*i//n;y=y0+(y1-y0)*i//n
            for dx in range(-r,r+1):
                for dy in range(-r,r+1):cv.set((x+dx)%W,y+dy,col)
    # Misty distant trunks, overlapping crowns and visible sky openings.
    for j in range(18):
        x=j*31+rng.randrange(-10,11);width=rng.randrange(3,7)
        branch(x,32,x+rng.randrange(-12,13),240,width,distant[0])
        for k in range(3):ellipse(x+rng.randrange(-18,19),35+k*26,rng.randrange(22,42),rng.randrange(15,26),distant[1+k%2])
    # Tall midground trees: shaded irregular trunks, forks and clustered canopy lobes.
    for x in [22,119,248,366,469]:
        lean=rng.randrange(-19,20);top=rng.randrange(18,46)
        for y in range(top,H):
            cx=x+lean*(H-y)//H+int(3*math.sin(y/47+x));width=5+(y//52)
            for q in range(-width,width+1):cv.set(cx+q,y,bark[0] if q<-width//2 else bark[1] if q<width//2 else bark[2])
        for side in [-1,1]:
            branch(x,110,x+side*34,62,3,bark[1]);branch(x+side*24,76,x+side*41,42,2,bark[0])
        for k in range(9):
            cx=x+rng.randrange(-44,45);cy=top+rng.randrange(-20,47)
            ellipse(cx,cy,25+rng.randrange(12),14+rng.randrange(11),foliage[2])
            ellipse(cx-4,cy-5,22+rng.randrange(8),11+rng.randrange(8),foliage[1])
            ellipse(cx-8,cy-8,16,7,foliage[0])
    # Ferns and low bushes live at the forest floor, away from the playable vine.
    for j in range(22):
        x=j*24+rng.randrange(-7,8);y=225+rng.randrange(12)
        ellipse(x,y,24,13,near[2]);ellipse(x-5,y-5,20,10,near[1]);ellipse(x-8,y-9,12,6,near[0])
    for j in range(12):
        x=j*43+14;y=235-rng.randrange(8)
        branch(x,y,x+2,y-23,0,near[0])
        for k in range(5):
            branch(x,y-k*4,x-10+k,y-k*4-5,1,near[0]);branch(x,y-k*4,x+11-k,y-k*4-5,1,near[1])
    for y in range(0,H,2):
        for x in range(0,W,2):
            col=cv.get(x,y);cv.rect(x,y,x+1,y+1,col)
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

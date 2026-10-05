#!/usr/bin/env python3
"""Nutty Branch: the code-drawn art, in the 8BCraft house style (docs/art-direction.md).

Code-drawn art only, no image generation. The modules:
  nb_pal.py       the palettes and the time-of-day / season bands
  nb_bark.py      the branch texture (the cylinder's bark, laid out by angle: x = R sin(angle))
  nb_render.py    a tiny 3D renderer: every prop seen from the game's camera, sitting on the curved bark
  nb_props.py     the props, traps and decorations at their pre-drawn sizes (LODS) and tilts (TILTS)
  nb_squirrel.py  the squirrel from behind
  nb_backdrop.py  the four forests behind (BG_STYLE): two parallax layers each
  make_art.py     this: the list of sprites, the sheets

Writes, in the art folder (default games/nuttybranch/art):
  sprites.png + sprites.json   every sprite frame (the sheet and its layout: anchor, palette group, resident)
  branch.png                   the affine branch texture (512x256)
  backdrop_<a-d>_near.png      BG3: each forest's near layer (256x256, repeating every 128 px)
  backdrop_<a-d>_far.png       BG4: its far layer
  palettes.json                the fixed palettes (sprites) and the bands (BG, props, sky)

    python3 games/nuttybranch/tools/make_art.py [--out DIR] [--preview]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE.
"""
import argparse
import json
import math
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.dirname(HERE)
ROOT = os.path.abspath(os.path.join(GAME, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "games", "common", "tools"))
import house_style as hs  # noqa: E402
import nb_backdrop  # noqa: E402
import nb_bark  # noqa: E402
import nb_props as P  # noqa: E402
import nb_render as nr  # noqa: E402
import nb_squirrel as S  # noqa: E402
from nb_pal import (BANDS, BIRD, BIRD_PAL, COL_PAL, FUR_HUES, HERO_PAL, P2_SHIFT, R_BLACK, R_GREY,  # noqa: E402
                    R_OWL, WOOD, WOOD_PAL, band_bark, band_col, band_scene, band_wood)

Canvas = hs.Canvas
LOD_SCALES = P.LODS
TILTS = P.TILTS


# ---- the birds (OBJ 7), in the 3D renderer ----------------------------------------------------------------------
def owl(k, frame):
    """An owl (by day a tawny hawk-owl) swooping across, seen from above and behind: wings spread, up or down."""
    wing = 0.35 if frame == 0 else -0.25
    prims = [nr.Ell((0, 10, 0), (6, 6.5, 9), R_OWL), nr.Ell((0, 15, 7), (5.4, 5, 5), R_OWL),
             nr.Ell((-2.6, 19.5, 6), (1.2, 2.4, 1.2), R_OWL), nr.Ell((2.6, 19.5, 6), (1.2, 2.4, 1.2), R_OWL),
             nr.Ell((-15, 12, 0), (13, 1.6, 6), R_OWL, nr.rot_z(-wing)), nr.Ell((15, 12, 0), (13, 1.6, 6), R_OWL, nr.rot_z(wing)),
             nr.Ell((0, 8, -9), (4, 1.4, 4), R_OWL)]
    return P.draw(prims, k, 0, None, BIRD["out"], BIRD["out"])


def woodpecker(k, frame):
    """Clinging to the side of the branch (drawn at 68 degrees, rolled with the bark): a black back barred with white,
    a red cap; frame 1 pecks."""
    peck = 2.0 * frame
    prims = [nr.Ell((0, 9, 0), (4.2, 9, 3.6), R_BLACK, tex=P.stripe_tex(1, 4)),
             nr.Ell((0, 19, 1 + peck), (3.6, 3.4, 3.4), R_BLACK), nr.Ell((0, 21.6, 0.4 + peck), (2.6, 1.6, 2.4), [BIRD["red"]] * 4),
             nr.Ell((0, 18.6, 4.6 + peck), (0.8, 0.8, 2.2), R_GREY), nr.Ell((0, 1.5, -2), (2, 3, 1.2), R_BLACK)]
    return P.draw(prims, k, 68, None, BIRD["out"], BIRD["out"])


# ---- small screen sprites -----------------------------------------------------------------------------------------
def leaf(frame):
    cv = Canvas(8, 8)
    L = [WOOD["l0"], WOOD["l0"], WOOD["l1"], WOOD["l2"]]
    if frame == 0:
        hs.ellipse(cv, 4, 4, 3.2, 1.6, L)
    else:
        hs.ellipse(cv, 4, 4, 1.6, 3.0, L)
    cv.set(4, 4, WOOD["l2"])
    return cv, 4, 4


def meter():
    """32x8: the balance arc, green in the middle, yellow, red at the ends (+-62 degrees)."""
    cv = Canvas(32, 8)
    for x in range(1, 31):
        t = abs(x + 0.5 - 16) / 15.0
        y = int(round(1 + 4.5 * t * t))
        c = BIRD["g2"] if t < 0.4 else BIRD["yellow"] if t < 0.7 else BIRD["red"]
        cv.set(x, y, c), cv.set(x, y + 1, c)
    cv.outline(BIRD["black"])
    return cv, 16, 0


def needle():
    cv = Canvas(8, 8)
    hs.ellipse(cv, 4, 4, 2.0, 2.0, None, flat=BIRD["white"])
    cv.outline(BIRD["black"])
    return cv, 4, 4


def crumb(frame):
    cv = Canvas(8, 8)
    for x, y in (((2, 3), (5, 2), (4, 5)) if frame == 0 else ((1, 2), (6, 1), (5, 6), (2, 6))):
        cv.set(x, y, WOOD["wm"]), cv.set(x + 1, y, WOOD["b2"])
    return cv, 4, 4


def puff(frame):
    cv = Canvas(8, 8)
    r = [1.6, 2.6, 3.4][frame]
    for y in range(8):
        for x in range(8):
            d = math.hypot(x + 0.5 - 4, y + 0.5 - 4)
            if d < r and (frame == 0 or hs.checker(x, y, frame)):
                cv.set(x, y, WOOD["wl"] if d < r - 1 else WOOD["b0"])
    return cv, 4, 4


# ---- the list of sprites: name, palette group, frames [(canvas, ax, ay)], resident ------------------------------
# resident sprites stay in VRAM; the others are streamed into a pool each frame (draw.c: the frames on screen)
LEANS = [0, 20, 40, 60]
TAILS = [0, 18, 36, 54, 72, 90]
SWING = P.SWING_ANGS


def lods(fn, *args):
    return [fn(k, *args) for k in LOD_SCALES]


def tilted(fn, *args):
    """the LOD frames for each tilt: frame = tilt * 8 + lod"""
    return [fn(k, t, *args) for t in TILTS for k in LOD_SCALES]


def sprite_defs():
    """name, palette group, frames, resident (1: always in VRAM; the others are streamed when on screen)"""
    L1, L2 = LOD_SCALES[1:], LOD_SCALES[2:]
    d = []
    d.append(("sq_run", "hero", [S.body(l, "run", f) for l in LEANS for f in (0, 1)], 0))
    d.append(("sq_jump", "hero", [S.body(0, "jump", f) for f in (0, 1)], 0))
    d.append(("sq_fall", "hero", [S.body(l, "fall", 0) for l in (110, 175)], 0))
    d.append(("sq_tail", "hero", [S.tail(a) for a in TAILS], 0))
    d.append(("sq_icon", "hero", [(S.icon(), 8, 8)], 0))
    d.append(("leaf", "wood", [leaf(0), leaf(1)], 1))
    d.append(("meter", "bird", [meter()], 1))
    d.append(("needle", "bird", [needle()], 1))
    d.append(("crumb", "wood", [crumb(0), crumb(1)], 1))
    d.append(("puff", "wood", [puff(0), puff(1), puff(2)], 1))
    d.append(("rain", "col", [P.rain(0), P.rain(1)], 1))
    d.append(("bee", "bird", [P.bee(0), P.bee(1)], 1))
    d.append(("fgleaf", "wood", [P.fg_leaf(i) for i in range(3)], 0))
    d.append(("acorn", "col", lods(P.acorn), 0))
    d.append(("hazel", "col", lods(P.hazel), 0))
    d.append(("walnut", "col", lods(P.walnut), 0))
    d.append(("limb", "wood", [P.flank_limb(k, kind) for kind in range(4) for k in L1], 0))
    d.append(("side", "wood", [P.outward_limb(k, kind) for kind in range(3) for k in L2], 0))
    d.append(("knot", "wood", tilted(P.burl), 0))
    d.append(("web", "col", tilted(P.web), 0))
    d.append(("sap", "col", tilted(P.sap), 0))
    d.append(("hole", "wood", tilted(P.knothole, 0) + tilted(P.knothole, 1), 0))
    d.append(("cone", "wood", tilted(P.pinecone), 0))
    d.append(("snow", "col", tilted(P.snow), 0))
    d.append(("cater", "bird", tilted(P.caterpillar, 0) + tilted(P.caterpillar, 1), 0))
    d.append(("snake", "bird", tilted(P.snake, 0) + tilted(P.snake, 1), 0))
    d.append(("shadow", "wood", lods(P.shadow, 6, 4), 0))
    d.append(("bigshadow", "bird", [P.shadow(k, 22, 7, BIRD["out"]) for k in LOD_SCALES], 0))
    d.append(("broken", "wood", lods(P.broken_near), 0))
    d.append(("pecker", "bird", lods(woodpecker, 0) + lods(woodpecker, 1), 0))
    d.append(("owl", "bird", lods(owl, 0) + lods(owl, 1), 0))
    d.append(("crow", "bird", lods(P.crow, 0) + lods(P.crow, 1) + lods(P.crow, 2), 0))
    d.append(("swing", "wood", [P.swing(k, a) for a in SWING for k in L1], 0))
    d.append(("over", "wood", [P.overhead(k, kind) for kind in range(2) for k in L1], 0))
    d.append(("mush", "wood", [P.mushrooms(k, 72) for k in L1], 0))
    d.append(("hive", "wood", [P.hive(k) for k in L1], 0))
    return d


# ---- the branch texture -------------------------------------------------------------------------------------------
def texture_image(T, pal):
    im = Image.new("RGB", (len(T[0]), len(T)), hs.MAGENTA)
    px = im.load()
    for y, row in enumerate(T):
        for x, i in enumerate(row):
            if i:
                px[x, y] = pal[i]
    return im


# ---- the sheet ----------------------------------------------------------------------------------------------------
GROUP_PAL = {"hero": HERO_PAL, "wood": WOOD_PAL, "col": COL_PAL, "bird": BIRD_PAL}
SHEET_W = 1024


def layout(defs, width=SHEET_W):
    """Shelf packing: returns [(name, group, resident, [(x, y, w, h, ax, ay)])] and the sheet height."""
    out, x, y, shelf = [], 0, 0, 0
    for name, group, frames, resident in defs:
        fr = []
        for cv, ax, ay in frames:
            if x + cv.w > width:
                x, y, shelf = 0, y + shelf, 0
            fr.append((x, y, cv.w, cv.h, ax, ay))
            x += cv.w
            shelf = max(shelf, cv.h)
        out.append((name, group, resident, fr))
    return out, y + shelf


def write_all(out, preview=False):
    os.makedirs(out, exist_ok=True)
    defs = sprite_defs()
    lay, H = layout(defs)
    sheet = Image.new("RGB", (SHEET_W, H), hs.MAGENTA)
    manifest = []
    for (name, group, frames, resident), (_n, _g, _r, fr) in zip(defs, lay):
        for (cv, ax, ay), (x, y, w, h, _ax, _ay) in zip(frames, fr):
            sheet.paste(cv.image(), (x, y))
        manifest.append(dict(name=name, group=group, resident=resident,
                             frames=[dict(x=x, y=y, w=w, h=h, ax=int(math.floor(ax)), ay=int(math.floor(ay)))
                                     for (x, y, w, h, ax, ay) in fr]))
    files = {"sprites.png": sheet}
    files["branch.png"] = texture_image(nb_bark.branch_texture(), band_bark(0))
    for st, fn in nb_backdrop.STYLES.items():
        near, far = fn()
        files["backdrop_%s_near.png" % st] = texture_image(near, band_scene(0))
        files["backdrop_%s_far.png" % st] = texture_image(far, band_scene(0))
    for f, im in files.items():
        im.save(os.path.join(out, f))
        if preview:
            os.makedirs(os.path.join(out, "preview"), exist_ok=True)
            im.resize((im.width * 2, im.height * 2), Image.NEAREST).save(os.path.join(out, "preview", f))
    with open(os.path.join(out, "sprites.json"), "w") as f:
        json.dump(manifest, f, indent=0)
    pals = dict(groups={g: [list(c) for c in p] for g, p in GROUP_PAL.items()},
                fur_hues=list(FUR_HUES), p2_shift=P2_SHIFT, lods=LOD_SCALES, tilts=TILTS, bg_styles=sorted(nb_backdrop.STYLES),
                bands=[dict(name=b["name"], sky=[list(c) for c in b["sky"]],
                            bark=[list(c) for c in band_bark(i)], scene=[list(c) for c in band_scene(i)],
                            wood=[list(c) for c in band_wood(i)], col=[list(c) for c in band_col(i)])
                       for i, b in enumerate(BANDS)])
    with open(os.path.join(out, "palettes.json"), "w") as f:
        json.dump(pals, f, indent=0)
    return sorted(files) + ["sprites.json", "palettes.json"]


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", default=os.path.join(GAME, "art"))
    ap.add_argument("--preview", action="store_true", help="also write 2x previews in <out>/preview")
    a = ap.parse_args(argv)
    for f in write_all(a.out, a.preview):
        print("wrote %s/%s" % (a.out, f))


if __name__ == "__main__":
    main()

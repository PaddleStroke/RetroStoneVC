#!/usr/bin/env python3
"""Ladybug Laps asset build: the code-drawn sheets and tracks (make_art.py) -> tiles, palettes, maps and the sprite
table; the music (make_music.py) -> build/gen/ladybuglaps/assets.c + assets.h.

    build_assets.py --out build/gen/ladybuglaps [--art games/ladybuglaps/art] [--tileset IGNORED]

VRAM (tiles of 32 bytes): BG1 the UI at 0 (the kit 0..296, the logo 320..639, the glyph pool 640..767); the track at
768 (BG3 the world and BG4 what is below it share it: each race loads its track's tiles there); the sprites after the
biggest track (the kit's after ours). Each track: at most 5 BG palettes for its world (1..4, 7), one for BG4 (6).

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/ladybuglaps/LICENSE.
"""
import argparse
import glob
import os
import random
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.dirname(HERE)
ROOT = os.path.abspath(os.path.join(GAME, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "tools"))
sys.path.insert(0, os.path.join(ROOT, "games", "common", "tools"))
import rsasset  # noqa: E402
import make_art as ma  # noqa: E402
import trackdata as td  # noqa: E402

PAL_WORLD, PAL_LOGO, PAL_FAR = 1, 5, 6          # BG palettes: the world uses 1..4 and 7
PAL_WORLD_SLOTS = (1, 2, 3, 4, 7)
VR_BG1, VR_TRACK = 0, 768
TRACK_TILE_MAX = 600                            # the biggest track's tiles (the build fails beyond)
OBJ_PAL = {"red": 0, "yellow": 1, "blue": 4, "green": 5}   # the racers (house_ui hu_player_pal: P1..P4)
PAL_PROPS, PAL_LEAVES = 2, 6                    # sprite palettes (3: the kit)


def cells8(img, w, h):
    return [rsasset.cell(img, x * 8, y * 8, 8, 8) for y in range(h // 8) for x in range(w // 8)]


def convert_world(cells, slots):
    """The world's 8x8 cells -> (tiles, map entries, palettes, remapped): the colour sets packed greedily into
    len(slots) palettes of 15 (the richest tiles first); a tile whose colours fit no palette takes the nearest colours
    of the one that suits it best (counted in `remapped`). Duplicate tiles (also flipped) are shared; tile 0 is blank."""
    work = [rsasset.quantize([c], 15)[0] if len(rsasset.colors_of(c)) > 15 else c for c in cells]
    sets = [frozenset(rsasset.colors_of(c)) for c in work]
    freq = {}
    for s in sets:
        freq[s] = freq.get(s, 0) + 1
    uniq = sorted((s for s in freq if s), key=lambda s: (-len(s), sorted(s)))

    def pack(order):
        pals, left = [], 0
        for s in order:
            if any(s <= p for p in pals):
                continue
            best = None
            for i, p in enumerate(pals):
                u = len(p | s)
                if u <= 15 and (best is None or u - len(p) < best[0]):
                    best = (u - len(p), i)
            if best:
                pals[best[1]] |= s
            elif len(pals) < len(slots):
                pals.append(set(s))
            else:
                left += freq[s]
        return left, pals

    # the richest sets first, then a few shuffles of the same (seeded): the packing that leaves the fewest tiles out
    best = pack(uniq)
    rnd = random.Random(1)
    for _ in range(60):
        if best[0] == 0:
            break
        order = sorted(uniq, key=lambda s: (-len(s) + rnd.random() * 4))
        r = pack(order)
        if r[0] < best[0]:
            best = r
    pals = [sorted(p) for p in best[1]]
    prgb = [[rsasset.rgb888(v) for v in p] for p in pals]
    tiles, index, entries, remapped, choice = [[0] * 64], {tuple([0] * 64): (0, 0, 0)}, [], 0, {}
    for c, s in zip(work, sets):
        if not s:
            entries.append(0)
            continue
        if s not in choice:
            fit = [i for i, p in enumerate(pals) if s <= set(p)]
            if fit:
                choice[s] = (fit[0], False)
            else:
                cost = [sum(rsasset.dist2(rsasset.rgb888(v), prgb[i][rsasset.nearest(rsasset.rgb888(v), prgb[i])])
                            for v in s if v not in pals[i]) for i in range(len(pals))]
                choice[s] = (cost.index(min(cost)), True)
        p, lossy = choice[s]
        remapped += lossy
        pal = pals[p]
        idx = []
        for v in rsasset.split_tiles(c)[0]:
            if v is None:
                idx.append(0)
            elif v in pal:
                idx.append(pal.index(v) + 1)
            else:
                idx.append(rsasset.nearest(rsasset.rgb888(v), prgb[p]) + 1)
        key = tuple(idx)
        hit = index.get(key)
        if hit is None:
            for hf, vf, f in ((1, 0, rsasset.hflip), (0, 1, rsasset.vflip), (1, 1, lambda x: rsasset.vflip(rsasset.hflip(x)))):
                k2 = tuple(f(idx))
                if k2 in index:
                    hit = (index[k2][0], hf, vf)
                    break
        if hit is None:
            tiles.append(idx)
            hit = (len(tiles) - 1, 0, 0)
            index[key] = hit
        entries.append(rsasset.rs_map(hit[0], slots[p], 0, hit[1], hit[2]))
    return tiles, entries, pals, remapped


def track_art(art, t, idx):
    """A track's tiles (world, then BG4's), its world map (one entry a tile), BG4's 8 x 8 map, its palettes (CGRAM
    order: BG 1..4, 6 BG4's, 7)."""
    img = rsasset.load(os.path.join(art, "tracks", t.name + ".png"))
    w, h = img.size
    tiles, wmap, wp, remapped = convert_world(cells8(img, w, h), PAL_WORLD_SLOTS)
    far = rsasset.load(os.path.join(art, "tracks", t.name + "-far.png"))
    rf = rsasset.convert_bg(cells8(far, 64, 64), 1, pal_base=PAL_FAR, tile_base=len(tiles), reserve_blank=False)
    tiles = tiles + rf.tiles
    bypal = {s: rsasset.palette16(wp[i]) if i < len(wp) else [0] * 16 for i, s in enumerate(PAL_WORLD_SLOTS)}
    bypal[PAL_FAR] = rsasset.palette16(rf.palettes[0])
    pals = []
    for s in (1, 2, 3, 4, 6, 7):
        pals += bypal[s]
    if remapped:
        print("%s: %d tiles took the nearest colours of their palette" % (t.name, remapped))
    return tiles, wmap, [m[0] for m in rf.metas], pals, len(wp)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--art", default=os.path.join(GAME, "art"))
    ap.add_argument("--tileset", default="", help="ignored (the art is code-drawn)")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    if not os.path.exists(os.path.join(a.art, "ladybugs.png")) or not os.path.exists(os.path.join(a.art, "tracks")):
        ma.write_all(a.art)
    c = ["/* Generated by games/ladybuglaps/tools/build_assets.py. Do not edit. */", '#include "assets.h"\n']
    h = ["/* Generated by games/ladybuglaps/tools/build_assets.py. Do not edit. */",
         "#ifndef GM_ASSETS_H\n#define GM_ASSETS_H\n#include <stdint.h>\n#include \"rs.h\"\n"]
    stats = []

    # ---- the tracks ----------------------------------------------------------------------------------------------
    tracks, _kinds = td.load()
    rows = []
    most = 0
    for i, t in enumerate(tracks):
        tiles, wmap, fmap, pals, npal = track_art(a.art, t, i)
        if len(tiles) > TRACK_TILE_MAX:
            sys.exit("track %s: %d tiles (the VRAM plan allows %d)" % (t.name, len(tiles), TRACK_TILE_MAX))
        most = max(most, len(tiles))
        c.append(rsasset.c_bytes("tr%d_tiles" % i, rsasset.tiles_bytes(tiles), static=True))
        c.append(rsasset.c_u16("tr%d_map" % i, wmap, static=True))
        c.append(rsasset.c_u16("tr%d_far" % i, fmap, static=True))
        c.append(rsasset.c_u16("tr%d_pals" % i, pals, static=True))
        rows.append("{tr%d_tiles, %d, tr%d_map, %d, %d, tr%d_far, tr%d_pals}" % (i, len(tiles), i, t.w // 8, t.h // 8,
                                                                                i, i))
        stats.append("%s %d tiles %d pals" % (t.name, len(tiles), npal))
    vr_obj = VR_TRACK + (most + 7) // 8 * 8
    h.append("#define VR_BG1 %d\n#define VR_TRACK %d\n#define VR_OBJ %d" % (VR_BG1, VR_TRACK, vr_obj))
    h.append("#define PAL_WORLD %d\n#define PAL_LOGO %d\n#define PAL_FAR %d" % (PAL_WORLD, PAL_LOGO, PAL_FAR))
    h.append("typedef struct gm_track_art {\n    const uint8_t *tiles;\n    int tile_count;\n    const uint16_t *map;   "
             "/* one entry a tile of the world */\n    int mw, mh;\n    const uint16_t *far;   /* 8 x 8 entries */\n"
             "    const uint16_t *pals;  /* 6 x 16: the BG palettes 1..4, 6 (BG4's), 7 */\n} gm_track_art;")
    h.append("extern const gm_track_art gm_tracks[%d];" % len(tracks))
    c.append("const gm_track_art gm_tracks[%d] = {\n    %s};" % (len(tracks), ",\n    ".join(rows)))

    # ---- the sprites: the ladybugs (the red palette's indices), the props, the leaves ----------------------------------
    tiles, srows, names = [], [], []
    for group, sheet, fn, pal, objpal in (("bugs", "ladybugs.png", ma.ladybugs, ma.SHEET_BUG, 0),
                                          ("props", "props.png", ma.props, ma.PROPS_PAL, PAL_PROPS),
                                          ("leaves", "leaves.png", ma.leaves, ma.LEAVES_PAL, PAL_LEAVES)):
        sprites = fn()
        pos, _ = ma.pack(sprites)
        img = rsasset.load(os.path.join(a.art, sheet))
        order = sorted(pos)
        frames = [rsasset.cell(img, *pos[n]) for n in order]
        ro = rsasset.convert_obj(frames, palette=[rsasset.to555(x) for x in pal])
        for k, n in enumerate(order):
            first, fw, fh = ro.frames[k]
            names.append("SPR_%s = %d" % (n.upper(), len(srows)))
            srows.append("{%d, %d, %d, %d}" % (len(tiles) + first, fw, fh, objpal))
        tiles += ro.tiles
    kit_tile = (len(tiles) + 7) // 8 * 8
    h.append("#define KIT_OBJ_TILE %d" % kit_tile)
    h.append("#define BUG_FRAMES %d" % len(ma.BUG_ANGLES))
    h.append("enum { %s, SPR_COUNT = %d };" % (", ".join(names), len(srows)))
    h.append("typedef struct gm_sprite_def { uint16_t tile; uint8_t w, h, pal; } gm_sprite_def;")
    h.append("extern const gm_sprite_def gm_spr[SPR_COUNT];")
    h.append("extern const uint8_t gm_obj_tiles[];\nextern const int gm_obj_tile_count;\nextern const uint16_t gm_obj_pals[128];")
    c.append("const gm_sprite_def gm_spr[SPR_COUNT] = {\n    " + ",\n    ".join(srows) + "};")
    c.append(rsasset.c_bytes("gm_obj_tiles", rsasset.tiles_bytes(tiles)))
    c.append("const int gm_obj_tile_count = %d;" % len(tiles))
    opal = [[0] * 16 for _ in range(8)]
    for colour, slot in OBJ_PAL.items():
        opal[slot] = rsasset.palette16([rsasset.to555(x) for x in ma.bug_palette(colour)])
    opal[PAL_PROPS] = rsasset.palette16([rsasset.to555(x) for x in ma.PROPS_PAL])
    opal[PAL_LEAVES] = rsasset.palette16([rsasset.to555(x) for x in ma.LEAVES_PAL])
    c.append(rsasset.c_u16("gm_obj_pals", [v for p in opal for v in p]))
    stats.append("OBJ %d" % len(tiles))

    # ---- the asset pack: the music --------------------------------------------------------------------------------
    music_dir = os.path.join(a.out, "music")
    subprocess.check_call([sys.executable, os.path.join(HERE, "make_music.py"), music_dir])
    pack = [("music/" + os.path.basename(p), p) for p in sorted(glob.glob(os.path.join(music_dir, "*.mod")))]
    entries = []
    for i, (name, path) in enumerate(pack):
        data = open(path, "rb").read()
        c.append(rsasset.c_bytes("pack_%d" % i, data, static=True))
        entries.append('    {"%s", pack_%d, %d}' % (name, i, len(data)))
    c.append("const rs_asset_entry gm_assets[] = {\n" + ",\n".join(entries) + ",\n    {0, 0, 0}};")
    h.append("extern const rs_asset_entry gm_assets[];\n#endif")

    open(os.path.join(a.out, "assets.c"), "w").write("\n".join(c) + "\n")
    open(os.path.join(a.out, "assets.h"), "w").write("\n".join(h) + "\n")
    print("assets: %s, %d pack files -> %s" % (", ".join(stats), len(pack), a.out))


if __name__ == "__main__":
    main()

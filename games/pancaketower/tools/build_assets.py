#!/usr/bin/env python3
"""Pancake Tower asset build: the code-drawn art (make_art.py) -> tiles, palettes, maps and sprites; the music
(make_music.py) -> build/gen/pancaketower/assets.c + assets.h.

    build_assets.py --out build/gen/pancaketower [--art IGNORED] [--tileset IGNORED]

The art is drawn in memory by make_art.build() (the PNGs in art/ are for review: make pancaketower-art).
VRAM (absolute 8x8 tiles; docs/spec.md "Video memory"):
    BG1 (UI, the house kit)   0 ..  639   the kit's font, panels and 2x glyphs (0..296), the title logo (320..)
    BG2 (the tower)         640 ..  959   made at run time by src/render.c (21 shared tiles, 4 per row and player)
    BG3 (near scenery)      960 .. 1663   the five segments, all loaded at start (2 players may show any two)
    BG4 (far scenery)      1664 .. 1983
    OBJ                    2048 ..        the sprites below, the run-time pancake tiles, the kit's sprites
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pancaketower/LICENSE.
"""
import argparse
import glob
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.dirname(HERE)
ROOT = os.path.abspath(os.path.join(GAME, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "tools"))
sys.path.insert(0, os.path.join(ROOT, "games", "common", "tools"))
import rsasset  # noqa: E402
import make_art  # noqa: E402

VR_BG1, VR_BG2, VR_BG3, VR_BG4, VR_OBJ = 0, 640, 1216, 2080, 2464
BG2_TILES, BG3_TILES, BG4_TILES = 576, 864, 384
PAL_TOWER, PAL_TOPPING, PAL_FAR, PAL_LOGO = 1, 2, 7, 5
NEAR_PAL_SLOTS = [3, 4, 6, 5]             # the near scenery (BG 5 is the logo's on the title: the kitchen avoids it)
NEAR_PALS = len(NEAR_PAL_SLOTS)
# the house palettes (docs/art-direction.md): OBJ 0 the hero, 1 player 2, 2 effects, 3 the kit, 4-7 the game
OBJ_PAL = {"chef": 0, "chef2": 1, "fx": 2, "food": 4, "topping": 5, "critter": 6, "medal": 7}
OBJ_DYN_TILES = 384          # run-time pancake sprites (render.c): the slider, the top pancake, 2 falling pieces
P2_SWAP = {(226, 72, 66): (74, 146, 232), (150, 36, 44): (38, 84, 168), (92, 104, 138): (96, 146, 88),
           (56, 64, 94): (58, 98, 56)}


def cname(s):
    return s.upper().replace("-", "_")


def canvas_cells(cv, x0, y0, w, h):
    out = []
    for ty in range(h // 8):
        for tx in range(w // 8):
            c = []
            for y in range(8):
                row = []
                for x in range(8):
                    v = cv.p[y0 + ty * 8 + y][x0 + tx * 8 + x]
                    row.append(None if v is None else rsasset.to555(v))
                c.append(row)
            out.append(c)
    return out


def frame_cell(cv):
    return [[None if v is None else rsasset.to555(v) for v in row] for row in cv.p]


def arr(name, data, static=False):
    return rsasset.c_u16(name, data, static)


def pal555(rgbs):
    return [0] + [rsasset.to555(c) for c in rgbs[1:]]


def convert_near(cells, palettes, kitchen_cells):
    """The near scenery with its explicit palettes (make_art.NEAR_PALETTES): every 8x8 cell must fit one palette
    exactly (the first that holds all its colours; the first kitchen_cells, the title's scenery, only the first two).
    Tiles are shared with flips, like rsasset.convert_bg."""
    pals = [[rsasset.to555(c) for c in p] for p in palettes]
    for p in pals:
        assert len(p) <= 15 and len(set(p)) == len(p), "a near palette has %d colours (or a repeat)" % len(p)
    out = rsasset.BGSet()
    out.palettes = pals
    out.tiles.append([0] * 64)
    index = {tuple([0] * 64): (0, 0, 0)}
    bad = []
    for ci, c in enumerate(cells):
        cols = rsasset.colors_of(c)
        allowed = 2 if ci < kitchen_cells else len(pals)
        cand = [k for k, p in enumerate(pals[:allowed]) if cols <= set(p)]
        if not cand:
            bad.append((ci, sorted(rsasset.rgb888(v) for v in cols)))
            cand = [0]
        k = cand[0]
        meta = []
        for t in rsasset.split_tiles(c):
            idx = [0 if v is None else (pals[k].index(v) + 1 if v in pals[k] else 1) for v in t]
            key = tuple(idx)
            hit = index.get(key)
            if hit is None:
                for hf, vf, f in ((1, 0, rsasset.hflip), (0, 1, rsasset.vflip),
                                  (1, 1, lambda x: rsasset.vflip(rsasset.hflip(x)))):
                    k2 = tuple(f(idx))
                    if k2 in index:
                        base = index[k2]
                        hit = (base[0], hf ^ base[1], vf ^ base[2])
                        break
            if hit is None:
                out.tiles.append(idx)
                hit = (len(out.tiles) - 1, 0, 0)
                index[key] = hit
            meta.append(rsasset.rs_map(hit[0], k, 0, hit[1], hit[2]))
        out.metas.append(meta)
    for ci, cols in bad[:20]:
        seg, rest = divmod(ci, 32 * 40)
        print("near scenery: segment %d cell (%d, %d) fits no palette: %s" % (seg, rest % 40, rest // 40, cols),
              file=sys.stderr)
    assert not bad, "%d near cells do not fit the palettes" % len(bad)
    return out


def tuning():
    """the #define NAME number lines of src/tuning.h (the art and the game share the house's heights)"""
    out = {}
    for line in open(os.path.join(GAME, "src", "tuning.h"), encoding="utf-8"):
        f = line.split()
        if len(f) >= 3 and f[0] == "#define":
            try:
                out[f[1]] = int(f[2].strip("()"))
            except ValueError:
                pass
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--art", default="", help="ignored (the art is drawn in memory)")
    ap.add_argument("--tileset", default="", help="ignored")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    art = make_art.build()
    c = ["/* Generated by games/pancaketower/tools/build_assets.py from tools/make_art.py. Do not edit. */",
         '#include "assets.h"\n']
    h = ["/* Generated by games/pancaketower/tools/build_assets.py. Do not edit. */",
         "#ifndef PT_ASSETS_H\n#define PT_ASSETS_H\n#include <stdint.h>\n#include \"rs.h\"\n",
         "#define VR_BG1 %d\n#define VR_BG2 %d\n#define VR_BG3 %d\n#define VR_BG4 %d\n#define VR_OBJ %d" %
         (VR_BG1, VR_BG2, VR_BG3, VR_BG4, VR_OBJ),
         "#define BG2_TILES %d" % BG2_TILES,
         "#define PAL_TOWER %d\n#define PAL_TOPPING %d\n#define PAL_FAR %d\n#define PAL_LOGO %d" %
         (PAL_TOWER, PAL_TOPPING, PAL_FAR, PAL_LOGO),
         "#define OBJ_FOOD %d\n#define OBJ_CHEF %d\n#define OBJ_CHEF2 %d\n#define OBJ_KIT 3\n#define OBJ_TOPPING %d\n"
         "#define OBJ_FX %d\n#define OBJ_CRITTER %d" % tuple(OBJ_PAL[k] for k in ("food", "chef", "chef2", "topping", "fx", "critter"))]
    stats = []

    # ---- the fixed palettes of the run-time renderer (render.c) --------------------------------------------------
    c.append(arr("pt_tower_pal", pal555(make_art.TOWER_PAL)))
    c.append(arr("pt_topping_pal", pal555(make_art.TOPPING_PAL)))
    h.append("extern const uint16_t pt_tower_pal[16];\nextern const uint16_t pt_topping_pal[16];")
    for k, v in make_art.T.items():
        h.append("#define TC_%s %d" % (k.upper(), v))
    for k, v in make_art.P.items():
        h.append("#define PC_%s %d" % (k.upper(), v))

    # ---- BG3: the five near segments (one tile set, NEAR_PALS palettes) ------------------------------------------------
    cells = []
    for seg in art["near"]:
        cells += canvas_cells(seg, 0, 0, 320, 256)
    r = convert_near(cells, make_art.NEAR_PALETTES, kitchen_cells=40 * 32)
    # the holes: draw.c breaks the ceiling's and the roof's rows at run time (copies of these tiles with the hole cut
    # out, pixel-exact). The hole's inside is the KIT "dark" colour and its broken rim "wood3": every tile of those
    # rows must have them in its palette (a hole drawn in another colour would clash).
    tun = tuning()
    for name in ("CEILING_Y", "CEILING_TOP", "ROOF_Y", "ROOF_TOP", "RIDGE_Y", "FLOOR_Y"):
        assert getattr(make_art, name, tun[name]) == tun[name], "make_art.%s != tuning.h" % name
    void555, rim555 = rsasset.to555(make_art.KIT["dark"]), rsasset.to555(make_art.KIT["wood3"])
    near_void = [p.index(void555) + 1 if void555 in p else 0 for p in r.palettes]
    near_rim = [p.index(rim555) + 1 if rim555 in p else 0 for p in r.palettes]
    carve_rows = list(range(tun["CEILING_Y"], tun["CEILING_TOP"], 8)) + list(range(tun["ROOF_Y"], tun["ROOF_TOP"], 8))
    for wy in carve_rows:
        k = wy // 8
        seg, row = (wy + 64) // 256, 31 - ((k + 8) & 31)
        for col in range(40):
            e = r.metas[(seg * 32 + row) * 40 + col][0]
            pk = (e >> 10) & 7
            assert near_void[pk] and near_rim[pk], "the hole's colours are not in near palette %d (row %d)" % (pk, wy)
    for m in r.metas:                     # palette k -> BG palette NEAR_PAL_SLOTS[k]
        for i, e in enumerate(m):
            m[i] = (e & ~(7 << 10)) | (NEAR_PAL_SLOTS[(e >> 10) & 7] << 10)
    assert len(r.tiles) <= BG3_TILES, "near scenery: %d tiles > %d" % (len(r.tiles), BG3_TILES)
    c.append(rsasset.c_bytes("pt_near_tiles", rsasset.tiles_bytes(r.tiles)))
    c.append("const int pt_near_tile_count = %d;" % len(r.tiles))
    c.append(arr("pt_near_map", [m[0] for m in r.metas]))
    pals = []
    for p in r.palettes:
        pals += rsasset.palette16(p)
    pals += [0] * (16 * NEAR_PALS - len(pals))
    c.append(arr("pt_near_pals", pals))
    h.append("#define NEAR_SEGS %d\n#define NEAR_W 40\n#define NEAR_H 32\n#define NEAR_PALS %d" % (len(art["near"]), NEAR_PALS))
    h.append("#define NEAR_PAL_SLOTS {%s}" % ", ".join(str(s) for s in NEAR_PAL_SLOTS))
    h.append("#define NEAR_VOID {%s}\n#define NEAR_RIM {%s}" % (", ".join(map(str, near_void)), ", ".join(map(str, near_rim))))
    h.append("#define BG3_TILES %d" % BG3_TILES)
    h.append("#define CHIMNEY_X %d\n#define CHIMNEY_TOP %d   /* the chimney's flue (panorama x, world y) */" %
             (make_art.CHIMNEY_X, make_art.CHIMNEY_TOP))
    h.append("extern const uint8_t pt_near_tiles[];\nextern const int pt_near_tile_count;")
    h.append("extern const uint16_t pt_near_map[NEAR_SEGS * NEAR_H * NEAR_W];\nextern const uint16_t pt_near_pals[16 * NEAR_PALS];")
    stats.append("BG3 %d (%d pals)" % (len(r.tiles), len(r.palettes)))

    # ---- BG4: the far layer -----------------------------------------------------------------------------------------------
    far = art["far"]
    r = rsasset.convert_bg(canvas_cells(far, 0, 0, 256, far.h), 1, pal_base=PAL_FAR, tile_base=0)
    assert len(r.tiles) <= BG4_TILES, "far scenery: %d tiles > %d" % (len(r.tiles), BG4_TILES)
    c.append(rsasset.c_bytes("pt_far_tiles", rsasset.tiles_bytes(r.tiles)))
    c.append("const int pt_far_tile_count = %d;" % len(r.tiles))
    c.append(arr("pt_far_map", [m[0] for m in r.metas]))
    c.append(arr("pt_far_pal", rsasset.palette16(r.palettes[0])))
    h.append("#define FAR_W 32\n#define FAR_H %d\n#define FAR_REPEAT 32" % (far.h // 8))
    h.append("extern const uint8_t pt_far_tiles[];\nextern const int pt_far_tile_count;")
    h.append("extern const uint16_t pt_far_map[FAR_H * FAR_W];\nextern const uint16_t pt_far_pal[16];")
    stats.append("BG4 %d" % len(r.tiles))

    # ---- sprites ----------------------------------------------------------------------------------------------------------
    fixed = {"food": [rsasset.to555(x) for x in make_art.TOWER_PAL[1:]],
             "topping": [rsasset.to555(x) for x in make_art.TOPPING_PAL[1:]]}
    tiles, rows, names, obj_pals = [], [], [], [0] * 128
    idx = 0
    for g in ("food", "topping", "chef", "fx", "critter", "medal"):
        ents = [e for e in art["sprites"] if e[1] == g]
        frames = [frame_cell(f) for e in ents for f in e[2]]
        ro = rsasset.convert_obj(frames, fixed.get(g))
        p16 = rsasset.palette16(ro.palette)
        pal = OBJ_PAL[g]
        obj_pals[pal * 16:pal * 16 + 16] = p16
        if g == "chef":
            p2 = [0]
            for v in p16[1:]:
                rgb = rsasset.rgb888(v)
                hit = [n for o, n in P2_SWAP.items() if rsasset.to555(o) == v]
                p2.append(rsasset.to555(hit[0]) if hit else v)
            obj_pals[OBJ_PAL["chef2"] * 16:OBJ_PAL["chef2"] * 16 + 16] = p2
        k = 0
        for name, _g, fr in ents:
            names.append("SPR_%s = %d" % (cname(name), idx))
            for f in fr:
                first, w, hh = ro.frames[k]
                rows.append("{%d, %d, %d, %d}" % (len(tiles) + first, w, hh, pal))
                k += 1
                idx += 1
        tiles += ro.tiles
        stats.append("%s %d" % (g, len(ro.tiles)))
    dyn = len(tiles)
    kit = dyn + OBJ_DYN_TILES
    h.append("enum { %s, SPR_COUNT = %d };" % (", ".join(names), idx))
    h.append("typedef struct pt_sprite_def { uint16_t tile; uint8_t w, h, pal; } pt_sprite_def;")
    h.append("extern const pt_sprite_def pt_spr[SPR_COUNT];")
    h.append("extern const uint8_t pt_obj_tiles[];\nextern const int pt_obj_tile_count;\nextern const uint16_t pt_obj_pals[128];")
    h.append("#define OBJ_DYN_TILE %d\n#define OBJ_DYN_TILES %d\n#define KIT_OBJ_TILE %d" % (dyn, OBJ_DYN_TILES, kit))
    c.append("const pt_sprite_def pt_spr[SPR_COUNT] = {\n    " + ",\n    ".join(rows) + "};")
    c.append(rsasset.c_bytes("pt_obj_tiles", rsasset.tiles_bytes(tiles)))
    c.append("const int pt_obj_tile_count = %d;" % len(tiles))
    c.append(rsasset.c_u16("pt_obj_pals", obj_pals))
    stats.append("OBJ %d + %d run-time" % (len(tiles), OBJ_DYN_TILES))

    # ---- asset pack: music --------------------------------------------------------------------------------------------------
    music_dir = os.path.join(a.out, "music")
    subprocess.check_call([sys.executable, os.path.join(HERE, "make_music.py"), music_dir])
    pack = [("music/" + os.path.basename(p), p) for p in sorted(glob.glob(os.path.join(music_dir, "*.mod")))]
    entries = []
    for i, (name, path) in enumerate(pack):
        data = open(path, "rb").read()
        c.append(rsasset.c_bytes("pack_%d" % i, data, static=True))
        entries.append('    {"%s", pack_%d, %d}' % (name, i, len(data)))
    c.append("const rs_asset_entry pt_assets[] = {\n" + ",\n".join(entries) + ",\n    {0, 0, 0}};")
    h.append("extern const rs_asset_entry pt_assets[];\n#endif")

    open(os.path.join(a.out, "assets.c"), "w").write("\n".join(c) + "\n")
    open(os.path.join(a.out, "assets.h"), "w").write("\n".join(h) + "\n")
    print("assets: tiles %s, %d pack files -> %s" % (", ".join(stats), len(pack), a.out))


if __name__ == "__main__":
    main()

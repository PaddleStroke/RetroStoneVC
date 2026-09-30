#!/usr/bin/env python3
"""Bomber Mole asset build: sheets -> tiles/palettes/metatiles/sprites, levels
and music -> build/gen/bombermole/assets.c + assets.h.

    build_assets.py --out build/gen/bombermole [--art games/bombermole/art]

All rights reserved, 8BCraft.
"""
import argparse
import glob
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.dirname(HERE)
ROOT = os.path.abspath(os.path.join(GAME, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import sheets  # noqa: E402
import rsasset  # noqa: E402
import palette_variants  # noqa: E402

TERRAIN_PALS = 4          # BG palettes 1..4
PAL_PROPS = 5             # BG palette of the terrain-like props
PAL_FX, PAL_HUD = 6, 7    # BG palettes
PROPS_TILE_BASE = 192     # relative to the playfield layers' tile base
FX_TILE_BASE = 384
HUD_TILE_BASE = 512       # relative to BG1's tile base (the font is at 0)
OBJ_GROUPS = ["mole", "ferret", "cat", "boss", "bomb", "pickup", "prop", "critter"]  # = OBJ palettes 0-7
BOSSES = ["boss_cat", "boss_farmer", "boss_badger", "boss_fox", "boss_owl"]  # loaded into OBJ palette 3


def cname(s):
    return s.upper().replace("-", "_").replace(".", "_")


def frames_of(img, e, season=0):
    return [rsasset.cell(img, x, y, w, h) for (x, y, w, h) in sheets.frame_rects(e, season)]


def grass_shadow(grass, rows=(0.55, 0.72, 0.88)):
    """Grass below a wall: the top rows darkened, each pixel snapped to the nearest colour
    of the grass tile itself (no new colour, so the season's palettes are unchanged). Tiles
    along a wall repeat seamlessly, whatever the art of the grass_edge cell looks like."""
    cols = sorted({v for row in grass for v in row if v is not None})
    rgb = [rsasset.rgb888(v) for v in cols]
    out = [list(r) for r in grass]
    for y, k in enumerate(rows):
        for x, v in enumerate(out[y]):
            if v is not None:
                r, g, b = rsasset.rgb888(v)
                out[y][x] = cols[rsasset.nearest((r * k, g * k, b * k), rgb)]
    return out


HILL_CELLS = 8          # the title hills repeat every 8 cells (128 px)


def _close(a, b, tol=60):
    ca, cb = rsasset.rgb888(a), rsasset.rgb888(b)
    return sum(abs(p - q) for p, q in zip(ca, cb)) <= tol


def key_background(cell):
    """Objects are drawn over the ground (sheets.is_overlay): an opaque cell (AI art drawn on its own
    grass or tunnel floor) gets its background removed. The background is the dominant colour of the
    cell's border, flood-filled from the border; nothing is removed when the middle of the cell has
    that colour too (a block that fills its cell, like stone bricks). A 1-px dithered contact shadow
    is added under the object. A cell with transparent pixels is returned as it is."""
    h, w = len(cell), len(cell[0])
    if any(v is None for row in cell for v in row):
        return cell
    border = [(x, y) for y in range(h) for x in range(w) if x in (0, w - 1) or y in (0, h - 1)]
    cols = [cell[y][x] for x, y in border]
    best, bn = None, 0
    for v in set(cols):
        n = sum(1 for u in cols if _close(u, v))
        if n > bn:
            best, bn = v, n
    if bn < 0.35 * len(cols):
        return cell
    centre = [cell[y][x] for y in range(h // 2 - 3, h // 2 + 3) for x in range(w // 2 - 3, w // 2 + 3)]
    if sum(1 for u in centre if _close(u, best)) > 0.3 * len(centre):
        return cell
    out = [list(r) for r in cell]
    todo = [(x, y) for x, y in border if _close(cell[y][x], best)]
    seen = set(todo)
    while todo:
        x, y = todo.pop()
        out[y][x] = None
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if 0 <= nx < w and 0 <= ny < h and (nx, ny) not in seen and _close(cell[ny][nx], best):
                seen.add((nx, ny))
                todo.append((nx, ny))
    if len(seen) > 0.85 * w * h:
        return cell
    opaque = [v for row in out for v in row if v is not None]
    dark = min(opaque, key=lambda v: sum(rsasset.rgb888(v)))
    for x in range(w):
        ys = [y for y in range(h) if out[y][x] is not None]
        if ys and ys[-1] + 1 < h and (x + ys[-1]) % 2 == 0:
            out[ys[-1] + 1][x] = dark
    return out


def mound(cell):
    """A soil block as a rounded mound (no art for it): the cell masked by an ellipse."""
    out = [list(r) for r in cell]
    for y in range(16):
        for x in range(16):
            if ((x - 7.5) / 7.6) ** 2 + ((y - 8.5) / 7.2) ** 2 > 1.0:
                out[y][x] = None
    return out


def hill_top(X):
    """Height of the hill surface (pixels from the top of the cell) at x = X: rounded humps."""
    import math
    return 2 + int(round(10 * (1 - math.sin(math.pi * (X % (HILL_CELLS * 16)) / (HILL_CELLS * 16)) ** 0.8)))


def title_hills(grass):
    """Top cells of the title-screen hills, drawn from the season's grass (no dirt): transparent sky
    above a rounded silhouette, a 1-px dark outline, a 2-px lighter rim, then the grass texture.
    Colours are snapped to the grass tile's own colours (the palettes do not change)."""
    cols = sorted({v for row in grass for v in row if v is not None})
    rgb = [rsasset.rgb888(v) for v in cols]
    lum = sorted(cols, key=lambda v: sum(rsasset.rgb888(v)))
    dark, light = lum[0], lum[-1]
    cells = []
    for i in range(HILL_CELLS):
        cell = [[None] * 16 for _ in range(16)]
        for x in range(16):
            top = hill_top(i * 16 + x)
            for y in range(16):
                if y < top - 1:
                    continue
                if y == top - 1:
                    cell[y][x] = dark
                elif y < top + 2:
                    r, g, b = rsasset.rgb888(grass[y][x] if grass[y][x] is not None else light)
                    cell[y][x] = cols[rsasset.nearest((min(255, r * 1.3), min(255, g * 1.3), min(255, b * 1.3)), rgb)]
                    if y == top:
                        cell[y][x] = light
                else:
                    cell[y][x] = grass[y][x]
        cells.append(cell)
    return cells


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--art", default=os.path.join(GAME, "art"))
    ap.add_argument("--tileset", default="",
                    help="folder with tiles.png (+ tiles_extra.png) replacing the art's tiles.png")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    img = {k: rsasset.load(os.path.join(a.art, v["file"])) for k, v in sheets.SHEETS.items()}
    extra_img = None
    if a.tileset and not os.path.exists(os.path.join(a.tileset, "tiles.png")):
        print("build_assets: no tileset in %s (not generated yet?): using the art's tiles.png" % a.tileset)
        a.tileset = ""
    if a.tileset:
        img["tiles"] = rsasset.load(os.path.join(a.tileset, "tiles.png"))
        p = os.path.join(a.tileset, "tiles_extra.png")
        extra_img = rsasset.load(p) if os.path.exists(p) else None
    # a tileset may also redraw some props and sprites (tilesets/<name>/<entry>.png, frames side by side)
    prop_override = {}
    keep = set()                  # transparent AI overlays imported by art_sync: no tileset redraws them
    ov = os.path.join(a.art, "overlays.txt")
    if os.path.exists(ov):
        keep = set(open(ov, encoding="utf-8").read().split())
    if a.tileset:
        for e in sheets.ENTRIES:
            q = os.path.join(a.tileset, e.name + ".png")
            if e.sheet == "props" and os.path.exists(q) and e.name not in keep:
                prop_override[e.name] = rsasset.load(q)
    c, h = [], []
    c.append("/* Generated by games/bombermole/tools/build_assets.py from %s (tiles: %s). Do not edit. */" %
             (os.path.relpath(a.art, ROOT), os.path.relpath(a.tileset, ROOT) if a.tileset else "the art's"))
    c.append('#include "assets.h"\n')
    h.append("/* Generated by games/bombermole/tools/build_assets.py. Do not edit. */")
    h.append("#ifndef BM_ASSETS_H\n#define BM_ASSETS_H\n#include <stdint.h>\n#include \"rs.h\"\n")
    h.append("#define TERRAIN_PALS %d\n#define PAL_FX %d\n#define PAL_HUD %d" % (TERRAIN_PALS, PAL_FX, PAL_HUD))
    h.append("#define FX_TILE_BASE %d\n#define HUD_TILE_BASE %d\n#define PROPS_TILE_BASE %d\n#define PAL_PROPS %d\n" %
             (FX_TILE_BASE, HUD_TILE_BASE, PROPS_TILE_BASE, PAL_PROPS))

    # ---- terrain (one tile set per season) --------------------------------------------------
    terr = sheets.entries("tiles")
    # + T_GRASS_SHADOW: grass with the shadow of the wall above it, derived from each season's grass
    h.append("enum { " + ", ".join("T_%s" % cname(e.name) for e in terr) +
             ", T_GRASS_SHADOW, T_TUNNEL_SHADOW, " + ", ".join("T_%s" % cname(n) for n, _ in sheets.TILE_EXTRAS) +
             ", T_HILL, T_COUNT = T_HILL + %d };" % HILL_CELLS)
    h.append("#define TILESET_HAS_EXTRAS %d" % (extra_img is not None))
    h.append("extern const uint8_t *const bm_terrain_tiles[4];\nextern const int bm_terrain_tile_count[4];")
    h.append("extern const uint16_t bm_terrain_pals[4][TERRAIN_PALS * 16];")
    h.append("extern const uint16_t bm_terrain_meta[4][T_COUNT][4];\n")
    metas, pals, counts = [], [], []
    for s in range(4):
        cells = [frames_of(img["tiles"], e, s)[0] for e in terr]
        cells = [key_background(c) if sheets.is_overlay(e.name) else c for c, e in zip(cells, terr)]
        names = [e.name for e in terr]
        grass = cells[names.index("grass")]
        cells.append(grass_shadow(grass))
        cells.append(grass_shadow(cells[names.index("tunnel")]))     # T_TUNNEL_SHADOW
        for i, (name, base) in enumerate(sheets.TILE_EXTRAS):
            ex = rsasset.cell(extra_img, i * 16, s * 16, 16, 16)                 if extra_img is not None and (i + 1) * 16 <= extra_img.width else None
            if ex is not None and any(v is not None for row in ex for v in row):
                cells.append(ex)
            elif name.startswith("dirt_mound"):   # no art: the soil block cut to a mound
                cells.append(mound(cells[names.index(base)]))
            elif name == "water_f2":          # no extras: the water shifted by 4 pixels
                w = cells[names.index("water")]
                cells.append([row[4:] + row[:4] for row in w])
            elif name == "water_edge":
                cells.append(grass_shadow(cells[names.index("water")]))
            else:
                cells.append(cells[names.index(base)])
        cells += title_hills(grass)             # T_HILL + 0..7: title-screen hill tops
        r = rsasset.convert_bg(cells, TERRAIN_PALS, pal_base=1, tile_base=0)
        if len(r.tiles) > PROPS_TILE_BASE:
            sys.exit("terrain of season %d needs %d tiles (max %d)" % (s, len(r.tiles), PROPS_TILE_BASE))
        c.append(rsasset.c_bytes("terrain_tiles_%d" % s, rsasset.tiles_bytes(r.tiles), static=True))
        counts.append(len(r.tiles))
        p = []
        for i in range(TERRAIN_PALS):
            p += rsasset.palette16(r.palettes[i] if i < len(r.palettes) else [])
        pals.append(p)
        metas.append(r.metas)
    c.append("const uint8_t *const bm_terrain_tiles[4] = {terrain_tiles_0, terrain_tiles_1, terrain_tiles_2, terrain_tiles_3};")
    c.append("const int bm_terrain_tile_count[4] = {%s};" % ", ".join(map(str, counts)))
    c.append("const uint16_t bm_terrain_pals[4][TERRAIN_PALS * 16] = {\n" +
             ",\n".join("    {" + ", ".join("0x%04x" % v for v in p) + "}" for p in pals) + "};")
    c.append("const uint16_t bm_terrain_meta[4][T_COUNT][4] = {\n" + ",\n".join(
        "    {" + ", ".join("{" + ", ".join("0x%04x" % v for v in m) + "}" for m in ms) + "}" for ms in metas) + "};\n")

    # ---- terrain-like props: one BG palette per colour family (sheets.PROP_PALETTES) -------------
    # The metatiles say palette PAL_PROPS; the game sets the palette bits per level to the slot
    # where the prop's family is loaded (BG palette 5, or 7 below the HUD band).
    pents = [e for e in sheets.entries("props") if e.group == "propbg"]
    names, fam, pcells = [], [], []
    for e in pents:
        names.append("PB_%s = %d" % (cname(e.name), len(fam)))
        fr = frames_of(prop_override.get(e.name, img["props"]), e) if e.name not in prop_override else \
            [rsasset.cell(prop_override[e.name], i * e.w, 0, e.w, e.h) for i in range(e.frames)]
        pcells += [key_background(c) if sheets.is_overlay(e.name) else c for c in fr]
        fam += [sheets.prop_palette(e.name)] * e.frames
    ptiles, ppals, pmetas = [], [], [None] * len(fam)
    for f in range(len(sheets.PROP_PALETTES)):
        ks = [k for k in range(len(fam)) if fam[k] == f]
        if not ks:
            ppals.append([])
            continue
        r = rsasset.convert_bg([pcells[k] for k in ks], 1, pal_base=PAL_PROPS,
                               tile_base=PROPS_TILE_BASE + len(ptiles), reserve_blank=not ptiles)
        ptiles += r.tiles
        ppals.append(r.palettes[0] if r.palettes else [])
        for k, m in zip(ks, r.metas):
            pmetas[k] = m
    if len(ptiles) > FX_TILE_BASE - PROPS_TILE_BASE:
        sys.exit("terrain-like props need %d tiles (max %d)" % (len(ptiles), FX_TILE_BASE - PROPS_TILE_BASE))
    h.append("enum { %s, PB_COUNT = %d };" % (", ".join(names), len(fam)))
    h.append("enum { %s, PB_PALS };" % ", ".join("PBP_%s" % cname(n) for n, _ in sheets.PROP_PALETTES))
    h.append("extern const uint8_t bm_propbg_tiles[];\nextern const int bm_propbg_tile_count;")
    h.append("extern const uint16_t bm_propbg_pals[PB_PALS][16];\nextern const uint8_t bm_propbg_family[PB_COUNT];")
    h.append("extern const uint16_t bm_propbg_meta[PB_COUNT][4];\n")
    c.append(rsasset.c_bytes("bm_propbg_tiles", rsasset.tiles_bytes(ptiles)))
    c.append("const int bm_propbg_tile_count = %d;" % len(ptiles))
    c.append("const uint16_t bm_propbg_pals[PB_PALS][16] = {\n" + ",\n".join(
        "    {" + ", ".join("0x%04x" % v for v in rsasset.palette16(p)) + "}" for p in ppals) + "};")
    c.append("const uint8_t bm_propbg_family[PB_COUNT] = {%s};" % ", ".join(map(str, fam)))
    c.append("const uint16_t bm_propbg_meta[PB_COUNT][4] = {\n" + ",\n".join(
        "    {" + ", ".join("0x%04x" % v for v in m) + "}" for m in pmetas) + "};\n")

    # ---- BG groups from items_fx: explosions and HUD ------------------------------------------
    for group, prefix, pal, base, sheet in (("fx", "FX", PAL_FX, FX_TILE_BASE, "items_fx"),
                                            ("hud", "HUD", PAL_HUD, HUD_TILE_BASE, "items_fx")):
        ents = [e for e in sheets.entries(sheet) if e.group == group]
        cells, names, idx = [], [], 0
        for e in ents:
            names.append("%s_%s = %d" % (prefix, cname(e.name.replace("expl_", "").replace("hud_", "")), idx))
            cells += frames_of(img[sheet], e)
            idx += e.frames
        r = rsasset.convert_bg(cells, 1, pal_base=pal, tile_base=base)
        h.append("enum { %s, %s_COUNT = %d };" % (", ".join(names), prefix, idx))
        h.append("extern const uint8_t bm_%s_tiles[];\nextern const int bm_%s_tile_count;" % (group, group))
        h.append("extern const uint16_t bm_%s_pal[16];\nextern const uint16_t bm_%s_meta[%s_COUNT][4];\n" %
                 (group, group, prefix))
        c.append(rsasset.c_bytes("bm_%s_tiles" % group, rsasset.tiles_bytes(r.tiles)))
        c.append("const int bm_%s_tile_count = %d;" % (group, len(r.tiles)))
        c.append(rsasset.c_u16("bm_%s_pal" % group, rsasset.palette16(r.palettes[0])))
        c.append("const uint16_t bm_%s_meta[%s_COUNT][4] = {\n" % (group, prefix) + ",\n".join(
            "    {" + ", ".join("0x%04x" % v for v in m) + "}" for m in r.metas) + "};\n")

    # ---- sprites ------------------------------------------------------------------------------
    # Base sprites share one tile block (always in VRAM). Each boss has its
    # own block + palette, loaded into the boss slot / OBJ palette 3 per level.
    sprite_entries = [e for e in sheets.ENTRIES if e.group in OBJ_GROUPS or e.group in BOSSES]
    tiles, defs, obj_pals, names = [], {}, [], []
    boss_tiles, boss_pals = [], []
    variant_base = {}
    for g in OBJ_GROUPS + BOSSES:
        ents = [e for e in sprite_entries if e.group == g and not sheets.mirror_source(e.name)]
        frames = []
        for e in ents:
            if e.name in prop_override:
                frames += [rsasset.cell(prop_override[e.name], i * e.w, 0, e.w, e.h) for i in range(e.frames)]
            else:
                frames += frames_of(img[e.sheet], e)
        nderived = 0
        if g == "prop":         # the vertical wind streaks: the wind frames turned a quarter (blowing down)
            wind = sheets.BY_NAME["wind"]
            for f in frames_of(img[wind.sheet], wind):
                frames.append([[f[x][y] for x in range(len(f))] for y in range(len(f[0]))])
                nderived += 1
        if not frames:                          # "boss" slot of the base palettes: filled per level
            obj_pals += [0] * 16
            continue
        r = rsasset.convert_obj(frames)
        if g in ("ferret", "cat"):          # base palettes of the palette-swap variants
            cnt = [0] * len(r.palette)
            for t in r.tiles:
                for v in t:
                    if v:
                        cnt[v - 1] += 1
            variant_base[g] = (r.palette, cnt)
        if g in BOSSES:
            block, pal, base = BOSSES.index(g) + 1, 3, 0
            boss_tiles.append(r.tiles)
            boss_pals.append(rsasset.palette16(r.palette))
        else:
            block, pal, base = 0, OBJ_GROUPS.index(g), len(tiles)
            obj_pals += rsasset.palette16(r.palette)
        k = 0
        for e in ents:
            defs[e.name] = []
            for f in range(e.frames):
                first, w, hh = r.frames[k]
                defs[e.name].append((base + first, w, hh, pal, block, 0))
                k += 1
        if nderived:
            defs["wind_v"] = []
            for f in range(nderived):
                first, w, hh = r.frames[k]
                defs["wind_v"].append((base + first, w, hh, pal, block, 0))
                k += 1
        if g not in BOSSES:
            tiles += r.tiles
    # mirrored right strips: the left frames with the h-flip bit (no tiles of their own)
    for e in sprite_entries:
        src = sheets.mirror_source(e.name)
        if src:
            defs[e.name] = [d[:5] + (1,) for d in defs[src]]
    spr_rows, idx = [], 0
    for name in [e.name for e in sprite_entries] + ["wind_v"]:
        names.append("SPR_%s = %d" % (cname(name), idx))
        for d in defs[name]:
            spr_rows.append("    {%d, %d, %d, %d, %d, %d}" % d)
            idx += 1
    h.append("enum { %s, SPR_COUNT = %d };" % (", ".join(names), idx))
    h.append("enum { " + ", ".join("OBJ_PAL_%s" % cname(g) for g in OBJ_GROUPS) + " };")
    h.append("enum { BOSS_NONE, " + ", ".join(cname(b) for b in BOSSES) + ", BOSS_KINDS };")
    h.append("typedef struct bm_sprite_def { uint16_t tile; uint8_t w, h, pal, block, hflip; } bm_sprite_def;")
    h.append("extern const bm_sprite_def bm_spr[SPR_COUNT];")
    h.append("extern const uint8_t bm_obj_tiles[];\nextern const int bm_obj_tile_count;")
    h.append("extern const uint16_t bm_obj_pals[%d];" % len(obj_pals))
    h.append("extern const uint8_t *const bm_boss_tiles[BOSS_KINDS];\nextern const int bm_boss_tile_count[BOSS_KINDS];")
    h.append("extern const uint16_t bm_boss_pals[BOSS_KINDS][16];\n")
    c.append("const bm_sprite_def bm_spr[SPR_COUNT] = {\n" + ",\n".join(spr_rows) + "};")
    c.append(rsasset.c_bytes("bm_obj_tiles", rsasset.tiles_bytes(tiles)))
    c.append("const int bm_obj_tile_count = %d;" % len(tiles))
    c.append(rsasset.c_u16("bm_obj_pals", obj_pals))
    for i, bt in enumerate(boss_tiles):
        c.append(rsasset.c_bytes("boss_tiles_%d" % i, rsasset.tiles_bytes(bt), static=True))
    c.append("const uint8_t *const bm_boss_tiles[BOSS_KINDS] = {0, %s};" %
             ", ".join("boss_tiles_%d" % i for i in range(len(boss_tiles))))
    c.append("const int bm_boss_tile_count[BOSS_KINDS] = {0, %s};" % ", ".join(str(len(b)) for b in boss_tiles))
    c.append("const uint16_t bm_boss_pals[BOSS_KINDS][16] = {{0}, %s};" % ", ".join(
        "{" + ", ".join("0x%04x" % v for v in p) + "}" for p in boss_pals))

    # ---- palette-swap enemy variants (one palette each, from the base sprite's ramps) ------------
    vnames, vpals = [], []
    for name, group, target, light in palette_variants.VARIANTS:
        pal, cnt = variant_base[group]
        vnames.append("VAR_%s" % cname(name))
        vpals.append(rsasset.palette16(palette_variants.variant_palette(pal, cnt, target, light)))
    h.append("enum { %s, VAR_COUNT };" % ", ".join(vnames))
    h.append("extern const uint16_t bm_variant_pals[VAR_COUNT][16];\n")
    c.append("const uint16_t bm_variant_pals[VAR_COUNT][16] = {\n" + ",\n".join(
        "    {" + ", ".join("0x%04x" % v for v in p) + "}" for p in vpals) + "};")

    # ---- title logo (art/title_logo.png, 256x64): BG tiles + map, one palette ------------------
    logo_path = os.path.join(a.art, "title_logo.png")
    h.append("#define LOGO_TILE_BASE 640\n#define LOGO_W 32\n#define LOGO_H 8")
    h.append("extern const uint8_t bm_logo_tiles[];\nextern const int bm_logo_tile_count;")
    h.append("extern const uint16_t bm_logo_pal[16];\nextern const uint16_t bm_logo_map[LOGO_W * LOGO_H];\n")
    if os.path.exists(logo_path):
        limg = rsasset.load(logo_path)
        cells = [rsasset.cell(limg, x * 8, y * 8, 8, 8) for y in range(8) for x in range(32)]
        r = rsasset.convert_bg(cells, 1, pal_base=5, tile_base=640)
        c.append(rsasset.c_bytes("bm_logo_tiles", rsasset.tiles_bytes(r.tiles)))
        c.append("const int bm_logo_tile_count = %d;" % len(r.tiles))
        c.append(rsasset.c_u16("bm_logo_pal", rsasset.palette16(r.palettes[0])))
        c.append(rsasset.c_u16("bm_logo_map", [m[0] for m in r.metas]))
    else:
        c.append("const uint8_t bm_logo_tiles[1];\nconst int bm_logo_tile_count = 0;")
        c.append("const uint16_t bm_logo_pal[16];\nconst uint16_t bm_logo_map[LOGO_W * LOGO_H];")

    # ---- asset pack: levels and music -------------------------------------------------------
    music_dir = os.path.join(a.out, "music")
    subprocess.check_call([sys.executable, os.path.join(HERE, "make_music.py"), music_dir])
    pack = []
    for p in sorted(glob.glob(os.path.join(GAME, "levels", "*.txt"))):
        pack.append(("levels/" + os.path.basename(p), p))
    for p in sorted(glob.glob(os.path.join(GAME, "arenas", "*.txt"))):     # battle arenas (multiplayer)
        pack.append(("arenas/" + os.path.basename(p), p))
    for p in sorted(glob.glob(os.path.join(music_dir, "*.mod"))):
        pack.append(("music/" + os.path.basename(p), p))
    entries = []
    for i, (name, path) in enumerate(pack):
        data = open(path, "rb").read()
        c.append(rsasset.c_bytes("pack_%d" % i, data, static=True))
        entries.append('    {"%s", pack_%d, %d}' % (name, i, len(data)))
    c.append("const rs_asset_entry bm_assets[] = {\n" + ",\n".join(entries) + ",\n    {0, 0, 0}};")
    h.append("extern const rs_asset_entry bm_assets[];\n#endif")

    open(os.path.join(a.out, "assets.c"), "w").write("\n".join(c) + "\n")
    open(os.path.join(a.out, "assets.h"), "w").write("\n".join(h) + "\n")
    print("assets: %d obj tiles, terrain %s tiles, %d pack files -> %s" %
          (len(tiles), counts, len(pack), a.out))


if __name__ == "__main__":
    main()

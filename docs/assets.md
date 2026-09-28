# Assets: sheet format and the asset tool

## The sheet format
A sheet is a PNG laid out on a grid of cells (usually 16x16).
- **Transparency**: magenta `#FF00FF`, or alpha below 128. Everything else is a colour.
- **Colours** are snapped to RGB555 (the console's colour format): 8-bit values lose their 3 low bits.
- **Background (BG) art** (terrain, HUD, explosions): each non-empty cell becomes a **metatile** of
  (w/8)x(h/8) map entries. A cell may use up to 15 colours + transparent.
- **Sprite (OBJ) art**: each cell (or multi-cell frame, such as 32x32) becomes one sprite frame of consecutive
  8x8 tiles in row-major order. All frames of one palette group share one 15-colour palette.
- Frames of an animation sit left to right. Bomber Mole's layout is in [art-sheets.md](art-sheets.md).

## `tools/rsasset.py`
Library (used by `games/bombermole/tools/build_assets.py`) and command line:

```
python3 tools/rsasset.py SHEET.png --grid 16x16 --mode bg  --name terrain --out terrain.c  --max-pals 4 --pal-base 1
python3 tools/rsasset.py SHEET.png --grid 16x16 --mode obj --name mole    --out mole.bin
```

| Option | Meaning |
|---|---|
| `--grid WxH` | cell size, multiples of 8 |
| `--mode bg` | metatiles: palette packing (up to `--max-pals` palettes of 15), duplicate tiles removed, also flipped ones |
| `--mode obj` | sprite frames: consecutive tiles, one shared palette (reduced to 15 colours by median cut if needed) |
| `--pal-base N` | first palette number written in the map entries (BG) |
| `--tile-base N` | added to the tile numbers in the map entries |
| `--out file.c` / `file.bin` | C arrays, or the RSA1 container |

**Palette packing (BG).** Each cell's colour set is placed into the palette whose union grows least (at most 15
colours). If the cells need more palettes than allowed, they are **clustered** by colour (k-means) into that many
groups and each group is reduced to 15 colours (median cut). Cells with more than 15 colours are reduced first.

**C output**: `<name>_tiles` (packed 4 bpp, 32 bytes per tile, high nibble = left pixel), `<name>_pals` (16 RGB555
entries per palette, entry 0 = transparent), `<name>_cells` (the map entries per cell for BG, or
`first_tile, width, height` per frame for OBJ), with counts.

**RSA1 binary** (little endian):

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | `RSA1` |
| 4 | 1 | kind: 0 = BG, 1 = OBJ |
| 5 | 1 | reserved |
| 6 | 2 | tile count |
| 8 | 2 | palette count |
| 10 | 2 | cell count |
| 12 | 2, 2 | cell width, cell height |
| 16 | 32 x tiles | tiles, packed 4 bpp |
| ... | 32 x palettes | palettes, 16 x u16 RGB555 |
| ... | | per cell: BG = (w/8)(h/8) u16 map entries; OBJ = u16 first tile, u16 width, u16 height |

Map entries use the SNES format `vhopppcc cccccccc` (docs/spec.md).

## The game's asset build
`games/bombermole/tools/build_assets.py` (run by `make`) converts the four sheets and the title logo, embeds the
level files and generates the placeholder music (`tools/make_music.py`, 4-channel MODs), and writes
`build/gen/bombermole/assets.c` and `assets.h`:
- terrain: one tile set and 4 palettes per season (BG palettes 1-4), metatiles `T_*`;
- props on the terrain layer (BG palette 5, `PB_*`), explosions (BG palette 6, `FX_*`), HUD (BG palette 7, `HUD_*`);
- sprites: one tile block and 8 palettes (`SPR_*`, `OBJ_PAL_*`), plus one block and palette per boss, loaded
  when that boss's level starts;
- the title logo (BG tiles, map and palette);
- the asset pack: `levels/*.txt`, `music/*.mod`.

Build with other art: `make ART=path/relative/to/games/bombermole` (e.g. a folder written by
`art_sync.py sync --out`), and with bigger characters: `make CHAR_SIZE=24`.

## Cutting AI images
- Whole sheets: `tools/cut_ai_sheet.py` ([art-sheets.md](art-sheets.md)).
- Strips (the normal workflow): `tools/art_sync.py` ([art-workflow.md](art-workflow.md)).

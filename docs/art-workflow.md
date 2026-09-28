# Art workflow: image agent, owner, game

Three parties share one folder: **`games/bombermole/art/incoming/`**
(on the owner's PC: `C:\Users\Pierre\Desktop\RetroStoneVC\games\bombermole\art\incoming\`).
The folder is versioned in git: the generated PNGs are part of the art history.

| Who | Does |
|---|---|
| Image agent | reads `TODO.md`, generates the rows marked **TODO** (and **REJECTED** ones, using the owner's note), writes `<ID>.png`, sets the row to **GENERATED** |
| Owner | looks at the images (and the previews), sets each row to **VALIDATED** or **REJECTED** + a note |
| `tools/art_sync.py` | imports the **VALIDATED** rows into the game sheets, writes `IMPORT_REPORT.md`; never edits `TODO.md` |

## The unit: one PNG per animation strip
- `mole_walk_left.png` = the 3 frames of that animation in one horizontal row; `tile_spring_grass.png` = one
  tile; `farmer.png` = the farmer's 8 frames of 32x32.
- Frames are evenly spaced, left to right, all the same size.
- Flat magenta `#FF00FF` background. Tiles fill their whole cell, with no magenta inside.
- Drawn at **8x** (a 16x16 frame about 128x128). The title logo is 256x64 drawn at 4x (1024x256); it is
  scaled down as a whole, not cut into frames.
- Regenerating a row: the old `<ID>.png` is kept as `<ID>.v1.png`, `<ID>.v2.png`...

## TODO.md
Created by `python3 tools/art_sync.py todo` from the sheet layout (`tools/sheets.py`) and the art brief
(`games/bombermole/tools/art_brief.py`). It has the instructions for the image agent, the **style guide**
(SNES 16-bit, 3/4 top-down like Super Bomberman, dark outlines, 15 colours, light from the top-left), the
**model sheet** (mole, ferret, cat, bosses, dog), the **season palettes**, and one row per strip:

`| ID | Status | Sheet | Frame size | Frames | Description / prompt | Notes |`

The description is a self-contained prompt fragment (subject, direction, the content of each frame). The rows
are grouped by priority: what spring levels 1-8 need first. Running `todo` again adds rows for new strips
and never changes an existing Status or Notes cell. Today: **169 rows**.

Statuses: `TODO`, `GENERATED`, `VALIDATED`, `REJECTED`. The image agent must never write VALIDATED.

## Commands
```
python3 tools/art_sync.py todo                       # create / extend TODO.md
python3 tools/art_sync.py sync --dry-run             # what would be imported
python3 tools/art_sync.py sync                       # import VALIDATED rows -> games/bombermole/art/*.png
make                                                  # rebuild the game with the new sheets
python3 tools/art_sync.py preview                    # build/art-preview: contact_sheet.png + one GIF per strip (4x)
python3 tools/art_sync.py preview --include-generated  # the same with GENERATED rows too (review before validating)
python3 tools/art_sync.py sync --include-generated --out build/art-gen --report build/art-gen/report.md
make ART=../../build/art-gen                          # play with the generated art without validating it
```
`make art` runs `sync`.

## What sync does
For every **VALIDATED** row:
1. checks that `<ID>.png` exists (else: *missing*);
2. finds the frames on the almost-magenta background (connected blobs; sparks, dust or stars near a drawing
   belong to it; stray pixels are dropped). If the blob count differs from the row's frame count, it tries
   an even split of the width; if a drawing straddles a split line it reports *frame-count mismatch*;
3. despills: pink anti-aliasing and pink halos take the colour of the nearest clean pixel;
4. scales each frame into its cell: one scale per palette group (so a character keeps its size across its
   animations; the biggest frame of the group fills its cell), anchored bottom-centre for characters,
   centred otherwise, stuck to the edge for explosion pieces; tiles fill the cell; area filter by default;
5. reduces the colours per palette group (15, or 60 per season for terrain) and reports
   *colours reduced from N to M*;
6. assembles `characters.png`, `tiles.png`, `items_fx.png`, `props.png` and `title_logo.png`: validated art
   replaces the placeholder cell by cell, every other cell keeps its placeholder, so the game always builds.

The report (`IMPORT_REPORT.md`, next to TODO.md) lists every row taken: imported (frames, scale, warnings,
colours) / missing / error.

## The first batch
The owner's first three AI sheets (whole sheets, made before this workflow) are in `incoming/first-batch/`.
They were cut once into strips with `art_sync.py import-sheet` and the map files next to them
(`characters.map`, `tiles.map`, `items_fx.map`: the strip IDs in reading order), which wrote 107 strips
(`incoming/<ID>.png`) and marked those rows **GENERATED** with the note "from first-batch sheet".
Nothing is validated yet, so the game still builds with the placeholders.

## Character size
The owner's characters are richly shaded, so they may read better above 16x16. The engine takes 16, 24 or 32
px characters on the 16-px grid (bottom-centre anchor, overlapping upwards): `make CHAR_SIZE=24`. Comparison
images in `docs/art-preview/`:
- `character_size_16_4x.png`, `_24_`, `_32_`: mole, ferret, cat, boss at each size with nearest, area, and
  area + palette snap, at 4x (`tools/art_compare.py`);
- `character_size_side_by_side_2x.png`: the three sizes side by side;
- `character_size_ingame_1x.png` / `_2x.png`: real game builds at each size with the first-batch art, on
  spring 3, spring 8 (boss) and spring 2 underground (`tools/art_compare_ingame.sh`).

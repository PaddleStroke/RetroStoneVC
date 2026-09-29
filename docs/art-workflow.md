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
python3 tools/art_review.py                          # the owner's review tool: http://localhost:8765 (below)
python3 tools/art_sync.py todo                       # create / extend TODO.md
python3 tools/art_sync.py sync --dry-run             # what would be imported
python3 tools/art_sync.py sync                       # import VALIDATED rows -> games/bombermole/art/*.png
make                                                  # rebuild the game with the new sheets
make preview                                          # dist/windows/BomberMole-preview.exe with ALL generated art
python3 tools/art_consistency.py --verbose           # the consistency pass alone: per-family summary and flags
python3 tools/art_sync.py preview --include-generated  # build/art-preview: contact_sheet.png + one GIF per strip (4x)
```
`make art` runs `sync`. The characters are 24x24 and the bosses 48x48 (the owner's reference): build with
`make CHAR_SIZE=24` after `art_sync.py sync --char-size 24` (`make preview` does both).

## Reviewing: tools/art_review.py
A tiny local web server (Python standard library; the image work is `tools/art_consistency.py`). Run it from
the checkout whose `TODO.md` is live, the one the image agent writes (`C:\Users\Pierre\Desktop\RetroStoneVC`):
```
wsl.exe -d Ubuntu-24.04 -- sh -c "cd /mnt/c/Users/Pierre/Desktop/RetroStoneVC && python3 tools/art_review.py"
```
then open **http://localhost:8765** in Windows (Ctrl+C stops it; `--port`, `--char-size 24`, `--incoming DIR`).
Per TODO row: the original AI strip, the processed frames at 1x and 4x, the animation at game size and at 4x,
the sprite on a real level screenshot (click to move it; tiles are shown repeated so seams show), the
palette, the consistency flags and notes, and **Validate / Reject (note) / Reset**. Filters per family and
status, "flagged only", a **family view** (every animation of one character side by side, same zoom) and
**Validate all unflagged in this family** (asks for confirmation). **Reprocess** re-runs the pass after the
image agent regenerated something (the page says when PNGs changed).

A button writes only that row's Status and Notes cells (the owner's note is appended as `Owner: ...`; Reset
sets the row back to GENERATED, or TODO without a PNG). TODO.md is re-read just before every write and the
edit re-applied if the image agent saved in the meantime; the write is atomic and the previous version is kept
in `build/art-review/backups/`.

## What sync does: the consistency pass (tools/art_consistency.py)
For every **VALIDATED** row (with `--include-generated`, GENERATED rows too):
1. checks that `<ID>.png` exists (else: *missing*);
2. cuts the frames (`tools/art_frames.py`): the background may be flat or noisy magenta, transparent (alpha,
   soft edges cut at 50%), both, or absent (an opaque tile); pink fringes are despilled. Big blobs are
   drawings, small ones (dirt, sparks, stars, a thrown tomato) are effects attached to the nearest frame;
   drawings overlapping in x form one frame; a frame drawn in pieces (a sprinkler and its detached spray) is
   joined only when its inner gap is clearly smaller than the gaps between frames; touching frames are split
   on an even grid only when no drawing straddles a line. Otherwise: *frame-count mismatch: found N
   drawing(s), the TODO row expects M* and the row is not imported (never a guess);
3. scale per family (mole_*, ferret_*, cat_*, dog_*, the barn cat, farmer, fox, owl, badger): every strip is
   normalised against the reference frame (`<family>_walk_down` frame 1) on the body without its effects
   (the largest blob): front/back walks by body height, side views and poses by body area (strips within
   10% are left alone; beyond x0.6-x1.6 it is flagged). The family scale makes the walking and digging bodies
   fit the cell; a pose that would not fit shrinks its own strip (noted);
4. anchoring: characters bottom-centre on the body (feet on the bottom row), items centred, explosion pieces
   stuck to their cell edge, tiles and terrain props fill the cell (holes filled);
5. one palette per palette group (15 colours + transparent, median cut over every strip of the group, each
   strip weighted the same; terrain: 60 per season row = the 4 terrain palettes); each game pixel takes the
   palette colour covering most of it (crisp pixel art, no blended colours);
6. a uniform 1-px dark outline is redrawn around every sprite (not on explosions, tiles, spray, wind, steam,
   shadows or specks under 10 px);
7. flags (in the report, the review tool and `art_consistency.py --verbose`): frames whose silhouette area,
   height or width (walks and digs) or colour histogram deviate from the family median, frame-to-frame
   jitter (walks, digs, bomb, grub, windmill, tomato, the bosses' idle pair), drawings cropped by the cell,
   strips drawn at a very different scale, tiles that do not repeat seamlessly (grass, dirt, tunnel, water,
   ice, mud, tall grass, corn, burnt ground, thin ice, rails);
8. assembles `characters.png`, `tiles.png`, `items_fx.png`, `props.png` and `title_logo.png`: imported art
   replaces the placeholder cell by cell, every other cell keeps its placeholder, so the game always builds.

The report (`IMPORT_REPORT.md`, next to TODO.md) lists every row taken: imported (frames, AI scale,
normalisation, flags) / missing / error. `sync --legacy-import` runs the old import (one scale per group).

## Playing with the AI art before validating it
`make preview` imports every generated strip whatever its status (`art_sync.py sync --include-generated
--char-size 24 --out build/art-preview-24`), builds `dist/windows/BomberMole-preview.exe` with 24-px
characters, and writes in-game screenshots to `docs/art-preview/ingame-ai-*.png` (title, spring surface,
underground, the farmer room, winter). The normal build keeps validated art + placeholders.

## The first batch
The owner's first three AI sheets (whole sheets, made before this workflow) are in `incoming/first-batch/`.
They were cut once into strips with `art_sync.py import-sheet` and the map files next to them
(`characters.map`, `tiles.map`, `items_fx.map`: the strip IDs in reading order), which wrote 107 strips
(`incoming/<ID>.png`) and marked those rows **GENERATED** with the note "from first-batch sheet".
Nothing is validated yet, so the game still builds with the placeholders.

## Character size
The owner's reference is **24x24 characters, 48x48 bosses** (the barn cat on the characters sheet, and the
farmer, fox, owl and badger, which grow with the characters: at CHAR_SIZE 24 or 32 the props sheet re-packs
its rows 4 and below and widens to fit the farmer's 8 frames). The engine takes 16, 24 or 32
px characters on the 16-px grid (bottom-centre anchor, overlapping upwards): `make CHAR_SIZE=24`. Comparison
images in `docs/art-preview/`:
- `character_size_16_4x.png`, `_24_`, `_32_`: mole, ferret, cat, boss at each size with nearest, area, and
  area + palette snap, at 4x (`tools/art_compare.py`);
- `character_size_side_by_side_2x.png`: the three sizes side by side;
- `character_size_ingame_1x.png` / `_2x.png`: real game builds at each size with the first-batch art, on
  spring 3, spring 8 (boss) and spring 2 underground (`tools/art_compare_ingame.sh`).

## Left and right
Right-facing character strips are **derived**: the build mirrors the left strip (the OAM h-flip, as SNES
games do; the right frames use the left tiles, so they cost no VRAM). `art_sync.py sync` writes the mirrored
left strip into the right cells, and a right strip drawn by the image agent is not used, unless its family
is listed in `tools/sheets.py ASYMMETRIC_FAMILIES` (for art that must not be mirrored, such as a lamp that
stays on one side). `tools/art_consistency.py` checks the facing of every frame of each left/right pair (the
frames are split into two orientation groups from their full-resolution shapes and brightness profiles) and
flags a frame that faces the wrong way, then **mirrors it automatically** (the imported frame is flipped so
the walk cycle never flip-flops in the game; the flag stays in the review tool so the strip still gets
regenerated). `games/bombermole/tests/facing_capture.py` (in `make check` and `make preview`) captures the
walk cycles in the game (`--opt spritetest=1`) and asserts the facing of every frame.

## Terrain tilesets
The terrain can come from three sets, chosen when building (`make TILESET=...`):
- `code` (the default, and the preview's): `tools/make_tiles.py` draws every tile in code with 16x16
  pixel-art technique (3-4 shades per material, large clear shapes, a lit top-left and a shaded
  bottom-right, a dark outline on boulders) using each season's colours taken from the AI tiles in
  `art/incoming/`. It also draws `tiles_extra.png`: two more variants of grass, soil and tunnel floor (the
  game picks one per cell with a fixed hash), the water's second shimmer frame and the water bank. Output:
  `games/bombermole/art/tilesets/code/` (`make tiles-code` redraws it).
- `ai`: the art's own `tiles.png` (placeholders + imported AI tiles).
- `ai_v2`: the low-detail AI set of the TODO's group 10 (`tile_v2_<season>_<tile>` rows, including the extras);
  `art_sync.py sync` assembles the imported rows into `<art>/tilesets/ai_v2/` (missing cells: the `ai` tiles).
`make preview` renders `docs/art-preview/tiles-compare.png`: the current AI tiles against the preview's set,
in the same three scenes, at 1x and 2x.

## Objects over the ground
The game draws only the base ground as full tiles (grass or snow, tunnel floor, water, ice; underground soil
blocks and thin floors). Everything placed on it (rocks, stone, surface dirt mounds, frozen soil, roots, leaf
piles, holes, ladders, the exit, crates, bridges, gates, plates, levers...) is drawn over it on the objects
layer, so it sits on the season's ground (games/bombermole/DESIGN.md, "Ground and objects"). Such art should
have flat magenta around the object. Art drawn on its own ground still works: when the game is built, an
opaque object cell gets its background keyed out (the border's main colour, flood-filled from the edges) and
a 1-px dithered contact shadow under it; a cell whose middle has the border's colour (a block that fills its
cell, like stone bricks) is kept as it is. `tools/sheets.py is_overlay()` lists which cells are objects.
Bridges exist in two orientations (`bridge`: crossed left-right; `bridge_v`: crossed up-down); the game picks
one from the water around the bridge.

## Other games (--game)
The same tools serve every game: `tools/art_sync.py --game <game> todo|sync`, `tools/art_review.py --game
<game>` and `tools/make_placeholders.py --game <game>` hand over to `games/<game>/tools/art_game.py` (the
game's strips, TODO writer and import) and `make_art.py` (its code-drawn placeholders); the cutting, scaling,
palettes and outlines stay `tools/art_consistency.py`'s. A strip may set `anchor = "center"` (frames centred on
the body, for rotated frames), `center_offsets` (per frame, the offset of the drawing's centre from the cell
centre in the game's own frames), `reference` (the family's reference strip) and `target_h` (the reference
frame's height in game pixels, for a character smaller than its cell). Leady Squid:
`games/leadysquid/art/incoming/TODO.md` (see games/leadysquid/DESIGN.md, "Art").

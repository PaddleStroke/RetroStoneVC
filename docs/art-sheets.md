# Bomber Mole: sprite-sheet layout

This is the **assembled layout** of the game's four sheets, in `games/bombermole/art/`. The placeholders
(`tools/make_placeholders.py`), the asset build, the cutter (`tools/cut_ai_sheet.py`) and the art sync
(`tools/art_sync.py`) all use it; the single source of truth is `tools/sheets.py`, and the table below is
generated from it (`python3 tools/sheets.py --markdown`; `make check` verifies they match).

New art is not drawn as whole sheets any more: the image agent draws **one PNG per animation strip** (see
[art-workflow.md](art-workflow.md) and `games/bombermole/art/incoming/TODO.md`), and `art_sync.py` assembles
the sheets below from the validated strips.

## Conventions
- Every sheet is a grid of **16x16 cells** on a **magenta `#FF00FF`** background (magenta = transparent).
  Column and row numbers in the table are in cells; x = column x cell size, y = row x cell size.
- The frames of an entry run **left to right** from its first cell.
- **Characters sheet**: its cell is the character size, `CHAR_SIZE` (16 by default; 24 or 32 with
  `make CHAR_SIZE=24`), so the mole, ferret, cat and guard dog are CHAR_SIZE square and the barn cat is 2x2
  cells (the props bosses grow the same way, see art-workflow.md). In the
  game, sprites are anchored at the **bottom centre** of their 16x16 grid cell and overlap upwards (like SNES
  Bomberman), so 24- or 32-px characters need no level change.
- **Tiles sheet**: one row per season (0 spring, 1 summer, 2 autumn, 3 winter). Tiles fill their cell edge to
  edge. Column 17 (water) is an optional extra.
- **Props sheet**: rows 0-1 (up to column 8) are terrain-like props drawn on the terrain layer; the rest are
  sprites. Rows 4-9 hold 32x32 frames (2x2 cells).
- Tiles and sprites use the **palette group** of their row: 15 colours + transparent per group. A group is
  one SNES-style 16-colour palette in the game: `mole`, `ferret`, `cat`, `bomb` (bomb and dust), `pickup`
  (grub and power-ups), `prop` (sprite props), `critter` (dog and bees), `fx` (explosions), `hud`, `propbg`
  (terrain-like props: one palette per colour family, plants / wood / stone / water, `PROP_PALETTES` in
  tools/sheets.py; a level uses two at most, see DESIGN.md "BG palettes"), and one palette per boss (`boss_cat`, `boss_farmer`, `boss_badger`, `boss_fox`,
  `boss_owl`, loaded when that boss's level starts). The terrain of one season may use 4 palettes (60 colours).
- Explosion pieces must reach the cell edges: the arms fill the cell across, the ends touch the edge on the
  centre's side (the cutter and art_sync stick them to that edge).

## Sheet sizes (CHAR_SIZE 16)
| Sheet | Grid | Pixels |
|---|---|---|
| characters.png | 16 x 6 cells | 256 x 96 |
| tiles.png | 18 x 4 cells | 288 x 64 |
| items_fx.png | 16 x 4 cells | 256 x 64 |
| props.png | 16 x 10 cells | 256 x 160 |
| title_logo.png | (not a grid) | 256 x 64 |

The title logo is imported differently: the whole image is scaled down to fit 256x64 with an area filter and
reduced to 15 colours, then used as background tiles on the title screen.

## Cutting a whole AI sheet (`tools/cut_ai_sheet.py`)
For sheets drawn before the strip workflow (the owner's first batch):
`python3 tools/cut_ai_sheet.py AI.png --sheet characters --out characters.png [--base placeholders.png]
[--map order.txt] [--scale 8] [--filter area|nearest] [--palette-from sheet.png] [--debug boxes.png]`.
It masks the almost-magenta background (and despills pink fringes), finds the sprites as blobs (small blobs such
as sparks and dirt are attached to the nearest sprite, stray pixels are dropped), maps them **in reading order**
onto the table order (or the order in `--map`), reports any count mismatch per row, scales each palette group
with one factor, downscales (area or nearest) and snaps the colours. `art_sync.py import-sheet` uses the same
code to turn a whole sheet into strips.

## The table

| Name | Sheet | Column | Row | Size | Frames | Palette group | Description |
|---|---|---|---|---|---|---|---|
| mole_walk_down | characters.png | 0-2 | 0 | 16x16 | 3 | mole | Mole walking toward the viewer (front view); frame 0 = standing |
| mole_walk_up | characters.png | 3-5 | 0 | 16x16 | 3 | mole | Mole walking away (back view) |
| mole_walk_left | characters.png | 6-8 | 0 | 16x16 | 3 | mole | Mole walking left (side view) |
| mole_walk_right | characters.png | 9-11 | 0 | 16x16 | 3 | mole | Mole walking right (side view) |
| mole_dig_down | characters.png | 12-13 | 0 | 16x16 | 2 | mole | Mole digging downward (front view), claws out, dirt flying |
| mole_dig_up | characters.png | 14-15 | 0 | 16x16 | 2 | mole | Mole digging upward |
| mole_dig_left | characters.png | 0-1 | 1 | 16x16 | 2 | mole | Mole digging left, claws forward |
| mole_dig_right | characters.png | 2-3 | 1 | 16x16 | 2 | mole | Mole digging right, claws forward |
| mole_place_bomb | characters.png | 4 | 1 | 16x16 | 1 | mole | Mole dropping a bomb |
| mole_hurt | characters.png | 5 | 1 | 16x16 | 1 | mole | Mole hurt: flinching, dazed |
| mole_death | characters.png | 6-9 | 1 | 16x16 | 4 | mole | Mole knocked out: dazed, falling, flat, ghost |
| mole_victory | characters.png | 10-11 | 1 | 16x16 | 2 | mole | Mole cheering, paws up (2-frame loop) |
| ferret_walk_down | characters.png | 0-1 | 2 | 16x16 | 2 | ferret | Ferret running toward the viewer |
| ferret_walk_up | characters.png | 2-3 | 2 | 16x16 | 2 | ferret | Ferret running away |
| ferret_walk_left | characters.png | 4-5 | 2 | 16x16 | 2 | ferret | Ferret running left (long body) |
| ferret_walk_right | characters.png | 6-7 | 2 | 16x16 | 2 | ferret | Ferret running right (long body) |
| ferret_stunned | characters.png | 8 | 2 | 16x16 | 1 | ferret | Ferret stunned: dizzy, stars over the head |
| dog_walk_left | characters.png | 9-10 | 2 | 16x16 | 2 | critter | Guard dog (ally) running left |
| dog_walk_right | characters.png | 11-12 | 2 | 16x16 | 2 | critter | Guard dog (ally) running right |
| dog_sleep | characters.png | 13 | 2 | 16x16 | 1 | critter | Guard dog asleep |
| cat_walk_down | characters.png | 0-1 | 3 | 16x16 | 2 | cat | Cat walking toward the viewer |
| cat_walk_up | characters.png | 2-3 | 3 | 16x16 | 2 | cat | Cat walking away |
| cat_walk_left | characters.png | 4-5 | 3 | 16x16 | 2 | cat | Cat walking left |
| cat_walk_right | characters.png | 6-7 | 3 | 16x16 | 2 | cat | Cat walking right |
| cat_pounce | characters.png | 8-9 | 3 | 16x16 | 2 | cat | Cat pounce: crouch, then leap (facing right; flipped in game) |
| boss | characters.png | 0-7 | 4-5 | 32x32 | 4 | boss_cat | Spring boss: the barn cat, 32x32: idle, walk, crouch, roar |
| grass | tiles.png | 0 | 0-3 (one per season) | 16x16 | 1 | terrain | Surface floor: grass (snow in winter) |
| grass_edge | tiles.png | 1 | 0-3 (one per season) | 16x16 | 1 | terrain | Surface floor with a shadow/edge along the top (below a wall) |
| soft_dirt | tiles.png | 2 | 0-3 (one per season) | 16x16 | 1 | terrain | Soft dirt block (diggable; dry soil in summer) |
| dirt_crack | tiles.png | 3 | 0-3 (one per season) | 16x16 | 1 | terrain | Soft dirt half dug, cracked |
| hard_rock | tiles.png | 4 | 0-3 (one per season) | 16x16 | 1 | terrain | Hard rock block (bombs only) |
| stone | tiles.png | 5 | 0-3 (one per season) | 16x16 | 1 | terrain | Unbreakable stone block |
| roots | tiles.png | 6 | 0-3 (one per season) | 16x16 | 1 | terrain | Tree roots blocking the way (a blast clears them, they regrow) |
| tunnel | tiles.png | 7 | 0-3 (one per season) | 16x16 | 1 | terrain | Tunnel floor (underground) |
| hole_down | tiles.png | 8 | 0-3 (one per season) | 16x16 | 1 | terrain | Burrow hole leading down |
| hole_up | tiles.png | 9 | 0-3 (one per season) | 16x16 | 1 | terrain | Hole up: light falling from the depth above |
| ladder | tiles.png | 10 | 0-3 (one per season) | 16x16 | 1 | terrain | Ladder leading up |
| thin_floor | tiles.png | 11 | 0-3 (one per season) | 16x16 | 1 | terrain | Thin floor: cracked, a bomb breaks it into a hole |
| exit_closed | tiles.png | 12 | 0-3 (one per season) | 16x16 | 1 | terrain | Exit molehill, closed |
| exit_open | tiles.png | 13 | 0-3 (one per season) | 16x16 | 1 | terrain | Exit molehill, open (glowing) |
| puddle | tiles.png | 14 | 0-3 (one per season) | 16x16 | 1 | terrain | Puddle (slows down; stops blasts) |
| frozen_dirt | tiles.png | 15 | 0-3 (one per season) | 16x16 | 1 | terrain | Snow and frozen dirt: frozen soil block (cannot be dug; bombs only) |
| leaves | tiles.png | 16 | 0-3 (one per season) | 16x16 | 1 | terrain | Pile of leaves (may hide an item; dig or blast it) |
| water | tiles.png | 17 | 0-3 (one per season) | 16x16 | 1 | terrain | EXTRA (optional): water, impassable, blasts cross it |
| bomb | items_fx.png | 0-2 | 0 | 16x16 | 3 | bomb | Bomb with a burning fuse (3-frame pulse) |
| expl_center | items_fx.png | 3-5 | 0 | 16x16 | 3 | fx | Explosion centre (3 frames: flash, full, fading) |
| expl_h | items_fx.png | 6-8 | 0 | 16x16 | 3 | fx | Explosion arm, horizontal (fills the cell width) |
| expl_v | items_fx.png | 9-11 | 0 | 16x16 | 3 | fx | Explosion arm, vertical (fills the cell height) |
| expl_end_left | items_fx.png | 12-14 | 0 | 16x16 | 3 | fx | Explosion arm end, tip pointing left |
| expl_end_right | items_fx.png | 0-2 | 1 | 16x16 | 3 | fx | Explosion arm end, tip pointing right |
| expl_end_up | items_fx.png | 3-5 | 1 | 16x16 | 3 | fx | Explosion arm end, tip pointing up |
| expl_end_down | items_fx.png | 6-8 | 1 | 16x16 | 3 | fx | Explosion arm end, tip pointing down |
| grub | items_fx.png | 9-10 | 1 | 16x16 | 2 | pickup | Golden grub (2-frame wiggle) |
| pu_bomb | items_fx.png | 11 | 1 | 16x16 | 1 | pickup | Power-up: one more bomb |
| pu_fire | items_fx.png | 12 | 1 | 16x16 | 1 | pickup | Power-up: longer blast |
| pu_speed | items_fx.png | 13 | 1 | 16x16 | 1 | pickup | Power-up: speed (claws) |
| pu_remote | items_fx.png | 14 | 1 | 16x16 | 1 | pickup | Power-up: remote detonator |
| pu_heart | items_fx.png | 15 | 1 | 16x16 | 1 | pickup | Power-up: heart (one extra hit) |
| hud_heart | items_fx.png | 0 | 2 | 16x16 | 1 | hud | HUD icon: heart / lives |
| hud_digit | items_fx.png | 1-10 | 2 | 16x16 | 10 | hud | HUD digits 0-9 (big, readable) |
| hud_bomb | items_fx.png | 11 | 2 | 16x16 | 1 | hud | HUD icon: bombs |
| hud_fire | items_fx.png | 12 | 2 | 16x16 | 1 | hud | HUD icon: blast range |
| hud_grub | items_fx.png | 13 | 2 | 16x16 | 1 | hud | HUD icon: golden grubs left |
| hud_speed | items_fx.png | 14 | 2 | 16x16 | 1 | hud | HUD icon: speed (claws) |
| hud_slash | items_fx.png | 15 | 2 | 16x16 | 1 | hud | HUD: slash between numbers |
| hud_depth_surface | items_fx.png | 0 | 3 | 16x16 | 1 | hud | HUD depth icon: surface |
| hud_depth_under1 | items_fx.png | 1 | 3 | 16x16 | 1 | hud | HUD depth icon: underground 1 |
| hud_depth_under2 | items_fx.png | 2 | 3 | 16x16 | 1 | hud | HUD depth icon: underground 2 |
| hud_danger | items_fx.png | 3 | 3 | 16x16 | 1 | hud | HUD icon: danger (exclamation) |
| hud_lock | items_fx.png | 4 | 3 | 16x16 | 1 | hud | Menu icon: locked level |
| hud_check | items_fx.png | 5 | 3 | 16x16 | 1 | hud | Menu icon: level cleared |
| hud_cursor | items_fx.png | 6 | 3 | 16x16 | 1 | hud | Menu cursor (arrow pointing right) |
| hud_panel | items_fx.png | 7 | 3 | 16x16 | 1 | hud | HUD panel background (solid, dark) |
| dust | items_fx.png | 8-10 | 3 | 16x16 | 3 | bomb | Dust puff (digging, landing), growing and fading |
| bomb_remote | items_fx.png | 11-12 | 3 | 16x16 | 2 | bomb | Remote-controlled bomb: no fuse, a short antenna with a light, blinking (light off, light on) |
| bridge | props.png | 0 | 0 | 16x16 | 1 | propbg | Wooden bridge crossed left to right: planks running up-down, rails along the top and bottom (drawn over the water; a blast destroys it) |
| ice | props.png | 1 | 0 | 16x16 | 1 | propbg | Slippery ice (you slide until you hit something) |
| thin_ice | props.png | 2-3 | 0 | 16x16 | 2 | propbg | Thin ice: intact, cracked (breaks into water after 2 crossings) |
| mud | props.png | 4 | 0 | 16x16 | 1 | propbg | Mud (under a puddle; slows you) |
| tall_grass | props.png | 5 | 0 | 16x16 | 1 | propbg | Tall grass: cover, hides you from cats, burns |
| corn | props.png | 6 | 0 | 16x16 | 1 | propbg | Corn field (summer cover), burns |
| burnt | props.png | 7 | 0 | 16x16 | 1 | propbg | Burnt ground (after cover burns) |
| gate | props.png | 8-9 | 0 | 16x16 | 2 | propbg | Gate: closed, open |
| plate | props.png | 10-11 | 0 | 16x16 | 2 | propbg | Pressure plate: up, pressed |
| lever | props.png | 12-13 | 0 | 16x16 | 2 | propbg | Lever: off (left), on (right) |
| steam_vent | props.png | 14-15 | 0 | 16x16 | 2 | propbg | Steam vent (summer, depth 2): idle, erupting |
| pipe | props.png | 0 | 1 | 16x16 | 1 | propbg | Drain pipe opening (teleports to its twin) |
| crate | props.png | 1 | 1 | 16x16 | 1 | propbg | Tomato crate (farmer boss): breakable |
| splat | props.png | 2 | 1 | 16x16 | 1 | propbg | Tomato splat on the floor (slippery for a few seconds) |
| beehive | props.png | 3 | 1 | 16x16 | 1 | propbg | Beehive (summer): bomb it and bees chase the nearest creature |
| well | props.png | 4 | 1 | 16x16 | 1 | propbg | Well with crank (elevator surface <-> depth 2) |
| mushroom | props.png | 5 | 1 | 16x16 | 1 | propbg | Bouncy mushroom (autumn): launches you 2 tiles |
| rails_h | props.png | 6 | 1 | 16x16 | 1 | propbg | Mine-cart rails, horizontal |
| rails_v | props.png | 7 | 1 | 16x16 | 1 | propbg | Mine-cart rails, vertical |
| apple_tree | props.png | 8 | 1 | 16x16 | 1 | propbg | Apple tree (autumn): bomb it, apples fall and stun |
| sprinkler | props.png | 9-12 | 1 | 16x16 | 4 | prop | Garden sprinkler, spraying up, right, down, left |
| spray | props.png | 13-14 | 1 | 16x16 | 2 | prop | Water spray / stream current particles |
| log | props.png | 15 | 1 | 16x16 | 1 | prop | Floating log (pushable bridge on water) |
| wind | props.png | 0-2 | 2 | 16x16 | 3 | prop | Wind gust particles (drawn blowing right; flipped) |
| pumpkin | props.png | 3 | 2 | 16x16 | 1 | prop | Pumpkin (autumn): push it to block enemies |
| snowball | props.png | 4-5 | 2 | 16x16 | 2 | prop | Snowball: small, big (rolls and grows when blasted) |
| icicle | props.png | 6 | 2 | 16x16 | 1 | prop | Icicle (falls after nearby blasts) |
| mine_cart | props.png | 7 | 2 | 16x16 | 1 | prop | Mine cart |
| bucket | props.png | 8 | 2 | 16x16 | 1 | prop | Well bucket (elevator) |
| tomato | props.png | 9-10 | 2 | 16x16 | 2 | prop | Thrown tomato (2-frame spin) |
| tomato_shadow | props.png | 11 | 2 | 16x16 | 1 | prop | Target shadow of a falling tomato |
| apple | props.png | 12 | 2 | 16x16 | 1 | prop | Falling apple |
| zzz | props.png | 13-14 | 2 | 16x16 | 2 | prop | Sleeping 'Zz' bubble |
| steam | props.png | 15 | 2 | 16x16 | 1 | prop | Steam puff (vents) |
| bees | props.png | 5-6 | 3 | 16x16 | 2 | critter | Swarm of bees |
| windmill | props.png | 0-7 | 4-5 | 32x32 | 4 | prop | Windmill (spring signature), sails turning (4 frames) |
| fox | props.png | 8-15 | 4-5 | 32x32 | 4 | boss_fox | Autumn boss: the fox (run, run, leap, hurt) |
| farmer | props.png | 0-15 | 6-7 | 32x32 | 8 | boss_farmer | Summer boss: the farmer: idle x2, throw x2, angry x2, hurt x2 |
| owl | props.png | 0-7 | 8-9 | 32x32 | 4 | boss_owl | Winter boss: the snowy owl (perch, flap, swoop, hurt) |
| badger | props.png | 8-15 | 8-9 | 32x32 | 4 | boss_badger | Summer mini-boss: the badger (walk x2, dig, hurt) |
| bridge_v | props.png | 0 | 3 | 16x16 | 1 | propbg | Wooden bridge crossed up and down: planks running left-right, rails along the left and right (drawn over the water; a blast destroys it) |
| windmill_side | props.png | 0-7 | 10-11 | 32x32 | 4 | prop | Windmill seen from the side, sails on the RIGHT (it blows to the right; mirrored for the left), 4 frames of the sails turning |

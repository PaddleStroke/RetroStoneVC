# Nutty Branch: design

(c) 2026 Pierre-Louis Boyer (8BCraft), CC BY-NC-SA 4.0 (games/nuttybranch/LICENSE). A game for the RetroStone
virtual console (docs/spec.md), in the 8BCraft house style (docs/art-direction.md).

**Working title.** The name lives in one place in the code: `src/tuning.h` (`GAME_NAME`, `GAME_LOGO`); the docs,
`game.mk` and `dist/README-windows.txt` only mention it in prose.

## Pitch
A red squirrel races along an endless oak branch, seen from behind, the branch rushing toward the camera like an
arcade road. The branch is a **cylinder**: the further the squirrel runs from the top, the harder gravity pulls it
off the side. Steer to stay balanced, jump the branches lying across it, gaps and caterpillars, and lean out toward
the edges for the nuts, as far as greed allows. One more run, a best distance to beat, up to four squirrels on the same branch.

## Controls
| Pad | Keyboard | Action |
|---|---|---|
| D-pad Left / Right | arrows | steer around the branch (with momentum) |
| A (also B) | X (Z) | jump: a tap hops, holding A jumps higher (up to `JUMP_HOLD_MAX` frames) |
| Select / Start | Backspace / Enter | pause |

The title is the only menu: **A starts** (Left / Right too, an extra start input), `PRESS A TO RUN` with the A
glyph; pads 2-4 join with A and leave with B; after the game over one press goes back to the title.

**Touch** (player 1; `rs_game.touch_native`: the phone apps pass the game its touches, docs/spec.md "Touch";
`touch_input()` in src/main.c):

| Touch | Action |
|---|---|
| hold the left / right half of the screen | steer (= Left / Right held; both halves = neither) |
| swipe up | jump (= A pressed); the jump is held while that finger stays down (up to `JUMP_HOLD_MAX`): a quick swipe, lifted at once, is the hop |
| tap | on the title: start (as A); on the game-over panel: back to the title (after the same `RETRY_LOCK`) |
| the app's pause button | pause (it presses Start; the app's own) |

A finger steers from the half it is on until it swipes up: from then on it is the jump finger and no longer steers.
A young finger already heading up (past the 8 px slop, a swipe in the making) does not steer either, so a jump does
not start with a nudge sideways (only the frame it touches down). So the usual grip is a thumb on each half, steering,
and a swipe up with either to jump: the other thumb keeps steering. A finger held down for longer than the swipe
window (24 frames) cannot become a swipe (the SDK's definition): lift it and swipe. Swipes down or sideways do
nothing. Nothing is kept between frames (the runtime's touches are the state): nothing to save, and while paused
the touches are not read, so a swipe during the pause is not kept for after it. Without touches the game is
byte for byte the same.

## The branch is a cylinder (the core mechanic)
Every squirrel has an **angle around the branch** `ang` (0 = the top; Q8 degrees in the code), an angular speed
`angv` and a height above the bark `h` (jumps). Integer fixed point only (tuning.h), a sine table.

Per frame, on the bark (`play.c sq_physics`):

    angacc = PULL * sin(ang)                      gravity tips you off: stronger the further from the top
           + CENTRIFUGAL * speed^2 * curve        a curve pushes you to its outside
           + WIND (a gust)                        telegraphed by leaves streaming across
           + steer * STEER_ACC * grip             the D-pad (grip < 1 on sap)
    angv  += angacc;  angv -= angv * DAMP;  ang += angv
    |ang| > SLIP_ANG  -> the squirrel slips off and falls: out

| Constant | Value | Meaning |
|---|---|---|
| `STEER_ACC` | 0.24 deg/frame^2 | the D-pad's push |
| `DAMP` | 1/12 per frame | friction: a terminal steering speed of 2.6 deg/frame, momentum decays in ~12 frames |
| `PULL` | 0.21 deg/frame^2 x sin(ang) | at the slip angle the pull is 77% of the steer: you can still climb back, slowly |
| `SLIP_ANG` | 62 deg | past it you fall |
| `WARN_ANG` | 40 deg | the wobble frames, the creak, the balance meter turns red |
| `CENTRIFUGAL` | a hard curve at top speed: 0.09 deg/frame^2 (37% of the steer) | curves matter more as you speed up |
| sap: `SAP_GRIP`, `SAP_DAMP`, `SAP_PULL` | steer x 0.45, damping / 3, pull x 1.35 | slippery: near the edge the pull beats the steer |
| web: `WEB_SLOW` | speed x 0.55, recovering in 90 frames; steer x 0.5 for 30 frames | |

Feel targets (tested in `tests/test_physics.c`): from the top with no input, a 2-degree nudge grows to a slip in
about 1.5-2.5 s (an inverted pendulum with friction); full steer from the top to 45 degrees takes about 0.4 s;
letting go keeps sliding for ~12 frames; at 55 degrees full steer still brings you back (no sap, no curve, no
wind); on sap at 55 degrees it does not.

In the air the pull and the curve force are off and the steer is halved (`AIR_STEER`): the squirrel keeps its
momentum, so a jump is also a way to cross the top quickly.

**Jump**: `JUMP_VH` 2.4 units/frame; gravity 0.16 (0.07 while A is held and the squirrel rises, for up to
`JUMP_HOLD_MAX` 14 frames). A tap hops 18 units high for 30 frames; held, 33 units for 43 frames. A press up to
`JUMP_BUFFER` 6 frames before landing is kept; a jump still works `COYOTE` 4 frames after running off the edge of
a gap (Celeste's grace windows).

**Speed** `SPEED0` 5 units/frame (4.7 m/s: a squirrel's sprint), +1 every 250 m, up to `SPEED_MAX` 10 (9.4 m/s).
1 m = 64 units; the branch radius is 64 units (a big old oak limb, 2 m across).

## Rendering: the branch as an arcade road
320x240 at 60 Hz. The branch is drawn like the roads of the arcade and SNES racers:

- **Segment projection** (`road.c road_project`; Lou's Pseudo 3d Page "3D-projected segments", Jake Gordon's
  racer): the branch ahead is a list of 16-unit segments, each with a curve (a second difference of x: `x += dx;
  dx += curve`) and a height. Every frame the 160 segments in front of the camera (2560 units) are projected with
  `y = HORIZON + (camh - h) * F / z`, `x = 160 + (x - camx) * F / z` (F = 160: a 90-degree field of view; `CAM_H`
  72 units above the bark, `CAM_BACK` 120 units behind the leader, `HORIZON_Y` 96). Lines are filled from the
  bottom up, interpolating 1/z between the segment ends (perspective-correct); a segment below the highest line
  already drawn is hidden (a crest hides the dip after it).
- **One texture row per scanline, scaled**: the branch layer (BG2) is the SDK's affine layer, and the raster
  callback (HDMA-like) sets its matrix on every line: `a` = z / F texels per pixel (the scale), `b = c = d = 0`,
  the centre row `cy` = the texture row of that line, and the line-scroll table shifts it so the branch centre
  lands on the projected x. This is the Mode 7 "perspective floor" trick of F-Zero used for a road: each line
  samples one row, horizontally scaled and shifted.
- **The bark is a cylinder** (512x256 texels, `tools/nb_bark.py`): a texel column is a place across the branch,
  x = R sin(angle), so everything on the bark is laid out by its ANGLE and squeezed toward the silhouette (the owner's
  playtest: evenly spaced furrows read as a flat road). The furrows run ALONG the branch, every 9 degrees of angle:
  wide apart on the top, bunching up and thinning toward the sides, and in the perspective they all run to the
  vanishing point (the speed, the roundness). Each furrow wanders a degree or so along the branch and swells and
  thins (a braided, irregular bark, not planks), its core deepest on the lit top (more contrast in the middle); the
  ridges between are split here and there, rough with specks and flecks of light, their left flanks lit in broken
  strokes; cross-cracks are rare. The pattern repeats every 32 rows so its tiles deduplicate. The shading follows
  the light from above, n . L with n = (sin a, cos a): a narrow highlight, darkening gradually to both sides over
  7 shades, the underside darkest, a dark rim at the silhouette, a slightly ragged edge and the odd bump.
  **Measured** (`tests/furrow_test.sh`): on 9 screen lines just below the squirrel the furrows are found as runs of
  pixels darker than the bark round them; they are 7-8 degrees apart all the way round (the texture's 9, a few
  merged or split), 13.0 px apart on the top, 11.0 px at 22-44 degrees and 9.0 px at 44-66 (cos law: 13.0, 11.1,
  7.6), 6.7, 5.9 and 4.5 px wide.
  Variety along the length: three 64-row patches side by side (plain bark with lichen, a burl, an old scar with
  moss), one picked per metre (the plain one 6 metres in 8); window 1 clips the layer to the line's own part of
  its row on every line, so the neighbouring patches never show. Rows 64-239: the fork rows (two sections side by
  side, centre to centre 16..352 texels in 16-texel steps, 8 identical rows each); a fork line picks the nearest
  separation: the branch the camera follows lands exactly, the other within 8 texels; below the leader only the
  followed branch is drawn (the other has swung away).
- **A gap is a real break** (rows 240-255 and the face): its near end is the bark torn into pointed teeth of all
  lengths in the bark's own shades, a lit lip and a dark torn edge (8 rows over 24 units, 16 of them reaching into
  the gap: they grow on screen as you come); its far end the same teeth pointing back at you, the dark broken face
  between them; and under the far end hangs its **broken face**: a disc of dark torn wood seen end-on (growth rings
  round the pith, splits, torn fibres, the bark round it, a torn outline), flat-topped like the branch's strip, as
  deep as the branch (128 units). It is drawn by the branch layer itself (`road.c`): its 64 rows sit beside the
  first fork rows (columns 0-127), and every empty line of the gap under the far end takes the far end's scale and
  centre and the row of its place down the face, so the face is attached to the far end at every distance, on
  curves and on slopes, and the near end's bark (nearer) keeps its lines over it. The gap reads dark.
- **Rings and haze and rain**: the raster callback loads one of 32 bark palettes per line (only when it changes):
  4 ring phases (OutRun's stripes, kept subtle), 4 haze levels toward the horizon's colour, dry or wet (after rain
  the lit top shines and the shadows deepen).
- **The forest behind**: four styles (`BG_STYLE` in `tuning.h`, or `--opt bg=a..d`; `tools/nb_backdrop.py`), each
  two parallax layers of 256x256 repeating every 128 px: BG4 the far forest (1/4 of the curves' drift, Jake Gordon:
  offset += speed * curve * k) and BG3 the near one (1/2), over the backdrop gradient (sky above the horizon, misty
  depths below, one colour per line), all in the scene palette so every style follows the seasons and the time of
  day: **a** a deep forest (tall trunks in two depths, misty far ones, dappled light, vines), **b** a sunlit canopy
  (a thick roof of leaves, leafy strands hanging, shafts of light), **c** misty pines (layered silhouettes fading
  into the mist; the owner's choice, the default), **d** birches and beeches (white trunks with dark dashes,
  round crowns, a bright sky). `docs/screenshots/bg-a.png` .. `bg-d.png`: the same moment.
  BG1 the house UI.
- **Things on the bark** (`tools/nb_render.py`, `tools/nb_props.py`): every prop is built from ellipsoids in 3D on
  the bark (x across, y up the normal, z along the branch) and ray-cast from the game's own camera (looking down
  31 degrees), shaded by the house light, outlined, with a contact shadow on the bark; a prop sitting at an angle
  round the branch is rolled by its tilt (0, 18 or 36 degrees, mirrored on the left), so it leans and narrows with
  the surface like the bark does. Each is pre-drawn at **16 sizes** (1.6 down to 0.13 px per unit, steps of 18%)
  and the size nearest the projected scale F / z is shown, so things grow steadily as they come. They are placed at
  `x = xc(line) + R sin(angle) F / z`, far to near; a crest hides what is behind it; whatever sits on the bark is
  kept within the span of the branch drawn on its line (`draw.c on_branch`: near a crest or the horizon one line can
  hold a long stretch of a turning branch but shows one centre) and is not drawn on a line without bark. Big frames
  are cut into pieces of 32x32 trimmed to their tiles. **Tested** (`tests/lod_test.sh`, `--opt lodcheck=1`): over
  18 runs (3 whole courses, 15 trap kinds alone) every object's drawn size, sampled every frame, only ever steps to
  the next larger size (17912 steps, none shrinking, none skipping a size, always the nearest size), every anchor
  of a thing on the bark or growing out of it is drawn on a line with bark within the branch's span (203067
  anchors, 0 off; 8787 of them kept on their line near crests and the horizon), and every hanging thing hangs from
  its drawn vine (3772, all).
- **Branches**: one style for all, big, knobbly, tapering, with sub-branches, full leaf clusters and the bark's own
  speckle. The **obstacles** (`nb_props.py flank_limb`) grow out of ONE flank, from a collar at its silhouette, and
  reach over PART of the path (the owner's playtest: a branch across the whole width, or an arch, cannot be
  avoided): a bough rising steadily from the flank to its leafy end, strands of leaves hanging from it down to the
  bark (a wall), a dappled leaf-shadow on the bark it covers, the other side bare and in the light; or a low limb
  lying along a flank. The **outward** ones (scenery) are drawn behind the branch layer, so they grow out from
  behind the silhouette at every distance.
- **Streamed sprite tiles**: with 16 sizes and 3 tilts there are 13309 sprite tiles, far more than VRAM. Only the
  small effects stay resident (18 tiles); every other frame (the squirrels too) is copied into a 502-tile pool when
  it is first drawn and stays there while it is on screen (`draw.c pool_get`: first fit, the frames not drawn this
  frame let go when the pool is full), the SNES way of DMA-ing sprite tiles. The pool is in the save state with
  VRAM. **VRAM**: 60.3-62.5 KB of 64 depending on the forest (a 62400, b 60352, c 62464, d 60288 bytes; the branch
  372 tiles, the forests 273-341).
- Times of day and seasons change every 400 m (spring morning, summer noon, autumn afternoon, dusk, night, winter
  dawn, then round again): sky, forest, canopy, bark, leaves and the props' bark blend over 32 m.

## Widescreen (docs/spec.md "Widescreen")
The game opts in (`max_width` = 560, `WIDESCREEN_nuttybranch = 1`): 320 x 240 is unchanged pixel for pixel; at 427
(16:9), 520 or 560 (phones) **the field of view of the scenery is wider, the game is the same**.
- **The stage centred**: `rs_screen_origin(rs_screen_margin())` (draw_init): the branch's projection, the squirrels,
  the traps and the HUD keep their 320-px places; the screen shows stage x -margin .. 320 + margin. The branch layer
  (the affine layer, its per-line matrix, line scroll and window 1) is in stage coordinates, so it is the same branch,
  as wide, at the same place on every line (where a near curve takes it past x 320 or 0 the wider screen shows more of
  it). Sprites are clipped to `rs_screen_left()` .. `rs_screen_right()`: the side branches, the fungi, the hive's
  branch and the obstacles' limbs that reach out of the 4:3 frame go on into the margins. A side branch (or the
  hive's) passing the camera, its root already below the screen, is drawn on until it has gone out at the real edge
  (the 4:3 screen lets it go once its root leaves the bottom line: drawn for the wider screen only).
- **The forest behind** (BG3, BG4: 32-tile maps of pictures that repeat every 128 px, `tools/nb_backdrop.py`) goes on
  to the edges. The misty pines (style c, the default) are flat silhouettes: on a wider screen each layer continues
  with its mirror image, [A | mirrored A] (`draw.c forest_maps`: the same tiles flipped, no VRAM), so the stands of
  pines repeat every 256 px instead of 128. The other three forests are lit from the top-left (trunks, crowns, shafts
  of light) and would be lit from the right mirrored: they repeat every 128 px as at 320. A wider picture of its own
  does not fit: VRAM leaves about 96 tiles with the default forest (its two layers take 341).
- **The weather from the real edges**: the wind's leaves stream from one real edge to the other (as many per column,
  each speed's leaves 123 px apart as at 320), the rain falls over the whole width, the big leaf of a side branch
  passing the camera sweeps out at the real edge. The owl swoops round the branch (its arc starts at the branch's
  flank, as at 320), the crow lands on the branch, the swinging branch hangs from above, the bees leave their hive:
  none of them comes from the screen's edge, so they are unchanged.
- **The HUD** is the house kit's, centred (`hu_ui_x()` is 0 with the centred origin); the UI map is 128 tiles wide
  beyond 512 columns. With 3-4 players the score chips go to the real corners, and the distance under each follows
  it (left of UI column 0 on the left: `draw.c ui_text`).
- **The sprite pool stays the 4:3 one**: the frames with a piece in the stage's 320 columns take the pool exactly as
  at 320 (the same loads, the same frames let go, the same "pool full"); a frame shown only in the margins uses its
  pool copy if it has one, or a copy of its own in the pool's free tiles, which the stage's frames take back (its
  sprites of that frame then hidden). The big leaf keeps its 4:3 place for this bookkeeping. So what the stage shows
  is the same at every width, and the VRAM is the same.
- **The same game**: the rules never look at the width; the bot reads the branch layer's per-line registers (stage
  lines, unchanged) and the sprites of the stage's 320 columns (what a 4:3 screen shows), so it decides the same
  frame by frame. tests/wide_test.sh compares the bot's every decision (`--opt botlog=1`) and the whole logs (the
  falls, the game-over frames, distances, scores, the final state's hash) of 1-, 2- and 4-player runs, a deep run
  and the trap scenes (wind, rain, owl, crow, swing, bees, shower, branches) at 320, 427, 520 and 560.
- **Save states load at any width** (the width is not in a state): `state_loaded` (draw_relayout) compares the width
  the UI map was laid out for (`laid_w`, saved) with this one; if it differs, the UI map and the forest maps of this
  width are set up and the screen's texts drawn again on the next frame. tests/wide_test.sh loads states saved at
  320, 427, 520 and 560 at the other widths, and changes the width during 1-player, 4-player and title runs
  (`--resize`): they go on exactly as the runs that never changed.
- `make nuttybranch-check` runs the libretro loader (16:9, Wide), the smoke, UI, state, LOD and furrow tests again
  at 427 and 520 (furrow_measure.py finds the stage in the wider picture), the frame cost at 520 (0.71-0.77 ms per
  frame on the host, the PPU drawing 1.6 times the columns: an A20 estimate of 10.7-15.4 ms of 16.7), wide_test.sh and
  the language test at 320, 427 and 520. Screenshots: title-, play-, gameover- and play-4-players-427/520.png.

## Tall screen (docs/spec.md "Tall screen", the zoomed recipe)
A phone held upright: `rs_screen_zoom(ZOOM_H)` (480 lines, tuning.h) first in `game_init`. The app scales the 480
lines to the display's height and crops the sides: 204 columns on a 1080x2520 phone, 222 on a 1080x2340 one, so the
branch and the squirrel are about 5 times their 4:3 size. The stage's 240 lines sit `TALL_ABOVE` (112) lines down;
128 lines below it.
- **The branch down to the screen's bottom**: road.c projects every line of the screen (`road_view.ext_top`,
  `ext_bot`: the lines past the stage, `road_ln()`), the same projection carried on toward the camera (z from 80
  units at the stage's last line to 42 at the screen's): more branch near the camera, the bark big. The stage's 240
  lines come out exactly the same (a segment below the stage only lowers the crest line where it stays below the
  stage). The raster sets the branch layer on the lines past the stage from their own tables (`raster_out`: no line
  scroll there, the layer's scroll itself), with the bark palettes per line as on the stage.
- **The things passed, near the camera**: a 4:3 screen hides a thing once its bark line leaves the stage's bottom
  (`road_point`'s crest test); on a tall screen it is drawn on down to the screen's bottom (`hidden` = 2: shown only
  there), the side branches and fungi too, until they pass the camera. The swinging branch's vine goes up to the
  screen's top, the wind's leaves and the rain cover every line.
- **The forest above, designed for the height**: the sky is one smooth gradient from the screen's top to the horizon
  (no dither steps there), the canopy's fringe at the screen's top (behind the HUD) and the forest's two layers
  stretched between it and the horizon (`bg_row`: 2.4 lines per row): the pines grow tall and reach up into the sky,
  in every style and season (the same tiles, no VRAM). From the horizon down, the stage's rows.
- **The HUD at the screen's top**: the UI layer's top 48 lines pinned there (`rs_bg_pin`; 72 with 3-4 players), the
  score digits at `rs_screen_top()` + 10, the distance under them; 2 players at the quarters of the columns shown, 3-4
  players two by two, each with its P1..P4 tag.
- **The same zoomed view on every screen**: the title, the game-over panel, the results and the pause never fill
  the width. They are the house's uniform portrait UI (games/common/src/house_tall.h, its default rows only, on the
  stage, as every zoomed game): the logo as NUTTY over BRANCH (`ht_logo`, row 2), the slots, the prompt, BEST, the
  join line and the credits (`ht_title_draw` from `HU_ROW_SLOTS`), the joined squirrels at the slots (not on the branch: they would cover the prompt);
  PAUSED (`ht_pause`); GAME OVER (`ht_banner`) and the game-over panel: METRES, BEST, MEDAL each over its value, the
  nuts as its note (`ht_gameover_panel_ex`); the ranking (`ht_results_*`).
- **The same game**: the rules never look at the height. The bot reads the stage's 240 lines and the sprites a 4:3
  screen draws: what only the tall screen draws (past the stage's lines, or hidden on a 4:3 screen) is marked
  (`draw_oam_tall`) and the stage's frames take the sprite pool as at 240 (the tall screen's own frames get their
  tiles last, like the widescreen margins'). tests/tall_test.sh compares the whole logs (the bot's every decision,
  the falls, the game-over frames, distances, scores, the final state's hash) of 1- and 4-player runs, a deep run,
  three runs from the title and 13 trap scenes at 320x240 and on 1080x2520, 1080x2340 and 720x1440 displays.
- Landscape and 4:3 are the same pixels as before (checked at 320x240, 427, 520 and 2340x1080).

## Scenery
Decorations never touch the rules and come from their own RNG (the course is the same with or without them):
- **side branches** grow out of both flanks every 160-400 units (leafy and reaching up, long and low, or carrying
  a bird's nest), from behind the silhouette (drawn behind the branch layer), never by a gap, a knot-hole, a fork
  or a branch obstacle; kept off the far end of the branch (beyond 1200 units) where the traps appear;
- **bracket fungi** on the flanks, the bees' **hive** hanging under its own long side branch;
- a **foreground leaf** sweeps past the edge of the screen when a side branch passes the camera (depth);
- nothing floats (the owner's playtest: leaf clumps hanging from limbs that were not drawn read as upside-down
  branches in the air, so they are gone): everything grows out of the branch, and the one thing hanging from above,
  the swinging branch, hangs from a vine drawn from its pivot up past the top edge of the screen (the canopy out of
  sight), as thick as its stem at that size;
- all of it in the season's leaf colours, drawn with the same sizes as the props.

## Obstacles and segments
The generator (`gen.c`) chains **chunks**; each chunk = a geometry (straight, curve left/right easy/medium/hard,
dip, S-bend, fork) + an obstacle group, then a rest stretch of at least `REST_FRAMES` at the current speed. Kinds
unlock and densities rise with distance (`UNLOCK_*` in tuning.h).

| Kind | What | How to beat it | Height | Unlock |
|---|---|---|---|---|
| branch | a branch out of ONE flank, its leaves hanging to the bark, over part of the path: short (from the flank to 30 degrees on its side), to near the top (8 degrees short of it, 80 m), past the top (14 degrees, 220 m); singles, or slaloms one side then the other, 66-104 frames apart | steer round it to the free side (with gravity pulling there: the risk) | wall | 0 m |
| low limb | a limb lying along one flank, to 26 degrees | steer round it, or tap jump it | 14 | 0 m |
| knot | a raised burl of the branch's own bark, knobbly, ringed | held jump, or go round | 24 | 60 m |
| crow | lands at the far end of a line of nuts and eats its way toward you; flies off when you come near | get there first (harmless) | - | 200 m |
| gap | the branch is broken: a missing length of 14-22 frames at the current speed; torn bark teeth at both ends, the dark broken face hanging under the far one | jump (held for the long ones) | - | 120 m |
| knot-hole | a hole sunk into the bark (a swollen lip, the inner wall, the dark depth; now and then a mouse peeks out), 30 degrees across | steer round it, or jump it | hole | 100 m |
| sap | an amber patch: less grip, a stronger pull | stay near the top over it | - | 180 m |
| web | a spider web strung between two shoots | go round, or be slowed | 36 | 250 m |
| acorn shower | for 4 s, acorns fall ahead, their shadows first; one landing on you is a bonk (a nudge, a moment slowed); the fallen ones are nuts | dodge the shadows, pick up the acorns | - | 280 m |
| woodpecker | clings to the side and pecks a hole in the top ahead (the hole opens as you approach, "tok tok") | go round the hole, or jump it | hole | 320 m |
| caterpillar | crawls across the top | tap jump, or time it | 8 | 380 m |
| wind | a gust pushes sideways for 1.5 s; leaves stream across 1 s before | lean into it | - | 450 m |
| pinecone | falls from the canopy onto the branch ahead; its shadow grows first | go round, or tap jump once it lies | 14 | 520 m |
| fork | the branch splits in two; the dead one ends in a jagged broken end with splinters | be on the live side at the split | - | 600 m |
| snake | slithers across the path, fast, a hiss when it starts | tap jump, or let it pass | 9 | 650 m |
| owl | at any time (a hoot at night, a screech by day): its shadow sweeps over the bark, then it swoops low across the branch ahead | tap jump as it passes, or be off its arc | 12 | 700 m |
| swinging branch | a leafy branch end on a vine from the canopy above swings across the path in rhythm (2.5 s) | time your pass, or a held jump | 26 | 760 m |
| rain | for 5 s the bark is wet: steer x 0.7, damping x 0.6, pull x 1.15; rain streaks 1.5 s before, the bark shines | keep near the top | - | 850 m |
| bees | a swarm drifts out of a hive and across the path at squirrel height (a buzz) | go round it (too high to jump) | 40 | 950 m |
| snow | winter only: lumps of snow on the bark | tap jump, or go round | 12 | winter |

Nuts: **acorns** (1 point) in lines that trace a good path through the next group, **hazelnuts** (2) alone near the
edges (40-52 degrees: greed pulls you where gravity is strongest), and a rare **golden walnut** (10) at a curve's
outside edge. Consecutive nuts within `COMBO_FRAMES` raise the crunch's pitch.

**Fairness** (`tests/test_physics.c`): for thousands of seeds and start distances, a planner with human limits
(a lateral speed of at most 70% of the steering terminal speed, a reaction time, holding the angle only where the
steer beats the pull with a margin, the wind and the curve force included) must find a way through every generated
stretch: a dynamic programme over (frame, angle) with running and jumping moves; the branches out of a flank are
walls (steer round them), and the branch groups alone (500 stretches, 10313 branches, every reach, singles and
slaloms) must be passable by steering only, with no jump at all. Chunks also respect by
construction: every gap shorter than the held jump at that speed minus 10 frames; never two jump obstacles closer
than a jump plus `LAND_FRAMES`; a fork's dead branch is visibly broken long before the split; no gust over sap.

## Scoring, medals, save
- **Distance** in metres (the main record) and the **score** (nut points). HUD: the score in big digits at the
  top, the distance under it; 3-4 players: the corner chips (score).
- Medals (house tiers) by distance: bronze 250 m, silver 500 m, gold 1000 m, pearl 1500 m (`MEDAL_METRES`).
- Save RAM (`NUTB`, versioned, checksummed): the best distance, the best score, runs, medals; written at each game
  over, never in a save state.
- Game over (1 player): the house banner and panel: METRES and NUTS in big digits, the MEDAL, then BEST n M or NEW
  BEST!; one press goes back to the title.
- Results with 2-4 players: ranked by distance (the last squirrel standing goes farthest), the distance shown.

## Up to four players: one shared screen
The squirrels run on the **same branch, on one screen**, in the house palette swaps (P1 red, P2-P4 the hue
rotations of house_style.player_palettes). The camera follows the leader; every squirrel runs at the course speed,
a slowed one (a web) falls back and catches up at +6% once free, never more than `PACK_LAG` units behind (the
bottom of the screen). Squirrels side by side **jostle**: within 12 units along the branch and 14 degrees round it
they push each other apart (`BUMP_ACC`), so the pack spreads round the top and a shove can tip a neighbour toward the
edge. A nut goes to whoever reaches it first. A squirrel that slips off or crashes is out; the last one standing (the
farthest) wins.

Why not a 2x2 split: the squirrels share one branch and one speed, so they are always within ~40 units of each
other along it; a shared screen shows all of them at a readable size (the leader at 1.33 px per unit, the last at
most 2) and draws the road and its per-line matrix once. A split would show the same stretch four times at a
quarter of the size (squirrels of 12 px, nuts of 3 px) and repeat the projection per view. The shared view stays
readable because each squirrel is its own colour, its P1-P4 tag is on the HUD and its balance meter sits under it.

## The squirrel
About 24x24 seen from behind (head and tufted ears, shoulders, haunches, the hind feet showing their pale soles as
they push off): the body in 4 lean angles (0, 20, 40, 60 degrees: it stands on the bark's normal, so it tilts as it
goes round; left = the mirror), 2 run frames each, each frame cropped to its box (16 to 40 px wide); a separate
**tail sprite** (6 angles, 0 to 90 degrees, mirrored), drawn in front of the body (we see its back), that sways
against the motion: tail angle = lean - `TAIL_K` x angular speed, so a balanced squirrel carries its tail upright and
a slipping one flails it; near the edge the body scrabbles (the lean frame flickers one step further, a 1-px
shiver); the jump (stretched, then reaching), the fall (two tumbling frames, the cream belly showing), its shadow
on the bark in the air, the 16x16 face icon (title slots, results). Balance meter: a small arc under the squirrel
(green, yellow, red) with a white needle, shown beyond 18 degrees (`METER_ANG`).

## Audio
Music (`tools/make_music.py`, the house kit): a playful woodland tune that **evolves over the distance**: the game
switches module with the time of day (`main.c music_update`): `tune.mod` for spring and summer (G major, speed 5,
pluck arpeggios, a bouncy bass, the pad, a whistled lead, a counter-melody on the second pass), `dusk.mod` for the
autumn afternoon and dusk (the same tune in E minor, speed 6), `night.mod` for the night and winter dawn (E dorian,
speed 7, sparse dew-drop plucks, the lead an octave down). Loudness: the house constants (`HA_MUSIC_VOL`, the
`HA_VOL_*` effect levels). Effects (`src/main.c sound_init`, the house synthesiser; the
low, noisy or long ones stored at 16 kHz to fit the 64 KiB of sample memory): the nut crunch (its pitch rises a
semitone per combo nut, up to 8), the jump, the slip creak, the fall, the woodpecker ("tok tok"), the wind, the owl
(a hoot at night, the same an octave up by day: a screech), the snake's hiss, the crow's caw, the bees' buzz, an
acorn's bonk, the rain's patter, the swinging branch's swish; the house set for the UI.

## Tests and measurements
`make nuttybranch-check` (all pass):
- `tests/test_physics.c`: the pull against the angle (= 0.21 x sin, symmetric, growing), the slip angle (the pull there
  is 77% of the steer), the feel targets (a 2-degree nudge slips in 123 frames, full steer to 45 degrees in 25,
  the slide halves in 9 frames, sap holds only up to 22 degrees), the curve force (a hard curve at top speed: 0.090
  deg/frame^2 = 38% of the steer, x speed squared, mirrored), the jumps (tap 16.8 units / 29 frames, held 31.7 /
  43), determinism (1 and 4 players, 3000 frames, the same world byte for byte), the projection; the **generator's
  fairness**: 2000 seeds x 300 m from start distances 0-2400 m (4.4 million frames planned), all passable by the
  planner (lateral speed 2 deg/frame, holding only where 75% of the steer beats the forces, running or tap / held
  jumps without steering in the air), and the planner refuses an injected 60-frame gap and a wall of webs.
- smoke: the title flow (A and Left start, game over back to the title, pause, save RAM, joining and leaving),
  1-4 players, no strict-mode warning (VRAM 60-62 KB of 64 with any forest, sample memory under 64 KB, at most 32
  sprites a line), determinism of scripted and bot runs with 1 and 4 players (state hash and picture).
- the bot (`src/bot.c`, the screen only: OAM, recognised by the frame its tiles hold, and the branch layer's per-line
  registers, a gap's broken face told from bark by its texture column): in the check, 12 seeds x 4 minutes, the
  median must reach 1000 m: min 893 / median 1748 / mean 1614 / max 1757 m, 10 runs at the cap, 2 crashes.
- the UI screens (the title with 0-3 players joined, play, pause, the five times of day, 2 and 4 players, the
  game over with a medal, the 4-player results, each of the 19 traps, a branch on each side, the scenery, the four forests: 40 screens),
  save states (title, lobby, a run, a race, 4 players, deep in the night, paused, the game over, the results;
  replayed in the same and in a fresh process), the audit.
- the props' sizes and anchors (`tests/lod_test.sh`) and the bark's furrows on screen (`tests/furrow_test.sh`):
  above.
- the touch controls (`tests/touch_test.sh`, scripted touches, at 320 and each widescreen width): a tap on the title
  starts (on the same frame as A), a held finger does not; holding the left / right half gives the same run as the
  pad's Left / Right (the same fall, frame and angle), both halves none; a swipe up held is the pad's held A (43
  frames in the air, held 14), a quick one the hop (34, held 4); swipes down or sideways do not jump; paused, a swipe
  and a held half do nothing and are not buffered; a tap on the game-over panel goes back to the title (not before
  `RETRY_LOCK`), and a tap there starts the next run (`--opt jumplog=1` logs each landing).
- **Frame cost** (`tests/bench_test.sh`, `make nuttybranch-bench`, 2100 frames each, music on): 1 player 0.49
  ms per frame on the host (update + draw 18 us, the PPU 0.47 ms: the affine branch layer decoded per pixel,
  clipped by window 1, two background layers and up to 54 sprites, 18 on a line), 4 players 0.51-0.55 ms; the A20
  estimate (x15-20) is **7.3-11.0 ms of the 16.7 ms** frame.

## Screenshots
`docs/screenshots/` (`make nuttybranch-screenshots`, 2x): title, title-2/3/4-players, play, pause, summer, autumn,
dusk, night, winter, race-2-players, play-4-players, gameover-medal, results-4-players, and one per trap:
trap-gap, trap-fork, trap-pecker, trap-web, trap-cater, trap-sap, trap-knot, trap-cone, trap-wind, trap-owl, trap-twig
and trap-twig-right (a branch out of the left, of the right flank), trap-hole, trap-snake, trap-swing, trap-bees, trap-snow, trap-crow, trap-shower,
trap-rain; scenery (side branches); bg-a .. bg-d (the four forests, the same moment);
before-after-branch (the bark and props rework), before-after-branch-obstacle (a branch across the whole path,
then a branch out of one flank), before-after-gap and
before-after-hole (the second rework: the same trap, the same seed, before and after).

## Sources
Numbers and techniques only; no code was copied.
- Louis Gorenfeld, "Lou's Pseudo 3d Page" (http://www.extentofthejam.com/pseudo/): the z-map, curves by
  accumulated dx/ddx per segment, 3D-projected segments for hills with clipping against the highest drawn line,
  sprite scaling by 1/z and clipping behind crests, alternating stripes / palettes for speed.
- Jake Gordon, "How to build a racing game" (https://jakesgordon.com/writing/javascript-racer-v1-straight/ and
  .../javascript-racer-v2-curves/): segment length 200 with a road half-width 2000 (ours 16 and 64), a 100-degree
  field of view (ours 90), camera depth 1/tan(fov/2), the curve values easy 2 / medium 4 / hard 6 (ours scaled to
  our segments), the centrifugal push `x -= dx * speedPercent * curve * 0.3` (ours on an angle and by speed
  squared), the background parallax from the curve.
- SNES Mode 7 with HDMA (Wikipedia "Mode 7", SNESLab wiki "Mode 7"): per-scanline matrix changes for perspective
  (F-Zero, Super Mario Kart).
- Celeste's grace windows (Maddy Thorson's game-feel write-ups): jump buffering (~0.1 s), coyote time, variable
  jump height by holding.
- Bark: oak bark has short, deep longitudinal furrows between ridges broken into plates (namethatplant.net, "Bark
  terms": furrowed, ridged, plated bark; Morton Arboretum, swamp white oak bark): the furrows along the branch, the
  plates and their cross-cracks.
- Human visual reaction time, about 0.2-0.25 s: every hazard is visible well before it must be answered
  (`REACT_FRAMES`).

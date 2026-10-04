# Nutty Branch: design

(c) 2026 Pierre-Louis Boyer (8BCraft), CC BY-NC-SA 4.0 (games/nuttybranch/LICENSE). A game for the RetroStone
virtual console (docs/spec.md), in the 8BCraft house style (docs/art-direction.md).

**Working title.** The name lives in one place in the code: `src/tuning.h` (`GAME_NAME`, `GAME_LOGO`); the docs,
`game.mk` and `dist/README-windows.txt` only mention it in prose.

## Pitch
A red squirrel races along an endless oak branch, seen from behind, the branch rushing toward the camera like an
arcade road. The branch is a **cylinder**: the further the squirrel runs from the top, the harder gravity pulls it
off the side. Steer to stay balanced, jump twigs, gaps and caterpillars, and lean out toward the edges for the
nuts, as far as greed allows. One more run, a best distance to beat, up to four squirrels on the same branch.

## Controls
| Pad | Keyboard | Action |
|---|---|---|
| D-pad Left / Right | arrows | steer around the branch (with momentum) |
| A (also B) | X (Z) | jump: a tap hops, holding A jumps higher (up to `JUMP_HOLD_MAX` frames) |
| Select / Start | Backspace / Enter | pause |

The title is the only menu: **A starts** (Left / Right too, an extra start input), `PRESS A TO RUN` with the A
glyph; pads 2-4 join with A and leave with B; after the game over one press goes back to the title.

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
- **The texture** (512x256 texels, `tools/make_art.py branch_texture`): the cross-section of the cylinder across X
  (bark furrows bunching toward the edges as the surface turns away: x = R sin(angle); house cylinder shading,
  light top-left, dark right, a dark rim), 32 rows of bark detail along the length (knots, moss, cracks) that scroll
  toward the camera, end-grain and jagged rows for the broken ends, and the **fork rows**: two sections side by
  side, centre to centre 16..384 texels in 16-texel steps (8 identical rows each: the tiles deduplicate to the
  plain branch's), for the forks. A fork line picks the nearest separation and rescales `a` slightly so both
  centres land exactly.
- **Rings**: OutRun's alternating stripes, by palette: the raster callback loads one of 4 bark ramps per line by
  `(s / RING_LEN) & 3`, so darker rings rush at you, and one of 4 haze levels by distance blends the far bark into
  the sky.
- Behind: the backdrop gradient (sky above the horizon, misty forest depths below, one colour per line), BG3 the
  canopy at the top (parallax 1/2 of the curve drift) and the far forest band at the horizon (1/4), one layer split
  by a line-scroll table. Curves drift the backgrounds sideways (Jake Gordon: offset += speed * curve * k). BG1 the
  house UI.
- **Sprites in perspective**: nuts and obstacles are drawn at 5 sizes (`LOD` scales 1.33, 0.9, 0.6, 0.38, 0.22 px
  per unit; code-drawn at each size, never resampled) and placed at `x = xc(line) + R sin(angle) F / z`; the nearest
  size is used (the SNES way: no sprite scaling hardware). Far to near; a crest hides an object whose base line is
  behind it. The squirrels are drawn on the same projection (a body sprite and a tail sprite).
- Times of day and seasons change every 400 m (spring morning, summer noon, autumn afternoon, dusk, night with the
  owls, winter dawn, then round again): sky, forest, canopy and bark palettes blend over 32 m.

## Obstacles and segments
The generator (`gen.c`) chains **chunks**; each chunk = a geometry (straight, curve left/right easy/medium/hard,
dip, S-bend, fork) + an obstacle group, then a rest stretch of at least `REST_FRAMES` at the current speed. Kinds
unlock and densities rise with distance (`UNLOCK_*` in tuning.h).

| Kind | What | How to beat it | Height | Unlock |
|---|---|---|---|---|
| twig | a short side shoot on top | tap jump, or go round | 10 | 0 m |
| knot | a big burl | held jump, or go round | 24 | 60 m |
| gap | the branch is broken: a missing length of 14-22 frames at the current speed | jump (held for the long ones) | - | 120 m |
| sap | an amber patch: less grip, a stronger pull | stay near the top over it | - | 180 m |
| web | a spider web between two twigs | go round, or be slowed | 30 | 250 m |
| woodpecker | clings to the side and pecks a hole in the top ahead (the hole opens as you approach, "tok tok") | go round the hole, or jump it | hole | 320 m |
| caterpillar | crawls across the top | tap jump, or time it | 8 | 380 m |
| wind | a gust pushes sideways for 1.5 s; leaves stream across 1 s before | lean into it | - | 450 m |
| pinecone | falls from the canopy onto the branch ahead; its shadow grows first | go round, or tap jump once it lies | 14 | 520 m |
| fork | the branch splits in two; one ends in a broken stump | be on the live side at the split | - | 600 m |
| owl | at night, swoops low across the branch ahead (a hoot first) | tap jump as it passes, or be off its arc | 12 | night |

Nuts: **acorns** (1 point) in lines that trace a good path through the next group, **hazelnuts** (2) alone near the
edges (40-52 degrees: greed pulls you where gravity is strongest), and a rare **golden walnut** (10) at a curve's
outside edge. Consecutive nuts within `COMBO_FRAMES` raise the crunch's pitch.

**Fairness** (`tests/test_physics.c`): for thousands of seeds and start distances, a planner with human limits
(a lateral speed of at most 70% of the steering terminal speed, a reaction time, holding the angle only where the
steer beats the pull with a margin, the wind and the curve force included) must find a way through every generated
stretch: a dynamic programme over (frame, angle) with running and jumping moves. Chunks also respect by
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
`HA_VOL_*` effect levels). Effects (`src/main.c sound_init`, the
house synthesiser): the nut crunch (a crack of noise and a short pluck; its pitch rises a semitone per combo nut,
up to 8), the jump (a quick upward chirp), the slip creak (a low wooden groan, repeated while beyond `WARN_ANG`),
the fall (a falling whistle), the woodpecker ("tok tok", dry knocks), the wind (a long filtered hiss), the owl's
hoot; the house set for the UI.

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
- Human visual reaction time, about 0.2-0.25 s: every hazard is visible well before it must be answered
  (`REACT_FRAMES`).

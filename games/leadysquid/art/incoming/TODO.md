# Leady Squid art TODO

> **STOP: nothing to generate for Leady Squid.** The owner decided on 2026-09-30 that the
> code-drawn art is final. Image agents: do not generate or edit anything in this file. The rows
> below are kept only as a record of the sprite list.

Drop folder (this folder): `<your checkout>\games\leadysquid\art\incoming`

## Instructions for the image agent

Workflow:
1. Pick rows whose Status is **TODO** (top to bottom: the groups are in priority order) or **REJECTED**.
2. Generate ONE PNG per row, named `<ID>.png`, in this folder. When you regenerate a row, first rename the old `<ID>.png` to `<ID>.v1.png` (then `.v2.png`, and so on), then write the new `<ID>.png`.
3. Set the row's Status to **GENERATED** and write a short note in Notes if useful.
4. For a **REJECTED** row, read the owner's note in Notes, regenerate taking it into account, then set the Status back to **GENERATED** (keep the owner's note, add yours after it).

Rules:
- Edit ONLY the Status and Notes cells of the rows you handle. Do not add, remove or reorder rows, and do not touch the other columns.
- NEVER mark anything VALIDATED: only the owner validates (VALIDATED or REJECTED + a note).
- One horizontal strip per PNG: the frames sit left to right in the order given, evenly spaced, all the same size.
- Background: flat magenta `#FF00FF` everywhere around the drawings. Tiles (the obstacle bodies) are the exception: each fills its whole cell edge to edge, with no magenta inside.
- Scale: draw at 8x the final size (a 32x32 frame about 256x256 pixels, a 24x16 frame about 192x128). The title logo is drawn at 4x (1024x256).
- `tools/art_sync.py --game leadysquid sync` imports VALIDATED rows into the game and writes `IMPORT_REPORT.md` here; it never edits this file.
- The BG tiles of the game (obstacle bodies, seabed, reef, rays) are code-drawn by default (AI tiles at this size read as noise): the body rows are the last, optional group.

## Style guide

- 16-bit SNES pixel art, an underwater world seen from the side (a side-scrolling game), in the spirit
  of the underwater stages of SNES platformers: clean shapes, rich but limited colours.
- Crisp pixels: no blur, no anti-aliasing against the background, no gradients made of noise, no soft
  shadows, no dithering noise. Dark outlines (1 pixel at the final size, so about 8 pixels in the image).
- Flat shades: 2 to 4 shades per material, large clear shapes, readable when shrunk to the final size.
- A limited palette: at most about 15 colours per sprite (and per obstacle theme), plus the magenta
  background.
- Light comes from the top-left (the water surface): highlights on the top-left, shading on the
  bottom-right.
- Background: flat magenta #FF00FF everywhere outside the drawing, no magenta inside a drawing. Tiles
  (obstacle bodies) fill their whole cell edge to edge and repeat seamlessly from top to bottom.
- Draw at 8x the final size: a 32x32 frame is about 256x256 pixels, a 24x16 frame about 192x128.
  Frames of one strip sit in ONE horizontal row, evenly spaced, the same size, in the order given.
- No text, no logos, no characters or art from existing games. Everything here is original.

### Model sheet

- **The squid (hero)**: a small, chubby, grumpy-cute squid, seen from the SIDE, swimming to the RIGHT,
  mantle first. About 24x20 pixels in the game, inside a 32x32 frame, centred on its body.
  - Body: a big rounded purple mantle (the pointed end, on the right) with two small triangular fins at
    its tip, one above and one below; a round head behind it (left of the mantle); four short thick
    tentacles trailing behind (to the left), slightly curled at the tips.
  - Colours: medium purple body (#9652C2), light lilac highlights on the top-left (#C286E6, #EAC8FA),
    dark purple shading (#642E8A), three small pink spots on the mantle (#DE6EB0), dark purple outline.
  - Face: ONE big eye visible (side view) on the head, white with a black pupil looking forward (to the
    right), a HEAVY LID covering its upper half and a slanted frowning brow (grumpy, a bit sleepy), a tiny
    pouting mouth under it near the tentacles. Cute, not scary.
  - **The lead weights**: a dark grey belt around the middle of the mantle with a small gold buckle, and
    grey lead diving-weight blocks on the belt sticking out above and below the body (like a scuba
    diver's weight belt). They are why it sinks so fast: make them readable.
  - The hitbox is the body (mantle and head); keep the body the same size in every frame.
- **Player 2** is the same squid recoloured pink by the game (do not draw it).
- **Obstacle themes** (columns 24 px wide, a top one hanging from the surface and a bottom one rising
  from the seabed, a gap between them): kelp (olive-green stalks, leaf blades, golden air bladders),
  coral (pink and coral-red branching coral, orange polyps, a brain-coral dome), sunken ship masts (dark
  wood with iron hoops, rope shrouds and ratlines; a yard with a torn sail, a crow's nest), anchor
  chains (big rusty links, a grey iron anchor hanging at the end, a rusty mooring buoy).
- **Mid-ground props**: muted, darker, blue-green tones (they sit far behind the obstacles): a shipwreck,
  a treasure chest with a gold glint, rocks with barnacles, coral and kelp clumps, an amphora.

### Palettes

- **Squid**: purples and lilacs, pink spots, lead greys, a gold buckle.
- **Kelp**: olive and yellow-greens, dark green outline, golden bladders.
- **Coral**: pinks, coral red, orange polyps, dark red outline.
- **Masts**: dark to light browns, iron greys, pale sail cloth.
- **Chains**: rust browns and oranges, iron greys.
- **Mid-ground**: deep blue-greens, a muted brown for wood, one gold glint.
- **UI**: white digits with a navy outline; bronze, silver, gold and pearl-pink shells.


## 1. The squid (the hero, every frame of play)

| ID | Status | Sheet | Frame size | Frames | Description / prompt | Notes |
|---|---|---|---|---|---|---|
| squid_tilt | TODO | sprites.png | 32x32 | 6 | The squid swimming to the RIGHT, the whole body rotated around its centre (SNES pre-rendered rotation), tentacles trailing straight behind. 6 frames left to right, evenly spaced: 1) tilted nose UP by 20 degrees; 2) level (horizontal); 3) nose down 22 degrees; 4) nose down 45 degrees; 5) nose down 67 degrees; 6) nose pointing straight DOWN (90 degrees): the tentacles trail UPWARD. Frame size 32x32 (draw each about 256x256). Flat magenta around the drawing. |  |
| squid_idle | TODO | sprites.png | 32x32 | 2 | The squid level, floating in place ('get ready' pose), looking a bit grumpy. 2 frames left to right, evenly spaced: 1) tentacles curled down; 2) tentacles curled up (a gentle bob). Frame size 32x32 (draw each about 256x256). Flat magenta around the drawing. |  |
| squid_flap | TODO | sprites.png | 32x32 | 3 | The squid's jet of water, tilted nose up by 20 degrees. 3 frames left to right, evenly spaced: 1) SQUEEZE: the mantle narrower and longer, tentacles bunched tight together; 2) JET: tentacles fanned wide open behind it; 3) RECOVER: halfway back to normal. Frame size 32x32 (draw each about 256x256). Flat magenta around the drawing. |  |
| squid_hit | TODO | sprites.png | 32x32 | 1 | The squid just hit something: dazed X-shaped eye, tentacles splayed, level. One frame. Frame size 32x32 (draw each about 256x256). Flat magenta around the drawing. |  |
| squid_sink | TODO | sprites.png | 32x32 | 2 | The dazed squid sinking nose-down (pointing straight down), X eye, limp tentacles trailing upward. 2 frames left to right, evenly spaced: 1) tentacles to the left; 2) tentacles to the right. Frame size 32x32 (draw each about 256x256). Flat magenta around the drawing. |  |
| squid_rest | TODO | sprites.png | 32x32 | 1 | The dazed squid knocked out on the seabed, nose down in the sand, X eye. One frame. Frame size 32x32 (draw each about 256x256). Flat magenta around the drawing. |  |

## 2. Obstacle caps (sprites at the gap edges)

| ID | Status | Sheet | Frame size | Frames | Description / prompt | Notes |
|---|---|---|---|---|---|---|
| kelp_cap_top | TODO | obstacles.png | 24x16 | 1 | Kelp column: the LOWER END of the column hanging from the surface: a crown of kelp blades with golden air bladders, hanging DOWN (the lower end of the top column). Its bottom edge is the top of the gap; the upper half joins the body. One frame. Frame size 24x16 (draw each about 192x128). Flat magenta around the drawing. |  |
| kelp_cap_bottom | TODO | obstacles.png | 24x16 | 1 | Kelp column: the UPPER END of the column rising from the seabed: a crown of kelp blades with golden air bladders, pointing UP (the upper end of the bottom column). Its top edge is the bottom of the gap; the lower half joins the body. One frame. Frame size 24x16 (draw each about 192x128). Flat magenta around the drawing. |  |
| coral_cap_top | TODO | obstacles.png | 24x16 | 1 | Coral pillar: the LOWER END of the column hanging from the surface: a rounded brain-coral dome, hanging DOWN (the lower end of the top column). Its bottom edge is the top of the gap; the upper half joins the body. One frame. Frame size 24x16 (draw each about 192x128). Flat magenta around the drawing. |  |
| coral_cap_bottom | TODO | obstacles.png | 24x16 | 1 | Coral pillar: the UPPER END of the column rising from the seabed: a rounded brain-coral dome, pointing UP (the upper end of the bottom column). Its top edge is the bottom of the gap; the lower half joins the body. One frame. Frame size 24x16 (draw each about 192x128). Flat magenta around the drawing. |  |
| masts_cap_top | TODO | obstacles.png | 24x16 | 1 | Sunken ship mast: the LOWER END of the column hanging from the surface: a wooden yard (a horizontal spar) with a torn pale sail hanging down from it. Its bottom edge is the top of the gap; the upper half joins the body. One frame. Frame size 24x16 (draw each about 192x128). Flat magenta around the drawing. |  |
| masts_cap_bottom | TODO | obstacles.png | 24x16 | 1 | Sunken ship mast: the UPPER END of the column rising from the seabed: a wooden crow's nest (a round lookout barrel with an iron rim). Its top edge is the bottom of the gap; the lower half joins the body. One frame. Frame size 24x16 (draw each about 192x128). Flat magenta around the drawing. |  |
| chains_cap_top | TODO | obstacles.png | 24x16 | 1 | Anchor chains: the LOWER END of the column hanging from the surface: a grey iron anchor hanging from the chains (stock, shank, curved arms and flukes). Its bottom edge is the top of the gap; the upper half joins the body. One frame. Frame size 24x16 (draw each about 192x128). Flat magenta around the drawing. |  |
| chains_cap_bottom | TODO | obstacles.png | 24x16 | 1 | Anchor chains: the UPPER END of the column rising from the seabed: a rusty round mooring buoy with rivets, the chain attached below it. Its top edge is the bottom of the gap; the lower half joins the body. One frame. Frame size 24x16 (draw each about 192x128). Flat magenta around the drawing. |  |

## 3. Score, medals, effects

| ID | Status | Sheet | Frame size | Frames | Description / prompt | Notes |
|---|---|---|---|---|---|---|
| digits | TODO | sprites.png | 16x16 | 10 | Big score digits 0 to 9, chunky and rounded, white with a light-blue lower half and a navy outline, about 12x16 pixels each inside 16x16. 10 frames left to right, evenly spaced: 1) 0; 2) 1; 3) 2; 4) 3; 5) 4; 6) 5; 7) 6; 8) 7; 9) 8; 10) 9. Frame size 16x16 (draw each about 128x128). Flat magenta around the drawing. |  |
| medal | TODO | sprites.png | 24x24 | 4 | Shell medals: a scallop shell (a fan with ribs, a hinge at the bottom) with a white shine. 4 frames left to right, evenly spaced: 1) bronze; 2) silver; 3) gold; 4) pearl: a pale pink open shell holding a white pearl. Frame size 24x24 (draw each about 192x192). Flat magenta around the drawing. |  |
| hint | TODO | sprites.png | 16x16 | 2 | A round silver game-pad button with a dark letter A. 2 frames left to right, evenly spaced: 1) up; 2) pressed (1 px lower). Frame size 16x16 (draw each about 128x128). Flat magenta around the drawing. |  |
| bubble | TODO | sprites.png | 8x8 | 4 | Air bubbles: a light-cyan ring with a white highlight, transparent inside. 4 frames left to right, evenly spaced: 1) small (3 px); 2) medium (5 px); 3) big (7 px); 4) popping (a broken ring). Frame size 8x8 (draw each about 64x64). Flat magenta around the drawing. |  |
| ink | TODO | sprites.png | 16x16 | 4 | The puff of dark purple ink and tiny bubbles left behind by a flap, fading. 4 frames left to right, evenly spaced: 1) a dense small cloud; 2) bigger and lighter; 3) thin and dithered; 4) almost gone. Frame size 16x16 (draw each about 128x128). Flat magenta around the drawing. |  |
| weight | TODO | sprites.png | 8x8 | 2 | A small grey lead diving weight block. 2 frames left to right, evenly spaced: 1) lying flat; 2) tilted, bouncing. Frame size 8x8 (draw each about 64x64). Flat magenta around the drawing. |  |
| sparkle | TODO | sprites.png | 8x8 | 2 | A small yellow-white four-pointed sparkle. 2 frames left to right, evenly spaced: 1) big; 2) small, with diagonals. Frame size 8x8 (draw each about 64x64). Flat magenta around the drawing. |  |

## 4. Title logo

| ID | Status | Sheet | Frame size | Frames | Description / prompt | Notes |
|---|---|---|---|---|---|---|
| title_logo | TODO | title logo | 256x64 | 1 | Title logo 'LEADY SQUID': chunky SNES-style letters, 'LEADY' in heavy riveted lead-grey metal, 'SQUID' in rounded glossy purple, a dark outline and a navy drop shadow, a few bubbles around. 256x64 in the game: draw it at 4x (1024x256) on flat magenta |  |

## 5. Mid-ground props

| ID | Status | Sheet | Frame size | Frames | Description / prompt | Notes |
|---|---|---|---|---|---|---|
| shipwreck | TODO | props.png | 128x64 | 1 | A sunken wooden sailing ship lying half in the sand, broken mast leaning, muted blue-grey-brown tones (mid-ground, low contrast). One frame. Frame size 128x64 (draw each about 1024x512). Flat magenta around the drawing. |  |
| chest | TODO | props.png | 32x24 | 1 | A treasure chest, lid ajar, a thin line of gold inside, muted. One frame. Frame size 32x24 (draw each about 256x192). Flat magenta around the drawing. |  |
| rock_small | TODO | props.png | 32x24 | 1 | A small rounded rock with a few barnacles, muted blue-grey. One frame. Frame size 32x24 (draw each about 256x192). Flat magenta around the drawing. |  |
| rock_big | TODO | props.png | 64x40 | 1 | A big rounded rock, muted blue-grey, lit from the top-left. One frame. Frame size 64x40 (draw each about 512x320). Flat magenta around the drawing. |  |
| coral_clump | TODO | props.png | 32x32 | 1 | A clump of branching coral, muted pink-grey. One frame. Frame size 32x32 (draw each about 256x256). Flat magenta around the drawing. |  |
| kelp_clump | TODO | props.png | 16x48 | 1 | A tall kelp plant, muted blue-green. One frame. Frame size 16x48 (draw each about 128x384). Flat magenta around the drawing. |  |
| amphora | TODO | props.png | 16x24 | 1 | An old clay amphora half buried in the sand, muted brown. One frame. Frame size 16x24 (draw each about 128x192). Flat magenta around the drawing. |  |

## 6. Obstacle bodies (BG tiles: code-drawn by default, AI versions optional)

| ID | Status | Sheet | Frame size | Frames | Description / prompt | Notes |
|---|---|---|---|---|---|---|
| kelp_body | TODO | obstacles.png | 24x16 | 2 | Kelp column, the repeating body: a bundle of three twisting olive-green kelp stalks with leaf blades filling the width. It must repeat seamlessly from top to bottom and fill the 24-px width (the whole column is solid). 2 frames left to right, evenly spaced: 1) variant 1; 2) variant 2. Frame size 24x16 (draw each about 192x128). Fills the whole cell edge to edge, no magenta inside. |  |
| coral_body | TODO | obstacles.png | 24x16 | 2 | Coral pillar, the repeating body: a pillar of pink branching coral with grooves, knobs and orange polyps. It must repeat seamlessly from top to bottom and fill the 24-px width (the whole column is solid). 2 frames left to right, evenly spaced: 1) variant 1; 2) variant 2. Frame size 24x16 (draw each about 192x128). Fills the whole cell edge to edge, no magenta inside. |  |
| masts_body | TODO | obstacles.png | 24x16 | 2 | Sunken ship mast, the repeating body: a dark wooden mast with iron hoops, rope shrouds and ratlines on both sides filling the width. It must repeat seamlessly from top to bottom and fill the 24-px width (the whole column is solid). 2 frames left to right, evenly spaced: 1) variant 1; 2) variant 2. Frame size 24x16 (draw each about 192x128). Fills the whole cell edge to edge, no magenta inside. |  |
| chains_body | TODO | obstacles.png | 24x16 | 2 | Anchor chains, the repeating body: two rusty anchor chains side by side, big links, filling the width. It must repeat seamlessly from top to bottom and fill the 24-px width (the whole column is solid). 2 frames left to right, evenly spaced: 1) variant 1; 2) variant 2. Frame size 24x16 (draw each about 192x128). Fills the whole cell edge to edge, no magenta inside. |  |

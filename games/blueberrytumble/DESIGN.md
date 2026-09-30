# Blueberry Tumble: design

(c) 2026 Pierre-Louis Boyer (8BCraft), CC BY-NC-SA 4.0 (games/blueberrytumble/LICENSE). A game for the RetroStone
virtual console (docs/spec.md), in the 8BCraft house style (docs/art-direction.md). The name is set in one place:
`GAME_TITLE` / `GAME_NAME` / `GAME_ID` in src/tuning.h (the logo, the frontends' name); the build's file names
(the exe) are in game.mk.

## Pitch
A blueberry fell off its bush at the top of the mountain and tumbles all the way down to the valley. It rolls by
itself; you only jump. Thorn bushes, rocks, fallen logs, crevasses, rolling pine cones and icy patches come on the
beat of the music: one button, jump on the beat, splat, one more try. It grows into a big snowberry in the snow,
rides a maple leaf through the forest, and the further it rolls the faster the music gets.

It is a rhythm auto-runner (the genre of Geometry Dash). Only the genre's **mechanics** are used: an automatic run,
a one-button jump, obstacles on the beat, jump pads, mid-air jump orbs, a flying mode, an instant retry. Nothing is
taken from Geometry Dash (RobTop Games, proprietary): no name, icon, cube, level, layout, music, UI or text. Its
feel is known from published measurements (see "Feel sources"); the numbers are converted to our grid, and all
the code, art and music are ours.

**Endless only** (the owner's decision, 2026-09-30): no levels, no course select, no practice mode. Like Leady
Squid: title -> get ready -> the run -> splat -> game over (distance, best, medal) -> one button to retry.

## The grid (why jumps are exact and learnable)
- A **block** (a cell) is 16 x 16 px, **1 metre** of distance. The berry is about one block.
- The music's **beat is 4 blocks** (64 px): a cell is a 16th note. Every object sits on a cell, so every obstacle
  is on a 16th-note subdivision of the beat (the beat-sync test proves it).
- The scroll speed is **tied to the tempo**: 64 px per beat, so speed = 64 x BPM / 3600 px per frame. The course
  position is computed from the frame count exactly (no drift): x(f) = x_b + (f - f_b) x 64 / frames_per_beat.
- The physics runs on this flat grid: the ground is at height 0, blocks are 16-px cells, jumps are integer
  fixed-point arcs (Q16). **The slope is only drawn**: the playfield is an affine (Mode-7-like) layer sheared
  downward (1 px down per 3 to 8 px across, per biome), and every sprite gets the same shear, so the berry visibly
  tumbles down the mountain while the jumps stay grid-exact.

## The berry
- 16 x 16 sprite: a deep-blue ball, a lighter highlight top-left, a dark navy outline, a tiny leaf stem and a
  **star-shaped calyx** that shows the rotation. It **rolls**: 16 pre-drawn angles (SNES style, no run-time
  rotation); the angle follows the distance rolled (arc length / radius) and it keeps spinning in the air.
- **Squash** on landing (house `squash(1.25)`), **stretch** when it takes off (`squash(0.82)`).
- **Snowberry** (the twist): rolling onto snow packs it in snow. It **grows** to 24 x 24 (a white snowball with the
  berry showing through), is **heavier** (its jump is 22 px high instead of 35) and **smashes** small obstacles
  (thorn bushes, pebbles, pine cones) with a crunch. A shallow stream washes the snow off and it **shrinks** back.
  Snow and water only come inside the snow patterns, which always end with water.
- **Leaf glider** (the mode change): a gust carries a maple leaf; the berry rides it and **glides**: hold A to
  rise, release to sink (the genre's flying mode). The leaf blows away at the end of the glide pattern.
- **Splat**: touching a hazard, running into a block, or falling into a crevasse bursts the berry into juice
  drops with a squelch and a small shake.
- Player 2 is a **raspberry** (the same sprites, hue-rotated: the house palette-swap rule).

## Controls
| Button | Action |
|---|---|
| A (B, X, Y, Up too) | jump; held on the ground: jumps again on each landing; in the air on a dew drop: jump again; gliding: rise |
| Start | start / retry |
| Select (or Start during a run) | pause |
| Pad 2, A on the title or get ready | player 2 joins (race) |

## Obstacles (all on the cell grid)
| Object | Rule | Hitbox (px, in its 16-px cell) |
|---|---|---|
| thorn bush (the spike) | hazard; smashed by a snowberry | 6 wide x 8 high, centred at the bottom |
| hanging thorns | hazard (under a ceiling block) | 6 x 8 at the top |
| pebble pile | hazard, low; smashed by a snowberry | 10 x 6 at the bottom |
| rock block, fallen log | solid: land on its top; running into its side or bottom is a splat | 16 x 16 |
| crevasse | no ground: falling 40 px into it is a splat | the column |
| mushroom (jump pad) | touching its cap launches the berry 72 px (4.5 blocks) high, automatically | 12 x 5 |
| dew drop (jump orb) | pressing A while touching it is a mid-air jump (once per drop) | 20 x 20 |
| pine cone | rolls toward the berry (1/3 or 1/2 of the run speed); hazard; smashed by a snowberry | 10 x 10 |
| icy patch | no grip: the berry slides (it stops rolling) and cannot jump on it | ground |
| snow / water | grow into a snowberry / shrink back | ground |
| leaf gust / leaf drop | start / end the glide (full-height gates) | column |
| golden blueberry | a rare pickup off the safe path: +50 points | 14 x 14 |

The berry's own boxes: a 10 x 10 hazard box (the body is 14 px round: the genre's leniency), a 14-px wide solid
box for landing and edge support, and a 6 x 6 core that crashes into walls. The snowberry: 16 x 16, 22, 10 x 10.

## Feel sources (numbers only)
| # | Source | Kind | What we took |
|---|---|---|---|
| 1 | kompletelykoolkid, "the real speed of the cube in Geometry Dash", GD Forum, https://gdforum.freeforums.net/thread/35765/real-speed-cube-geometry-dash (measured on a 10-million-block level with the level-length indicator) | forum post, facts | the speeds: slow 8.3719, normal 10.3859, 2x 12.9139, 3x 15.5999 blocks/s |
| 2 | "Speed and Maths: the numbers behind GD speeds", GD Forum, https://gdforum.freeforums.net/thread/55538/easy-speed-maths-numbers-speeds | forum post, facts | the same (8.368 / 10.376 / 12.903 / 15.595, 4x 19.185 blocks/s); 1 block = 30 units |
| 3 | "Length of a jump (helpful for creating)", Geometry Dash Wiki discussions, https://geometry-dash.fandom.com/f/p/2846072015683784356 (read through the search engine's excerpt; the page answers 402) | forum post, facts | a normal-speed jump is 3.6 blocks long, +0.9 block per speed step; its height is "a little over 2 blocks" at every speed (the jump is fixed in time) |
| 4 | "How much does the cube rotate when you jump", Steam community, https://steamcommunity.com/app/322170/discussions/0/5729109343950073367/ | forum, facts | at 1x only three spikes in a row can be jumped |
| 5 | GD Creator School, "Advanced hitboxes", https://www.gdcreatorschool.com/docs/guides/gameplay-1/advanced-hitboxes/ | guide, facts | the player's main hitbox is one block (30 x 30 units); a smaller inner hitbox is used against solid blocks (running into a wall kills); spike hitboxes are smaller than their sprites |
| 6 | Steam guide "How to PROPERLY sync levels", https://steamcommunity.com/sharedfiles/filedetails/?id=792172526, and ludo.guide "Syncing to music: manual and automatic" | guides, facts | the beat-sync approach: blocks per beat = speed / (BPM / 60); obstacles and clicks go on the beat guidelines |

Looked at and **not used**: gehuybre/geo-dash (no licence file: nothing taken), OpenGD (GPL-3: not read),
StarryDawn72/open-source-gd-project (reconstructions of the proprietary game's code: not read). No clone's code was
read or copied; only the facts above were used.

### From the sources to our grid (src/tuning.h)
| Quantity | Reference | Blueberry Tumble |
|---|---|---|
| block | 30 units | 16 px = 1 m |
| normal speed | 10.386 blocks/s | 10.0 blocks/s at 150 BPM (64 px per beat); the tiers: 8.5 (128 BPM, like the reference's slow 8.37) to 11.4 (171 BPM) |
| jump length | 3.6 blocks at normal speed = 0.347 s | 21 frames (0.35 s): 3.5 blocks at 150 BPM, 3.0 at 128, 4.0 at 171 (reference: 2.7 slow, 3.6 normal, 4.5 at 2x) |
| jump height | a little over 2 blocks | 35 px = 2.19 blocks (lands on a 2-block step) |
| gravity, jump speed | (derived) | g = 0.636 px/frame^2, v = 7.0 px/frame: h(n) = 7n - 0.318 n(n+1), apex 35.0 px at frame 11, down at frame 21 |
| spikes in a row | 3 at normal speed | 2 below 160 BPM; 3 only where the validator finds a >= 3-frame window |
| player hitbox | 1 block, an inner box for walls | a 10-px hazard box, a 14-px solid box, a 6-px core |
| spike hitbox | smaller than the sprite | 6 x 8 px of the 16 x 16 cell |
| jump pad, orb | (not measured) | pad 72 px (4.5 blocks); orb = a full jump from where the berry is |

## Tempo, speed, biomes and music
The mountain, top to bottom, then again at night, faster:

| Biome | Metres | Tempo | Frames/beat | Speed | Slope | Time of day |
|---|---|---|---|---|---|---|
| Snowy summit | 0-383 | 128 BPM | 28.125 | 2.28 px/f (8.5 m/s) | 1/3 | morning |
| Pine forest | 384-767 | 137.1 | 26.25 | 2.44 (9.1) | 1/4 | midday |
| Blueberry meadows | 768-1151 | 150 | 24 | 2.67 (10.0) | 1/5 | afternoon |
| Valley village | 1152-1535 | 160 | 22.5 | 2.84 (10.7) | 1/8 | sunset |
| the loop at night | 1536+ | 171.4 | 21 | 3.05 (11.4) | per biome | night |

- A biome is **24 bars** (384 m). The **tempo and the scroll speed step up at each biome change** (a gate: flat
  ground, a signpost). The tempos are those whose ProTracker tick is a whole number of samples at 32 kHz
  (tick = 80000 / T samples) and whose biome lasts a whole number of frames, so the music never drifts.
- Each biome has its own code-made track (tools/make_music.py: the house instruments and drums), a 24-bar song
  (6 patterns of 4 bars) that starts on the gate's first beat: the music **evolves with the biome**. The night loop
  plays night variants at 171 BPM.
- **Beat pulse**: the sky brightens a little on every beat (more on the downbeat) and decays in 10 frames, like the
  genre's pulsing background. It is computed from the berry's x (x / 64 = beats).

## Patterns (the content, src/patterns.c)
A pattern is **one idea**, 1 to 4 bars long: a small C function that places cells on the grid from a parameter
vector. Every parameter has a range (a few ranges depend on the tempo tier), so the whole space of a pattern is
**enumerable** and the validator tests every combination at every tier. The director draws the parameters inside
the difficulty band, so runs never repeat; the scenery (flowers, stones, grass, bushes, the mid-ground) comes from
a hash of the column, so it varies too without touching the physics. The list, with each pattern's measured
difficulty, is in "Pattern table" at the end.

## The director (src/director.c): a stream that feels hand-made
- Seeded (a run's seed comes from the frame of the first press; `--opt seed=N` fixes it): same seed, same run.
- **Difficulty is measured, not guessed** (next section). Every pattern instance (pattern + parameters + tier) has
  a score 0-100 in the generated table src/tables.inc. The director aims at a **target curve** (src/tuning.h
  `DIFF_*`): a slow saturating ramp with distance plus **waves**: the tension rises through a wave (about 15 s),
  then a **breather**, then the next wave starts a little higher. It picks a pattern whose scores meet the band
  [target - DIFF_BAND_LO, target + DIFF_BAND_HI] (25 below, 5 above), then parameters inside the band. No instance exceeds
  target + DIFF_SPIKE.
- The first 256 m (30 s) use only easy instances (score <= DIFF_EASY_MAX): new players learn the jump first.
- **No pattern twice in a row**, and the last DIRECTOR_MEMORY patterns are avoided unless nothing else fits.
- **Links**: every pattern ends in normal mode (mode changes happen only inside their own patterns). The validator
  measures each pattern's *tail* (how many frames after its end the berry can still be airborne when the player hits
  the windows, even at their late edge) and *head* (how late the berry may land before it and still clear it with
  3-frame windows); the director inserts a **bridge** of flat ground (whole beats) where tail(A) > head(B). Every
  pair is then simulated.
- **Biomes**: at each 384-m boundary the director fills to the bar and places the gate; tempo, slope, palette,
  sky and music change there.
- **Golden blueberries**: patterns with a coin spot place one with a small chance (COIN_CHANCE, never in the first
  150 m); the validator proves every coin spot is collectable with >= 3-frame windows.

## Measured difficulty
For every pattern instance the validator's solver finds the path a careful player takes (every press in the middle
of its timing window) and measures:

| Measure | Meaning | Normalised (0..1) |
|---|---|---|
| W | the tightest timing window, frames (the frames on which the decisive press or release still works) | w = clamp((14 - W) / 11): 3 frames -> 1, 14+ -> 0 |
| D | required presses per second on the careful path | d = clamp(D / 3) |
| S | distinct skills (jump, orb, pad, glide, snow, ice, cone) | s = clamp((S - 1) / 3) |
| R | reaction time: the least time between an obstacle entering the screen and the press it needs, frames | r = clamp((70 - R) / 50) |

**score = 100 x (0.40 w + 0.30 d + 0.15 s + 0.15 r)** (weights `DIFF_W_*` in src/tuning.h). The screen bot's
failure rate per pattern over many full runs is the sanity check: `make blueberrytumble-difficulty` prints both
and their rank correlation, and draws docs/difficulty.png (the generated difficulty over distance for a few seeds).

## The validator (tests/validate.c, in `make blueberrytumble-check`)
The **input-search solver** (tests/solver.c) runs the game's own physics (src/physics.c on src/course.c): a
depth-first search over "A held / A released" on every frame, with a memo of dead states. The careful-player path:
keep the current input while that still leads to the end; when it must change, find every frame on which changing
still works (the **window**) and change in its middle. It proves:
1. every pattern, at every parameter combination and every tier it can be generated at, is completable with every
   window >= 3 frames (the tightest window per pattern is printed);
2. every coin spot is collectable with windows >= 3 frames;
3. the links: tails and heads match the committed table; every pair the director can chain, with its bridge, is
   completable (the tail-heaviest and head-tightest combinations and random ones);
4. the stream: thousands of seeds from the real director are completable end to end (a horizon solver); the
   pattern frequencies are printed;
5. beat sync: every object is on the 16-px grid (a 16th note), every pattern starts on a beat, every biome on a bar,
   and speed x frames per beat = 64 px on every tier;
6. the difficulty curve: the rolling 30-s mean rises, no instance exceeds target + DIFF_SPIKE, breathers are easier
   than their neighbours, the first 256 m are easy;
7. the committed src/tables.inc matches what the solver measures now (`make blueberrytumble-tables` regenerates it).

## Screens (the house flow, the games/common kit)
title (the logo, the berry rolling in place under it, PRESS A TO ROLL with the A glyph, BEST, the copyright line)
-> get ready (the berry at the top of the first slope, the 2P join line) -> the run (the distance in big digits,
the golden blueberries, "ATTEMPT n" in the world at the start, the biome's name at each gate) -> splat (hit-stop,
shake, juice) -> game over (the GAME OVER banner, the panel sliding up: SCORE, BEST or NEW BEST, MEDAL, and the
distance) -> A: get ready again.

- **Medals by distance** (the house tiers): bronze 200 m, silver 500 m, gold 1000 m, pearl 1600 m (the night).
- **Score** = metres + 50 per golden blueberry. Save RAM: best score, best distance, runs (the attempt counter),
  golden blueberries collected, medals won.

## Two players (race)
Player 2 joins with A on pad 2 on the title or get ready. The raspberry rolls the same course 28 px behind the
blueberry, with its own pad; the scroll goes on while one is alive. The panel shows both scores and the winner.

## Screen and layers
| Layer | Contents | Scroll |
|---|---|---|
| backdrop | the sky gradient per biome and time of day, one colour per line (raster), the beat pulse | - |
| BG4 | the far mountains (no outline, 2-3 tones) | 1/4 |
| BG3 | the biome's mid-ground (snow ridges, pines, berry bushes, village roofs) | 1/2 |
| BG2 | the playfield: ground, blocks, logs, thorns, snow, ice, water, crevasses; **affine, sheared** into the slope | 1 |
| BG1 | the house UI kit | fixed |
| OBJ | the berries (OBJ 0, P2 OBJ 1), dew drops, mushrooms, cones, coins, leaves, juice (OBJ 2), the kit (OBJ 3) | |

## Art and sound
Code-drawn only (tools/make_art.py with games/common/tools/house_style.py): no image generation. Effects synthesised
at start-up with the house synthesiser: squelch (splat), boing (pad), chime (dew drop), whoosh (glide), crunch
(smash), puff (grow), splash (shrink), and the house UI set. Music: tools/make_music.py with house_music.py.

## Tests (make blueberrytumble-check)
- `tests/test_physics.c`: the tuning against the sources (speeds per tier, the jump arc, pad and snow heights, the
  frame clock), collisions (thorn edges, walls, landing, snap, gaps, ice, pads, orbs, cones, snow and water, the
  glide) and the director's determinism.
- `tests/validate.c`: the validator above.
- `tests/smoke_test.sh`: the flow, pause, save RAM, 2 players, determinism (state hash and picture, twice), the bot.
- **The bot** (`--opt bot=1`, src/bot.c) plays from the **screen only** (the PPU's playfield map and scroll, and the
  OAM), like a player; its average distance over 10 seeds must reach a target.
- `tests/ui_test.sh`: the screens; `tests/state_test.sh` and tools/state_audit.py: save states.

## Pattern table
The 21 patterns (src/patterns.c). Score = the measured difficulty (min / mean / max over the parameter combinations)
at tier 2 (150 BPM); "window" = the tightest timing window over all tiers, frames. Coin = a golden-blueberry spot.
The full table per tier (combinations, scores, windows, tails, heads, coin windows) is printed by
`make blueberrytumble-difficulty` and `build/blueberrytumble-validate.txt`.

| # | Pattern | Idea | Parameters (ranges) | Biomes | Score @150 BPM | Window |
|---|---|---|---|---|---|---|
| 1 | hop | thorn bushes on the beat | count 1-4, spacing 1 / 1.5 / 2 beats, single or double bushes | all | 12 / 22 / 35 | 5 |
| 2 | rows | long thorn rows in one jump | rows 1-2, width 2 (3 at 160+ BPM), tight or loose | all | 25 / 26 / 27 (76 at 160) | 4 |
| 3 | stairs | climb the rocks, drop off the top | steps 1-3, step 3-5 cells, a thorn at the bottom | all | 8 / 13 / 19 | 7 |
| 4 | logs | roll along fallen logs over thorn pits | logs 1-3, length 3-5, pit 1-2 | forest+ | 4 / 10 / 17 | 14 |
| 5 | pillars | hop from pillar to pillar over thorns | count 2-4 (3 at night), level / up-down / rising, spacing 3-4 | all | 37 / 55 / 71 | 3 |
| 6 | gaps | jump the crevasses | count 1-3, width 2-3, spacing 4-7 | all | 8 / 19 / 31 | 7 |
| 7 | islands | stepping stones over a long crevasse | stones 2-4, spacing 3-4 (+1 at 160+) | all | 37 / 54 / 79 | 3 |
| 8 | mushroom | a pad launches over a wall or thorns | wall or thorn field, size 1-3 (by tier) | all | 0 (no input) | - |
| 9 | padsteps | a pad throws the berry onto a ledge | height 2-3, length 3-6, thorns after | all | 0 / 0 / 0 | 14 |
| 10 | dewdrop | a dew drop over a crevasse: jump again | width 4-6 (5 at 128), a bush on the far side | all | 25 / 44 / 67 | 3 |
| 11 | dewchain | a rhythm of dew drops across the void | drops 2-3 (4 at night), level / zigzag / rising, a far bush | all | 53 / 65 / 72 | 3 |
| 12 | cones | pine cones roll down at you | cones 1-3, spacing 6-10, slow or fast | forest+ | 21 / 24 / 29 | 12 |
| 13 | conehop | a thorn bush and a cone | order, spacing 5-8, bush width 1-2 | forest+ | 23 / 31 / 43 | 7 |
| 14 | ice | an icy patch: no grip, slide, then jump | ice 3-6 cells, icicles, the thorn 1-2 cells after | summit | 24 / 26 / 30 | 5 |
| 15 | snowsmash | grow into a snowberry and smash through | smashables 2-5, rocks 0-2, cones | summit | 0 / 9 / 17 | 14 |
| 16 | snowjumps | the heavy snowberry's low jumps | count 1-3, a rock instead of a gap, spacing 4-6 | summit | 0 / 2 / 9 | 12 |
| 17 | leaftunnel | ride the leaf between floor and ceiling thorns | segments 2-4, height 5-7 blocks, floor or ceiling first | forest+ | 3 / 8 / 14 | 25 |
| 18 | leafweave | glide through low and high openings | fences 2-4, opening 3-4 blocks, spacing 6-8 | forest+ | 13 / 38 / 52 | 8 |
| 19 | ledge | run a rock ledge, hop its thorns, jump off | length 4-8, thorns 0-2, a drop over thorns | all | 6 / 16 / 55 | 4 |
| 20 | phrase | a two-bar phrase of bushes and steps | 8 hand-written phrases, forwards or mirrored | all | 11 / 21 / 26 | 7 |
| 21 | breather | open ground to breathe | 1-2 bars, a pebble | all | 0 / 2 / 6 | 12 |

"all" = every biome; "forest+" = forest, meadows, village; the night loop takes every pattern. 1680 instances in all.
Ranking (the mean score at 150 BPM): mushroom, padsteps, breather, snowjumps < leaftunnel, snowsmash, logs, stairs <
ledge, gaps, phrase, hop, cones, rows, ice < conehop, leafweave, dewdrop < islands, pillars, dewchain: pads and snow
(automatic) are the easiest, single thorns and gaps the core, orbs and precise landings the hardest, as a player would
rank them. The glide patterns score low because their windows are wide (the leaf is forgiving).

## Results (2026-10-01)
- **Validator** (`build/blueberrytumble-validate.txt`): 1680 instances, all completable with every window >= 3 frames;
  every coin spot collectable; 1578 chainable pairs x 3 combinations all fair (830 pairs get a 1-beat bridge);
  2000 streams x 700 m and 60 x 3000 m (every tier, the night loop) solved end to end; the rolling 30-s mean rises
  from 11 to 38 over 2600 m; 97% of breathers are easier than the two patterns before them.
- **The bot** (screen only), 10 seeds: 1241 to 2103 m, mean 1537 m (the test's target: 800 m). Over 20 more seeds
  (`make blueberrytumble-difficulty`): 1210 to 2159 m, mean 1602 m; its splats are mostly on pillars (13 of 20:
  landing on a one-block top from its own model of the berry). Rank correlation between the measured score and its
  failure rate: 0.29 (few failures: one per run).
- **The curve**: docs/difficulty.png (5 seeds, 3200 m: the instances, their 300-m rolling mean, the target and its
  waves, the gates).
- **Cost** (`make blueberrytumble-bench`, a whole god-mode run through every biome and the night): 436 us per frame on
  average on the development PC (425 us of it the PPU, the affine playfield included), 1122 us at worst (Leady
  Squid: 451 / 1851 us). At 15-20x on the RetroStone2's A20: 6.5-8.7 ms on average, 17-22 ms for the single worst
  frame (a host outlier, as Leady Squid's). VRAM (tiles and the 24 KiB of maps) under the 64 KiB guideline and
  the sample memory under 64 KiB (the game's sounds are stored at 16 kHz): strict mode checks both in the smoke test.

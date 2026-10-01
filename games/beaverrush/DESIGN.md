# Beaver Rush: design

(c) 2026 Pierre-Louis Boyer (8BCraft): code MIT, assets (art, music, sound, this document) CC BY-NC-SA 4.0
(games/beaverrush/LICENSE). A game for the
RetroStone virtual console (docs/spec.md), in the 8BCraft house style (docs/art-direction.md, games/common).

## Pitch
A young beaver has one job: gnaw the giant tree, log by log, before the river runs dry. Every press of Left
or Right hops the beaver to that side of the trunk and gnaws the bottom log away: the trunk drops by one,
chips fly, and the log tumbles into the river, floats off and becomes part of the family's dam in the
background. Branches come down with the trunk: never be under one. A timer bar drains faster and faster;
only gnawing fills it. One life, instant retry, a best score to beat.

The mechanics follow the genre's reference feel (Timberman, Digital Melody, 2014): a trunk of stacked
segments, a two-sided chop that also moves the player, a branch at head height after the drop kills, a
draining bar that each chop refills a little, the drain speeding up by level. Nothing else is taken from it:
no name, art, sound, layout or text. The twist (the dam, the scenes, the woodpecker, the versus rules), the
characters, the screens, the sounds and the music are ours.

## Legal and originality
- Timberman is proprietary. We use only its **mechanics** (not protected), as described by published reviews
  and guides and as measured in permissively licensed clones.
- Only MIT-licensed clones were read, only for their numbers and rules; the code of Beaver Rush is our own
  (C11 on the RetroStone SDK, integer fixed point). No clone's art or sound was looked at or used.
- All sources and their licences: "Feel sources" below.

## Rules
### The trunk
- A stack of **segments** 24 px tall. Segment 0 is the bottom one, level with the beaver's head and its
  teeth; segments 0..7 are on screen (a branch is visible 7 gnaws before it reaches the head: the reference
  shows it 4 or more gnaws ahead).
- Each segment has a branch on the **left**, on the **right** or **none**; some are **golden logs**.
- **Gnaw** (Left or Right): the beaver moves to that side and gnaws segment 0 away. The trunk drops by one.
  - **Hit A (the drop)**: if the new segment 0 has a branch on the beaver's side, the branch lands on its
    head: bonk, game over.
  - **Hit B (walking in)**: pressing the side where segment 0 holds a branch walks the beaver into it
    (the branch is at head height on that side): bonk, game over. The log is not gnawed.
  - The rules are instant: the drop is shown in 4 frames, but a press is resolved on the frame it happens,
    so fast players are never slowed by the animation.
- **Fairness**: with hit B, a branch on one side followed directly (the next segment up) by a branch on the
  other side cannot be survived: the beaver cannot move under the first and stays under the second. The
  generator therefore never puts two branches on **opposite sides in adjacent segments**; alternating
  branches always have an empty segment between them. Every other sequence is survivable (the proof is
  short: under a branch the beaver must stay, and the next segment is empty or on the same side; under an
  empty segment it can choose the side the next segment leaves free). The fairness test checks it over
  thousands of seeds by brute force (every reachable beaver position, every step).

### The timer
- A bar on top of the screen. It starts **full** and does not move until the first gnaw.
- It drains continuously; each gnaw refills a little; a golden log refills a quarter. Empty: the beaver runs
  out of breath (it slumps, "zzz"): game over.
- The drain goes up by **level**: one level per **20 logs** (as the reference). The curve is the reference
  clone's break-even chop rate (below): 2.9 gnaws per second to hold the bar at level 0, 4.6 at level 1, 5.3
  at level 2, 5.8 at level 4, 6.5 at level 15 (300 logs). From level 15 on we add 0.12 gnaws/s per level (an
  endgame ramp, ours), so every run ends: 7.7/s at 500 logs, 8.9/s at 700.

### Generation
Segments are made one at a time by a small generator with its own xorshift RNG (the seed is the run's; the
2-player game gives both trees the same seed, so the same sequence). Per **stage** (every 50 logs):

| Stage (segments) | Single-segment branch chance | Chunks | Branch share (measured, 2000 seeds) |
|---|---|---|---|
| 0 (0-49) | 40% (the reference's 2 in 5) | singles only; the first 4 segments are empty | 39.5% |
| 1 (50-99) | 44% | + same-side stacks of 2-3 (15% of the chunks) | 55% |
| 2 (100-149) | 48% | + zig-zag runs (10% of the chunks), the woodpecker arrives | 57% |
| 3.. | +4% per stage up to 60% | zig-zags +4% per stage up to 26%, 3 to 6 branches long | 56-58% |

- A branch right above another one is always on the **same side** (a "stack"); otherwise the side is a coin
  toss. When the next planned branch is on the other side of the one just made, an empty segment is put in
  first (the fairness gap).
- **Zig-zag runs**: branches alternating sides with exactly one empty segment between them (L . R . L), the
  fastest fair pattern: a side switch on every second gnaw.
- At most 3 empty segments in a row (the rhythm never goes slack), at most 4 same-side branches in a row.
- **Golden logs**: an empty segment turns golden with a 1-in-24 chance once at least 30 segments have passed
  since the last one (measured: one in 87 segments). Gnawing it: 5 points instead of 1 and a quarter of the
  bar; it floats to the dam as a golden log.

### Stages and the dam (the twist)
- Every gnawed log splashes into the river and **floats to the dam** in the background; the dam visibly grows
  **log by log** (each log is drawn into the dam's tiles at its slot) and the pond behind it rises.
- The dam has **6 sections** of **50 logs** (a 5 x 10 brick pattern of small logs). **Every 50 logs** (a
  milestone) a section completes: a splash on the dam, the beaver family on the dam **cheers**, and the scene
  **advances**: dawn, day, sunset, night; then the next season (summer, autumn, winter, spring) with its
  palettes (foliage, snow, blossoms, falling leaves, snowflakes, fireflies at summer night), and so on.
  After 300 logs the dam is whole: the next milestones plaster the sections with mud and grass, log by log.
- The **woodpecker** (from stage 2): it clings to the trunk beside the next branch on the beaver's side (the
  next place where the beaver will have to switch) and pecks at it; it pecks twice as fast when a zig-zag run
  is on screen. A hint, not a rule: it never changes the game.

### Scoring, medals, save
- Score = logs gnawed (a golden log counts 5). Big digits at the top (the house digits).
- Medals: **acorns** in the four house tiers and colours (docs/art-direction.md: bronze, silver, gold, pearl),
  drawn by make_art.py on the kit's sprite palette: bronze at 50, silver at 100, gold at 200, a pearl
  (crystal) acorn at 300. The milestones (scenes, dam sections) count logs; the score and medals count points.
- Save RAM: the best score, the number of runs, the medals won, the versus wins of each pad.

## Flow
1. **Title** (the house layout): the logo "BEAVER RUSH" (BEAVER in the beaver accent, RUSH in gold), the scene
   at summer dawn with the family on the bank, the beaver at the foot of the tree, the blinking "PRESS A TO
   GNAW" with the A glyph, the best score, "(C) 2026 8BCRAFT - RETROSTONE VC". A gnaw starts the run at once
   (and gnaws); Start goes to get ready.
2. **Get ready** (after a run, or when player 2 joins): the same scene, "GET READY"; the first gnaw starts.
3. **Play**: the score (house digits) and the timer bar at the top; after each milestone the scene's name
   ("SUMMER DAY") for 1.5 s.
4. **Bonk** (a branch) or **out of breath** (the timer): the dizzy face with stars circling, or the slumped
   beaver with rising "z"s; the house shake (3 px, 12 frames) on a bonk; the world stops.
5. **Game over**, 40 frames later: the house banner and panel slide up: SCORE, BEST (NEW BEST), MEDAL (the
   acorn, a sparkle blinking by it). After 0.6 s any gnaw button retries at once: back to 2 with a fresh tree
   (the dam, the pond and the scene start again).

**Pause**: Start during a run (the house pause: dimmed picture, PAUSED); Start again resumes.

## Controls
| Button | Action |
|---|---|
| Left, B (also Y) | gnaw on the left |
| Right, A (also X) | gnaw on the right |
| Start | pause / resume (starts on the title) |
| Pad 2, a gnaw button on the title or get-ready screen | player 2 joins (versus) |

## 2-player versus
Split screen (SDK viewports): the left half is player 1, the right half player 2, each with its tree, its
beaver, its timer and score. Both trees have the **same seed** (the same branch and golden-log sequence) and
the same timer rules: the timers race. The **last beaver standing wins** (a bonk or an empty bar ends that
player; the round ends when one is out; both on the same frame: the higher score wins, else a draw).
**Stolen chips**: each time a player reaches a milestone (every 50 logs) they send **one branch** to the
rival: it appears at the top of the rival's visible trunk (segment 7, seven gnaws of warning, marked with a
red ribbon), on the side where the rival stands, in the first empty segment from 7 up where the fairness rule
still holds (it is never next to a branch of the other side, never on a golden log). Both players get the
same number of chances; a stolen branch is visible as long as any other one.

## Screen and layers
320x240 at 60 Hz. Ground (the near bank) at y = 200; the trunk is 48 px wide, centred (x 136-184).

| Layer | Contents | Scroll |
|---|---|---|
| backdrop | the sky gradient (raster, one colour per line, per time of day, the house dither step), the pond and the river (water colours per line, a slow shimmer) | - |
| BG4 | clouds, far mountains with snow caps, the far forest (the pond covers its foot as it rises: window 1 per line), the river's ripples | per line: clouds drift; mountains 1, forest 3/4 of the camera lean; ripples flow |
| BG3 | the dam (a canvas of 120 unique tiles: each log is drawn into them, pixel by pixel), the far banks and their trees, the near bank with the stump (its cut rings show while the trunk drops) | per line: 1/2 of the lean above the near bank, 0 for it |
| BG2 | the trunk and its branches (a 32 x 32 map redrawn on each gnaw; the drop is a vertical scroll in 4 frames; window 2 hides it under the stump) | the drop |
| BG1 | the house UI (games/common house_ui): text, panels, the logo, the timer bar (tiles of its own) | fixed |
| OBJ | the beaver(s), the tumbling log, chips, its branch, splashes, floating logs, the family, the woodpecker, the sun, moon and stars, the season's particles, the house digits, the acorns | |

Priorities: the far layer and the banks are high-priority tiles, so the sun, the moon, the stars and the floating
logs (sprites of priority 0) pass behind the mountains, the near bank and the dam; the log falling into the river
switches to priority 0 behind the near bank.

**Camera lean**: when the beaver changes side the camera eases 6 px toward it and the background follows at its
depth (the mountains the most, the dam half as much, the near bank not at all): a small parallax that makes the
valley deep without moving the trunk. The clouds drift and the ripples flow all the time.

**Palettes** (docs/art-direction.md): BG 0 the house UI (+ the timer bar in entries 11-14), 1 the trunk, 2 the
golden trunk, 3 the near bank and stump, 4 the dam, 5 the title logo, 6 the banks and their trees, 7 the far layer;
OBJ 0 the beaver, 1 player 2 (the same tiles, the fur's hue turned by house_style.hue_swap), 2 the wood (logs,
chips), 3 the house kit and the acorns, 4 sky and water effects, 5 golden wood, 6 the woodpecker, 7 the season's
particles. Every colour of the art belongs to these palettes (make_art.py): the C side swaps the season's ramps
(foliage, grass, snow, blossoms) and tints every palette by the time of day (dawn pink, sunset orange, night blue
at 46% brightness; sprites half as much; the sky effects not at all), fading over 1.5 s at a milestone.

2 players: two viewports of 159 px (the SDK's standard left/right layout). The BG2 map holds both trees (player
2's 16 columns further, wrapping), each viewport scrolls to its own; both show the middle of the same valley.
On the result, two more viewports lay the house banner and panel over both halves.

## The beaver
About 24x24 on screen (32x32 cells), code-drawn with the house helpers (ellipses and capsules shaded from the
top-left, the house eye, a 1-px dark brown outline): brown fur, a lighter belly and muzzle, **big orange
teeth**, a flat cross-hatched paddle tail, a small round ear. Frames: idle (2: a blink and a tail twitch),
**gnaw** (wind-up 0.96 x 1.04, the **bite squashed 1.12 x 0.9** with the head forward, the recovery), hop,
bonk (X eyes, a bump, stars circling), out of breath (slumped, eyes shut, rising z's), cheer (2, arms up).
Player 2 is the same beaver with a darker red-brown fur. The family on the bank: three 16x16 beavers (idle,
cheer). The woodpecker: 16x16, black and white with a red cap, two frames (head back, the peck).

## Audio
- Sound effects synthesised at start-up (src/sfx.c) with the house synthesiser (games/common house_audio):
  **chomp** (three quick bright noise bursts over a falling wood knock; each bite plays at a pitch varied
  +-6% by a small RNG), the **log splash** (a rush of low-passed noise and two bubbles), the **golden chime**
  (C6 E6 G6 C7 bells), the **branch bonk** (a hollow knock, then a boing up and down), the **milestone cheer**
  (G5 B5 D6 G6 over a swell of noise), the slump of the out-of-breath beaver, the woodpecker's tok, a whoosh for
  a stolen branch; the UI sounds are the house's (confirm, pause, swish, medal, the game-over sting). The low
  sounds are stored at 16 kHz to stay within the 64 KiB of sample memory.
- Loudness (house): chomp 88, bonk 110, chime and cheer 70, splash 56, tok 34; echo on the wet sounds (splash,
  chime). The voice budget: 8 voices with the music's 4; the frequent soft sounds (splash, tok) wait for their
  last one, and one voice is always kept free for a bonk.
- Music: a lively banjo/folk tune (tools/make_music.py with house_music.py): a 4-channel MOD in G major,
  a Karplus-Strong banjo rolling 16ths, a plucked double bass (boom on 1, the fifth on 3, walks), kick,
  brush and hat, a bowed fiddle tune with vibrato on its long notes. Two 8-bar sections (A: the fiddle tune,
  B: the banjo leads), each rendered at four tempos, **BPM 128, 144, 150, 160**. The game plays A, B, A, B...
  and picks the next section's tempo from the gnaws per second that hold the bar (under 5: 128, under 5.8: 144,
  under 6.4: 150, then 160), so the music speeds up with the drain. A section lasts 115200 / BPM frames, a whole
  number at these four tempos: the switch is counted in game frames, so a save state resumes it exactly.
## Feel sources
The reference: Timberman (Digital Melody, 2014, proprietary). Its feel is described by reviews and guides,
and several permissively licensed clones were tuned against it. Only numbers and rules were taken.

| # | Source | Licence | What we took (numbers and rules only) |
|---|---|---|---|
| 1 | **Timber** (the "Timber!!!" game of *Beginning C++ Game Programming*, J. Horton), the clone at https://github.com/aholmes2534/Timberman (`main.cpp`) | **MIT** (LICENSE file of the repo) | a 6-s bar (drawn 400 px wide), drained 1 s per second; each chop adds (2 / score + 0.15) s; 6 branch positions; a new branch is left 1/5, right 1/5, none 3/5; the hit check is on the lowest position, after the trunk moved down |
| 2 | **Timberman-clone**, carlosebmachado-games, https://github.com/carlosebmachado-games/Timberman-clone (`scripts/game.gd`, `scripts/time.gd`) | **MIT** (LICENSE file) | never two branch segments in a row, the first 3 segments plain, 10 segments; the bar drains 10% per second, a chop adds 1.8% |
| 3 | **lumberjack**, jayhxmo, https://github.com/jayhxmo/lumberjack (`Source/Tree/Tree.cs`, `Source/Mechanics/Health.cs`) | **MIT** (LICENSE file) | 16 segments; branches on 4 in 5 segments, the side after a branch forced; the bar (100) drains 1 per 50 ms (5 s) |
| 4 | **GameGrin**, "Timberman Review", https://www.gamegrin.com/reviews/timberman-review/ | article (facts cited) | the timer "gets quicker with each level", a level "every 20 branches chopped" |
| 5 | **iMore**, "Timberman: Top 8 tips, hints, and cheats", https://www.imore.com/timberman-top-8-tips-hints-and-cheats-you-need-know | article | a branch appears at the top of the screen 4 chops before it reaches the head |
| 6 | **Pure Nintendo**, "Review: Timberman (Nintendo 3DS)", https://purenintendo.com/review-timberman-nintendo-3ds/ | article | the 3DS version levels up every 50 chops (we use 50 for our scene milestones) |

### From the sources to Beaver Rush
- **The drain curve**. In source 1 the chop rate needed to hold the bar at score s is
  R(s) = 1 s/s / (2/s + 0.15) s = s / (2 + 0.15 s) chops per second (2.9 at s = 10, 5 at 40, 6 at 120, a
  limit of 6.67). Timberman raises the drain by level (sources 4, 6) instead of lowering the refill, so we
  keep the refill **fixed** and make the drain of level L equal to the refill times R(20 L + 10) (the rate
  at the middle of the level): the same pressure curve, in steps, as the reference.
- **The bar at level 0** lasts 6 s from full (source 1; source 3: 5 s, source 2: 10 s). With R(10) = 2.857
  chops/s to hold it, one gnaw refills 1/6 / 2.857 = **5.83%** of the bar (0.35 s of level-0 time).
- **Branches**: 40% at the start (source 1: 2 in 5), up to 60% (source 3 goes to 80%, too dense with our
  fairness gaps); source 2's "never two in a row" is relaxed into our fairness rule (same-side stacks are
  allowed, opposite sides need a gap), which keeps the reference's "switch or die" decisions.
- **Look-ahead**: 8 segments on screen (source 1 shows 6, source 2 10, source 3 16; the guide: 4 chops of
  warning at least).

### The final table (games/beaverrush/src/tuning.h)
| Quantity | Reference | Beaver Rush |
|---|---|---|
| bar at level 0, from full | 6 s | 6 s (360 frames) |
| refill per gnaw | 0.15 s + 2 s / score | 5.83% of the bar (fixed) |
| break-even rate | s / (2 + 0.15 s) chops/s, rising with every chop | the same curve per level of 20 logs: 2.9, 4.6, 5.3, 5.6, 5.8 ... 6.5 at 300, then +0.12 per level |
| level length | 20 branches (review) | 20 logs |
| branch chance | 40% | 40% rising to 60% (+4% per 50 logs) |
| segments shown | 6 | 8 (0..7) |
| golden log | - | 1 in 24 after 30 plain segments; +5 logs, +25% bar |
| milestone | 50 chops (3DS) | 50 logs: a dam section, the next scene |

## Tests (make beaverrush-check)
- `test_libretro`: the core loads, runs 600 frames, sound, save RAM, save states through the libretro API.
- `tests/test_rules.c` (14005 checks): the **timer curves** (a full bar lasts 360 frames at level 0; the refill
  is 7/120; the break-even rate of every level 0..60 within 1% of R(20 L + 10) plus the endgame ramp, growing),
  the **gnaw rules** (hit A, hit B, a branch leaving with its log, no drain before the first gnaw, out of breath
  after 6 s, golden logs, milestones and levels, medals), the generator's **fairness over 20000 seeds x 1000
  gnaws** (the invariant, and a brute-force check of the safe sides at every step; half the runs get stolen
  branches inserted at random; the checker itself is shown an impossible pair), the **statistics** per stage
  (the table above), the stolen-branch insertion, **determinism** (the same seed, the same trunk and run, in
  versus too; a re-roll keeps the segments on screen) and a perfect player at fixed paces (5/s: out at 165
  logs, 6/s: 450, 7.5/s: 721, 8.6/s: 888).
- `tests/smoke_test.sh`: strict mode clean with the music (VRAM, sprites per line, voices, sample memory) in 1P
  and versus; the title waits; one gnaw then nothing runs out of breath after 360 frames; one side only gets
  bonked; B and A gnaw; retry in one press; Start pauses and resumes; the best score persists in save RAM;
  player 2 joins; a versus round names a winner and a milestone sends a branch; the music's tempo rises with
  the level; **the bot** over 10 seeds (below); determinism of a scripted, a bot and a versus run (the state
  hash and the picture, twice).
- **The bot** (`--opt bot=1`, bot.c) plays from the **screen state only**: the BG2 map (which rows of its tree
  hold branch tiles, on which side) and OAM (its beaver's side, from the sprite's flip); it never reads the world.
  It plays like a quick, careful human: 7 to 9 frames between gnaws, 3 more to switch sides. Result (seeds 1-10,
  never bonked, always out of breath in the endgame ramp): **611 600 619 564 576 558 509 594 583 594, average
  580** (min 509, max 619); the test requires an average of 300 or more.
- `tests/ui_test.sh` + `tests/ui_check.py` (the UI screenshot test): ten screens shot at exact frames (title,
  play, the first milestone, the first golden log, night, winter, pause, game over with a medal, versus and its
  result) and checked against the run's state: the timer bar's filled pixels against the timer (to 1 px), the
  score digits, the beaver's teeth on its side (with the time-of-day tint computed as scene.c does), the logo,
  the prompt glyph, the copyright, the scene's name after the milestone, the golden log, the night sky, the
  dimmed pause, the panel, GAME OVER in gold and the bronze acorn, the split screen's divider, a score and a bar
  in each half, the result panel over both halves; the screens all differ; the title renders the same twice.
- `tests/state_test.sh`: save states at the title, in a run, in versus (pads and bots, with stolen branches),
  paused, over the panel and a retry, a winter night with the dam half built, and at five points of long runs:
  the state and the picture replay the same frame by frame, in the same process and in a fresh one; bad and
  foreign states are refused. `tools/state_audit.py`: every mutable static is saved or listed in state_audit.txt.

## Performance
`make beaverrush-bench` (tools/bench.sh, host: Ryzen 7 9800X3D under WSL2, music on): 1 player on a winter night
(snow, the woodpecker, the dam half built, logs in flight and floating) **0.52 ms per frame on average**
(PPU 0.51 ms), versus in two viewports **0.56-0.58 ms**; the worst frames measured 1.1-1.9 ms and move from run to
run (host scheduling; the restart of a skipped test run redraws 560 dam logs at once). At the usual 15-20x for
the A20 (Cortex-A7): **8-12 ms on average, 50-70% of the 16.7-ms frame**, with the rare worst frames at 17-38 ms.
Sprites: 103 at most, 32 on a line at most (the guideline); VRAM, voices and sample memory within the guidelines.

## Screenshots
`make beaverrush-screenshots` (tools/screenshots.sh, 2x): docs/screenshots/ title, early-play, milestone,
golden-log, night, autumn-sunset, winter, spring, pause, gameover-silver, get-ready-2-players, versus,
versus-over.

## Remaining work: the house title flow and the 4-player versus (owner request, unfinished)
The game above still has the old flow (get ready, 2 players). A work-in-progress conversion sits on the branch
`wip/beaverrush-4p` (commit d4e01eb, src/ and tools/ only). It builds and plays 1-4 players, but it does not pass
`make beaverrush-check` yet. It contains:
- the kit's title as the only menu (`hu_title_setup` with Left/Right/A/B: "PRESS <-/-> TO GNAW"; pads 2-4 join
  with A and leave with B; the start press is P1's first gnaw), instant retry into play (the press gnaws), Select
  on the results back to the title; `players=N` and `bot=N` up to 4;
- the 4-player layout: **4 full-height columns** (`hu_split` HU_SPLIT_COLUMNS, 78 px; 3 players: 105 px). The
  columns keep all 8 segments of look-ahead and the full-height scene (the raster sky, the windows and the line
  scrolls are by screen line), where 2x2 quadrants (119 px tall) would show only 4 segments. The BG2 map is
  64 x 32 (four trees 16 columns apart); in 4 columns the beaver stands 6 px closer to the trunk;
- the palettes: one beaver palette (OBJ 0) for four beavers: the raster loads each viewport's player colours
  (entries 1-5, `house_style.player_palettes`) per line; on the title OBJ 5 and 6 (gold, bird: unused there)
  hold P3's and P4's colours for their icons; a 16x16 beaver-head icon (make_art.py `icon`);
- the rules (world.c): the stolen branch goes to the leader among the other beavers still gnawing (ties: the
  first after the sender in turn order; so the runner-up when the sender leads); a beaver out is out, the others
  go on, the last one standing wins (all out at once: by score, else a draw); the ranking is the elimination
  order, then the score (`world_rank_keys`, `hu_rank`), shown with `hu_results_panel` in overlay viewports.

To finish:
1. Adapt the tests to the new flow: smoke_test.sh (no get ready: st=1 is gone, `ready=1` now waits in play;
   retry plays at once), ui_test.sh / ui_check.py (the title's D-pad glyph instead of the A glyph, the 2-player
   results panel), state_test.sh; then add the 4-player ones (joins on the title, a 4-bot run, leave with B,
   the stolen branch to the leader in test_rules.c, the last beaver standing, determinism and save states with 4
   players, title shots with 0-3 players joined, 4-player play and results).
2. Strict mode: VRAM is 66752 bytes on the branch (guideline 65536: the 64-wide BG2 map, the D-pad glyph, the
   icon): trim about 1.3 KB (BG2 back to 32 x 32 with trees packed only as wide as a column shows, or fewer
   logo tiles); 34 sprites on one line with 4 bots (fewer chips or weather sprites in 4 columns).
3. Save RAM version 2 (vs_wins for 4 pads, version 1 read once) is in main.c: test it.
4. Screenshots, DESIGN.md (flow, controls, versus, palettes, tests), dist/README-windows.txt, `make
   beaverrush-dist`.
5. Kit notes: `hu_results_sprites` draws at screen coordinates (a game showing the panel in an overlay viewport
   must move the sprites itself); the kit draws its own medals (Beaver Rush draws its acorns at the kit's spots).
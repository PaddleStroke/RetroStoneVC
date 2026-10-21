# Prickle Patch: design

(c) 2026 Pierre-Louis Boyer (8BCraft), CC BY-NC-SA 4.0 (games/pricklepatch/LICENSE). A game for the RetroStone
virtual console (docs/spec.md), in the 8BCraft house style (docs/art-direction.md). The owner's idea: a survivor
auto-shooter, the genre where you only move and your weapons work by themselves against endless waves. The theme, the
name, the gardeners, the pests, the tools, the art, the music and every number are our own.

## Pitch
A little hedgehog in a straw hat keeps the vegetable patch. Slugs, aphids, beetles, ants, wasps, walking thistles,
crows and locusts come from every side, more and tougher with the minutes. The hedgehog only walks: its garden tools
work by themselves (a seed shot, a watering can, bees, a toy mower, a boomerang rake, pumpkin bombs, a ring of
prickles, a chili trail, dandelion puffs). Pests pop into puffs and leave gems; gems bring levels; each level offers
three choices: a new tool or a passive, or a level more of one. A tool at its top level with its passive evolves.
One endless run: survive as long as you can. Bosses come at 5:00, 10:00 and 15:00; chasing off the Crow King is a
milestone (GARDEN SAVED!, the pests on the field pop, a big chest), then the run goes on, the bosses come back
stronger every five minutes and the garden grows wilder until the gardener is down. The score: the time survived
(and the pests chased). Coins buy small boosts in the garden shed for the next runs, and two more gardeners: a sturdy tortoise and a quick hen. Family-friendly: pests
are chased off and pop, nothing is hurt.

## Controls
| | Console (pad) | Phone (touch, the game reads the fingers) | Keyboard |
|---|---|---|---|
| walk (8 ways) | D-pad | a floating stick: put a finger down anywhere and drag (the farther, the faster) | arrows |
| a level-up's choice | Up / Down, A | tap it | arrows, X |
| a chest | A | tap | X |
| pause | Start or Select | the app's pause button | Enter / Backspace |
| paused: back to the title (the run kept) | B | tap B: TITLE (a tap elsewhere: go on) | Z |
| title: the gardener | Left / Right | tap the name's left / right half | arrows |
| title: the garden (once open) | Up / Down | - | arrows |
| title: start / continue | A or Start | tap the prompt | X / Enter |
| title: a new run (when one is kept) | B | tap B: NEW RUN | Z |
| title: the garden shed | X, Y or Select | tap its line | V, C |
| shed: choose, buy, back | Up / Down, A, B | tap a boost (again: buy), tap the keys' line | arrows, X, Z |

`rs_game.touch_native = 1`: the app passes the fingers (apps/launcher/games.cfg has no zone for it). The level-up
panel ignores the fingers and buttons for 18 frames (a walk does not pick by accident).

## The run (src/world.c: the rules, no SDK call but its RNG; src/game.h: the numbers)
Positions are in 1/16 px (Q4), 60 Hz, deterministic (the world's own xorshift RNG, seeded from the start's frame or
`--opt seed`). The field is endless (the garden's look comes from a hash of the world position). The pests spawn on an
ellipse of 300 x 236 px around the gardener (outside every screen, the same at every screen size: a run is the same in
landscape, 4:3 and upright, frame for frame) and walk to it; one farther than 560 px is brought back to the ring, ahead
when the gardener walks.

| | Value |
|---|---|
| the gardener | hedgehog 100 health, 1.0 px/f, the seed shot; tortoise 140, 0.81 px/f, armour 2, the watering can (250 coins); hen 80, 1.19 px/f, gem reach +30%, the bees (500 coins) |
| a touch | the pest's damage minus the armour, then 30 frames safe (blinking) |
| gems | reach 30 px (the magnet +25% a level), then they fly in, faster and faster; one 8-px cell holds one gem (two merge: 1-4 violet, 5-19 red, 20+ gold) |
| levels | level n to n+1: 5 + 3(n-1) + (n-1)^2/4 gems (5, 8, 12 ... 52 at level 10, 147 at 20) |
| choices | 3, weighted: a level of an owned tool 8, an evolution 20, a new tool 4 (6 at most), a level of a passive 5, a new passive 3 (6 at most); everything at its top: a snack (+30 health) or a coin pouch (+25) |
| drops | a gem always; a coin 1.4% (+0.3% a clover level), an apple (+30 health) 0.3%, a magnet (every gem flies in) 0.2% after 1:00 |
| chests | an elite's (1 upgrade), a boss's (3): an evolution first when one is ready, else levels of what is owned |

### The spawner, minute by minute (world.c WAVES)
The number of pests kept on the field grows smoothly through each minute; the mix (weights) changes:

| minute | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| pests | 12 | 26 | 40 | 55 | 70 | 85 | 100 | 120 | 140 | 160 | 180 | 200 | 220 | 240 | 260 | 280 |

They come at 2 + minute a second at most (a trickle at first, 17 a second at 15:00). Their health grows with the time (x1
at 0:00, x1.67 at 10:00, x2 at 15:00), their touch too (x1.25 at 7:30, x1.5 at 15:00). A boss out: three quarters
of the others. The moonlit garden (opened by a run past 10:00): health x1.25, speed x1.125, 12% more pests.

| pest | health | speed (px/f) | touch | gem | from |
|---|---|---|---|---|---|
| slug | 8 | 0.44 | 5 | 1 | 0:00 |
| aphid | 5 | 0.69 | 3 | 1 | 1:00 (rings) |
| beetle | 26 | 0.44 | 7 | 2 | 2:00 (heavy: little knock-back) |
| ant | 12 | 0.94 | 6 | 1 | 3:00 (marching columns) |
| wasp | 18 | 0.81 | 7 | 2 | 4:00 (zigzags) |
| thistle | 60 | 0.31 | 10 | 4 | 5:00 |
| crow | 40 | 1.12 | 9 | 3 | 7:00 (swoops through) |
| locust | 28 | 0.88 (hops x1.5) | 8 | 3 | 8:00 |
| Slug Queen | 2500 | 0.5 | 14 | 60 | 5:00: six slugs come out every 4 s |
| Stag Beetle | 9000 | 0.44 | 15 | 100 | 10:00: stops, shakes 0.75 s (the warning), charges 3 px/f for 0.73 s |
| Crow King | 36000 | 0.75 | 22 | 140 | 15:00: circles at 110 px, dives through, calls crows; chased off: the 15:00 milestone, the run goes on |

Events: a ring of 20 aphids closing in at 1:30, an elite beetle with a chest at 3:00, a column of 18 ants at 4:00, a
ring of 32 aphids at 6:30, an elite thistle at 7:30, a wasp swarm at 8:30, a ring of 36 locusts at 11:30, an elite
crow at 12:30, a column of 30 ants at 13:30, a crow swarm at 14:00. An elite: health x12, gold, a chest.

### The tools (world.c fire(); levels 1..5, then the evolution with its passive)
| tool | level 1 | levels 2-5 | evolution (with) |
|---|---|---|---|
| seed shot | 1 seed at the nearest pest, 10, every 52 f | +1 seed, 14, +1 seed, 18 and goes through one | Sunflower Storm (seed pouch): 4 seeds of 24 through 3, a ring of 8 every other volley, every 36 f |
| watering can | a splash ahead (52 x 26 px), 13, every 75 f, pushes back | also behind, 20, bigger, 27 every 62 f | Rainbow Spray (sun hat): a ring 64 px round, 36, every 54 f |
| bee buddies | 1 bee circling at 30 px, 7 a sting (each pest every 30 f) | +1 bee, 11, +1 bee, +1 bee and 38 px | Royal Hive (honey pot): 6 bees and their queen at 46 px, 16 |
| toy mower | rolls 220 px along the walk, 22 to all it meets, every 140 f | 30, every 115 f, a second one backwards, 40 | Turbo Mower (sundial): bounces round the gardener 4 s, 50, hits again |
| boomerang rake | thrown at the nearest pest, comes back, 16 both ways, every 100 f | 22, +1, 30 and faster, +1 | Golden Rake (magnet): 4 rakes round, 40, they gather gems |
| pumpkin bomb | lobbed at a pest, bursts 26 px, 30, every 110 f | +1, 34 px, 45, +1 | Giant Pumpkin (compost): 52 px, 80, 8 seeds fly out |
| prickle ring | a pulse 28 px round, 5, every 24 f | 34 px, 8, 40 px, 11 every 20 f and pushes | Bramble Crown (honey pot): 56 px, 18 every 16 f, heals on what it chases off |
| chili trail | a flame behind the walk every 20 f, 10 px, 6 a tick, 2 s | 9, 3 s, 13 px, 13 every 14 f | Fire Path (garden boots): 15 px, 20, every 8 f (also standing) |
| dandelion puffs | 2 puffs drifting out, 9, 4 s, every 70 f | +1, 13, +1, 6 s | Wishing Wind (clover): 6 puffs that find the pests, 20 |

Passives (5 levels): compost +10% damage, sun hat +10% area, garden boots +8% speed, honey pot +15 health and 1 health
every 2 s a level, magnet +25% gem reach, sundial -6% cooldowns, seed pouch +1 shot (levels 1, 3, 5: seeds, rakes,
pumpkins, bees, mowers, puffs), clover +8% XP and +15% coins (and luckier drops).

### The endless run
There is no end: the gardener plays until it is down. The 15:00 milestone (the Crow King chased off): the banner
GARDEN SAVED!, two seconds with no new pest while the ones on the field pop, the boss's chest (3 upgrades) and its
coins. Then: a boss again every 5 minutes from 20:00 (the Slug Queen, the Stag Beetle, the Crow King in turn, with
health x(2 + round)), a ring, a column, a swarm or an elite every minute from 16:00, the pests' health climbing
faster and faster (x10 at 20:00, x28 at 25:00, x58 at 30:00), their touch +0.2 a minute (x2.6 at 20:00, x3.8 at
25:00), their speed +3% a minute. The results: TIME (NEW BEST, or the best to beat), PESTS CHASED, LEVEL, COINS; the
best time and its pests are kept (the title shows them). The bot's strongest runs (a full build) end at 26:00-28:00.

### Difficulty
A gentle first three minutes (the tests: the bot, with random choices, survives them on three seeds, and 5:00 on a
fourth), the first boss and a real crowd by 5:00-7:00, the Stag Beetle and a full arena at 10:00, the most pests and
the Crow King at 15:00: survivable with a good build, tense; then harder and harder. The bot (it flees the pests, leaves many gems, chooses at
random) lasts to 10:00-11:00 on most seeds, to 15:00 on about one in four; a player who gathers the gems and chooses
well gets further. A normal phone player meets the first elite (a chest) at 3:00 and should reach the Slug Queen.

### The garden shed (the meta-progression, save RAM)
Coins: picked up in a run, plus 1 per 12 seconds survived and 25 per boss chased off. Six boosts of 5 levels (price:
base x (level + 1)): might +4% damage (60), vigor +8 health (50), swift +3% speed (60), reach +10% gem reach (40),
growth +4% XP (70), haste -3% cooldowns (80). The tortoise 250 coins, the hen 500. The moonlit garden opens after a
run past 10:00.

## The soft sprite layer (src/soft.c): hundreds of pests within the sprite limits
The console has 128 sprites, 32 on a line. The swarm (every pest and boss), the seeds, the puffs, the gems flying and
the poofs are drawn by the CPU into the 8x8 tiles of BG2, like a bitmap but only where something is: each frame, each
8x8 cell of the screen an object touches takes a tile from a pool (768 at 320 x 240, within the 64 KiB VRAM guideline
with everything else; 1023 on the phone's wider or taller screens), pixels painted under what is there (the nearest
pests first: a pool too small leaves out the farthest, at the screen's edge), uploaded with `rs_tiles_load8`, the map
entries set, last frame's cleared. The empty cells show the pool's tile 0, kept clear: uploaded blank with the used
tiles every frame, and the title's logo (which borrows the pool's tiles while no run is drawn) starts at tile 1
(build_assets.py checks the VRAM ranges at build time; tests/tall_test.sh compares the garden after the title with a
run's at nine screen sizes: the logo once left a dash in every cell). One 15-colour palette for all of them; a hit's flash and an elite's gold are
colour remaps. The art is cropped to what it draws (fewer cells). The gems lying on the ground are tiles of the
garden's map (BG4) itself: an 8-px cell, its tile the gem's (a gem's glint cycles in the palette). The hardware
sprites are kept for the gardener and its health bar, the tools that move (bees, mowers, rakes, pumpkins, bursts,
flames, the splash, the prickle ring), the pickups, the number pops (16 sprites of 16x8 drawn at run time) and the UI;
a budget in draw.c keeps them under 124 and 30 a band of 8 lines (strict mode never warns: the tests run with it).

Measured (tests/bench_test.sh, host; the A20 estimate x15-20): the first minutes 0.26 ms a frame, minute 10 with the
Stag Beetle and a full build 0.31 ms, minute 14 0.34 ms (0.48 at 520 columns, 0.45 upright), all under the 0.83 ms
budget (A20 estimate at 4:3: 5-7 ms of the 16.7). The pool peaks at 581 tiles at 4:3 (of 768), never short in the
benches; with 272 pests packed round a god-mode bot (an earlier, denser spawner) it ran short in 8% of the frames, the
far pests then not drawn for a frame.

## Screen and layers
320 x 240 at 60 Hz. Back to front: BG4 the garden (and the gems), sprites prio 1 (pickups), BG2 the soft layer,
sprites prio 2 (the gardener, the tools, the pops; OBJ 4 added at half: the splash, the ring, the touch stick, the
glows), BG1 the house kit (the HUD: the XP bar, the timer in 2x, the level, the pests chased; the boss's bar; the
panels), prio 3 the kit's sprites and the panels' icons. The camera stays on the gardener (shake on a hit, 2 px; on a
boss chased off, 4 px; a 30-frame hit-stop).

**Widescreen**: more garden on both sides. **Tall screen** (a phone held upright): `rs_screen_zoom(440)` (202 of the
320 columns on a 1080x2340 phone, the garden zoomed and taller: more of it above and below), the HUD pinned at the
screen's top (`rs_bg_pin`), the level counter under the timer (clear of the app's corner buttons), the title, the
panels and the pause in house_tall.h's layouts (the panels upright show without the HUD). A state saved upright loads
in landscape and back (`ht_shape_reset`, tests/tall_test.sh).

## Sound and music
House synthesiser effects (main.c sound_init): a seed's pop, a hit's tick, a pest's pop, the gem's ping (pitched up
by a quick run of gems; a coin: the same higher), the level-up chime, a hurt "oof", the splash, a pumpkin's burst, a
rake's or a mower's whoosh, a chest's fanfare, a boss's rumble; the house set for the rest (the medal for a boss chased
off and the 15:00 milestone). Two 4-channel MODs (tools/make_music.py, original): "Garden Patrol" (G mixolydian, the
title and the run) and "Big Pest" (A minor, while a boss is out).

## Save RAM
`PRKL`, version 1: the best run (its seconds at offset 8: apps/launcher/games.cfg best=8; its pests, its level), the
coins, the gardeners bought, the boosts, the gardens open, the last gardener and garden, runs, the runs past
15:00, and the run in progress (a snapshot: the time, level, XP, health, tools and passives, the spawner's events
done, the endless part's rounds, the RNG; written every 30 s and when the run is left from the pause; CONTINUE
starts it again on an empty field at that time).

## Tests
`make pricklepatch-check`: the libretro loader; the smoke tests (the title, buying a gardener, the shed, a level-up's
choice, a chest, pause, the run kept and continued or replaced, the game over and the best run in save RAM, the bot
surviving 3:00 with random choices on three seeds and 5:00 on a fourth, minute 10 and 15 within the guidelines in
strict mode, determinism); the screens; the save states (title, run, level-up, paused, shed, a late garden, the game
over) and their audit; the widths 320 / 427 / 520 and states across them; the tall screen and the phone turning; the
frame cost (bench_test.sh); the 22 languages (tools/i18n_test.sh, tests/i18n_runs.txt).

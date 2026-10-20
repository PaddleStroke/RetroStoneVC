# Puffin Plunge: design

(c) 2026 Pierre-Louis Boyer (8BCraft), CC BY-NC-SA 4.0 (games/puffinplunge/LICENSE). A game for the RetroStone
virtual console (docs/spec.md), in the 8BCraft house style (docs/art-direction.md). The owner's idea: a vertical fall
shooter. The theme, the name, the hero, the critters, the art, the music and every chunk of the well are our own.

## Pitch
A little puffin in bubble boots dives down a well that never ends. It runs and jumps on the ledges; in the air the same
button fires bubbles straight down from its boots: they pop the critters below and each shot slows the fall. The bubbles
run out; landing refills them, and so does a stomp on a soft critter (it bounces the puffin up). Critters popped one
after the other without touching the rock make a combo, paid in gems on landing. Gems buy upgrades at the rest stops
and in the little side rooms on the way down; hearts and upgrades last the whole dive. The deeper, the harder: four
zones again and again, tinted darker each round, a grumpy walrus at the end of every frozen zone. The score is the
depth plus the gems and combos; the deepest dive is a gold rope in the well.

## Controls
| | Console (pad) | Phone (touch, apps/launcher/games.cfg) | Keyboard |
|---|---|---|---|
| run | D-pad Left / Right | the drawn Left / Right buttons (bottom left) | arrows |
| jump (on a ledge; released early: lower) | A (B, X, Y too) | the big A button (bottom right) | X |
| fire down (in the air; held: again and again) | A (B, X, Y) | the big A button | X |
| a side room | push into its door from the ledge | the same | |
| shops: choose, buy | the D-pad, A; the last line dives on | Left / Right, A | arrows, X |
| pause | Start or Select (paused: A goes on, B the title) | the app's pause button | Enter / Backspace |

The title: A dives; it shows BEST (the score) and DEEPEST: 534 M. A dive left from the pause (B) is kept in save RAM
(also at each new zone): the title then says IN PROGRESS: 534 M and offers CONTINUE THE DIVE or NEW DIVE (Left / Right
choose, A goes; B: a new dive); it goes on from the start of the chunk where it was left, with everything the puffin
carried. A new zone shows its card (its name, its depth); a first dive shows the controls' hint.

## The puffin (src/game.h: every number; src/world.c: the rules, no SDK call)
Positions are whole pixels with a 1/256 remainder; every move goes pixel by pixel against the rock. 60 Hz.

| | Value | |
|---|---|---|
| run | 2.0 px/f; full speed in 4 frames, a stop in 3-4, a turn in 2-3 | the air: 0.375 px/f^2, a third of the drag |
| jump | 4.6 px/f, gravity 0.28 px/f^2 | about 38 px (2.4 cells); released while rising: cut to 1.5 px/f |
| fall | 4.0 px/f | terminal velocity |
| coyote time | 5 frames | A just after leaving a ledge still jumps (not fires) |
| jump buffer | 6 frames | A just before landing with no bubbles left: a jump on landing |
| fire | a new press in the air, then held: again every `period` frames | each shot sets the fall to `hover` (a little up) |
| bubbles | 8 (the tank: up to 16) | refilled on landing and by a stomp |
| stomp | 4.0 px/f up (A held: 4.8), 2 damage | only on soft critters; a spiky one stings and bounces |
| hurt | one heart, 90 frames safe (blinking), a 6-frame hit-stop, a small kick up | the bubble shield takes one hit (back at each rest stop and zone) |
| hearts | 4 (up to 8) | |
| gems | fly to the puffin from 22 px (the magnet: 64) | a big gem is 5 |

The boots' modules (one at a time; the shops sell them):

| | Period | Bubbles | Damage | Hover | |
|---|---|---|---|---|---|
| bubbles | 7 | 1 | 1 | -0.6 px/f | one straight down, 7 px/f |
| triple bubbles | 11 | 1 | 1 each | -0.2 | three in a fan |
| sparkle beam | 16 | 2 | 3 | -1.6 | a ray 176 px down through every critter, breaks a crystal |
| rapid bubbles | 4 | 1 | 1 | -0.4 | |

Combos: each critter popped while the feet are off the rock counts; landing pays combo x 2 gems from 4 (combo x 3
from 8) and a heart from 10. The camera keeps the puffin a third of the way down the screen and looks up to 36 px
further ahead of a fall (smoothed in the world: the same at every screen size).

## The endless well (levels/README.md: the chunks' format; levels/chunks.txt; src/world.c gen_chunk)
1 m = one cell (16 px). The well is a ring of 128 rows round the puffin (made 64 rows ahead, forgotten 44 rows above),
cut in chunks of 10 x 12 cells; chunk k (metre 12 k) is made from k and the dive's seed alone, so a CONTINUE makes the
same well. Every chunk and every join is checked passable down to 4800 m (tests/test_world.c).

| Every | What |
|---|---|
| chunk | one of its zone's hand-made chunks (24, 18-19 a zone), maybe mirrored |
| 300 m (25 chunks) | a new zone: grotto, coral, mushroom, frozen, then the four again; a gate (two ledges, a gap) at its top, its card and its tune; from the second round the zone is tinted (violet, crimson, then night) |
| 180 m (15 chunks) | a rest stop: a glowing ledge across the shaft with a gap at one side; landing on it opens its shop (once) |
| 1200 m (the end of each frozen zone) | the walrus's cave (two chunks): the Grumpy Walrus on a floor that gives way once it is beaten |

The difficulty (src/game.h DIFF_M, GENTLE_M): the first 180 m are as gentle as the first level was (critter letters
bring one 56% of the time, half the urchins are snails); then a step every 120 m: critters 60% + 4% a step (95% at most)
and 5% faster a step (twice as fast at most), fewer safe ledges (a `?` cell is rock 55% - 3% a step, 25% at least),
prices up a step. From the second round of the zones: spikier (a walker or flyer may be the zone's spiky one, 15% + 5% a
round) and tougher (+1 hit every two rounds). Side rooms: a door in a wall on a ledge (70% of the `D` letters, at most
one a chunk), a little shop (2 offers, 20% cheaper) or a gift (one of two free), a gem or a heart bobbing over its door;
push into the door from the ledge to go in.

| Zone | Critters | Look |
|---|---|---|
| 1 the crystal grotto | snails (walk the ledges), bats (hang under the rock or hover; wake and flutter at the puffin), crystal urchins (spiky) | violet rock, crystal shards, cyan glints |
| 2 the coral trench | crabs (fast), fish (swim across), jellyfish (sting, bob), pufferfish (soft, spiky when puffed), sea urchins | blue rock, pink coral |
| 3 the mushroom cave | beetles (3 hits), moths (weaving), puffcaps (shoot spores up), jellyfish, thorny burrs | brown rock, moss, glowing mushrooms |
| 4 the frozen deep | woolly grubs, ice bats (fast), icicles (drop when the puffin passes under), frost wisps (drift through the rock to it), ice spikes | icy rock, snow, frozen fish |

The Grumpy Walrus (24 hits, 10 more each round; its health bar over the shaft): it waits for the puffin, slides along
the floor, rears up and throws snowballs at it (three when angry, under half its health), bounces on its belly. Stomps
hurt it (soft), bubbles too; its snowballs pop. Beaten: 300 points, 20 gems, a heart, YOU DID IT! and its floor gives way.

## The shops
At a rest stop: three offers among those the puffin can use (a heal always when hurt), Up / Down (or Left / Right)
choose, A buys, the last line (DIVE ON) goes on down. Prices: base + per difficulty step (src/world.c PRICE, 14 steps at
most): extra heart 24 + 5, warm soup (2 hearts) 12 + 3, bigger tank (+2 bubbles) 16 + 4, triple bubbles 30 + 5,
sparkle beam 36 + 6, rapid bubbles 30 + 5, gem magnet 24 + 4, spring boots (bigger stomps) 24 + 4, feather cape (a
slower fall firing) 24 + 4, bubble shield (blocks a hit; back at each rest stop and each zone) 28 + 4, lucky clover (half as
many gems more) 24 + 4.

## Score
A point a metre, gems picked up (1, a big one 5), 5 a critter, 2 a crystal, combo bonuses x 2, 300 a walrus. The best
score (games.cfg best=8) and the deepest dive in save RAM; passing the deepest dive: NEW BEST! (the rope, its pennant
and its depth in the well). Medals at 400, 1200, 2500, 4500.

## Screen and layers
320 x 240 at 60 Hz. BG3 the back wall (the zone's dark rock, at half the speed), BG2 the shaft (the rock autotiled from
8x8 quarters with the zone's cap, crystals, doors, the floor) and the rock beside it (a darker palette, the zone's
decorations), BG1 the house kit (cards, shops, results, PAUSED). Sprites: the puffin (OBJ 0), the critters (OBJ 1, the
zone's palette), items and the HUD (OBJ 2), the kit (OBJ 3), the effects (OBJ 4), the walrus (OBJ 5), the hit flash
(OBJ 6: white). The HUD: hearts, the gems (big digits, a pulse when one arrives, gems zip to it), the depth (534M), the
combo, the bubbles' gauge beside the shaft. Two zones can show at once (a zone's change): their tiles and colours in two
slots (zone & 1: BG palettes 1-3 and 4, 6, 7; the critters OBJ 1 and 7).

**Widescreen**: the shaft (192 px) centred, more rock and decorations on its sides; the HUD stays by the shaft.
**Tall screen** (a phone held upright): `rs_screen_zoom(480)`: 202-220 columns shown, the shaft fills them; the stage's
bottom at the screen's bottom (the touch buttons' band, over the shaft far below), the puffin at 30% of the height:
about 300 px of the well below it. The HUD at the screen's top (sprites), the bubbles' gauge over the right wall. The
title, the cards, the shops and the results in house_tall.h's layouts, pinned on the screen. A state saved upright
loads in landscape and back (`ht_shape_reset`).

## Feel
Squash on landing, stretch on the jump, dust (jump, landing, stomps), a muzzle flash and a spent shell spinning away at
every shot, a white flash on every hit, a poof and sparks and a 2-px shake on a pop, shards on a crystal, the combo
counter popping over the puffin and by the HUD, gems zipping to the counter, hit-stop on a stomp (2 frames) and a hurt
(6), a 3-px shake on a hurt.

## Sound and music
House synthesiser effects (main.c sound_init): jump, bubble shot, beam, hit, pop, stomp, gem (pitched up along a
chain), landing, hurt, crystal, empty, combo (pitched up), buy, door, the walrus. A 4-channel MOD per zone and one for
the walrus (tools/make_music.py: square lead, pluck arpeggio, bass, drums), original.

## Tests
`make puffinplunge-check`: the rules (tests/test_world.c: every chunk passable, the well down to 4800 m on three
seeds, every chunk and join passable, a CONTINUE makes the same well, the bot past 150 m on 40 seeds and how deep it
goes, determinism), the libretro loader, the smoke tests (the flow, a rest stop's shop, the side rooms, pause, the dive
kept and continued, the game over, the best score and depth kept, the deepest dive's marker passed, the bot past 150 m
with strict mode, five depths down to 2700 m, a walrus beaten, determinism), the screens, the save states and their audit, the widths 320 / 427 / 520 and
states across them, the tall screen and the phone turning (tall_test), the frame cost (bench_test), the languages
(tools/i18n_test.sh).
